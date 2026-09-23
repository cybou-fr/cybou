// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BLOCK_EXECUTOR_H
#define CYBOU_BLOCK_EXECUTOR_H

#include <cybou/protocol_operation.h>
#include <cybou/state.h>

#include <optional>
#include <vector>

namespace cybou {

struct BlockExecutionResult {
    std::optional<CybouState> state;
    uint256 state_root;
    OperationExecutionResult operation_result{};
    bool fee_routing_failed{false};
    bool too_many_account_creates{false};

    explicit operator bool() const { return state.has_value(); }
};

/** Execute all operations and fee routing against an isolated parent-state copy. */
BlockExecutionResult ExecuteBlockOperations(
    const CybouState& parent_state,
    const std::vector<ProtocolOperationV1>& operations,
    const ProtocolExecutionContextV1& context);

} // namespace cybou

#endif // CYBOU_BLOCK_EXECUTOR_H
