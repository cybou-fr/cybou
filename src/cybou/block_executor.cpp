// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block_executor.h>

#include <algorithm>
#include <limits>

namespace cybou {

BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params)
{
    const auto fail = [](BlockExecutionError error) {
        BlockExecutionResult result{};
        result.error = error;
        return result;
    };
    if (ValidateCybouState(parent) != StateValidationError::NONE) return fail(BlockExecutionError::INVALID_STATE);
    const uint64_t initial_supply = TotalSupply(parent);
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOp>(operation);
    });
    if (creates > params.max_account_creates_per_block) return fail(BlockExecutionError::TOO_MANY_ACCOUNT_CREATES);
    auto candidate = parent;
    if (params.name_commit_max_lifetime > 0) {
        std::erase_if(candidate.names.pending_commits, [&](const auto& item) {
            return block_height > item.second.commit_height &&
                block_height - item.second.commit_height > params.name_commit_max_lifetime;
        });
    }
    for (size_t i{0}; i < operations.size(); ++i) {
        if (const auto* create = std::get_if<AccountCreateOp>(&operations[i])) {
            const auto result = ApplyAccountCreate(*create, network_id, block_height, params, candidate);
            if (result != AccountCreateStateError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_ACCOUNT_CREATE);
                failure.failed_operation_index = i;
                failure.create_error = result;
                return failure;
            }
        } else if (const auto* payment = std::get_if<AuthorizedPayment>(&operations[i])) {
            const auto result = ApplyPayment(*payment, network_id, params, candidate);
            if (result != PaymentError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_PAYMENT);
                failure.failed_operation_index = i;
                failure.payment_error = result;
                return failure;
            }
        } else if (const auto* add = std::get_if<DeviceAdd>(&operations[i])) {
            const auto result = candidate.identities.AddDevice(*add, network_id);
            if (result != IdentityRegistryError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_DEVICE_ADD);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* revoke = std::get_if<DeviceRevoke>(&operations[i])) {
            const auto result = candidate.identities.RevokeDevice(*revoke, network_id);
            if (result != IdentityRegistryError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_DEVICE_REVOKE);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* rotate = std::get_if<RecoveryRotate>(&operations[i])) {
            const auto result = candidate.identities.RotateRecovery(*rotate, network_id);
            if (result != IdentityRegistryError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_RECOVERY_ROTATE);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* lock = std::get_if<AuthorizedSystemLock>(&operations[i])) {
            const auto result = ApplySystemLock(*lock, network_id, candidate);
            if (result != SystemLockError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_SYSTEM_LOCK);
                failure.failed_operation_index = i;
                failure.lock_error = result;
                return failure;
            }
        } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operations[i])) {
            const auto result = ApplyNameCommit(*commit, network_id, block_height, params, candidate);
            if (result != NameCommitError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_NAME_COMMIT);
                failure.failed_operation_index = i;
                failure.name_commit_error = result;
                return failure;
            }
        } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operations[i])) {
            const auto result = ApplyNameReveal(*reveal, network_id, block_height, params, candidate);
            if (result != NameRevealError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_NAME_REVEAL);
                failure.failed_operation_index = i;
                failure.name_reveal_error = result;
                return failure;
            }
        } else if (const auto* mail = std::get_if<AuthorizedMail>(&operations[i])) {
            const auto result = ApplyMail(*mail, network_id, block_height, params, candidate);
            if (result != MailError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_MAIL);
                failure.failed_operation_index = i;
                failure.mail_error = result;
                return failure;
            }
        }
    }
    const uint64_t chunks = candidate.pending_fee_pool / 4;
    const uint64_t security_addition = chunks * 3;
    if (candidate.security_reward_pool > std::numeric_limits<uint64_t>::max() - security_addition ||
        candidate.onboarding_pool > std::numeric_limits<uint64_t>::max() - chunks) return fail(BlockExecutionError::FEE_ROUTING_OVERFLOW);
    candidate.security_reward_pool += security_addition;
    candidate.onboarding_pool += chunks;
    candidate.pending_fee_pool %= 4;
    const uint64_t final_supply = TotalSupply(candidate);
    if (final_supply != initial_supply) return fail(BlockExecutionError::FEE_ROUTING_OVERFLOW);
    if (ValidateCybouState(candidate) != StateValidationError::NONE) return fail(BlockExecutionError::INVALID_STATE);
    const auto root = CybouStateHash(candidate);
    if (!root) return fail(BlockExecutionError::INVALID_STATE);
    BlockExecutionResult success{};
    success.state = std::move(candidate);
    success.state_root = *root;
    return success;
}

} // namespace cybou
