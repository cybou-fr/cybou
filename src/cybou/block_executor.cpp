// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/block_executor.h>

#include <algorithm>

namespace cybou {

BlockExecutionResult ExecuteBlockOperations(
    const CybouState& parent_state,
    const std::vector<ProtocolOperationV1>& operations,
    const ProtocolExecutionContextV1& context)
{
    const size_t create_count = static_cast<size_t>(std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return OperationType(operation) == ProtocolOperationType::ACCOUNT_CREATE;
    }));
    if (create_count > context.params.max_account_creates_per_block) {
        BlockExecutionResult failure;
        failure.too_many_account_creates = true;
        return failure;
    }
    auto candidate = parent_state;
    for (const auto& operation : operations) {
        auto result = ApplyProtocolOperation(operation, context, candidate);
        if (!result) {
            BlockExecutionResult failure;
            failure.operation_result = result;
            return failure;
        }
    }
    if (candidate.pending_fee_pool > 0 && !RoutePendingFees(candidate)) {
        BlockExecutionResult failure;
        failure.fee_routing_failed = true;
        return failure;
    }
    const auto root = CybouStateHash(candidate);
    return {.state = std::move(candidate), .state_root = root};
}

} // namespace cybou
