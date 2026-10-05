// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Канонические payload и state-transition helpers для Payment и SystemLock.

#ifndef CYBOU_PAYMENT_H
#define CYBOU_PAYMENT_H

#include <cybou/state.h>

#include <array>
#include <optional>

namespace cybou {

/// \brief Размер канонического payload перевода.
/// \details `recipient` 32 байта + `amount` 8 байт.
inline constexpr size_t PAYMENT_PAYLOAD_SIZE{40};

/// \brief Канонический payload перевода spendable Balance между аккаунтами.
struct PaymentPayload {
    AccountId recipient; ///< Получатель spendable Balance.
    uint64_t amount{0};  ///< Переводимая сумма в CYBOU; ноль запрещён.

    friend bool operator==(const PaymentPayload&, const PaymentPayload&) = default;
};

/// \brief Identity-authorized Payment, пригодный для включения в блок.
struct AuthorizedPayment {
    IdentityOperationAuthorization authorization;
    PaymentPayload payment;

    friend bool operator==(const AuthorizedPayment&, const AuthorizedPayment&) = default;
};

/// \brief Размер канонического payload перемещения средств в `System Balance`.
/// \details Одно поле `amount` в 8 байт little-endian.
inline constexpr size_t SYSTEM_LOCK_PAYLOAD_SIZE{8};

/// \brief Канонический payload необратимого перевода Balance -> System Balance.
struct SystemLockPayload {
    uint64_t amount{0}; ///< Сумма необратимого перевода `Balance -> System Balance`.

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
    NONE,                     ///< Переход выполнен.
    INVALID_PAYLOAD,          ///< Неканоничный payload либо неверный payload commitment.
    INVALID_AUTHORIZATION,    ///< Identity authorization не совпала с payload или сетью.
    SENDER_NOT_FOUND,         ///< Авторизующий аккаунт отсутствует.
    RECIPIENT_NOT_FOUND,      ///< Получатель отсутствует.
    SELF_PAYMENT,             ///< Платёж самому себе запрещён.
    ZERO_AMOUNT,              ///< Нулевая сумма перевода запрещена.
    INSUFFICIENT_BALANCE,     ///< Недостаточно spendable Balance.
    INSUFFICIENT_SYSTEM_BALANCE, ///< Недостаточно `System Balance` для protocol fee.
    RECIPIENT_OVERFLOW,       ///< У получателя переполнится Balance.
    FEE_TRANSFER_FAILED,      ///< Комиссию нельзя безопасно зачислить Central Authority.
    INCONSISTENT_STATE,       ///< Нарушена связь `accounts`/`identities` или fee destination.
};

/// \brief Ошибки применения SystemLock к кандидатному состоянию.
enum class SystemLockError : uint8_t {
    NONE,                    ///< Переход выполнен.
    INVALID_PAYLOAD,         ///< Неканоничный payload либо неверный payload commitment.
    INVALID_AUTHORIZATION,   ///< Identity authorization недействительна.
    ACCOUNT_NOT_FOUND,       ///< Авторизующий аккаунт отсутствует.
    ZERO_AMOUNT,             ///< Нулевая сумма запрещена.
    INSUFFICIENT_BALANCE,    ///< Недостаточно spendable Balance.
    SYSTEM_BALANCE_OVERFLOW, ///< `System Balance` переполнится.
    INCONSISTENT_STATE,      ///< Нарушены внутренние инварианты состояния.
};

/// \brief Сериализует payload перевода в канонический бинарный формат.
std::optional<std::array<unsigned char, PAYMENT_PAYLOAD_SIZE>> SerializePaymentPayload(const PaymentPayload& payment);
/// \brief Десериализует payload перевода.
std::optional<PaymentPayload> DeserializePaymentPayload(std::span<const unsigned char> bytes);
/// \brief Вычисляет payload commitment для Payment.
std::optional<IdentityKeyId> ComputePaymentPayloadCommitment(const PaymentPayload& payment);
/// \brief Применяет перевод и маршрутизацию комиссии Central Authority.
/// \param operation Identity-authorized `Payment`.
/// \param network_binding Привязка текущей сети.
/// \param params Активные protocol parameters.
/// \param state Кандидатное состояние, изменяемое только при успехе.
/// \return Детализированный код причины отказа либо `NONE`.
/// \post При успехе `amount` переводится из `Balance` отправителя в `Balance` получателя,
///       а protocol fee списывается из `System Balance` и зачисляется Central Authority.
PaymentError ApplyPayment(const AuthorizedPayment& operation,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params,
    CybouState& state);

/// \brief Сериализует payload SystemLock в канонический бинарный формат.
std::optional<std::array<unsigned char, SYSTEM_LOCK_PAYLOAD_SIZE>> SerializeSystemLockPayload(const SystemLockPayload& lock);
/// \brief Десериализует payload SystemLock.
std::optional<SystemLockPayload> DeserializeSystemLockPayload(std::span<const unsigned char> bytes);
/// \brief Вычисляет payload commitment для SystemLock.
std::optional<IdentityKeyId> ComputeSystemLockPayloadCommitment(const SystemLockPayload& lock);
/// \brief Применяет `Balance -> System Balance` к кандидатному состоянию.
/// \param operation Identity-authorized `SystemLock`.
/// \param network_binding Привязка текущей сети.
/// \param state Кандидатное состояние.
/// \return Код ошибки либо `NONE`.
/// \post При успехе сумма атомарно переносится между двумя canonical account values без изменения total supply.
SystemLockError ApplySystemLock(const AuthorizedSystemLock& operation,
    const cybou::Hash256& network_binding,
    CybouState& state);

} // namespace cybou
#endif // CYBOU_PAYMENT_H
