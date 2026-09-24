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
};

struct AuthorizedPaymentV2 {
    DeviceAuthorizationV2 authorization;
    PaymentPayloadV2 payment;
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

std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE_V2>> SerializePaymentPayloadV2(const PaymentPayloadV2& payment);
std::optional<PaymentPayloadV2> DeserializePaymentPayloadV2(std::span<const unsigned char> bytes);
std::optional<IdentityKeyIdV2> ComputePaymentPayloadCommitmentV2(const PaymentPayloadV2& payment);
PaymentErrorV2 ApplyPaymentV2(const AuthorizedPaymentV2& operation,
    const uint256& network_id, const CybouProtocolParameters& params,
    CybouStateV2& state);

} // namespace cybou
#endif
