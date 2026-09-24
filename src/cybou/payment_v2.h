// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PAYMENT_V2_H
#define CYBOU_PAYMENT_V2_H

#include <cybou/state_v2.h>

#include <array>
#include <optional>

namespace cybou {

inline constexpr size_t PAYMENT_PAYLOAD_SIZE_V2{41};

struct PaymentPayloadV2 {
    AccountId recipient;
    uint64_t amount{0};

    friend bool operator==(const PaymentPayloadV2&, const PaymentPayloadV2&) = default;
};

struct AuthorizedPaymentV2 {
    DeviceAuthorizationV2 authorization;
    PaymentPayloadV2 payment;

    friend bool operator==(const AuthorizedPaymentV2&, const AuthorizedPaymentV2&) = default;
};

inline constexpr size_t SYSTEM_LOCK_PAYLOAD_SIZE_V2{9};

struct SystemLockPayloadV2 {
    uint64_t amount{0};

    friend bool operator==(const SystemLockPayloadV2&, const SystemLockPayloadV2&) = default;
};

struct AuthorizedSystemLockV2 {
    DeviceAuthorizationV2 authorization;
    SystemLockPayloadV2 lock;

    friend bool operator==(const AuthorizedSystemLockV2&, const AuthorizedSystemLockV2&) = default;
};

enum class PaymentErrorV2 : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    SENDER_NOT_FOUND,
    RECIPIENT_NOT_FOUND,
    SELF_PAYMENT,
    ZERO_AMOUNT,
    INSUFFICIENT_BALANCE,
    INSUFFICIENT_SYSTEM_BALANCE,
    RECIPIENT_OVERFLOW,
    FEE_POOL_OVERFLOW,
    INCONSISTENT_STATE,
};

enum class SystemLockErrorV2 : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    ACCOUNT_NOT_FOUND,
    ZERO_AMOUNT,
    INSUFFICIENT_BALANCE,
    SYSTEM_BALANCE_OVERFLOW,
    INCONSISTENT_STATE,
};

std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE_V2>> SerializePaymentPayloadV2(const PaymentPayloadV2& payment);
std::optional<PaymentPayloadV2> DeserializePaymentPayloadV2(std::span<const unsigned char> bytes);
std::optional<IdentityKeyIdV2> ComputePaymentPayloadCommitmentV2(const PaymentPayloadV2& payment);
PaymentErrorV2 ApplyPaymentV2(const AuthorizedPaymentV2& operation,
    const uint256& network_id, const CybouProtocolParameters& params,
    CybouStateV2& state);

std::optional<std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE_V2>> SerializeSystemLockPayloadV2(const SystemLockPayloadV2& lock);
std::optional<SystemLockPayloadV2> DeserializeSystemLockPayloadV2(std::span<const unsigned char> bytes);
std::optional<IdentityKeyIdV2> ComputeSystemLockPayloadCommitmentV2(const SystemLockPayloadV2& lock);
SystemLockErrorV2 ApplySystemLockV2(const AuthorizedSystemLockV2& operation,
    const uint256& network_id,
    CybouStateV2& state);

using PaymentPayload = PaymentPayloadV2;
using AuthorizedPayment = AuthorizedPaymentV2;
using PaymentError = PaymentErrorV2;
using SystemLockPayload = SystemLockPayloadV2;
using AuthorizedSystemLock = AuthorizedSystemLockV2;
using SystemLockError = SystemLockErrorV2;
inline constexpr size_t PAYMENT_PAYLOAD_SIZE{PAYMENT_PAYLOAD_SIZE_V2};
inline constexpr size_t SYSTEM_LOCK_PAYLOAD_SIZE{SYSTEM_LOCK_PAYLOAD_SIZE_V2};

} // namespace cybou
#endif
