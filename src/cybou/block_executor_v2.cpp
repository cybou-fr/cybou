// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block_executor_v2.h>

#include <algorithm>
#include <limits>

namespace cybou {

BlockExecutionResultV2 ExecuteBlockOperationsV2(const CybouStateV2& parent,
    const std::vector<ProtocolOperationV2>& operations,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params)
{
    const auto fail = [](BlockExecutionErrorV2 error) {
        BlockExecutionResultV2 result{};
        result.error = error;
        return result;
    };
    if (ValidateCybouStateV2(parent) != StateValidationErrorV2::NONE) return fail(BlockExecutionErrorV2::INVALID_STATE);
    const uint64_t initial_supply = TotalSupply(parent);
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOpV2>(operation);
    });
    if (creates > params.max_account_creates_per_block) return fail(BlockExecutionErrorV2::TOO_MANY_ACCOUNT_CREATES);
    auto candidate = parent;
    if (params.name_commit_max_lifetime > 0) {
        std::erase_if(candidate.names.pending_commits, [&](const auto& item) {
            return block_height > item.second.commit_height + params.name_commit_max_lifetime;
        });
    }
    for (size_t i{0}; i < operations.size(); ++i) {
        if (const auto* create = std::get_if<AccountCreateOpV2>(&operations[i])) {
            const auto result = ApplyAccountCreateV2(*create, network_id, block_height, params, candidate);
            if (result != AccountCreateStateErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_ACCOUNT_CREATE);
                failure.failed_operation_index = i;
                failure.create_error = result;
                return failure;
            }
        } else if (const auto* payment = std::get_if<AuthorizedPaymentV2>(&operations[i])) {
            const auto result = ApplyPaymentV2(*payment, network_id, params, candidate);
            if (result != PaymentErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_PAYMENT);
                failure.failed_operation_index = i;
                failure.payment_error = result;
                return failure;
            }
        } else if (const auto* add = std::get_if<DeviceAddV2>(&operations[i])) {
            const auto result = candidate.identities.AddDevice(*add, network_id);
            if (result != IdentityRegistryErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_DEVICE_ADD);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* revoke = std::get_if<DeviceRevokeV2>(&operations[i])) {
            const auto result = candidate.identities.RevokeDevice(*revoke, network_id);
            if (result != IdentityRegistryErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_DEVICE_REVOKE);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* rotate = std::get_if<RecoveryRotateV2>(&operations[i])) {
            const auto result = candidate.identities.RotateRecovery(*rotate, network_id);
            if (result != IdentityRegistryErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_RECOVERY_ROTATE);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* lock = std::get_if<AuthorizedSystemLockV2>(&operations[i])) {
            const auto result = ApplySystemLockV2(*lock, network_id, candidate);
            if (result != SystemLockErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_SYSTEM_LOCK);
                failure.failed_operation_index = i;
                failure.lock_error = result;
                return failure;
            }
        } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operations[i])) {
            const auto result = ApplyNameCommit(*commit, network_id, block_height, params, candidate);
            if (result != NameCommitError::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_NAME_COMMIT);
                failure.failed_operation_index = i;
                failure.name_commit_error = result;
                return failure;
            }
        } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operations[i])) {
            const auto result = ApplyNameReveal(*reveal, network_id, block_height, params, candidate);
            if (result != NameRevealError::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_NAME_REVEAL);
                failure.failed_operation_index = i;
                failure.name_reveal_error = result;
                return failure;
            }
        } else if (const auto* mail = std::get_if<AuthorizedMail>(&operations[i])) {
            const auto result = ApplyMail(*mail, network_id, block_height, params, candidate);
            if (result != MailError::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_MAIL);
                failure.failed_operation_index = i;
                failure.mail_error = result;
                return failure;
            }
        }
    }
    const uint64_t chunks = candidate.pending_fee_pool / 4;
    const uint64_t security_addition = chunks * 3;
    if (candidate.security_reward_pool > std::numeric_limits<uint64_t>::max() - security_addition ||
        candidate.onboarding_pool > std::numeric_limits<uint64_t>::max() - chunks) return fail(BlockExecutionErrorV2::FEE_ROUTING_OVERFLOW);
    candidate.security_reward_pool += security_addition;
    candidate.onboarding_pool += chunks;
    candidate.pending_fee_pool %= 4;
    const uint64_t final_supply = TotalSupply(candidate);
    if (final_supply != initial_supply) return fail(BlockExecutionErrorV2::FEE_ROUTING_OVERFLOW);
    if (ValidateCybouStateV2(candidate) != StateValidationErrorV2::NONE) return fail(BlockExecutionErrorV2::INVALID_STATE);
    const auto root = CybouStateHashV2(candidate);
    if (!root) return fail(BlockExecutionErrorV2::INVALID_STATE);
    BlockExecutionResultV2 success{};
    success.state = std::move(candidate);
    success.state_root = *root;
    return success;
}
} // namespace cybou
