// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Канонический формат AccountCreate и его proof-of-work / proof-of-possession проверки.

#ifndef CYBOU_ACCOUNT_CREATION_H
#define CYBOU_ACCOUNT_CREATION_H

#include <cybou/account_id.h>
#include <cybou/identity_authorization.h>
#include <cybou/identity_kem.h>
#include <cybou/protocol_params.h>
#include <cybou/hash256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cybou {

/// \brief Размер канонической структуры работы `AccountCreate`.
/// \details `network_binding` 32 + `account_id` 32 + `authorization_commitment` 32 +
/// `work_epoch` 8 + `nonce` 8 = 112 байт.
inline constexpr size_t ACCOUNT_CREATE_WORK_SIZE{112};
/// \brief Размер канонической сериализации `AccountCreateOp`.
/// \details
/// - `account_id`: 32;
/// - `IdentityAuthorization`: `IDENTITY_AUTHORIZATION_SIZE`;
/// - `IdentityKemPackage`: `IDENTITY_KEM_PACKAGE_SIZE`;
/// - `AccountCreationWork`: 112;
/// - `recovery_pop`: Ed25519 64 + ML-DSA-65 3309;
/// - `authorization_pop`: Ed25519 64 + ML-DSA-44 2420.
inline constexpr size_t ACCOUNT_CREATE_SIZE{32 + IDENTITY_AUTHORIZATION_SIZE + IDENTITY_KEM_PACKAGE_SIZE +
    ACCOUNT_CREATE_WORK_SIZE + 64 + 3309 + 64 + 2420};

/// \brief Данные proof-of-work, привязанные к сети, аккаунту и authorization commitment.
struct AccountCreationWork {
    cybou::Hash256 network_binding; ///< Сеть, для которой вычислялся PoW.
    AccountId account_id; ///< Стабильный идентификатор создаваемого аккаунта.
    std::array<unsigned char, 32> authorization_commitment{}; ///< Коммитмент на Authorization+KEM, привязанный к PoW.
    uint64_t work_epoch{0}; ///< Epoch для anti-precomputation окна сложности.
    uint64_t nonce{0}; ///< Перебираемый nonce proof-of-work.

    friend bool operator==(const AccountCreationWork&, const AccountCreationWork&) = default;
};

/// \brief Полная операция создания аккаунта с PoW и двумя proof-of-possession.
struct AccountCreateOp {
    AccountId account_id; ///< Создаваемый `AccountId`; нулевое значение недопустимо.
    IdentityAuthorization authorization; ///< Публикуемые Recovery/Authorization ключи нового Identity.
    IdentityKemPackage kem_package{}; ///< Начальный KEM package аккаунта.
    AccountCreationWork work; ///< PoW, связывающий сеть, аккаунт и authorization commitment.
    IdentityHybridSignature recovery_pop; ///< Proof-of-possession Recovery key (Ed25519 + ML-DSA-65).
    IdentityHybridSignature authorization_pop; ///< Proof-of-possession Authorization key (Ed25519 + ML-DSA-44).

    friend bool operator==(const AccountCreateOp&, const AccountCreateOp&) = default;
};

/// \brief Ошибки форматной и криптографической проверки AccountCreate.
enum class AccountCreateError : uint8_t {
    NONE,                    ///< Формат, PoW и оба proof-of-possession корректны.
    INVALID_FORMAT,          ///< Каноническая сериализация невозможна либо размеры ключей/подписей неверны.
    NULL_ACCOUNT_ID,         ///< Запрещённый нулевой `AccountId`.
    NETWORK_MISMATCH,        ///< PoW собран не для текущего `NetworkBinding`.
    ACCOUNT_ID_MISMATCH,     ///< `work.account_id` не совпадает с верхнеуровневым `account_id`.
    COMMITMENT_MISMATCH,     ///< PoW привязан к другому набору Authorization/KEM данных.
    FUTURE_WORK_EPOCH,       ///< Работа помечена будущим epoch относительно высоты блока.
    EXPIRED_WORK_EPOCH,      ///< Работа вышла за допустимое anti-precomputation окно.
    INSUFFICIENT_WORK,       ///< Хэш работы не достигает требуемой сложности.
    INVALID_RECOVERY_POP,    ///< Recovery proof-of-possession недействителен.
    INVALID_AUTHORIZATION_POP, ///< Authorization proof-of-possession недействителен.
};

/// \brief Сериализует структуру `AccountCreationWork` в канонический бинарный формат.
/// \param work Структура работы.
/// \return 112 байт канонического формата либо `std::nullopt`, если обязательные поля нулевые.
/// \post При успехе сериализация однозначна и пригодна для консенсусного хэширования.
std::optional<std::array<unsigned char, ACCOUNT_CREATE_WORK_SIZE>> SerializeAccountCreationWork(
    const AccountCreationWork& work);
/// \brief Сериализует AccountCreateOp в канонический бинарный формат.
std::optional<std::array<unsigned char, ACCOUNT_CREATE_SIZE>> SerializeAccountCreateOp(
    const AccountCreateOp& op);
/// \brief Десериализует AccountCreateOp из канонического бинарного формата.
std::optional<AccountCreateOp> DeserializeAccountCreateOp(std::span<const unsigned char> bytes);
/// \brief Вычисляет domain-separated hash структуры AccountCreationWork.
std::optional<std::array<unsigned char, 32>> ComputeAccountCreateWorkHash(const AccountCreationWork& work);
/// \brief Вычисляет общий digest для recovery/auth proof-of-possession.
std::optional<std::array<unsigned char, 32>> ComputeAccountCreatePopDigest(
    const cybou::Hash256& network_binding, const AccountId& account_id,
    const IdentityAuthorization& authorization,
    std::span<const unsigned char, 32> kem_package_id);
/// \brief Вычисляет commitment authorization+KEM для привязки proof-of-work.
std::optional<std::array<unsigned char, 32>> ComputeAccountCreateAuthorizationCommitment(
    const IdentityAuthorization& authorization,
    std::span<const unsigned char, 32> kem_package_id);
/// \brief Выполняет полную статическую валидацию `AccountCreate` против текущих протокольных параметров.
/// \param op Проверяемая кандидат-операция.
/// \param network_binding Привязка активной сети.
/// \param block_height Высота блока, в котором операция претендует на финализацию.
/// \param params Активные protocol parameters.
/// \return Детализированный код причины отказа либо `NONE`.
/// \post Состояние не меняется; проверка пригодна как для relay, так и для block execution.
/// \note Потокобезопасно и детерминировано.
AccountCreateError ValidateAccountCreateOp(
    const AccountCreateOp& op, const cybou::Hash256& network_binding,
    uint64_t block_height, const CybouProtocolParameters& params);

/// \brief Проверяет, что хэш AccountCreationWork удовлетворяет целевой сложности.
inline bool CheckAccountCreationWork(const AccountCreationWork& work, unsigned required_bits)
{
    const auto hash = ComputeAccountCreateWorkHash(work);
    if (!hash) return false;
    if (required_bits > 256) return false;
    unsigned count{0};
    for (const unsigned char byte : *hash) {
        if (byte == 0) { count += 8; continue; }
        count += std::countl_zero(byte);
        break;
    }
    return count >= required_bits;
}

} // namespace cybou

#endif // CYBOU_ACCOUNT_CREATION_H
