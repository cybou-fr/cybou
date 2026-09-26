// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft_engine.h>
#include <support/cleanse.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace cybou {

namespace {

inline void AppendUint64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

constexpr size_t SIGNING_RECORD_CBS1_SIZE{4 + 32 + 32 + 8 + 4 + 1 + 32 + 32};
constexpr size_t SIGNING_RECORD_CBS2_MIN_SIZE{4 + 32 + 32 + 8 + 4 + 1 + 32 + 4 + 4 + 32};

struct SigningRecord {
    uint8_t version{1};
    uint256 network_id;
    uint256 validator_id;
    uint64_t height{0};
    uint32_t round{0};
    BftStep step{BftStep::PROPOSE};
    uint256 digest;
    int32_t locked_round{-1};
    std::optional<CybouBlock> locked_block{std::nullopt};
};

uint64_t ReadUint64LE(const unsigned char* bytes)
{
    uint64_t value{0};
    for (int i = 0; i < 8; ++i) value |= uint64_t{bytes[i]} << (8 * i);
    return value;
}

uint32_t ReadUint32LE(const unsigned char* bytes)
{
    uint32_t value{0};
    for (int i = 0; i < 4; ++i) value |= uint32_t{bytes[i]} << (8 * i);
    return value;
}

bool WriteSigningRecord(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    auto temp = path;
    temp += ".tmp";
#ifdef _WIN32
    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written{0};
    const bool written_ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) &&
        written == bytes.size() && FlushFileBuffers(file);
    CloseHandle(file);
    if (!written_ok) { DeleteFileW(temp.c_str()); return false; }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return true;
#else
    const int fd = open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, S_IRUSR | S_IWUSR);
    if (fd < 0) return false;
    size_t offset{0};
    while (offset < bytes.size()) {
        const ssize_t n = write(fd, bytes.data() + offset, bytes.size() - offset);
        if (n <= 0) break;
        offset += static_cast<size_t>(n);
    }
    const bool written_ok = offset == bytes.size() && fsync(fd) == 0;
    if (close(fd) != 0 || !written_ok) { unlink(temp.c_str()); return false; }
    if (rename(temp.c_str(), path.c_str()) != 0) { unlink(temp.c_str()); return false; }
    const auto parent = path.parent_path().empty() ? std::filesystem::path{"."} : path.parent_path();
    const int dirfd = open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (dirfd < 0) return false;
    const bool synced = fsync(dirfd) == 0;
    close(dirfd);
    return synced;
#endif
}

std::optional<SigningRecord> ReadSigningRecord(const std::filesystem::path& path)
{
    std::error_code ec;
    if (std::filesystem::is_symlink(path, ec) || ec) return std::nullopt;
    const auto file_size = std::filesystem::file_size(path, ec);
    if (ec || file_size < SIGNING_RECORD_CBS1_SIZE || file_size > MAX_SIGNING_RECORD_BYTES) return std::nullopt;

    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> bytes(file_size);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return std::nullopt;

    if (file_size == SIGNING_RECORD_CBS1_SIZE && std::equal(bytes.begin(), bytes.begin() + 4, "CBS1")) {
        uint256 checksum;
        CSHA256().Write(bytes.data(), SIGNING_RECORD_CBS1_SIZE - 32).Finalize(checksum.begin());
        if (!std::equal(checksum.begin(), checksum.end(), bytes.begin() + SIGNING_RECORD_CBS1_SIZE - 32)) {
            return std::nullopt;
        }
        if (bytes[80] > static_cast<unsigned char>(BftStep::PRECOMMIT)) return std::nullopt;
        SigningRecord record;
        record.version = 1;
        std::copy_n(bytes.begin() + 4, 32, record.network_id.begin());
        std::copy_n(bytes.begin() + 36, 32, record.validator_id.begin());
        record.height = ReadUint64LE(bytes.data() + 68);
        record.round = ReadUint32LE(bytes.data() + 76);
        record.step = static_cast<BftStep>(bytes[80]);
        std::copy_n(bytes.begin() + 81, 32, record.digest.begin());
        record.locked_round = -1;
        record.locked_block = std::nullopt;
        return record;
    }

    if (file_size >= SIGNING_RECORD_CBS2_MIN_SIZE && std::equal(bytes.begin(), bytes.begin() + 4, "CBS2")) {
        uint256 checksum;
        CSHA256().Write(bytes.data(), file_size - 32).Finalize(checksum.begin());
        if (!std::equal(checksum.begin(), checksum.end(), bytes.begin() + file_size - 32)) {
            return std::nullopt;
        }
        if (bytes[80] > static_cast<unsigned char>(BftStep::PRECOMMIT)) return std::nullopt;
        const uint32_t block_len = ReadUint32LE(bytes.data() + 117);
        if (file_size != SIGNING_RECORD_CBS2_MIN_SIZE + block_len) return std::nullopt;

        SigningRecord record;
        record.version = 2;
        std::copy_n(bytes.begin() + 4, 32, record.network_id.begin());
        std::copy_n(bytes.begin() + 36, 32, record.validator_id.begin());
        record.height = ReadUint64LE(bytes.data() + 68);
        record.round = ReadUint32LE(bytes.data() + 76);
        record.step = static_cast<BftStep>(bytes[80]);
        std::copy_n(bytes.begin() + 81, 32, record.digest.begin());
        record.locked_round = static_cast<int32_t>(ReadUint32LE(bytes.data() + 113));
        if (block_len > 0) {
            auto blk = DeserializeBlock(std::span<const unsigned char>(bytes.data() + 121, block_len));
            if (!blk || blk->height != record.height) return std::nullopt;
            record.locked_block = std::move(*blk);
        } else {
            record.locked_block = std::nullopt;
        }
        return record;
    }

    return std::nullopt;
}
} // namespace

