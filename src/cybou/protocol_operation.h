// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION{5};
inline constexpr size_t AUTHORIZED_PAYMENT_SIZE{IDENTITY_OPERATION_AUTH_SIZE + 41};
inline constexpr size_t IDENTITY_ROTATE_SIZE{13825};
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_SIZE{IDENTITY_OPERATION_AUTH_SIZE + 9};

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

using ProtocolOperation = std::variant<
    AccountCreateOp,
    AuthorizedPayment,
    IdentityRotate,
    AuthorizedSystemLock,
    AuthorizedNameCommit,
    AuthorizedNameReveal,
    AuthorizedRootPublication,
    PoaAuthAdjustment>;

std::optional<std::vector<unsigned char>> SerializeProtocolOperation(const ProtocolOperation& operation);
std::optional<ProtocolOperation> DeserializeProtocolOperation(std::span<const unsigned char> bytes);
std::optional<cybou::Hash256> ComputeOperationId(const ProtocolOperation& operation);
/** The existing Identity that authorizes the operation; nullopt for AccountCreate and PoaAuthAdjustment. */
std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation);
/** The account that earns AUTH upon finalized execution. Only utility operations (RootPublication, SystemLock) earn AUTH. */
std::optional<AccountId> AuthorityEarningAccount(const ProtocolOperation& operation);
/** Verify operation signatures and payload bindings before volatile mesh relay. */
bool VerifyProtocolOperationRelayProofs(const ProtocolOperation& operation,
    const cybou::Hash256& network_binding, const IdentityRegistry& identities);

} // namespace cybou
#endif // CYBOU_PROTOCOL_OPERATION_H
