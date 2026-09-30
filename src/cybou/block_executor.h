// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BLOCK_EXECUTOR_H
#define CYBOU_BLOCK_EXECUTOR_H

#include <cybou/protocol_operation.h>

#include <optional>
#include <vector>
#include <functional>

namespace cybou {

enum class BlockExecutionError : uint8_t {
    NONE,
    TOO_MANY_ACCOUNT_CREATES,
    INVALID_ACCOUNT_CREATE,
    INVALID_PAYMENT,
    INVALID_IDENTITY_ROTATE,
    INVALID_SYSTEM_LOCK,
    INVALID_NAME_COMMIT,
    INVALID_NAME_REVEAL,
    INVALID_ROOT_PUBLICATION,
    FEE_ROUTING_OVERFLOW,
    INVALID_STATE,
    AUTHORITY_BUDGET_EXHAUSTED,
    INVALID_NODE_BINDING,
    INVALID_SERVICE_EVIDENCE, INVALID_RESOURCE_RESERVATION,
};

struct BlockExecutionResult {
    BlockExecutionError error{BlockExecutionError::NONE};
    size_t failed_operation_index{0};
    AccountCreateStateError create_error{AccountCreateStateError::NONE};
    PaymentError payment_error{PaymentError::NONE};
    IdentityRegistryError identity_error{IdentityRegistryError::NONE};
    SystemLockError lock_error{SystemLockError::NONE};
    NameCommitError name_commit_error{NameCommitError::NONE};
    NameRevealError name_reveal_error{NameRevealError::NONE};
    RootPublicationError root_publication_error{RootPublicationError::NONE};
    std::optional<CybouState> state;
    std::optional<uint256> state_root;

    explicit operator bool() const { return error == BlockExecutionError::NONE && state.has_value() && state_root.has_value(); }
};

BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, const uint256& parent_block_id = {},
    const std::function<std::optional<RootPublication>(const uint256&)>& finalized_publication = {});

} // namespace cybou
#endif // CYBOU_BLOCK_EXECUTOR_H
