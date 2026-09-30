// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_AUTHORITY_H
#define CYBOU_AUTHORITY_H

#include <cybou/account_id.h>
#include <cybou/protocol_operation.h>

#include <cstdint>
#include <map>
#include <filesystem>
#include <array>
#include <mutex>
#include <optional>

namespace cybou {

class CybouNodeRuntime;
struct AccountState;

/** Immutable policy is NetworkID-bound; records project canonical accumulators. */
struct AuthorityBudgets {
    std::uint64_t protocol_operations_per_epoch{0};
    std::uint64_t storage_bytes{0};
    std::uint64_t bandwidth_bytes_per_epoch{0};
    bool operator==(const AuthorityBudgets&) const = default;
};

/** Earnings and penalties are accumulated separately. */
struct AuthorityRecord {
    AccountId account_id;
    std::uint64_t creation_epoch{0};
    std::uint64_t current_epoch{0};
    std::uint64_t age{0};
    std::uint64_t activity{0};
    std::uint64_t system_contribution{0};
    /** Zero until canonical NodeID binding and uptime evidence exist. */
    std::uint64_t liveness{0};
    /** Zero until canonical verified-storage evidence exists. */
    std::uint64_t storage{0};
    std::uint64_t validation{0};
    /** Zero until canonical attributable misbehaviour evidence exists. */
    std::uint64_t penalty_debt{0};
    std::uint64_t earned{0};
    std::uint64_t effective{0};
    std::uint32_t tier{0};
    AuthorityBudgets budgets;
    /** ProtocolBudget is enforced canonically; remote budgets have separate provider accounting. */
    bool enforced{false};
};

/** Saturating integer arithmetic used by every Authority computation. */
constexpr std::uint64_t SaturatingAdd(std::uint64_t a, std::uint64_t b)
{
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

constexpr std::uint64_t SaturatingMul(std::uint64_t a, std::uint64_t b)
{
    return a != 0 && b > UINT64_MAX / a ? UINT64_MAX : a * b;
}

/** tier = min(max_tier, floor(log2(effective + 1))), with integer bit operations. */
std::uint32_t AuthorityTier(std::uint64_t effective, std::uint32_t max_tier);
AuthorityBudgets AuthorityBudgetsForTier(std::uint32_t tier, const AuthorityPolicy& policy);

/** The Identity whose authorization a finalized operation carries, if any. */
std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation);

/** Read-only canonical Authority projection. No policy override or trusted cache. */
AuthorityRecord ComputeAuthorityRecord(const AccountId& account, const AccountState& state,
    uint64_t epoch, const AuthorityPolicy& policy);
class AuthorityIndex final {
public:
    explicit AuthorityIndex(CybouNodeRuntime& runtime) : m_runtime{runtime} {}
    uint64_t Sync(uint64_t max_blocks = 4096);
    std::optional<AuthorityRecord> Get(const AccountId& account);
    uint64_t ScannedHeight() const;
private:
    CybouNodeRuntime& m_runtime;
};
} // namespace cybou
#endif // CYBOU_AUTHORITY_H
