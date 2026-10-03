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
    NONE,                        ///< Все операции применены, итоговый `state root` вычислен.
    TOO_MANY_ACCOUNT_CREATES,    ///< Блок превысил лимит `AccountCreate` на высоту.
    INVALID_ACCOUNT_CREATE,      ///< Одна из операций `AccountCreate` отвергнута.
    INVALID_PAYMENT,             ///< Одна из операций `Payment` отвергнута.
    INVALID_IDENTITY_ROTATE,     ///< Одна из операций `IdentityRotate` отвергнута.
    INVALID_SYSTEM_LOCK,         ///< Одна из операций `SystemLock` отвергнута.
    INVALID_NAME_COMMIT,         ///< Одна из операций `NameCommit` отвергнута.
    INVALID_NAME_REVEAL,         ///< Одна из операций `NameReveal` отвергнута.
    INVALID_ROOT_PUBLICATION,    ///< Одна из операций `RootPublication` отвергнута.
    INVALID_POA_AUTH_ADJUSTMENT, ///< Одна из операций `PoaAuthAdjustment` отвергнута.
    SUPPLY_CHANGED,              ///< Исполнение нарушило инвариант total supply из `docs/cybou/05_CHAIN_STATE.md`.
    INVALID_STATE,               ///< Родительское или итоговое состояние нарушает канонические инварианты.
};

/// \brief Результат выполнения операций блока и вычисления итогового state root.
struct BlockExecutionResult {
    BlockExecutionError error{BlockExecutionError::NONE}; ///< Верхнеуровневый итог исполнения.
    size_t failed_operation_index{0}; ///< Индекс первой отказавшей кандидат-операции.
    AccountCreateStateError create_error{AccountCreateStateError::NONE}; ///< Детализация для `INVALID_ACCOUNT_CREATE`.
    PaymentError payment_error{PaymentError::NONE}; ///< Детализация для `INVALID_PAYMENT`.
    IdentityRegistryError identity_error{IdentityRegistryError::NONE}; ///< Детализация для `INVALID_IDENTITY_ROTATE`.
    SystemLockError lock_error{SystemLockError::NONE}; ///< Детализация для `INVALID_SYSTEM_LOCK`.
    NameCommitError name_commit_error{NameCommitError::NONE}; ///< Детализация для `INVALID_NAME_COMMIT`.
    NameRevealError name_reveal_error{NameRevealError::NONE}; ///< Детализация для `INVALID_NAME_REVEAL`.
    RootPublicationError root_publication_error{RootPublicationError::NONE}; ///< Детализация для `INVALID_ROOT_PUBLICATION`.
    PoaAuthAdjustmentError poa_auth_error{PoaAuthAdjustmentError::NONE}; ///< Детализация для `INVALID_POA_AUTH_ADJUSTMENT`.
    std::optional<CybouState> state; ///< Кандидатное итоговое состояние при успехе.
    std::optional<cybou::Hash256> state_root; ///< Детерминированный `state root` итогового состояния.

    explicit operator bool() const { return error == BlockExecutionError::NONE && state.has_value() && state_root.has_value(); }
};

/// \brief Плоская награда AUTH за один финализированный utility-оператор в пределах блока.
/// \details Значение зафиксировано правилами `docs/cybou/57_IDENTITY_AUTHORITY.md`: utility даёт +1 AUTH,
/// а velocity limit применяет не более одного начисления на аккаунт за блок.
inline constexpr uint64_t AUTH_PER_FINALIZED_OPERATION{1};

/// \brief Выполняет операции блока, сохраняя консенсусные инварианты supply, AUTH и `state root`.
/// \param parent Финализированное родительское состояние.
/// \param operations Упорядоченный список кандидат-операций блока.
/// \param network_binding Привязка активной сети.
/// \param block_height Высота исполняемого блока.
/// \param params Активные protocol parameters.
/// \param poa_key Genesis-authorized PoA key для проверки `PoaAuthAdjustment`; `nullptr` запрещает такие операции.
/// \return Полный результат исполнения с первым детализированным отказом либо итоговым состоянием.
/// \pre `parent` должно быть канонически валидным.
/// \post При неуспехе не публикуется частично изменённое состояние; вызывающая сторона получает только диагностический код.
/// \note Потокобезопасно при неизменяемом доступе к входам; функция детерминирована и не обращается к хранилищу.
BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key = nullptr);

} // namespace cybou
#endif // CYBOU_BLOCK_EXECUTOR_H
