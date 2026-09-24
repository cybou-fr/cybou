// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_OPERATION_V2_H
#define CYBOU_PROTOCOL_OPERATION_V2_H

#include <cybou/account_creation_v2.h>
#include <cybou/mail_tx.h>
#include <cybou/name_registry.h>
#include <cybou/payment_v2.h>

#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace cybou {

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION_V2{2};
inline constexpr size_t AUTHORIZED_PAYMENT_V2_SIZE{2638};
inline constexpr size_t DEVICE_ADD_V2_SIZE{7241};
inline constexpr size_t DEVICE_REVOKE_V2_SIZE{3445};
inline constexpr size_t RECOVERY_ROTATE_V2_SIZE{8770};
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_V2_SIZE{2606};
inline constexpr size_t AUTHORIZED_NAME_COMMIT_V2_SIZE{AUTHORIZED_NAME_COMMIT_SIZE};
inline constexpr size_t AUTHORIZED_NAME_REVEAL_V2_SIZE{AUTHORIZED_NAME_REVEAL_SIZE};

enum class ProtocolOperationKindV2 : uint8_t {
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

using ProtocolOperationV2 = std::variant<
    AccountCreateOpV2,
    AuthorizedPaymentV2,
    DeviceAddV2,
    DeviceRevokeV2,
    RecoveryRotateV2,
    AuthorizedSystemLockV2,
    AuthorizedNameCommit,
    AuthorizedNameReveal,
    AuthorizedMail>;

std::optional<std::vector<unsigned char>> SerializeProtocolOperationV2(const ProtocolOperationV2& operation);
std::optional<ProtocolOperationV2> DeserializeProtocolOperationV2(std::span<const unsigned char> bytes);
std::optional<uint256> ComputeOperationIdV2(const ProtocolOperationV2& operation);

inline constexpr uint8_t PROTOCOL_OPERATION_VERSION{PROTOCOL_OPERATION_VERSION_V2};
using ProtocolOperationKind = ProtocolOperationKindV2;
using ProtocolOperation = ProtocolOperationV2;
using AuthorizedNameCommitV2 = AuthorizedNameCommit;
using AuthorizedNameRevealV2 = AuthorizedNameReveal;
using AuthorizedMailV2 = AuthorizedMail;
inline constexpr size_t AUTHORIZED_PAYMENT_SIZE{AUTHORIZED_PAYMENT_V2_SIZE};
inline constexpr size_t DEVICE_ADD_SIZE{DEVICE_ADD_V2_SIZE};
inline constexpr size_t DEVICE_REVOKE_SIZE{DEVICE_REVOKE_V2_SIZE};
inline constexpr size_t RECOVERY_ROTATE_SIZE{RECOVERY_ROTATE_V2_SIZE};
inline constexpr size_t AUTHORIZED_SYSTEM_LOCK_SIZE{AUTHORIZED_SYSTEM_LOCK_V2_SIZE};
inline constexpr size_t AUTHORIZED_NAME_COMMIT_SIZE_ALIAS{AUTHORIZED_NAME_COMMIT_V2_SIZE};
inline constexpr size_t AUTHORIZED_NAME_REVEAL_SIZE_ALIAS{AUTHORIZED_NAME_REVEAL_V2_SIZE};

} // namespace cybou
#endif
