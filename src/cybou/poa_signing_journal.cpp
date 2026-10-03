// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/poa_signing_journal.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

namespace cybou {
namespace {

constexpr unsigned char JOURNAL_FORMAT_VERSION{1};
constexpr size_t JOURNAL_METADATA_SIZE{1 + 32 + 32 + 32};
constexpr size_t JOURNAL_HEAD_SIZE{1 + 8 + 32 + 32};

std::string Hex(const uint256& value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.resize(value.size() * 2);
    for (size_t i = 0; i < value.size(); ++i) {
        out[2 * i] = digits[value.begin()[i] >> 4];
        out[2 * i + 1] = digits[value.begin()[i] & 0x0f];
    }
    return out;
}

void AppendUint64LE(std::vector<unsigned char>& out, const uint64_t value)
{
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t ReadUint64LE(const std::span<const unsigned char> bytes, const size_t offset)
{
    uint64_t value{0};
    for (int i = 0; i < 8; ++i) value |= uint64_t{bytes[offset + i]} << (8 * i);
    return value;
}

std::vector<unsigned char> EncodeHead(const PoaJournalHead& head)
{
    std::vector<unsigned char> bytes;
    bytes.reserve(JOURNAL_HEAD_SIZE);
    bytes.push_back(JOURNAL_FORMAT_VERSION);
    AppendUint64LE(bytes, head.height);
    bytes.insert(bytes.end(), head.parent_block_id.begin(), head.parent_block_id.end());
    bytes.insert(bytes.end(), head.block_id.begin(), head.block_id.end());
    return bytes;
}

std::optional<PoaJournalHead> DecodeHead(const std::span<const unsigned char> bytes)
{
    if (bytes.size() != JOURNAL_HEAD_SIZE || bytes[0] != JOURNAL_FORMAT_VERSION) return std::nullopt;
    PoaJournalHead head;
    head.height = ReadUint64LE(bytes, 1);
    std::copy_n(bytes.begin() + 9, 32, head.parent_block_id.begin());
    std::copy_n(bytes.begin() + 41, 32, head.block_id.begin());
    if (head.block_id.IsNull() || (head.height == 0 && !head.parent_block_id.IsNull()) ||
        (head.height != 0 && head.parent_block_id.IsNull())) return std::nullopt;
    return head;
}

} // namespace

PoaSigningJournal::PoaSigningJournal(KVStore& db, const uint256& network_binding,
    const uint256& genesis_block_id, const IdentityHybridPublicKey& finalizer_key)
    : m_db{db}, m_network_binding{network_binding}, m_genesis_block_id{genesis_block_id},
      m_finalizer_key_id{[&finalizer_key] {
          const auto id = ComputePoaFinalizerKeyId(finalizer_key);
          if (!id) throw std::invalid_argument{"invalid PoA finalizer public key"};
          return *id;
      }()},
      m_prefix{"poa-finalizer-journal/" + Hex(network_binding) + "/"}
{
    if (network_binding.IsNull() || genesis_block_id.IsNull()) {
        throw std::invalid_argument{"invalid PoA signing journal identity"};
    }
    const auto metadata_key = m_prefix + "metadata";
    const auto head_key = m_prefix + "head";
    const auto halt_key = m_prefix + "halt";
    const auto key_id = m_finalizer_key_id;
    std::vector<unsigned char> expected_metadata;
    expected_metadata.reserve(JOURNAL_METADATA_SIZE);
    expected_metadata.push_back(JOURNAL_FORMAT_VERSION);
    expected_metadata.insert(expected_metadata.end(), network_binding.begin(), network_binding.end());
    expected_metadata.insert(expected_metadata.end(), key_id.begin(), key_id.end());
    expected_metadata.insert(expected_metadata.end(), genesis_block_id.begin(), genesis_block_id.end());
    if (expected_metadata.size() != JOURNAL_METADATA_SIZE) {
        throw std::logic_error{"invalid PoA signing journal metadata encoding"};
    }

    std::vector<unsigned char> metadata;
    if (m_db.Read(metadata_key, metadata)) {
        if (metadata != expected_metadata) throw std::runtime_error{"PoA signing journal identity mismatch"};
        std::vector<unsigned char> encoded_head;
        if (!m_db.Read(head_key, encoded_head)) throw std::runtime_error{"missing PoA signing journal head"};
        const auto head = DecodeHead(encoded_head);
        if (!head || (head->height == 0 && head->block_id != genesis_block_id)) {
            throw std::runtime_error{"corrupt PoA signing journal head"};
        }
        m_head = *head;
        std::vector<unsigned char> halt;
        if (m_db.Read(halt_key, halt)) {
            const auto reason = halt.size() == 2 ? static_cast<PoaJournalStatus>(halt[1]) :
                PoaJournalStatus::NONE;
            if (halt.size() != 2 || halt[0] != JOURNAL_FORMAT_VERSION ||
                (reason != PoaJournalStatus::HISTORY_MISMATCH &&
                    reason != PoaJournalStatus::EQUIVOCATION)) {
                throw std::runtime_error{"corrupt PoA signing journal halt record"};
            }
            m_halted = true;
        } else if (m_db.Exists(halt_key)) {
            throw std::runtime_error{"corrupt PoA signing journal halt record"};
        }
    } else {
        if (m_db.Exists(metadata_key) || m_db.Exists(head_key) || m_db.Exists(halt_key)) {
            throw std::runtime_error{"incomplete PoA signing journal"};
        }
        m_head = PoaJournalHead{.height = 0, .parent_block_id = {}, .block_id = genesis_block_id};
        KVStore::Batch batch;
        batch.Write(metadata_key, expected_metadata);
        batch.Write(head_key, EncodeHead(m_head));
        m_db.WriteBatch(batch, true);
    }
}

PoaJournalStatus PoaSigningJournal::CheckCanonicalTip(
    const uint64_t finalized_height, const uint256& finalized_tip)
{
    std::lock_guard lock{m_mutex};
    if (m_halted) return PoaJournalStatus::JOURNAL_HALTED;

    bool matches{false};
    if (m_head.height == 0) {
        matches = finalized_height == 0 && finalized_tip == m_genesis_block_id;
    } else if (finalized_height == m_head.height) {
        matches = finalized_tip == m_head.block_id;
    } else if (finalized_height != std::numeric_limits<uint64_t>::max() &&
        finalized_height + 1 == m_head.height) {
        matches = finalized_tip == m_head.parent_block_id;
    }
    if (!matches) {
        m_history_verified = false;
        return PersistHalt(PoaJournalStatus::HISTORY_MISMATCH) ?
            PoaJournalStatus::HISTORY_MISMATCH : PoaJournalStatus::STORAGE_ERROR;
    }
    m_verified_height = finalized_height;
    m_verified_tip = finalized_tip;
    m_history_verified = true;
    return PoaJournalStatus::NONE;
}

PoaJournalStatus PoaSigningJournal::PrepareToSign(const uint64_t height,
    const uint256& parent_block_id, const uint256& block_id)
{
    std::lock_guard lock{m_mutex};
    if (m_halted) return PoaJournalStatus::JOURNAL_HALTED;
    if (!m_history_verified) return PoaJournalStatus::HISTORY_NOT_VERIFIED;
    if (height == 0 || parent_block_id.IsNull() || block_id.IsNull()) return PoaJournalStatus::INVALID_REQUEST;

    if (height == m_head.height) {
        if (parent_block_id == m_head.parent_block_id && block_id == m_head.block_id) {
            return PoaJournalStatus::ALREADY_PREPARED;
        }
        return PersistHalt(PoaJournalStatus::EQUIVOCATION) ?
            PoaJournalStatus::EQUIVOCATION : PoaJournalStatus::STORAGE_ERROR;
    }
    if (m_head.height == std::numeric_limits<uint64_t>::max() || height != m_head.height + 1) {
        return PoaJournalStatus::HEIGHT_MISMATCH;
    }
    if (m_verified_height != m_head.height || m_verified_tip != m_head.block_id) {
        return PoaJournalStatus::HISTORY_NOT_VERIFIED;
    }
    if (parent_block_id != m_head.block_id) return PoaJournalStatus::PARENT_MISMATCH;

    const PoaJournalHead next{.height = height, .parent_block_id = parent_block_id, .block_id = block_id};
    try {
        m_db.Write(m_prefix + "head", EncodeHead(next), true);
        m_head = next;
        return PoaJournalStatus::NONE;
    } catch (...) {
        return PoaJournalStatus::STORAGE_ERROR;
    }
}

PoaJournalHead PoaSigningJournal::Head() const
{
    std::lock_guard lock{m_mutex};
    return m_head;
}

bool PoaSigningJournal::SafetyHalted() const
{
    std::lock_guard lock{m_mutex};
    return m_halted;
}

bool PoaSigningJournal::PersistHalt(const PoaJournalStatus reason) noexcept
{
    // Fail closed in this process even when the durable write itself fails.
    // The caller receives STORAGE_ERROR and must stop the node; it must not
    // retry signing against an unverified or conflicting history.
    m_halted = true;
    m_history_verified = false;
    try {
        m_db.Write(m_prefix + "halt", std::vector<unsigned char>{
            JOURNAL_FORMAT_VERSION, static_cast<unsigned char>(reason)}, true);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cybou
