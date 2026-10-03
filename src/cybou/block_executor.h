// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Детерминированное выполнение операций блока поверх родительского состояния.

#ifndef CYBOU_BLOCK_EXECUTOR_H
#define CYBOU_BLOCK_EXECUTOR_H

#include <cybou/protocol_operation.h>

#include <optional>
#include <vector>

namespace cybou {

/// \brief Ошибки детерминированного применения набора операций блока.
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
    INVALID_POA_AUTH_ADJUSTMENT,
    SUPPLY_CHANGED,
    INVALID_STATE,
};

/// \brief Результат выполнения операций блока и вычисления итогового state root.
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
    PoaAuthAdjustmentError poa_auth_error{PoaAuthAdjustmentError::NONE};
    std::optional<CybouState> state;
    std::optional<cybou::Hash256> state_root;

    explicit operator bool() const { return error == BlockExecutionError::NONE && state.has_value() && state_root.has_value(); }
};

/// \brief Плоская награда AUTH за один финализированный utility-оператор в пределах блока.
inline constexpr uint64_t AUTH_PER_FINALIZED_OPERATION{1};

/// \brief Выполняет операции блока, сохраняя консенсусные инварианты supply, AUTH и state root.
BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key = nullptr);

} // namespace cybou
#endif // CYBOU_BLOCK_EXECUTOR_H
