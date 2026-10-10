// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Детерминированное выполнение операций блока поверх родительского состояния.

#ifndef CYBOU_BLOCK_EXECUTOR_H
#define CYBOU_BLOCK_EXECUTOR_H

#include <cybou/protocol_operation.h>
#include <cybou/protocol_params.h>
#include <cybou/state.h>

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

namespace cybou {

/// \brief Хэшер 32-байтовых массивов и Hash256 для детерминированных множеств при исполнении блока.
struct ByteArray32Hasher {
    size_t operator()(const std::array<unsigned char, 32>& value) const noexcept
    {
        size_t hash{1469598103934665603ull};
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    size_t operator()(const cybou::Hash256& value) const noexcept
    {
        size_t hash{1469598103934665603ull};
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

/// \brief Хэшер AccountId для детерминированных множеств при исполнении блока.
struct AccountIdHasher {
    size_t operator()(const AccountId& account) const noexcept
    {
        return ByteArray32Hasher{}(account.Value());
    }
};

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
    INVALID_REVOKE_PUBLICATION,  ///< Одна из операций `RevokePublication` отвергнута.
    INVALID_STORAGE_LEASE,       ///< Одна из операций `StorageLease` отвергнута.
    INVALID_STORAGE_SETTLEMENT,  ///< `StorageSettlement` отвергнут.
    SUPPLY_CHANGED,              ///< Исполнение изменило TotalCybou: mint и burn запрещены (DEC-277).
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
    RevokePublicationError revoke_error{RevokePublicationError::NONE}; ///< Детализация для `INVALID_REVOKE_PUBLICATION`.
    StorageLeaseError lease_error{StorageLeaseError::NONE}; ///< Детализация для `INVALID_STORAGE_LEASE`.
    StorageSettlementError settlement_error{StorageSettlementError::NONE}; ///< Детализация для `INVALID_STORAGE_SETTLEMENT`.
    std::optional<CybouState> state; ///< Кандидатное итоговое состояние при успехе.
    std::optional<cybou::Hash256> state_root; ///< Детерминированный `state root` итогового состояния.

    /// \brief Проверяет отсутствие ошибки применения операций (без требования вычисленного state_root).
    bool IsOk() const { return error == BlockExecutionError::NONE; }

    explicit operator bool() const { return error == BlockExecutionError::NONE && state.has_value() && state_root.has_value(); }
};

/// \brief Инкрементальное применение операций блока поверх неизменяемого родительского состояния.
class BlockExecutor {
public:
    BlockExecutor(const CybouState& parent,
                  const cybou::Hash256& network_binding,
                  uint64_t block_height,
                  const CybouProtocolParameters& params,
                  const IdentityHybridPublicKey* poa_key = nullptr,
                  const cybou::Hash256& verified_parent_id = {});

    /// \brief Устанавливает указатель на родительское состояние при перемещении внешнего контекста.
    void SetParent(const CybouState& parent) noexcept { m_parent = &parent; }

    /// \brief Признак успешной инициализации родительского состояния.
    bool IsValid() const noexcept { return m_valid; }
    /// \brief Ошибка инициализации, если IsValid() == false.
    BlockExecutionError InitError() const noexcept { return m_init_error; }

    /// \brief Применяет одну кандидат-операцию к накапливающемуся состоянию.
    /// \return Успех либо ошибка с детализацией.
    /// \note При ошибке состояние `m_valid` становится `false` (инвалидация контекста).
    BlockExecutionResult ApplyOperation(const ProtocolOperation& operation);

    /// \brief Проверяет возможность финализации блока без копирования состояния.
    bool CanFinalize() const;

    /// \brief Финализирует блок: проверяет supply, валидирует состояние и считает `state root`.
    BlockExecutionResult Finalize() const &;
    BlockExecutionResult Finalize() &&;

    /// \brief Текущее накапливающееся кандидатное состояние.
    const CybouState& GetState() const noexcept { return m_candidate; }
    /// \brief Базовое родительское состояние.
    const CybouState& GetParent() const noexcept { return *m_parent; }
    uint64_t GetBlockHeight() const noexcept { return m_block_height; }
    /// Chain-supplied parent; never taken from an operation payload.
    const cybou::Hash256& GetVerifiedParentId() const noexcept { return m_verified_parent_id; }
    /// ACTIVATE is permitted only two heights after finalized PREPARE.
    std::optional<cybou::Hash256> StorageAssignmentSeed(uint64_t preparation_height) const noexcept;
    size_t GetAccountCreates() const noexcept { return m_account_creates; }

private:
    const CybouState* m_parent{nullptr};
    CybouState m_candidate;
    cybou::Hash256 m_network_binding;
    uint64_t m_block_height{0};
    cybou::Hash256 m_verified_parent_id;
    uint64_t m_initial_supply{0};
    size_t m_account_creates{0};
    CybouProtocolParameters m_params;
    const IdentityHybridPublicKey* m_poa_key{nullptr};
    bool m_valid{false};
    BlockExecutionError m_init_error{BlockExecutionError::NONE};
};

/// \brief Выполняет операции блока, сохраняя консенсусные инварианты TotalCybou и `state root`.
/// \param parent Финализированное родительское состояние.
/// \param operations Упорядоченный список кандидат-операций блока.
/// \param network_binding Привязка активной сети.
/// \param block_height Высота исполняемого блока.
/// \param params Активные protocol parameters.
/// \param poa_key Genesis-authorized PoA key для проверки `StorageSettlement`; `nullptr` запрещает такие операции.
/// \return Полный результат исполнения с первым детализированным отказом либо итоговым состоянием.
/// \pre `parent` должно быть канонически валидным.
/// \post При неуспехе не публикуется частично изменённое состояние; вызывающая сторона получает только диагностический код.
/// \note Потокобезопасно при неизменяемом доступе к входам; функция детерминирована и не обращается к хранилищу.
BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key = nullptr,
    const cybou::Hash256& verified_parent_id = {});

} // namespace cybou
#endif // CYBOU_BLOCK_EXECUTOR_H
