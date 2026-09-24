// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_OPERATION_H
#define CYBOU_PROTOCOL_OPERATION_H

#include <cybou/account_creation.h>
#include <cybou/mail_tx.h>
#include <cybou/name_registry.h>
#include <cybou/payment.h>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace cybou {

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION{2};
inline constexpr size_t AUTHORIZED_PAYMENT_SIZE{2638};
inline constexpr size_t DEVICE_ADD_SIZE{7241};
inline constexpr size_t DEVICE_REVOKE_SIZE{3445};
inline constexpr size_t RECOVERY_ROTATE_SIZE{8770};
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_SIZE{2606};

enum class ProtocolOperationKind : uint8_t {
    ACCOUNT_CREATE = 1,
    PAYMENT = 2,
    DEVICE_ADD = 3,
    DEVICE_REVOKE = 4,
    RECOVERY_ROTATE = 5,
    SYSTEM_LOCK = 6,
    NAME_COMMIT = 7,
    NAME_REVEAL = 8,
    MAIL = 9,
};

using ProtocolOperation = std::variant<
    AccountCreateOp,
    AuthorizedPayment,
    DeviceAdd,
    DeviceRevoke,
    RecoveryRotate,
    AuthorizedSystemLock,
    AuthorizedNameCommit,
    AuthorizedNameReveal,
    AuthorizedMail>;

std::optional<std::vector<unsigned char>> SerializeProtocolOperation(const ProtocolOperation& operation);
std::optional<ProtocolOperation> DeserializeProtocolOperation(std::span<const unsigned char> bytes);
std::optional<uint256> ComputeOperationId(const ProtocolOperation& operation);

// Transition aliases
using ProtocolOperationV1 = ProtocolOperation;
inline constexpr uint8_t PROTOCOL_OPERATION_VERSION_V2{PROTOCOL_OPERATION_VERSION};
inline constexpr size_t AUTHORIZED_PAYMENT_V2_SIZE{AUTHORIZED_PAYMENT_SIZE};
inline constexpr size_t DEVICE_ADD_V2_SIZE{DEVICE_ADD_SIZE};
inline constexpr size_t DEVICE_REVOKE_V2_SIZE{DEVICE_REVOKE_SIZE};
inline constexpr size_t RECOVERY_ROTATE_V2_SIZE{RECOVERY_ROTATE_SIZE};
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_V2_SIZE{AUTHORIZED_SYSTEM_LOCK_SIZE};
inline constexpr size_t AUTHORIZED_NAME_COMMIT_V2_SIZE{AUTHORIZED_NAME_COMMIT_SIZE};
inline constexpr size_t AUTHORIZED_NAME_REVEAL_V2_SIZE{AUTHORIZED_NAME_REVEAL_SIZE};
using ProtocolOperationKindV2 = ProtocolOperationKind;
using ProtocolOperationV2 = ProtocolOperation;
using AuthorizedNameCommitV2 = AuthorizedNameCommit;
using AuthorizedNameRevealV2 = AuthorizedNameReveal;
using AuthorizedMailV2 = AuthorizedMail;

inline std::optional<std::vector<unsigned char>> SerializeProtocolOperationV2(const ProtocolOperation& operation)
{
    return SerializeProtocolOperation(operation);
}

inline std::optional<ProtocolOperation> DeserializeProtocolOperationV2(std::span<const unsigned char> bytes)
{
    return DeserializeProtocolOperation(bytes);
}

inline std::optional<uint256> ComputeOperationIdV2(const ProtocolOperation& operation)
{
    return ComputeOperationId(operation);
}

} // namespace cybou
#endif // CYBOU_PROTOCOL_OPERATION_H
