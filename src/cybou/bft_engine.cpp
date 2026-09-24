// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft_engine.h>
#include <support/cleanse.h>

#include <crypto/sha256.h>

#include <algorithm>
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

constexpr size_t SIGNING_RECORD_SIZE{4 + 32 + 32 + 8 + 4 + 1 + 32 + 32};

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

std::optional<std::vector<unsigned char>> ReadSigningRecord(const std::filesystem::path& path)
{
    std::error_code ec;
    if (std::filesystem::is_symlink(path, ec) || ec || std::filesystem::file_size(path, ec) != SIGNING_RECORD_SIZE || ec) {
        return std::nullopt;
    }
    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> bytes(SIGNING_RECORD_SIZE);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return std::nullopt;
    return bytes;
}

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
        std::error_code ec;
        const bool exists = std::filesystem::exists(*m_signing_journal, ec);
        if (ec) { m_journal_valid = false; return; }
        if (exists) {
            const auto bytes = ReadSigningRecord(*m_signing_journal);
            if (!bytes || !std::equal(bytes->begin(), bytes->begin() + 4, "CBS1") ||
                !std::equal(m_network_id.begin(), m_network_id.end(), bytes->begin() + 4) ||
                !std::equal(m_validator_id.begin(), m_validator_id.end(), bytes->begin() + 36) ||
                (*bytes)[80] > static_cast<unsigned char>(BftStep::PRECOMMIT)) {
                m_journal_valid = false;
                return;
            }
            uint256 checksum;
            CSHA256().Write(bytes->data(), SIGNING_RECORD_SIZE - 32).Finalize(checksum.begin());
            if (!std::equal(checksum.begin(), checksum.end(), bytes->begin() + SIGNING_RECORD_SIZE - 32)) {
                m_journal_valid = false;
                return;
            }
            m_last_signed_height = ReadUint64LE(bytes->data() + 68);
            m_last_signed_round = ReadUint32LE(bytes->data() + 76);
            m_last_signed_step = static_cast<BftStep>((*bytes)[80]);
            m_has_signed = true;
            m_restarted = true;
        }
    }
}

bool BftValidatorNode::RecordSigningIntent(const BftStep step, const uint256& digest)
{
    if (m_validator_id.IsNull() || !m_journal_valid) return false;
    if (m_has_signed) {
        if (m_height < m_last_signed_height || (m_restarted && m_height == m_last_signed_height)) return false;
        if (m_height == m_last_signed_height &&
            (m_round < m_last_signed_round ||
             (m_round == m_last_signed_round && step <= m_last_signed_step))) return false;
    }
    if (m_signing_journal) {
        std::vector<unsigned char> bytes{'C', 'B', 'S', '1'};
        bytes.insert(bytes.end(), m_network_id.begin(), m_network_id.end());
        bytes.insert(bytes.end(), m_validator_id.begin(), m_validator_id.end());
        AppendUint64LE(bytes, m_height);
        AppendUint32LE(bytes, m_round);
        bytes.push_back(static_cast<unsigned char>(step));
        bytes.insert(bytes.end(), digest.begin(), digest.end());
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
    m_round = 0;
    m_step = BftStep::PROPOSE;
    m_locked_block.reset();
    m_locked_round = -1;
    m_current_proposal.reset();
    m_current_proposal_valid = false;
    m_prevotes.clear();
    m_precommits.clear();
    m_prevoted = false;
    m_precommitted = false;
    m_finalized_block.reset();
}

std::optional<BftProposalMsg> BftValidatorNode::StartRound(
    uint32_t round,
    const std::vector<ProtocolOperation>& pending_ops)
{
    m_round = round;
    m_step = BftStep::PROPOSE;
    m_current_proposal.reset();
    m_current_proposal_valid = false;
    m_prevotes.clear();
    m_precommits.clear();
    m_prevoted = false;
    m_precommitted = false;
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

std::optional<BftPrevoteMsg> BftValidatorNode::ReceiveProposal(const BftProposalMsg& proposal)
{
    if (m_prevoted) return std::nullopt;
    if (proposal.network_id != m_network_id || proposal.height != m_height || proposal.round != m_round) {
        return std::nullopt;
    }

    const size_t leader_idx = BftLeaderIndex(m_height, m_round, m_validator_set.validators.size());
    if (leader_idx >= m_validator_set.validators.size()) return std::nullopt;
    if (proposal.proposer_id != m_validator_set.validators[leader_idx].validator_id) {
        return std::nullopt;
    }

    const uint256 block_id = ComputeBlockId(proposal.block);
    const uint256 digest = ComputeProposalDigest(m_network_id, m_height, m_round, proposal.proposer_id, block_id);
    if (!VerifyValidatorSignature(m_validator_set.validators[leader_idx].consensus_public_key, proposal.signature, digest)) {
        return std::nullopt;
    }

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
    if (!RecordSigningIntent(BftStep::PREVOTE, prevote_digest)) return std::nullopt;
    const auto sig = SignValidatorVote(m_private_key_seed, prevote_digest);
    if (!sig) return std::nullopt;

    BftPrevoteMsg msg{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .validator_id = m_validator_id,
        .block_id = vote_block,
        .signature = *sig,
    };

    m_prevotes[m_validator_id] = msg;
    return msg;
}

std::optional<BftPrecommitMsg> BftValidatorNode::ReceivePrevote(const BftPrevoteMsg& prevote)
{
    if (prevote.network_id != m_network_id || prevote.height != m_height || prevote.round != m_round) {
        return std::nullopt;
    }

    const auto* val = m_validator_set.FindValidator(prevote.validator_id);
    if (!val) return std::nullopt;

    const uint256 digest = ComputePrevoteDigest(m_network_id, m_height, m_round, prevote.validator_id, prevote.block_id);
    if (!VerifyValidatorSignature(val->consensus_public_key, prevote.signature, digest)) {
        return std::nullopt;
    }

    if (const auto existing = m_prevotes.find(prevote.validator_id);
        existing != m_prevotes.end() && existing->second != prevote) {
        return std::nullopt;
    }

    m_prevotes[prevote.validator_id] = prevote;

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
    if (precommit.network_id != m_network_id || precommit.height != m_height || precommit.round != m_round) {
        return false;
    }

    const auto* val = m_validator_set.FindValidator(precommit.validator_id);
    if (!val) return false;

    if (precommit.block_id.has_value()) {
        const uint256 commit_digest = ComputeBftCommitDigest(
            m_network_id, *precommit.block_id, m_height, m_round, m_validator_set_commitment);
        if (!VerifyValidatorSignature(val->consensus_public_key, precommit.signature, commit_digest)) {
            return false;
        }
    } else {
        const uint256 nil_digest = ComputePrecommitNilDigest(
            m_network_id, m_height, m_round, precommit.validator_id);
        if (!VerifyValidatorSignature(val->consensus_public_key, precommit.signature, nil_digest)) {
            return false;
        }
    }

    if (const auto existing = m_precommits.find(precommit.validator_id);
        existing != m_precommits.end() && existing->second != precommit) return false;
    m_precommits[precommit.validator_id] = precommit;

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
    for (size_t i = 0; i < n; ++i) {
        if (!m_online[i]) continue;
        if (i != leader_idx) {
            m_nodes[i]->StartRound(round, ops);
        }
        if (proposal.has_value() && m_can_communicate[leader_idx][i]) {
            auto pv = m_nodes[i]->ReceiveProposal(*proposal);
            if (pv) prevotes.push_back(*pv);
        }
    }

    std::vector<BftPrecommitMsg> precommits;
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
