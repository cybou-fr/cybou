// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BLOCK_EXECUTOR_V2_H
#define CYBOU_BLOCK_EXECUTOR_V2_H

#include <cybou/protocol_operation_v2.h>

#include <optional>
#include <vector>

namespace cybou {

enum class BlockExecutionErrorV2 : uint8_t {
    NONE,
    TOO_MANY_ACCOUNT_CREATES,
    INVALID_ACCOUNT_CREATE,
    INVALID_PAYMENT,
    INVALID_DEVICE_ADD,
    INVALID_DEVICE_REVOKE,
    INVALID_RECOVERY_ROTATE,
    INVALID_SYSTEM_LOCK,
    INVALID_NAME_COMMIT,
    INVALID_NAME_REVEAL,
    INVALID_MAIL,
    FEE_ROUTING_OVERFLOW,
    INVALID_STATE,
};

struct BlockExecutionResultV2 {
    BlockExecutionErrorV2 error{BlockExecutionErrorV2::NONE};
    size_t failed_operation_index{0};
    AccountCreateStateErrorV2 create_error{AccountCreateStateErrorV2::NONE};
    PaymentErrorV2 payment_error{PaymentErrorV2::NONE};
    IdentityRegistryErrorV2 identity_error{IdentityRegistryErrorV2::NONE};
    SystemLockErrorV2 lock_error{SystemLockErrorV2::NONE};
    NameCommitError name_commit_error{NameCommitError::NONE};
    NameRevealError name_reveal_error{NameRevealError::NONE};
    MailError mail_error{MailError::NONE};
    std::optional<CybouStateV2> state;
    std::optional<uint256> state_root;

    explicit operator bool() const { return error == BlockExecutionErrorV2::NONE && state.has_value() && state_root.has_value(); }
};

BlockExecutionResultV2 ExecuteBlockOperationsV2(const CybouStateV2& parent,
    const std::vector<ProtocolOperationV2>& operations,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params);

using BlockExecutionError = BlockExecutionErrorV2;
using BlockExecutionResult = BlockExecutionResultV2;

} // namespace cybou
#endif
