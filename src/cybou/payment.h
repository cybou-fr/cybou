// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Канонические payload и state-transition helpers для Payment и SystemLock.

#ifndef CYBOU_PAYMENT_H
#define CYBOU_PAYMENT_H

#include <cybou/state.h>

#include <array>
#include <optional>

namespace cybou {

/// \brief Размер канонического payload перевода.
inline constexpr size_t PAYMENT_PAYLOAD_SIZE{40};

/// \brief Канонический payload перевода spendable Balance между аккаунтами.
struct PaymentPayload {
    AccountId recipient;
    uint64_t amount{0};

    friend bool operator==(const PaymentPayload&, const PaymentPayload&) = default;
};

/// \brief Identity-authorized Payment, пригодный для включения в блок.
struct AuthorizedPayment {
    IdentityOperationAuthorization authorization;
    PaymentPayload payment;

    friend bool operator==(const AuthorizedPayment&, const AuthorizedPayment&) = default;
};

/// \brief Размер канонического payload перемещения средств в System Balance.
inline constexpr size_t SYSTEM_LOCK_PAYLOAD_SIZE{8};

/// \brief Канонический payload необратимого перевода Balance -> System Balance.
struct SystemLockPayload {
    uint64_t amount{0};

    friend bool operator==(const SystemLockPayload&, const SystemLockPayload&) = default;
};

/// \brief Identity-authorized SystemLock, пригодный для включения в блок.
struct AuthorizedSystemLock {
    IdentityOperationAuthorization authorization;
    SystemLockPayload lock;

    friend bool operator==(const AuthorizedSystemLock&, const AuthorizedSystemLock&) = default;
};

/// \brief Ошибки применения Payment к кандидатному состоянию.
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
    FEE_TRANSFER_FAILED,
    INCONSISTENT_STATE,
};

/// \brief Ошибки применения SystemLock к кандидатному состоянию.
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

/// \brief Сериализует payload перевода в канонический бинарный формат.
std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE>> SerializePaymentPayload(const PaymentPayload& payment);
/// \brief Десериализует payload перевода.
std::optional<PaymentPayload> DeserializePaymentPayload(std::span<const unsigned char> bytes);
/// \brief Вычисляет payload commitment для Payment.
std::optional<IdentityKeyId> ComputePaymentPayloadCommitment(const PaymentPayload& payment);
/// \brief Применяет перевод и маршрутизацию комиссии Central Authority.
PaymentError ApplyPayment(const AuthorizedPayment& operation,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params,
    CybouState& state);

/// \brief Сериализует payload SystemLock в канонический бинарный формат.
std::optional<std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE>> SerializeSystemLockPayload(const SystemLockPayload& lock);
/// \brief Десериализует payload SystemLock.
std::optional<SystemLockPayload> DeserializeSystemLockPayload(std::span<const unsigned char> bytes);
/// \brief Вычисляет payload commitment для SystemLock.
std::optional<IdentityKeyId> ComputeSystemLockPayloadCommitment(const SystemLockPayload& lock);
/// \brief Применяет Balance -> System Balance к кандидатному состоянию.
SystemLockError ApplySystemLock(const AuthorizedSystemLock& operation,
    const cybou::Hash256& network_binding,
    CybouState& state);

} // namespace cybou
#endif // CYBOU_PAYMENT_H
