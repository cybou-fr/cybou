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
    if (!SerializeCybouStateV2(parent)) return fail(BlockExecutionErrorV2::INVALID_STATE);
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOpV2>(operation);
    });
    if (creates > params.max_account_creates_per_block) return fail(BlockExecutionErrorV2::TOO_MANY_ACCOUNT_CREATES);
    auto candidate = parent;
    for (size_t i{0}; i < operations.size(); ++i) {
        if (const auto* create = std::get_if<AccountCreateOpV2>(&operations[i])) {
            const auto result = ApplyAccountCreateV2(*create, network_id, block_height, params, candidate);
            if (result != AccountCreateStateErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_ACCOUNT_CREATE);
                failure.failed_operation_index = i;
                failure.create_error = result;
                return failure;
            }
        } else {
            const auto result = ApplyPaymentV2(std::get<AuthorizedPaymentV2>(operations[i]), network_id, params, candidate);
            if (result != PaymentErrorV2::NONE) {
                auto failure = fail(BlockExecutionErrorV2::INVALID_PAYMENT);
                failure.failed_operation_index = i;
                failure.payment_error = result;
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
    const auto root = CybouStateHashV2(candidate);
    if (!root) return fail(BlockExecutionErrorV2::INVALID_STATE);
    BlockExecutionResultV2 success{};
    success.state = std::move(candidate);
    success.state_root = *root;
    return success;
}
} // namespace cybou
