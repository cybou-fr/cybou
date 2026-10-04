// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Канонический tagged-union формат протокольных операций CYBOU.

#ifndef CYBOU_PROTOCOL_OPERATION_H
#define CYBOU_PROTOCOL_OPERATION_H

#include <cybou/account_creation.h>
#include <cybou/name_registry.h>
#include <cybou/payment.h>
#include <cybou/poa_auth_adjustment.h>
#include <cybou/root_publication.h>
#include <cybou/identity_registry.h>
#include <cybou/storage_lease.h>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace cybou {

/// \brief Размер канонической сериализации AuthorizedPayment.
/// \details
/// `IDENTITY_OPERATION_AUTH_SIZE` покрывает общий префикс авторизации
/// (`account_id` 32 байта + `nonce` 8 + `key_epoch` 8 + `kind` 1 +
/// `payload_commitment` 32 + Ed25519 signature 64 + ML-DSA-44 signature 2420),
/// а `PAYMENT_PAYLOAD_SIZE` добавляет `recipient` 32 + `amount` 8.
inline constexpr size_t AUTHORIZED_PAYMENT_SIZE{IDENTITY_OPERATION_AUTH_SIZE + PAYMENT_PAYLOAD_SIZE};
/// \brief Размер канонической сериализации IdentityRotate.
/// \details
/// Сумма полей:
/// - `account_id`: 32;
/// - `new_recovery_key`: Ed25519 public key 32 + ML-DSA-65 public key 1952;
/// - `new_authorization_key`: Ed25519 public key 32 + ML-DSA-44 public key 1312;
/// - `new_kem_package`: `IDENTITY_KEM_PACKAGE_SIZE`;
/// - `nonce`: 8;
/// - `key_epoch`: 8;
/// - `old_recovery_signature`: Ed25519 64 + ML-DSA-65 3309;
/// - `new_recovery_pop`: Ed25519 64 + ML-DSA-65 3309;
/// - `new_authorization_pop`: Ed25519 64 + ML-DSA-44 2420.
inline constexpr size_t IDENTITY_ROTATE_SIZE{32 + 32 + 1952 + 32 + 1312 + IDENTITY_KEM_PACKAGE_SIZE +
    8 + 8 + 2 * (64 + 3309) + 64 + 2420};
/// \brief Размер канонической сериализации AuthorizedSystemLock.
/// \details Общая авторизация `IDENTITY_OPERATION_AUTH_SIZE` + payload `amount` 8 байт.
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_SIZE{IDENTITY_OPERATION_AUTH_SIZE + SYSTEM_LOCK_PAYLOAD_SIZE};

/// \brief Размер канонической сериализации AuthorizedRevokePublication.
inline constexpr size_t AUTHORIZED_REVOKE_PUBLICATION_SIZE{IDENTITY_OPERATION_AUTH_SIZE + REVOKE_PUBLICATION_PAYLOAD_SIZE};
/// \brief Размер канонической сериализации AuthorizedStorageLease.
inline constexpr size_t AUTHORIZED_STORAGE_LEASE_SIZE{IDENTITY_OPERATION_AUTH_SIZE + STORAGE_LEASE_PAYLOAD_SIZE};

/// \brief Дискриминатор канонического бинарного формата ProtocolOperation.
enum class ProtocolOperationKind : uint8_t {
    ACCOUNT_CREATE = 1,      ///< Payload: `AccountCreateOp`, включая PoW и два proof-of-possession.
    PAYMENT = 2,             ///< Payload: `AuthorizedPayment`, перевод `Balance` плюс identity-авторизация.
    IDENTITY_ROTATE = 3,     ///< Payload: `IdentityRotate`, атомарная смена Recovery/Authorization/KEM ролей.
    SYSTEM_LOCK = 4,         ///< Payload: `AuthorizedSystemLock`, необратимый перевод `Balance -> System Balance`.
    NAME_COMMIT = 5,         ///< Payload: `AuthorizedNameCommit`, commit фазы claim-а имени.
    NAME_REVEAL = 6,         ///< Payload: `AuthorizedNameReveal`, reveal фазы claim-а имени с PoW.
    ROOT_PUBLICATION = 7,    ///< Payload: `AuthorizedRootPublication`, единственная кандидат-операция публикации контента.
    POA_AUTH_ADJUSTMENT = 8, ///< Payload: `PoaAuthAdjustment`, PoA-подписанная корректировка AUTH для следующего блока.
    REVOKE_PUBLICATION = 9,  ///< Payload: `AuthorizedRevokePublication`, отзыв собственной публикации автором.
    STORAGE_LEASE = 10,      ///< Payload: `AuthorizedStorageLease`, аренда хранения через StorageEscrow.
    STORAGE_SETTLEMENT = 11, ///< Payload: `StorageSettlement`, PoA-подписанные выплаты providers за период.
};

