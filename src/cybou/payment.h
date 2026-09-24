// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PAYMENT_H
#define CYBOU_PAYMENT_H

#include <cybou/state.h>

#include <array>
#include <optional>

namespace cybou {

inline constexpr size_t PAYMENT_PAYLOAD_SIZE{41};

struct PaymentPayload {
    AccountId recipient;
    uint64_t amount{0};

    friend bool operator==(const PaymentPayload&, const PaymentPayload&) = default;
};

struct AuthorizedPayment {
    DeviceAuthorization authorization;
    PaymentPayload payment;

    friend bool operator==(const AuthorizedPayment&, const AuthorizedPayment&) = default;
};

inline constexpr size_t SYSTEM_LOCK_PAYLOAD_SIZE{9};

struct SystemLockPayload {
    uint64_t amount{0};

    friend bool operator==(const SystemLockPayload&, const SystemLockPayload&) = default;
};

struct AuthorizedSystemLock {
    DeviceAuthorization authorization;
    SystemLockPayload lock;

    friend bool operator==(const AuthorizedSystemLock&, const AuthorizedSystemLock&) = default;
};

enum class PaymentError : uint8_t {
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

enum class SystemLockError : uint8_t {
    NONE,
    INVALID_PAYLOAD,
    INVALID_AUTHORIZATION,
    ACCOUNT_NOT_FOUND,
    ZERO_AMOUNT,
    INSUFFICIENT_BALANCE,
    SYSTEM_BALANCE_OVERFLOW,
    INCONSISTENT_STATE,
};

std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE>> SerializePaymentPayload(const PaymentPayload& payment);
std::optional<PaymentPayload> DeserializePaymentPayload(std::span<const unsigned char> bytes);
std::optional<IdentityKeyId> ComputePaymentPayloadCommitment(const PaymentPayload& payment);
PaymentError ApplyPayment(const AuthorizedPayment& operation,
    const uint256& network_id, const CybouProtocolParameters& params,
    CybouState& state);

std::optional<std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE>> SerializeSystemLockPayload(const SystemLockPayload& lock);
std::optional<SystemLockPayload> DeserializeSystemLockPayload(std::span<const unsigned char> bytes);
std::optional<IdentityKeyId> ComputeSystemLockPayloadCommitment(const SystemLockPayload& lock);
SystemLockError ApplySystemLock(const AuthorizedSystemLock& operation,
    const uint256& network_id,
    CybouState& state);

// Transition aliases
using PaymentPayloadV2 = PaymentPayload;
using AuthorizedPaymentV2 = AuthorizedPayment;
using PaymentErrorV2 = PaymentError;
using SystemLockPayloadV2 = SystemLockPayload;
using AuthorizedSystemLockV2 = AuthorizedSystemLock;
using SystemLockErrorV2 = SystemLockError;
inline constexpr size_t PAYMENT_PAYLOAD_SIZE_V2{PAYMENT_PAYLOAD_SIZE};
inline constexpr size_t SYSTEM_LOCK_PAYLOAD_SIZE_V2{SYSTEM_LOCK_PAYLOAD_SIZE};

inline std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE>> SerializePaymentPayloadV2(const PaymentPayload& p) {
    return SerializePaymentPayload(p);
}
inline std::optional<PaymentPayload> DeserializePaymentPayloadV2(std::span<const unsigned char> b) {
    return DeserializePaymentPayload(b);
}
inline std::optional<IdentityKeyId> ComputePaymentPayloadCommitmentV2(const PaymentPayload& p) {
    return ComputePaymentPayloadCommitment(p);
}
inline PaymentError ApplyPaymentV2(const AuthorizedPayment& op, const uint256& nid, const CybouProtocolParameters& params, CybouState& s) {
    return ApplyPayment(op, nid, params, s);
}
inline std::optional<std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE>> SerializeSystemLockPayloadV2(const SystemLockPayload& l) {
    return SerializeSystemLockPayload(l);
}
inline std::optional<SystemLockPayload> DeserializeSystemLockPayloadV2(std::span<const unsigned char> b) {
    return DeserializeSystemLockPayload(b);
}
inline std::optional<IdentityKeyId> ComputeSystemLockPayloadCommitmentV2(const SystemLockPayload& l) {
    return ComputeSystemLockPayloadCommitment(l);
}
inline SystemLockError ApplySystemLockV2(const AuthorizedSystemLock& op, const uint256& nid, CybouState& s) {
    return ApplySystemLock(op, nid, s);
}

} // namespace cybou
#endif // CYBOU_PAYMENT_H
