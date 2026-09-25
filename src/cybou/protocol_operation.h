// Copyright (c) 2026 Stanislav SAVELIEV
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

} // namespace cybou
#endif // CYBOU_PROTOCOL_OPERATION_H
