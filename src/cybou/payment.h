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

} // namespace cybou
#endif // CYBOU_PAYMENT_H