uint256 ComputeProposalDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& proposer_id,
    const uint256& block_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_PROPOSAL/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());

    unsigned char h_bytes[8];
    for (int i = 0; i < 8; ++i) h_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    hasher.Write(h_bytes, sizeof(h_bytes));

    unsigned char r_bytes[4];
    for (int i = 0; i < 4; ++i) r_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    hasher.Write(r_bytes, sizeof(r_bytes));

    hasher.Write(proposer_id.begin(), proposer_id.size());
    hasher.Write(block_id.begin(), block_id.size());

    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

uint256 ComputePrevoteDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_id,
    const std::optional<uint256>& block_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_PREVOTE/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());

    unsigned char h_bytes[8];
    for (int i = 0; i < 8; ++i) h_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    hasher.Write(h_bytes, sizeof(h_bytes));

    unsigned char r_bytes[4];
    for (int i = 0; i < 4; ++i) r_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    hasher.Write(r_bytes, sizeof(r_bytes));

    hasher.Write(validator_id.begin(), validator_id.size());

    const uint256 blk = block_id.value_or(uint256{});
    hasher.Write(blk.begin(), blk.size());

    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

uint256 ComputePrecommitNilDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_PRECOMMIT_NIL/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());

    unsigned char h_bytes[8];
    for (int i = 0; i < 8; ++i) h_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    hasher.Write(h_bytes, sizeof(h_bytes));

    unsigned char r_bytes[4];
    for (int i = 0; i < 4; ++i) r_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    hasher.Write(r_bytes, sizeof(r_bytes));

    hasher.Write(validator_id.begin(), validator_id.size());

    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