/// \brief Канонический tagged union всех операций, попадающих в блок.
using ProtocolOperation = std::variant<
    AccountCreateOp,
    AuthorizedPayment,
    IdentityRotate,
    AuthorizedSystemLock,
    AuthorizedNameCommit,
    AuthorizedNameReveal,
    AuthorizedRootPublication,
    PoaAuthAdjustment,
    AuthorizedRevokePublication,
    AuthorizedStorageLease,
    StorageSettlement>;

/// \brief Сериализует tagged union операции в канонический бинарный формат.
/// \param operation Операция в одном из поддерживаемых вариантов `ProtocolOperation`.
/// \return Канонические байты `kind || payload`, либо `std::nullopt`, если конкретный payload
///         нарушает свой формат, содержит неверные размеры ключей/подписей или выходит за лимиты.
/// \pre `operation` уже нормализована в текущий единственный поддерживаемый wire-формат.
/// \post При успехе выходные байты детерминированы и одинаковы на всех Full Node.
/// \note Потокобезопасно; функция не использует глобальное состояние.
std::optional<std::vector<unsigned char>> SerializeProtocolOperation(const ProtocolOperation& operation);
/// \brief Десериализует tagged union операции из канонического бинарного формата.
/// \param bytes Байты `kind || payload`.
/// \return Операция, если `kind` известен, длина точна и payload проходит локальную форматную проверку;
///         иначе `std::nullopt`.
/// \pre `bytes` относятся к одной операции без внешнего framing.
/// \post Возвращаемое значение находится в каноническом внутреннем представлении без legacy-вариантов.
/// \note Потокобезопасно; функция детерминирована и fail-closed.
std::optional<ProtocolOperation> DeserializeProtocolOperation(std::span<const unsigned char> bytes);
/// \brief Вычисляет domain-separated OperationID из сериализованных байтов операции.
/// \param serialized_operation Канонические сериализованные байты операции.
/// \return `OperationID`, либо `std::nullopt`, если вход пуст.
/// \note Потокобезопасно и детерминировано.
std::optional<cybou::Hash256> ComputeOperationId(std::span<const unsigned char> serialized_operation);
/// \brief Вычисляет domain-separated OperationID из канонической сериализации.
/// \param operation Операция в каноническом представлении.
/// \return `OperationID`, либо `std::nullopt`, если операция не сериализуется канонически.
/// \post При успехе идентификатор определяется только байтами операции.
/// \note Потокобезопасно и детерминировано.
std::optional<cybou::Hash256> ComputeOperationId(const ProtocolOperation& operation);
/// \brief Возвращает существующий `AccountId`-авторизатор операции.
/// \param operation Кандидат-операция любого поддерживаемого типа.
/// \return `AccountId` для identity-authorized операций и `IdentityRotate`; `std::nullopt` для
///         `AccountCreate` и `PoaAuthAdjustment`, где аккаунт не авторизует payload текущим Authorization key.
/// \note Потокобезопасно; не проверяет существование аккаунта в состоянии.
std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation);
/// \brief Возвращает аккаунт, который получает AUTH после финализации utility-операции.
/// \param operation Кандидат-операция.
/// \return `AccountId` только для `RootPublication` и `SystemLock`; для всех остальных операций `std::nullopt`.
/// \post Функция отражает правило из `docs/cybou/57_IDENTITY_AUTHORITY.md`: AUTH начисляется
///       только за финализированную сетевую utility, а не за платежи, ротации или name-операции.
/// \note Потокобезопасно и детерминировано.
std::optional<AccountId> AuthorityEarningAccount(const ProtocolOperation& operation);
/// \brief Проверяет подписи и binding payload перед ретрансляцией в volatile mesh.
/// \param operation Кандидат-операция, пришедшая до финализации.
/// \param network_binding Привязка активной сети.
/// \param identities Локально известный финализированный Identity registry.
/// \return `true`, если операция статически корректна и её relay proofs совпадают с финализированным
///         состоянием; `false` при любой неопределённости, включая `PoaAuthAdjustment`.
/// \pre `network_binding` должен относиться к текущей официальной сети.
/// \post Функция не меняет состояние и не признаёт финализацию; Validation и PoA-поведение остаются отдельными.
/// \note Потокобезопасно, детерминировано и fail-closed.
bool VerifyProtocolOperationRelayProofs(const ProtocolOperation& operation,
    const cybou::Hash256& network_binding, const IdentityRegistry& identities);

} // namespace cybou
#endif // CYBOU_PROTOCOL_OPERATION_H
