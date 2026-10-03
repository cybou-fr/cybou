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

/// \brief Размер канонической структуры работы AccountCreate.
inline constexpr size_t ACCOUNT_CREATE_WORK_SIZE{112};
/// \brief Размер канонической сериализации AccountCreateOp.
inline constexpr size_t ACCOUNT_CREATE_SIZE{32 + IDENTITY_AUTHORIZATION_SIZE + IDENTITY_KEM_PACKAGE_SIZE +
    ACCOUNT_CREATE_WORK_SIZE + 64 + 3309 + 64 + 2420};

/// \brief Данные proof-of-work, привязанные к сети, аккаунту и authorization commitment.
struct AccountCreationWork {
    cybou::Hash256 network_binding;
    AccountId account_id;
    std::array<unsigned char, 32> authorization_commitment{};
    uint64_t work_epoch{0};
    uint64_t nonce{0};

    friend bool operator==(const AccountCreationWork&, const AccountCreationWork&) = default;
};

/// \brief Полная операция создания аккаунта с PoW и двумя proof-of-possession.
struct AccountCreateOp {
    AccountId account_id;
    IdentityAuthorization authorization;
    IdentityKemPackage kem_package{};
    AccountCreationWork work;
    IdentityHybridSignature recovery_pop;
    IdentityHybridSignature authorization_pop;

    friend bool operator==(const AccountCreateOp&, const AccountCreateOp&) = default;
};

/// \brief Ошибки форматной и криптографической проверки AccountCreate.
enum class AccountCreateError : uint8_t {
    NONE,
    INVALID_FORMAT,
    NULL_ACCOUNT_ID,
    NETWORK_MISMATCH,
    ACCOUNT_ID_MISMATCH,
    COMMITMENT_MISMATCH,
    FUTURE_WORK_EPOCH,
    EXPIRED_WORK_EPOCH,
    INSUFFICIENT_WORK,
    INVALID_RECOVERY_POP,
    INVALID_AUTHORIZATION_POP,
};

/// \brief Сериализует структуру AccountCreationWork в канонический бинарный формат.
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
/// \brief Выполняет полную статическую валидацию AccountCreate против текущих протокольных параметров.
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
