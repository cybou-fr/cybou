// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_VALIDATION_POOL_H
#define CYBOU_VALIDATION_POOL_H

#include <cybou/validation_attestation.h>

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace cybou {

inline constexpr size_t MAX_VALIDATED_OPERATIONS{256};
inline constexpr size_t MAX_ATTESTATIONS_PER_OPERATION{16};

enum class ValidationPoolAdd : uint8_t { ADDED, DUPLICATE, STALE_BASE, FULL };

/**
 * Volatile RAM sidecar of verified attestations, keyed by OperationID with one
 * attestation per validator AccountID. Every entry shares the current
 * finalized base; advancing the base discards them all. Not state, not a DB.
 */
class ValidationPool
{
public:
    using Key = std::pair<uint256, AccountId>;

    /** Caller has verified the attestation and executed its operation. */
    ValidationPoolAdd Add(const ValidationAttestation& attestation);
    /** A new finalized tip makes every held attestation stale. */
    void ResetBase(const uint256& finalized_tip);
    void Drop(const uint256& operation_id);
    size_t Count(const uint256& operation_id) const;
    std::vector<ValidationAttestation> ForOperation(const uint256& operation_id) const;
    /** First held attestation whose key `skip` rejects is not yet known to the caller. */
    std::optional<ValidationAttestation> First(const std::function<bool(const Key&)>& skip) const;
    size_t Operations() const { return m_entries.size(); }

private:
    uint256 m_base;
    std::map<uint256, std::map<AccountId, ValidationAttestation>> m_entries;
};

} // namespace cybou

#endif // CYBOU_VALIDATION_POOL_H