std::optional<std::vector<unsigned char>> SerializeBftProposalMsg(const BftProposalMsg& msg)
{
    if (msg.network_id.IsNull() || msg.proposer_id.IsNull() || msg.height == 0) return std::nullopt;
    if (msg.signature.ml_dsa.size() != 3309) return std::nullopt;
    if (std::all_of(msg.signature.ed25519.begin(), msg.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(msg.signature.ml_dsa.begin(), msg.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
        return std::nullopt;
    }
    const auto block_bytes = SerializeBlock(msg.block);
    if (!block_bytes) return std::nullopt;

    std::vector<unsigned char> out;
    out.reserve(32 + 8 + 4 + 32 + 64 + 4 + msg.signature.ml_dsa.size() + 4 + block_bytes->size());
    out.insert(out.end(), msg.network_id.begin(), msg.network_id.end());
    AppendUint64LE(out, msg.height);
    AppendUint32LE(out, msg.round);
    out.insert(out.end(), msg.proposer_id.begin(), msg.proposer_id.end());
    out.insert(out.end(), msg.signature.ed25519.begin(), msg.signature.ed25519.end());
    AppendUint32LE(out, static_cast<uint32_t>(msg.signature.ml_dsa.size()));
    out.insert(out.end(), msg.signature.ml_dsa.begin(), msg.signature.ml_dsa.end());
    AppendUint32LE(out, static_cast<uint32_t>(block_bytes->size()));
    out.insert(out.end(), block_bytes->begin(), block_bytes->end());
    return out;
}

std::optional<BftProposalMsg> DeserializeBftProposalMsg(std::span<const unsigned char> bytes)
{
    static constexpr size_t MIN_HEADER{32 + 8 + 4 + 32 + 64 + 4 + 4};
    if (bytes.size() < MIN_HEADER) return std::nullopt;
    size_t offset{0};

    BftProposalMsg msg;
    std::copy_n(bytes.begin() + offset, 32, msg.network_id.begin());
    offset += 32;

    msg.height = ReadUint64LE(bytes.data() + offset);
    offset += 8;

    msg.round = ReadUint32LE(bytes.data() + offset);
    offset += 4;

    std::copy_n(bytes.begin() + offset, 32, msg.proposer_id.begin());
    offset += 32;

    std::copy_n(bytes.begin() + offset, 64, msg.signature.ed25519.begin());
    offset += 64;

    const uint32_t ml_dsa_len = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (ml_dsa_len != 3309 || offset + ml_dsa_len + 4 > bytes.size()) return std::nullopt;

    msg.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + ml_dsa_len);
    offset += ml_dsa_len;

    const uint32_t block_len = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (offset + block_len != bytes.size()) return std::nullopt;

    const auto block = DeserializeBlock(bytes.subspan(offset, block_len));
    if (!block || block->height != msg.height) return std::nullopt;
    msg.block = std::move(*block);

    if (msg.network_id.IsNull() || msg.proposer_id.IsNull() || msg.height == 0) return std::nullopt;
    return msg;
}

std::optional<std::vector<unsigned char>> SerializeBftPrevoteMsg(const BftPrevoteMsg& msg)
{
    if (msg.network_id.IsNull() || msg.validator_id.IsNull() || msg.height == 0) return std::nullopt;
    if (msg.signature.ml_dsa.size() != 3309) return std::nullopt;
    if (std::all_of(msg.signature.ed25519.begin(), msg.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(msg.signature.ml_dsa.begin(), msg.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
        return std::nullopt;
    }

    std::vector<unsigned char> out;
    out.reserve(32 + 8 + 4 + 32 + 1 + (msg.block_id ? 32 : 0) + 64 + 4 + msg.signature.ml_dsa.size());
    out.insert(out.end(), msg.network_id.begin(), msg.network_id.end());
    AppendUint64LE(out, msg.height);
    AppendUint32LE(out, msg.round);
    out.insert(out.end(), msg.validator_id.begin(), msg.validator_id.end());
    if (msg.block_id.has_value()) {
        out.push_back(1);
        out.insert(out.end(), msg.block_id->begin(), msg.block_id->end());
    } else {
        out.push_back(0);
    }
    out.insert(out.end(), msg.signature.ed25519.begin(), msg.signature.ed25519.end());
    AppendUint32LE(out, static_cast<uint32_t>(msg.signature.ml_dsa.size()));
    out.insert(out.end(), msg.signature.ml_dsa.begin(), msg.signature.ml_dsa.end());
    return out;
}

std::optional<BftPrevoteMsg> DeserializeBftPrevoteMsg(std::span<const unsigned char> bytes)
{
    static constexpr size_t MIN_HEADER{32 + 8 + 4 + 32 + 1 + 64 + 4 + 3309};
    if (bytes.size() < MIN_HEADER) return std::nullopt;
    size_t offset{0};

    BftPrevoteMsg msg;
    std::copy_n(bytes.begin() + offset, 32, msg.network_id.begin());
    offset += 32;

    msg.height = ReadUint64LE(bytes.data() + offset);
    offset += 8;

    msg.round = ReadUint32LE(bytes.data() + offset);
    offset += 4;

    std::copy_n(bytes.begin() + offset, 32, msg.validator_id.begin());
    offset += 32;

    const uint8_t has_block_id = bytes[offset++];
    if (has_block_id == 1) {
        if (offset + 32 > bytes.size()) return std::nullopt;
        uint256 id;
        std::copy_n(bytes.begin() + offset, 32, id.begin());
        offset += 32;
        if (id.IsNull()) return std::nullopt;
        msg.block_id = id;
    } else if (has_block_id == 0) {
        msg.block_id = std::nullopt;
    } else {
        return std::nullopt;
    }

    if (offset + 64 + 4 > bytes.size()) return std::nullopt;
    std::copy_n(bytes.begin() + offset, 64, msg.signature.ed25519.begin());
    offset += 64;

    const uint32_t ml_dsa_len = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (ml_dsa_len != 3309 || offset + ml_dsa_len != bytes.size()) return std::nullopt;

    msg.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + ml_dsa_len);
    if (msg.network_id.IsNull() || msg.validator_id.IsNull() || msg.height == 0) return std::nullopt;
    return msg;
}

std::optional<std::vector<unsigned char>> SerializeBftPrecommitMsg(const BftPrecommitMsg& msg)
{
    if (msg.network_id.IsNull() || msg.validator_id.IsNull() || msg.height == 0) return std::nullopt;
    if (msg.signature.ml_dsa.size() != 3309) return std::nullopt;
    if (std::all_of(msg.signature.ed25519.begin(), msg.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(msg.signature.ml_dsa.begin(), msg.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
        return std::nullopt;
    }

    std::vector<unsigned char> out;
    out.reserve(32 + 8 + 4 + 32 + 1 + (msg.block_id ? 32 : 0) + 64 + 4 + msg.signature.ml_dsa.size());
    out.insert(out.end(), msg.network_id.begin(), msg.network_id.end());
    AppendUint64LE(out, msg.height);
    AppendUint32LE(out, msg.round);
    out.insert(out.end(), msg.validator_id.begin(), msg.validator_id.end());
    if (msg.block_id.has_value()) {
        out.push_back(1);
        out.insert(out.end(), msg.block_id->begin(), msg.block_id->end());
    } else {
        out.push_back(0);
    }
    out.insert(out.end(), msg.signature.ed25519.begin(), msg.signature.ed25519.end());
    AppendUint32LE(out, static_cast<uint32_t>(msg.signature.ml_dsa.size()));
    out.insert(out.end(), msg.signature.ml_dsa.begin(), msg.signature.ml_dsa.end());
    return out;
}

std::optional<BftPrecommitMsg> DeserializeBftPrecommitMsg(std::span<const unsigned char> bytes)
{
    static constexpr size_t MIN_HEADER{32 + 8 + 4 + 32 + 1 + 64 + 4 + 3309};
    if (bytes.size() < MIN_HEADER) return std::nullopt;
    size_t offset{0};

    BftPrecommitMsg msg;
    std::copy_n(bytes.begin() + offset, 32, msg.network_id.begin());
    offset += 32;

    msg.height = ReadUint64LE(bytes.data() + offset);
    offset += 8;

    msg.round = ReadUint32LE(bytes.data() + offset);
    offset += 4;

    std::copy_n(bytes.begin() + offset, 32, msg.validator_id.begin());
    offset += 32;

    const uint8_t has_block_id = bytes[offset++];
    if (has_block_id == 1) {
        if (offset + 32 > bytes.size()) return std::nullopt;
        uint256 id;
        std::copy_n(bytes.begin() + offset, 32, id.begin());
        offset += 32;
        if (id.IsNull()) return std::nullopt;
        msg.block_id = id;
    } else if (has_block_id == 0) {
        msg.block_id = std::nullopt;
    } else {
        return std::nullopt;
    }

    if (offset + 64 + 4 > bytes.size()) return std::nullopt;
    std::copy_n(bytes.begin() + offset, 64, msg.signature.ed25519.begin());
    offset += 64;

    const uint32_t ml_dsa_len = ReadUint32LE(bytes.data() + offset);
    offset += 4;
    if (ml_dsa_len != 3309 || offset + ml_dsa_len != bytes.size()) return std::nullopt;

    msg.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + ml_dsa_len);
    if (msg.network_id.IsNull() || msg.validator_id.IsNull() || msg.height == 0) return std::nullopt;
    return msg;
}

BftValidatorNode::BftValidatorNode(
    size_t node_index,
    std::array<unsigned char, 32> private_key_seed,
    uint256 network_id,
    ValidatorSet validator_set,
    ExecuteOperations execute_operations,
    std::optional<std::filesystem::path> signing_journal)
    : m_node_index{node_index},
      m_private_key_seed{private_key_seed},
      m_network_id{network_id},
      m_validator_set{std::move(validator_set)},
      m_validator_set_commitment{ComputeValidatorSetCommitment(m_validator_set)},
      m_execute_operations{std::move(execute_operations)},
      m_signing_journal{std::move(signing_journal)}
{
    const auto keypair = GenerateValidatorKeyPair(m_private_key_seed);
    if (keypair && node_index < m_validator_set.validators.size() &&
        m_validator_set.validators[node_index].consensus_public_key == keypair->public_key) {
        m_validator_id = m_validator_set.validators[node_index].validator_id;
    }
    if (m_signing_journal) {
        // Crash recovery for a stale journal temp file. A signature is only
        // produced after the record has been atomically renamed into place, so
        // a leftover ".tmp" can never correspond to a signature that was sent;
        // it is always safe to discard and keeps the next O_EXCL publish from
        // permanently disabling signing (safety-safe but liveness-bad).
        std::error_code tmp_ec;
        auto tmp_path = *m_signing_journal;
        tmp_path += ".tmp";
        if (std::filesystem::exists(tmp_path, tmp_ec) && !tmp_ec) {
            std::filesystem::remove(tmp_path, tmp_ec);
        }
        std::error_code ec;
        const bool exists = std::filesystem::exists(*m_signing_journal, ec);
        if (ec) { m_journal_valid = false; return; }
        if (exists) {
            const auto record = ReadSigningRecord(*m_signing_journal);
            if (!record || record->network_id != m_network_id || record->validator_id != m_validator_id) {
                m_journal_valid = false;
                return;
            }
            m_last_signed_height = record->height;
            m_last_signed_round = record->round;
            m_last_signed_step = record->step;
            m_has_signed = true;
            m_restarted = true;
            if (record->version == 1) {
                m_restarted_cbs1 = true;
                m_recovered_locked_round = -1;
                m_recovered_locked_block.reset();
            } else {
                m_restarted_cbs1 = false;
                m_recovered_locked_round = record->locked_round;
                m_recovered_locked_block = std::move(record->locked_block);
            }
        }
    }
}

bool BftValidatorNode::RecordSigningIntent(const BftStep step, const uint256& digest)
{
    if (m_validator_id.IsNull() || !m_journal_valid) return false;
    if (m_has_signed) {
        if (m_restarted_cbs1 && m_height == m_last_signed_height) return false;
        if (m_height < m_last_signed_height) return false;
        if (m_height == m_last_signed_height) {
            if (m_round < m_last_signed_round) return false;
            if (m_round == m_last_signed_round && step <= m_last_signed_step) return false;
        }
    }
    if (m_signing_journal) {
        std::vector<unsigned char> block_bytes;
        if (m_locked_block.has_value()) {
            const auto opt_bytes = SerializeBlock(*m_locked_block);
            if (!opt_bytes) return false;
            block_bytes = std::move(*opt_bytes);
        }
        std::vector<unsigned char> bytes{'C', 'B', 'S', '2'};
        bytes.insert(bytes.end(), m_network_id.begin(), m_network_id.end());
        bytes.insert(bytes.end(), m_validator_id.begin(), m_validator_id.end());
        AppendUint64LE(bytes, m_height);
        AppendUint32LE(bytes, m_round);
        bytes.push_back(static_cast<unsigned char>(step));
        bytes.insert(bytes.end(), digest.begin(), digest.end());
        AppendUint32LE(bytes, static_cast<uint32_t>(m_locked_round));
        AppendUint32LE(bytes, static_cast<uint32_t>(block_bytes.size()));
        bytes.insert(bytes.end(), block_bytes.begin(), block_bytes.end());
        uint256 checksum;
        CSHA256().Write(bytes.data(), bytes.size()).Finalize(checksum.begin());
        bytes.insert(bytes.end(), checksum.begin(), checksum.end());
        if (!WriteSigningRecord(*m_signing_journal, bytes)) {
            m_journal_valid = false;
            return false;
        }
    }
    m_last_signed_height = m_height;
    m_last_signed_round = m_round;
    m_last_signed_step = step;
    m_has_signed = true;
    m_restarted = false;
    // Deterministic crash boundary for the four-process smoke test: exit
    // immediately after the signing intent is durable, before any signature
    // for this step is produced. Only active when the test hook is set.
    if (const char* hook = std::getenv("CYBOU_TEST_EXIT_AFTER_SIGN")) {
        const std::string_view want{hook};
        const bool match = want == "ANY" ||
            (want == "PROPOSE" && step == BftStep::PROPOSE) ||
            (want == "PREVOTE" && step == BftStep::PREVOTE) ||
            (want == "PRECOMMIT" && step == BftStep::PRECOMMIT);
        if (match) {
            std::fflush(stdout);
            std::fflush(stderr);
            std::_Exit(120);
        }
    }
    return true;
}

BftValidatorNode::~BftValidatorNode()
{
    memory_cleanse(m_private_key_seed.data(), m_private_key_seed.size());
}

void BftValidatorNode::SetHeight(uint64_t height, const uint256& last_block_id, ValidatorSet validator_set)
{
    m_validator_set = std::move(validator_set);
    m_validator_set_commitment = ComputeValidatorSetCommitment(m_validator_set);
    const auto keypair = GenerateValidatorKeyPair(m_private_key_seed);
    m_validator_id = uint256{};
    if (keypair) {
        for (size_t i = 0; i < m_validator_set.validators.size(); ++i) {
            if (m_validator_set.validators[i].consensus_public_key == keypair->public_key) {
                m_node_index = i;
                m_validator_id = m_validator_set.validators[i].validator_id;
                break;
            }
        }
    }
    m_height = height;
    m_last_block_id = last_block_id;
    m_current_proposal.reset();
    m_current_proposal_valid = false;
    m_prevotes.clear();
    m_precommits.clear();
    m_finalized_block.reset();
    m_future_prevotes.clear();
    m_future_precommits.clear();

    if (m_restarted && m_has_signed && height == m_last_signed_height) {
        m_round = m_last_signed_round;
        m_step = m_last_signed_step;
        m_locked_block = m_recovered_locked_block;
        m_locked_round = m_recovered_locked_round;
        m_prevoted = (m_last_signed_step >= BftStep::PREVOTE);
        m_precommitted = (m_last_signed_step >= BftStep::PRECOMMIT);
    } else {
        m_round = 0;
        m_step = BftStep::PROPOSE;
        m_locked_block.reset();
        m_locked_round = -1;
        m_prevoted = false;
        m_precommitted = false;
        if (height != m_last_signed_height) {
            m_restarted = false;
            m_restarted_cbs1 = false;
        }
    }
}

std::optional<BftProposalMsg> BftValidatorNode::StartRound(
    uint32_t round,
    const std::vector<ProtocolOperation>& pending_ops)
{
    if (round < m_round ||
        (round == m_round &&
         (m_current_proposal || m_prevoted || m_precommitted ||
          !m_prevotes.empty() || !m_precommits.empty() || m_step == BftStep::FINALIZED))) {
        return std::nullopt;
    }
    EnterRound(round);
    m_finalized_block.reset();

    if (BftLeaderIndex(m_height, m_round, m_validator_set.validators.size()) != m_node_index) {
        return std::nullopt;
    }
    if (m_validator_id.IsNull()) return std::nullopt;
    if (!m_execute_operations) return std::nullopt;

    CybouBlock block;
    if (m_locked_block.has_value()) {
        block = *m_locked_block;
    } else {
        const auto state_root = m_execute_operations(pending_ops, m_height);
        if (!state_root) return std::nullopt;
        block = CybouBlock{
            .version = CYBOU_BLOCK_VERSION,
            .parent_block_id = m_last_block_id,
            .height = m_height,
            .operations = pending_ops,
            .resulting_state_root = *state_root,
        };
    }

    const uint256 block_id = ComputeBlockId(block);
    const uint256 digest = ComputeProposalDigest(m_network_id, m_height, m_round, m_validator_id, block_id);
    if (!RecordSigningIntent(BftStep::PROPOSE, digest)) return std::nullopt;
    const auto sig = SignValidatorVote(m_private_key_seed, digest);
    if (!sig) return std::nullopt;

    BftProposalMsg proposal{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .proposer_id = m_validator_id,
        .block = std::move(block),
        .signature = *sig,
    };

    m_current_proposal = proposal;
    m_current_proposal_valid = true;
    return proposal;
}

void BftValidatorNode::EnterRound(uint32_t round)
{
    m_round = round;
    m_step = BftStep::PROPOSE;
    m_current_proposal.reset();
    m_current_proposal_valid = false;
    m_prevotes.clear();
    m_precommits.clear();
    m_prevoted = false;
    m_precommitted = false;
    // Buffered future votes at or below the new round are stale forever.
    const auto stale_prevotes = m_future_prevotes.upper_bound(round);
    m_future_prevotes.erase(m_future_prevotes.begin(), stale_prevotes);
    const auto stale_precommits = m_future_precommits.upper_bound(round);
    m_future_precommits.erase(m_future_precommits.begin(), stale_precommits);
    // The triggering quorum was moved out before entering the round and is
    // replayed by the caller after this cleanup.
}

void BftValidatorNode::EnterRoundWithPrevoteEvidence(
    const uint32_t round, std::map<uint256, BftPrevoteMsg> evidence)
{
    EnterRound(round);
    for (auto& [validator_id, vote] : evidence) {
        m_prevotes.emplace(validator_id, std::move(vote));
    }
}

void BftValidatorNode::EnterRoundWithPrecommitEvidence(
    const uint32_t round, std::map<uint256, BftPrecommitMsg> evidence)
{
    EnterRound(round);
    for (auto& [validator_id, vote] : evidence) {
        m_precommits.emplace(validator_id, std::move(vote));
    }
}

bool BftValidatorNode::CanBufferFutureRound(const uint256& validator_id, const uint32_t round) const
{
    std::set<uint32_t> buffered_rounds;
    for (const auto& [buffered_round, votes] : m_future_prevotes) {
        if (votes.contains(validator_id)) buffered_rounds.insert(buffered_round);
    }
    for (const auto& [buffered_round, votes] : m_future_precommits) {
        if (votes.contains(validator_id)) buffered_rounds.insert(buffered_round);
    }
    if (buffered_rounds.contains(round)) return true;
    return buffered_rounds.size() < MAX_BUFFERED_FUTURE_ROUNDS_PER_VALIDATOR;
}

bool BftValidatorNode::BufferFuturePrevote(const BftPrevoteMsg& prevote)
{
    auto round = m_future_prevotes.find(prevote.round);
    if (round != m_future_prevotes.end()) {
        const auto existing = round->second.find(prevote.validator_id);
        if (existing != round->second.end()) return existing->second == prevote;
    }
    if (!CanBufferFutureRound(prevote.validator_id, prevote.round)) return false;

    m_future_prevotes[prevote.round].emplace(prevote.validator_id, prevote);
    // Cap the number of buffered rounds; drop the highest (least useful)
    // rounds first. Honest gradual round movement never relies on the
    // buffer, only on the MAX_FUTURE_ROUND_ADVANCE window.
    while (m_future_prevotes.size() > MAX_BUFFERED_FUTURE_ROUNDS) {
        m_future_prevotes.erase(std::prev(m_future_prevotes.end()));
    }
    return true;
}

bool BftValidatorNode::BufferFuturePrecommit(const BftPrecommitMsg& precommit)
{
    auto round = m_future_precommits.find(precommit.round);
    if (round != m_future_precommits.end()) {
        const auto existing = round->second.find(precommit.validator_id);
        if (existing != round->second.end()) return existing->second == precommit;
    }
    if (!CanBufferFutureRound(precommit.validator_id, precommit.round)) return false;

    m_future_precommits[precommit.round].emplace(precommit.validator_id, precommit);
    while (m_future_precommits.size() > MAX_BUFFERED_FUTURE_ROUNDS) {
        m_future_precommits.erase(std::prev(m_future_precommits.end()));
    }
    return true;
}

uint32_t BftValidatorNode::QuorumBackedFutureRound() const
{
    const size_t quorum = m_validator_set.QuorumThreshold();
    uint32_t best{0};
    for (const auto& [round, votes] : m_future_prevotes) {
        if (round > m_round && votes.size() >= quorum) {
            best = round;
            break;
        }
    }
    for (const auto& [round, votes] : m_future_precommits) {
        if (round > m_round && round < (best ? best : UINT32_MAX) && votes.size() >= quorum) {
            best = round;
            break;
        }
    }
    return best;
}

BftProposalResult BftValidatorNode::ReceiveProposal(const BftProposalMsg& proposal)
{
    if (proposal.network_id != m_network_id || proposal.height != m_height ||
        proposal.round < m_round || proposal.round - m_round > MAX_FUTURE_ROUND_ADVANCE) {
        return {};
    }

    const size_t leader_idx = BftLeaderIndex(m_height, proposal.round, m_validator_set.validators.size());
    if (leader_idx >= m_validator_set.validators.size()) return {};
    if (proposal.proposer_id != m_validator_set.validators[leader_idx].validator_id) {
        return {};
    }

    const uint256 block_id = ComputeBlockId(proposal.block);
    const uint256 digest = ComputeProposalDigest(m_network_id, m_height, proposal.round, proposal.proposer_id, block_id);
    if (!VerifyValidatorSignature(m_validator_set.validators[leader_idx].consensus_public_key, proposal.signature, digest)) {
        return {};
    }

    // A signed proposal from the elected leader is evidence of a later round.
    // Preserve any block lock while discarding only the older round's votes.
    if (proposal.round > m_round) {
        EnterRound(proposal.round);
    }
    if (m_prevoted) return {};

    bool valid_block = (proposal.block.height == m_height && proposal.block.parent_block_id == m_last_block_id);
    const auto computed_root = m_execute_operations ? m_execute_operations(proposal.block.operations, m_height) : std::nullopt;
    if (!computed_root || *computed_root != proposal.block.resulting_state_root) valid_block = false;
    if (m_locked_block.has_value()) {
        if (ComputeBlockId(*m_locked_block) != block_id) {
            valid_block = false;
        }
    }

    m_current_proposal = proposal;
    m_current_proposal_valid = valid_block;
    m_step = BftStep::PREVOTE;
    m_prevoted = true;

    std::optional<uint256> vote_block = valid_block ? std::optional<uint256>(block_id) : std::nullopt;
    const uint256 prevote_digest = ComputePrevoteDigest(m_network_id, m_height, m_round, m_validator_id, vote_block);
    if (!RecordSigningIntent(BftStep::PREVOTE, prevote_digest)) return {};
    const auto sig = SignValidatorVote(m_private_key_seed, prevote_digest);
    if (!sig) return {};

    BftPrevoteMsg msg{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .validator_id = m_validator_id,
        .block_id = vote_block,
        .signature = *sig,
    };

    // Re-evaluate votes replayed before the proposal existed. Keep the newly
    // signed local prevote out of this pass so the normal proposal->prevote
    // path can still drive a one-validator set through its local vote.
    auto precommit = EvaluatePrevoteQuorum();
    if (const auto existing = m_prevotes.find(m_validator_id);
        existing != m_prevotes.end() && existing->second != msg) {
        return {};
    }
    m_prevotes[m_validator_id] = msg;
    // Proposal and its delayed quorum evidence can be delivered in either
    // order. Recheck already received commits after the block becomes known.
    EvaluatePrecommitQuorum();
    return BftProposalResult{
        .prevote = msg,
        .precommit = std::move(precommit),
        .finalized = m_finalized_block.has_value(),
    };
}

std::optional<BftPrecommitMsg> BftValidatorNode::ReceivePrevote(const BftPrevoteMsg& prevote)
{
    if (prevote.network_id != m_network_id || prevote.height != m_height) {
        return std::nullopt;
    }

    const auto* val = m_validator_set.FindValidator(prevote.validator_id);
    if (!val) return std::nullopt;

    const uint256 digest = ComputePrevoteDigest(m_network_id, m_height, prevote.round, prevote.validator_id, prevote.block_id);
    if (!VerifyValidatorSignature(val->consensus_public_key, prevote.signature, digest)) {
        return std::nullopt;
    }

    if (prevote.round < m_round) return std::nullopt;
    const bool vote_was_future = prevote.round > m_round;
    if (vote_was_future) {
        if (!BufferFuturePrevote(prevote)) return std::nullopt;
        const uint32_t evidence_round = QuorumBackedFutureRound();
        if (evidence_round <= m_round) {
            return std::nullopt;
        }
        auto evidence_it = m_future_prevotes.find(evidence_round);
        if (evidence_it == m_future_prevotes.end()) return std::nullopt;
        auto evidence = std::move(evidence_it->second);
        m_future_prevotes.erase(evidence_it);
        EnterRoundWithPrevoteEvidence(evidence_round, std::move(evidence));
        if (prevote.round != m_round) return std::nullopt;
    }

    if (const auto existing = m_prevotes.find(prevote.validator_id);
        existing != m_prevotes.end() && existing->second != prevote) {
        return std::nullopt;
    }

    m_prevotes[prevote.validator_id] = prevote;

    return EvaluatePrevoteQuorum();
}

std::optional<BftPrecommitMsg> BftValidatorNode::EvaluatePrevoteQuorum()
{
    if (m_precommitted) return std::nullopt;

    std::map<uint256, size_t> block_counts;
    size_t nil_counts{0};
    for (const auto& [vid, pv] : m_prevotes) {
        if (pv.block_id.has_value()) {
            block_counts[*pv.block_id]++;
        } else {
            nil_counts++;
        }
    }

    const size_t quorum = m_validator_set.QuorumThreshold();

    // Polka unlock: a conflicting lock from an earlier round must not deadlock
    // the height. If the current round gathers a quorum of prevotes for the
    // current proposal's block, the lock moves to that block (standard
    // unlock-on-polka), letting the precommit path below fire. The proposal
    // is revalidated without the lock clause so an invalid block can never
    // grab the lock.
    if (!m_current_proposal_valid && m_current_proposal.has_value() && m_locked_block.has_value()) {
        const uint256 current_id = ComputeBlockId(m_current_proposal->block);
        const auto polka = block_counts.find(current_id);
        if (polka != block_counts.end() && polka->second >= quorum) {
            const bool valid_apart_from_lock = (m_current_proposal->block.height == m_height &&
                m_current_proposal->block.parent_block_id == m_last_block_id);
            const auto root = m_execute_operations ?
                m_execute_operations(m_current_proposal->block.operations, m_height) : std::nullopt;
            if (valid_apart_from_lock && root && *root == m_current_proposal->block.resulting_state_root) {
                m_locked_block = m_current_proposal->block;
                m_locked_round = static_cast<int32_t>(m_round);
                m_current_proposal_valid = true;
            }
        }
    }

    for (const auto& [blk_id, count] : block_counts) {
        if (count >= quorum && m_current_proposal_valid && m_current_proposal.has_value() &&
            ComputeBlockId(m_current_proposal->block) == blk_id) {
            m_locked_block = m_current_proposal->block;
            m_locked_round = static_cast<int32_t>(m_round);
            m_step = BftStep::PRECOMMIT;
            m_precommitted = true;

            const uint256 commit_digest = ComputeBftCommitDigest(
                m_network_id, blk_id, m_height, m_round, m_validator_set_commitment);
            if (!RecordSigningIntent(BftStep::PRECOMMIT, commit_digest)) return std::nullopt;
            const auto sig = SignValidatorVote(m_private_key_seed, commit_digest);
            if (!sig) return std::nullopt;

            BftPrecommitMsg msg{
                .network_id = m_network_id,
                .height = m_height,
                .round = m_round,
                .validator_id = m_validator_id,
                .block_id = blk_id,
                .signature = *sig,
            };
            m_precommits[m_validator_id] = msg;
            return msg;
        }
    }

    if (nil_counts >= quorum || m_prevotes.size() == m_validator_set.validators.size()) {
        m_step = BftStep::PRECOMMIT;
        m_precommitted = true;
        const uint256 nil_digest = ComputePrecommitNilDigest(m_network_id, m_height, m_round, m_validator_id);
        if (!RecordSigningIntent(BftStep::PRECOMMIT, nil_digest)) return std::nullopt;
        const auto sig = SignValidatorVote(m_private_key_seed, nil_digest);
        if (!sig) return std::nullopt;

        BftPrecommitMsg msg{
            .network_id = m_network_id,
            .height = m_height,
            .round = m_round,
            .validator_id = m_validator_id,
            .block_id = std::nullopt,
            .signature = *sig,
        };
        m_precommits[m_validator_id] = msg;
        return msg;
    }

    return std::nullopt;
}

bool BftValidatorNode::ReceivePrecommit(const BftPrecommitMsg& precommit)
{
    if (precommit.network_id != m_network_id || precommit.height != m_height) {
        return false;
    }

    const auto* val = m_validator_set.FindValidator(precommit.validator_id);
    if (!val) return false;

    uint256 digest;
    if (precommit.block_id.has_value()) {
        digest = ComputeBftCommitDigest(
            m_network_id, *precommit.block_id, m_height, precommit.round, m_validator_set_commitment);
    } else {
        digest = ComputePrecommitNilDigest(
            m_network_id, m_height, precommit.round, precommit.validator_id);
    }
    if (!VerifyValidatorSignature(val->consensus_public_key, precommit.signature, digest)) {
        return false;
    }

    if (precommit.round < m_round) return false;
    const bool vote_was_future = precommit.round > m_round;
    if (vote_was_future) {
        if (!BufferFuturePrecommit(precommit)) return false;
        const uint32_t evidence_round = QuorumBackedFutureRound();
        if (evidence_round <= m_round) {
            return false;
        }
        auto evidence_it = m_future_precommits.find(evidence_round);
        if (evidence_it == m_future_precommits.end()) return false;
        auto evidence = std::move(evidence_it->second);
        m_future_precommits.erase(evidence_it);
        EnterRoundWithPrecommitEvidence(evidence_round, std::move(evidence));
        if (precommit.round != m_round) return false;
    }

    if (const auto existing = m_precommits.find(precommit.validator_id);
        existing != m_precommits.end() && existing->second != precommit) return false;
    m_precommits[precommit.validator_id] = precommit;

    if (m_step == BftStep::FINALIZED) return true;

    return EvaluatePrecommitQuorum();
}

bool BftValidatorNode::EvaluatePrecommitQuorum()
{
    if (m_step == BftStep::FINALIZED) return true;

    std::map<uint256, std::vector<BftCommitVote>> commit_votes_by_block;
    for (const auto& [vid, pc] : m_precommits) {
        if (pc.block_id.has_value()) {
            commit_votes_by_block[*pc.block_id].push_back(BftCommitVote{
                .validator_id = vid,
                .signature = pc.signature,
            });
        }
    }

    const size_t quorum = m_validator_set.QuorumThreshold();
    for (auto& [blk_id, votes] : commit_votes_by_block) {
        if (votes.size() >= quorum && m_current_proposal_valid && m_current_proposal.has_value() &&
            ComputeBlockId(m_current_proposal->block) == blk_id) {
            BftFinalityCertificate cert{
                .version = BFT_FINALITY_CERTIFICATE_VERSION,
                .network_id = m_network_id,
                .block_id = blk_id,
                .height = m_height,
                .round = m_round,
                .validator_set_commitment = m_validator_set_commitment,
                .commit_votes = std::move(votes),
            };

            if (VerifyFinalityCertificate(cert, m_validator_set, m_network_id) == FinalityVerificationError::NONE) {
                m_finalized_block = FinalizedBlock{
                    .block = m_current_proposal->block,
                    .certificate = std::move(cert),
                };
                m_step = BftStep::FINALIZED;
                m_locked_block.reset();
                m_locked_round = -1;
                return true;
            }
        }
    }

    return false;
}

std::optional<BftPrevoteMsg> BftValidatorNode::OnProposalTimeout()
{
    if (m_prevoted || m_step == BftStep::FINALIZED) return std::nullopt;
    const uint256 digest = ComputePrevoteDigest(m_network_id, m_height, m_round,
        m_validator_id, std::nullopt);
    if (!RecordSigningIntent(BftStep::PREVOTE, digest)) return std::nullopt;
    const auto signature = SignValidatorVote(m_private_key_seed, digest);
    if (!signature) return std::nullopt;
    m_step = BftStep::PREVOTE;
    m_prevoted = true;
    BftPrevoteMsg vote{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .validator_id = m_validator_id,
        .block_id = std::nullopt,
        .signature = *signature,
    };
    m_prevotes[m_validator_id] = vote;
    return vote;
}

std::optional<BftPrecommitMsg> BftValidatorNode::OnPrevoteTimeout()
{
    if (m_precommitted) return std::nullopt;
    m_step = BftStep::PRECOMMIT;
    m_precommitted = true;

    const uint256 nil_digest = ComputePrecommitNilDigest(m_network_id, m_height, m_round, m_validator_id);
    if (!RecordSigningIntent(BftStep::PRECOMMIT, nil_digest)) return std::nullopt;
    const auto sig = SignValidatorVote(m_private_key_seed, nil_digest);
    if (!sig) return std::nullopt;

    BftPrecommitMsg msg{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .validator_id = m_validator_id,
        .block_id = std::nullopt,
        .signature = *sig,
    };
    m_precommits[m_validator_id] = msg;
    return msg;
}

void BftValidatorNode::OnRoundTimeout()
{
    if (m_step != BftStep::FINALIZED) {
        m_round += 1;
        m_step = BftStep::PROPOSE;
        m_current_proposal.reset();
        m_current_proposal_valid = false;
        m_prevotes.clear();
        m_precommits.clear();
        m_prevoted = false;
        m_precommitted = false;
    }
}

// ------------------------------------------------------------------------------------------------
// BftSimulator
// ------------------------------------------------------------------------------------------------

BftSimulator::BftSimulator(const uint256& network_id, size_t validator_count)
    : m_network_id{network_id}
{
    validator_count = std::max<size_t>(1, validator_count);
    m_validator_set.version = VALIDATOR_SET_VERSION;
    m_validator_set.validators.resize(validator_count);
    m_online.assign(validator_count, true);
    m_can_communicate.assign(validator_count, std::vector<bool>(validator_count, true));

    std::vector<std::array<unsigned char, 32>> seeds;
    for (size_t i = 0; i < validator_count; ++i) {
        std::string seed_str = "CYBOU_SIM_VALIDATOR_SEED_" + std::to_string(i);
        uint256 seed_hash;
        CSHA256().Write(reinterpret_cast<const unsigned char*>(seed_str.data()), seed_str.size()).Finalize(seed_hash.begin());

        std::array<unsigned char, 32> seed{};
        std::copy_n(seed_hash.begin(), 32, seed.begin());
        seeds.push_back(seed);

        const auto keypair = GenerateValidatorKeyPair(seed);
        m_validator_set.validators[i] = Validator{
            .validator_id = ComputeValidatorId(keypair->public_key),
            .consensus_public_key = keypair->public_key,
            .weight = 1,
        };
    }

    for (size_t i = 0; i < validator_count; ++i) {
        m_nodes.push_back(std::make_unique<BftValidatorNode>(
            i, seeds[i], m_network_id, m_validator_set,
            [this](const std::vector<ProtocolOperation>& ops, uint64_t) -> std::optional<uint256> {
                if (!ops.empty()) return std::nullopt;
                return m_expected_state_root;
            }));
    }

    ClearPartition();
}

void BftSimulator::SetNodeOnline(size_t index, bool online)
{
    if (index < m_online.size()) {
        m_online[index] = online;
    }
}

void BftSimulator::SetPartition(const std::vector<size_t>& partition_a, const std::vector<size_t>& partition_b)
{
    const size_t n = m_nodes.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            m_can_communicate[i][j] = false;
        }
    }
    for (size_t i : partition_a) {
        for (size_t j : partition_a) {
            if (i < n && j < n) m_can_communicate[i][j] = true;
        }
    }
    for (size_t i : partition_b) {
        for (size_t j : partition_b) {
            if (i < n && j < n) m_can_communicate[i][j] = true;
        }
    }
}

void BftSimulator::ClearPartition()
{
    const size_t n = m_nodes.size();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = 0; j < n; ++j) {
            m_can_communicate[i][j] = true;
        }
    }
}

