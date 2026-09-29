// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/authority.h>

#include <cybou/node_runtime.h>
#include <cybou/protocol_params.h>

#include <algorithm>
#include <bit>
#include <type_traits>
#include <variant>

namespace cybou {

std::uint32_t AuthorityTier(const std::uint64_t effective, const std::uint32_t max_tier)
{
    // floor(log2(effective + 1)); saturates so effective == UINT64_MAX stays defined.
    const std::uint64_t value = SaturatingAdd(effective, 1);
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
        } else {
            return op.authorization.account_id;
        }
    }, operation);
}

AuthorityIndex::AuthorityIndex(CybouNodeRuntime& runtime, AuthorityPolicy policy)
    : m_runtime{runtime}, m_policy{policy}
{
}

std::uint64_t AuthorityIndex::ScannedHeight() const
{
    std::lock_guard lock{m_mutex};
    return m_height;
}

std::uint64_t AuthorityIndex::Sync(const std::uint64_t max_blocks)
{
    std::lock_guard lock{m_mutex};
    const auto finalized = m_runtime.GetFinalizedHeight().value_or(0);
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    for (std::uint64_t scanned{0}; scanned < max_blocks && m_height < finalized; ++scanned) {
        const auto block = m_runtime.GetBlockAtHeight(m_height + 1);
        if (!block) break;
        const auto epoch = EpochForHeight(block->block.height, params);
        for (const auto& operation : block->block.operations) {
            const auto account = AuthorizingAccount(operation);
            if (!account) continue;
            auto& tally = m_tallies[*account];
            if (tally.epoch != epoch) {
                tally.epoch = epoch;
                tally.epoch_count = 0;
            }
            // Activity: +1 per finalized Identity operation, capped per epoch.
            if (tally.epoch_count < m_policy.activity_cap_per_epoch) {
                ++tally.epoch_count;
                tally.activity = SaturatingAdd(tally.activity, 1);
            }
            // Voluntary Balance -> System Balance lock: one-time contribution.
            if (const auto* lock_op = std::get_if<AuthorizedSystemLock>(&operation)) {
                tally.system_contribution = SaturatingAdd(tally.system_contribution, lock_op->lock.amount);
            }
        }
        ++m_height;
    }
    return m_height;
}

std::optional<AuthorityRecord> AuthorityIndex::Get(const AccountId& account)
{
    std::lock_guard lock{m_mutex};
    const auto state = m_runtime.GetAccountState(account);
    if (!state) return std::nullopt;
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    AuthorityRecord record;
    record.account_id = account;
    record.creation_epoch = state->creation_epoch;
    record.current_epoch = EpochForHeight(m_height, params);
    // Age: completed epochs of Identity lifetime.
    record.age = record.current_epoch > record.creation_epoch ? record.current_epoch - record.creation_epoch : 0;
    if (const auto tally = m_tallies.find(account); tally != m_tallies.end()) {
        record.activity = tally->second.activity;
        record.system_contribution = tally->second.system_contribution;
    }
    record.earned = SaturatingAdd(SaturatingAdd(SaturatingAdd(record.age, record.activity),
        SaturatingAdd(record.system_contribution, record.liveness)), record.storage);
    record.effective = record.earned > record.penalty_debt ? record.earned - record.penalty_debt : 0;
    record.tier = AuthorityTier(record.effective, m_policy.max_tier);
    record.budgets = AuthorityBudgetsForTier(record.tier, m_policy);
    record.enforced = false;
    return record;
}

} // namespace cybou
