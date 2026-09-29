// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/poa_conflict_detector.h>

#include <cybou/identity_crypto.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

namespace cybou {
namespace {

constexpr unsigned char RECORD_VERSION{1};
constexpr unsigned char HALT_CORRUPT_STORAGE{1};
constexpr unsigned char HALT_EQUIVOCATION{2};
constexpr size_t METADATA_SIZE{1 + 32 + 32};
constexpr size_t EQUIVOCATION_RECORD_SIZE{2 + 2 * POA_FINALITY_CERTIFICATE_SIZE};

std::string Hex(const std::span<const unsigned char> bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out(bytes.size() * 2, '0');
    for (size_t i = 0; i < bytes.size(); ++i) {
        out[2 * i] = digits[bytes[i] >> 4];
        out[2 * i + 1] = digits[bytes[i] & 0x0f];
    }
    return out;
}

std::string Hex(const uint256& value)
{
    return Hex(std::span<const unsigned char>{value.begin(), value.size()});
}

std::string ObservationKey(const std::string& prefix, const PoaFinalityCertificate& certificate)
{
    std::array<unsigned char, 8> height{};
    for (size_t i = 0; i < height.size(); ++i) {
        height[i] = static_cast<unsigned char>(certificate.height >> (8 * i));
    }
    return prefix + "observed/" + Hex(height) + "/" + Hex(certificate.parent_block_id);
}

bool ValidConflictRecord(const std::span<const unsigned char> record,
    const uint256& network_id, const IdentityHybridPublicKey& finalizer_key)
{
    if (record.size() != EQUIVOCATION_RECORD_SIZE || record[0] != RECORD_VERSION ||
        record[1] != HALT_EQUIVOCATION) return false;
    const auto cert_size = POA_FINALITY_CERTIFICATE_SIZE;
    const auto first = DeserializePoaFinalityCertificate(record.subspan(2, cert_size));
    const auto second = DeserializePoaFinalityCertificate(record.subspan(2 + cert_size, cert_size));
    return first && second && first->network_id == network_id && second->network_id == network_id &&
        first->height == second->height && first->parent_block_id == second->parent_block_id &&
        first->block_id != second->block_id &&
        VerifyPoaFinalityCertificate(*first, finalizer_key, network_id,
            first->block_id, first->height, first->parent_block_id) &&
        VerifyPoaFinalityCertificate(*second, finalizer_key, network_id,
            second->block_id, second->height, second->parent_block_id);
}

} // namespace

PoaConflictDetector::PoaConflictDetector(KVStore& db, const uint256& network_id,
    const IdentityHybridPublicKey& genesis_finalizer_key)
    : m_db{db}, m_network_id{network_id}, m_genesis_finalizer_key{genesis_finalizer_key},
      m_prefix{[&network_id, &genesis_finalizer_key] {
          const auto key_id = ComputePoaFinalizerKeyId(genesis_finalizer_key);
          if (network_id.IsNull() || !key_id) {
              throw std::invalid_argument{"invalid PoA conflict detector identity"};
          }
          return std::string{"poa-conflict-detector/"} + Hex(network_id) + "/" + Hex(*key_id) + "/";
      }()}
{
    const auto key_id = ComputePoaFinalizerKeyId(m_genesis_finalizer_key);
    if (!key_id) throw std::invalid_argument{"invalid PoA finalizer public key"};
    std::vector<unsigned char> expected_metadata;
    expected_metadata.reserve(METADATA_SIZE);
    expected_metadata.push_back(RECORD_VERSION);
    expected_metadata.insert(expected_metadata.end(), m_network_id.begin(), m_network_id.end());
    expected_metadata.insert(expected_metadata.end(), key_id->begin(), key_id->end());

    std::vector<unsigned char> metadata;
    const auto metadata_key = m_prefix + "metadata";
    const auto halt_key = m_prefix + "halt";
    if (m_db.Read(metadata_key, metadata)) {
        if (metadata != expected_metadata) throw std::runtime_error{"PoA conflict detector identity mismatch"};
        std::vector<unsigned char> halt;
        if (m_db.Read(halt_key, halt)) {
            const bool valid_reason_only = halt.size() == 2 && halt[0] == RECORD_VERSION &&
                halt[1] == HALT_CORRUPT_STORAGE;
            if (!valid_reason_only && !ValidConflictRecord(halt, m_network_id, m_genesis_finalizer_key)) {
                throw std::runtime_error{"corrupt PoA conflict detector halt record"};
            }
            m_halted = true;
        } else if (m_db.Exists(halt_key)) {
            throw std::runtime_error{"corrupt PoA conflict detector halt record"};
        }
    } else {
        if (m_db.Exists(metadata_key) || m_db.Exists(halt_key)) {
            throw std::runtime_error{"incomplete PoA conflict detector state"};
        }
        m_db.Write(metadata_key, expected_metadata, true);
    }
}

PoaConflictStatus PoaConflictDetector::Observe(
    const PoaFinalityCertificate& certificate, const CybouBlock& block)
{
    const auto encoded = SerializePoaFinalityCertificate(certificate);
    if (!encoded || !VerifyPoaCertificateForBlock(certificate, m_genesis_finalizer_key, m_network_id, block)) {
        return PoaConflictStatus::INVALID_CERTIFICATE;
    }

    std::lock_guard lock{m_mutex};
    if (m_halted) return PoaConflictStatus::ALREADY_HALTED;

    const auto observation_key = ObservationKey(m_prefix, certificate);
    std::vector<unsigned char> previous_bytes;
    if (!m_db.Read(observation_key, previous_bytes)) {
        if (m_db.Exists(observation_key)) {
            const std::vector<unsigned char> halt{RECORD_VERSION, HALT_CORRUPT_STORAGE};
            return PersistHalt(halt) ? PoaConflictStatus::CORRUPT_STORAGE : PoaConflictStatus::STORAGE_ERROR;
        }
        try {
            m_db.Write(observation_key, *encoded, true);
            return PoaConflictStatus::OBSERVED;
        } catch (...) {
            m_halted = true;
            return PoaConflictStatus::STORAGE_ERROR;
        }
    }

    const auto previous = DeserializePoaFinalityCertificate(previous_bytes);
    if (!previous || previous->network_id != m_network_id ||
        previous->height != certificate.height || previous->parent_block_id != certificate.parent_block_id ||
        !VerifyPoaFinalityCertificate(*previous, m_genesis_finalizer_key, m_network_id,
            previous->block_id, previous->height, previous->parent_block_id)) {
        const std::vector<unsigned char> halt{RECORD_VERSION, HALT_CORRUPT_STORAGE};
        return PersistHalt(halt) ? PoaConflictStatus::CORRUPT_STORAGE : PoaConflictStatus::STORAGE_ERROR;
    }
    if (previous->block_id == certificate.block_id) return PoaConflictStatus::ALREADY_OBSERVED;

    std::vector<unsigned char> halt;
    halt.reserve(EQUIVOCATION_RECORD_SIZE);
    halt.push_back(RECORD_VERSION);
    halt.push_back(HALT_EQUIVOCATION);
    halt.insert(halt.end(), previous_bytes.begin(), previous_bytes.end());
    halt.insert(halt.end(), encoded->begin(), encoded->end());
    return PersistHalt(halt) ? PoaConflictStatus::SAFETY_CONFLICT : PoaConflictStatus::STORAGE_ERROR;
}

bool PoaConflictDetector::SafetyHalted() const
{
    std::lock_guard lock{m_mutex};
    return m_halted;
}

bool PoaConflictDetector::PersistHalt(const std::vector<unsigned char>& record) noexcept
{
    m_halted = true;
    try {
        m_db.Write(m_prefix + "halt", record, true);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cybou
