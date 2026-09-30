// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/authority.h>

#include <cybou/block.h>
#include <cybou/chunk_id.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_params.h>

#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <variant>

namespace cybou {

std::uint32_t AuthorityTier(const std::uint64_t effective, const std::uint32_t max_tier)
{
    // floor(log2(effective + 1)); saturates so effective == UINT64_MAX stays defined.
    if (effective == UINT64_MAX) return std::min(64U, max_tier);
    const std::uint64_t value = effective + 1;
    const auto tier = static_cast<std::uint32_t>(63 - std::countl_zero(value));
    return std::min(tier, max_tier);
}

AuthorityBudgets AuthorityBudgetsForTier(const std::uint32_t tier, const AuthorityPolicy& policy)
{
    const auto budget = [tier](std::uint64_t base, std::uint64_t per_tier, std::uint64_t ceiling) {
        return std::min(SaturatingAdd(base, SaturatingMul(per_tier, tier)), ceiling);
    };
    return {
        .protocol_operations_per_epoch = budget(policy.protocol_base, policy.protocol_per_tier, policy.protocol_ceiling),
        .storage_bytes = budget(policy.storage_base, policy.storage_per_tier, policy.storage_ceiling),
        .bandwidth_bytes_per_epoch = budget(policy.bandwidth_base, policy.bandwidth_per_tier, policy.bandwidth_ceiling),
    };
}

std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation)
{
    return std::visit([](const auto& op) -> std::optional<AccountId> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, AccountCreateOp>) {
            // Creation establishes the Identity; it is not qualifying activity.
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, IdentityRotate>) {
            return op.account_id;
        } else if constexpr (std::is_same_v<T, ServiceEvidence>) {
            return op.account_id;
        } else {
            return op.authorization.account_id;
        }
    }, operation);
}

AuthorityRecord ComputeAuthorityRecord(const AccountId& account, const AccountState& state,
    uint64_t epoch, const AuthorityPolicy& policy)
{
    AuthorityRecord record;
    record.account_id = account; record.creation_epoch = state.creation_epoch; record.current_epoch = epoch;
    record.age = epoch > state.creation_epoch ? epoch - state.creation_epoch : 0;
    record.activity = state.authority.activity;
    record.system_contribution = state.authority.system_contribution;
    record.penalty_debt = state.authority.penalty_debt;
    record.liveness = state.authority.liveness; record.storage = state.authority.storage;
    record.validation = state.authority.validation;
    record.earned = SaturatingAdd(SaturatingAdd(record.age, record.activity), record.system_contribution);
    record.earned = SaturatingAdd(record.earned, SaturatingAdd(record.liveness, record.storage));
    record.earned = SaturatingAdd(record.earned, record.validation);
    record.effective = record.earned > record.penalty_debt ? record.earned - record.penalty_debt : 0;
    record.tier = AuthorityTier(record.effective, policy.max_tier);
    record.budgets = AuthorityBudgetsForTier(record.tier, policy);
    record.enforced = true;
    return record;
}
uint64_t AuthorityIndex::Sync(uint64_t) { return ScannedHeight(); }
uint64_t AuthorityIndex::ScannedHeight() const { return m_runtime.GetFinalizedHeight().value_or(0); }
std::optional<AuthorityRecord> AuthorityIndex::Get(const AccountId& account)
{
    std::optional<AuthorityRecord> result;
    m_runtime.ReadFinalizedValidationSnapshot([&](const FinalizedValidationSnapshot& snapshot) {
        const auto owner = snapshot.state.accounts.find(account);
        if (owner != snapshot.state.accounts.end()) result = ComputeAuthorityRecord(account, owner->second,
            EpochForHeight(snapshot.base.height, snapshot.parameters), snapshot.parameters.authority);
    });
    return result;
}
} // namespace cybou
