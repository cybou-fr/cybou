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

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace cybou {

/// \brief Размеры канонических сериализаций наиболее частых протокольных операций.
inline constexpr size_t AUTHORIZED_PAYMENT_SIZE{IDENTITY_OPERATION_AUTH_SIZE + PAYMENT_PAYLOAD_SIZE};
inline constexpr size_t IDENTITY_ROTATE_SIZE{32 + 32 + 1952 + 32 + 1312 + IDENTITY_KEM_PACKAGE_SIZE +
    8 + 8 + 2 * (64 + 3309) + 64 + 2420};
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_SIZE{IDENTITY_OPERATION_AUTH_SIZE + SYSTEM_LOCK_PAYLOAD_SIZE};

/// \brief Дискриминатор канонического бинарного формата ProtocolOperation.
enum class ProtocolOperationKind : uint8_t {
    ACCOUNT_CREATE = 1,
    PAYMENT = 2,
    IDENTITY_ROTATE = 3,
    SYSTEM_LOCK = 4,
    NAME_COMMIT = 5,
    NAME_REVEAL = 6,
    ROOT_PUBLICATION = 7,
    POA_AUTH_ADJUSTMENT = 8,
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
    PoaAuthAdjustment>;

/// \brief Сериализует tagged union операции в канонический бинарный формат.
std::optional<std::vector<unsigned char>> SerializeProtocolOperation(const ProtocolOperation& operation);
/// \brief Десериализует tagged union операции из канонического бинарного формата.
std::optional<ProtocolOperation> DeserializeProtocolOperation(std::span<const unsigned char> bytes);
/// \brief Вычисляет domain-separated OperationID из канонической сериализации.
std::optional<cybou::Hash256> ComputeOperationId(const ProtocolOperation& operation);
/// \brief Возвращает существующий AccountId-авторизатор операции; nullopt для AccountCreate и PoaAuthAdjustment.
std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation);
/// \brief Возвращает аккаунт, который получает AUTH после финализации utility-операции.
std::optional<AccountId> AuthorityEarningAccount(const ProtocolOperation& operation);
/// \brief Проверяет подписи и binding payload перед ретрансляцией в volatile mesh.
bool VerifyProtocolOperationRelayProofs(const ProtocolOperation& operation,
    const cybou::Hash256& network_binding, const IdentityRegistry& identities);

} // namespace cybou
#endif // CYBOU_PROTOCOL_OPERATION_H