bool BftSimulator::StepRound(
    uint64_t height,
    uint32_t round,
    const std::vector<ProtocolOperation>& ops,
    const uint256& resulting_state_root)
{
    const size_t n = m_nodes.size();
    m_expected_state_root = resulting_state_root;
    const size_t leader_idx = BftLeaderIndex(height, round, n);
    std::optional<BftProposalMsg> proposal;

    if (m_online[leader_idx]) {
        proposal = m_nodes[leader_idx]->StartRound(round, ops);
    }

    std::vector<BftPrevoteMsg> prevotes;
    std::vector<BftPrecommitMsg> precommits;
    for (size_t i = 0; i < n; ++i) {
        if (!m_online[i]) continue;
        if (i != leader_idx) {
            m_nodes[i]->StartRound(round, ops);
        }
        if (proposal.has_value() && m_can_communicate[leader_idx][i]) {
            auto result = m_nodes[i]->ReceiveProposal(*proposal);
            if (result.prevote) prevotes.push_back(*result.prevote);
            if (result.precommit) precommits.push_back(*result.precommit);
        }
    }

    for (size_t i = 0; i < n; ++i) {
        if (!m_online[i]) continue;
        for (const auto& pv : prevotes) {
            size_t sender = 0;
            for (size_t k = 0; k < n; ++k) {
                if (m_nodes[k]->GetValidatorId() == pv.validator_id) sender = k;
            }
            if (m_can_communicate[sender][i]) {
                auto pc = m_nodes[i]->ReceivePrevote(pv);
                if (pc) precommits.push_back(*pc);
            }
        }
    }

    size_t finalized_count = 0;
    for (size_t i = 0; i < n; ++i) {
        if (!m_online[i]) continue;
        for (const auto& pc : precommits) {
            size_t sender = 0;
            for (size_t k = 0; k < n; ++k) {
                if (m_nodes[k]->GetValidatorId() == pc.validator_id) sender = k;
            }
            if (m_can_communicate[sender][i]) {
                m_nodes[i]->ReceivePrecommit(pc);
            }
        }
        if (m_nodes[i]->GetStep() == BftStep::FINALIZED) {
            finalized_count++;
        }
    }

    return finalized_count >= m_validator_set.QuorumThreshold();
}

} // namespace cybou
