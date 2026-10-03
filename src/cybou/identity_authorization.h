// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Каноническая сериализация набора Recovery/Authorization публичных ключей.

#ifndef CYBOU_IDENTITY_AUTHORIZATION_H
#define CYBOU_IDENTITY_AUTHORIZATION_H

#include <cybou/identity_crypto.h>

#include <array>
#include <optional>
#include <span>

namespace cybou {

/// 3330 = 1 suite byte + 32 Ed25519 + 1952 ML-DSA-65 + 1 suite byte + 32 Ed25519 + 1312 ML-DSA-44.
inline constexpr size_t IDENTITY_AUTHORIZATION_SIZE{3330};
/// Фиксированное бинарное представление дескриптора авторизации Identity.
using IdentityAuthorizationBytes = std::array<unsigned char, IDENTITY_AUTHORIZATION_SIZE>;

/// Публичные ключи, публикуемые при создании Identity.
struct IdentityAuthorization {
    /// Recovery public key для восстановления и последующей ротации.
    IdentityHybridPublicKey recovery_root;
    /// Authorization public key для обычных пользовательских кандидат-операций.
    IdentityHybridPublicKey authorization_key;

    friend bool operator==(const IdentityAuthorization&, const IdentityAuthorization&) = default;
};

/// Сериализует дескриптор Identity в канонический фиксированный формат.
/// \param authorization Пара Recovery/Authorization публичных ключей.
/// \return Фиксированный буфер длиной `IDENTITY_AUTHORIZATION_SIZE` или `std::nullopt`, если роли/размеры/ключи недопустимы.
/// \pre `recovery_root` и `authorization_key` относятся к разным ролям и не совпадают по Ed25519-компоненту.
/// \post Возвращаемые байты пригодны для коммитмента и wire без дополнительной нормализации.
/// \thread_safety Потокобезопасна.
std::optional<IdentityAuthorizationBytes> SerializeIdentityAuthorization(
    const IdentityAuthorization& authorization);
/// Десериализует и валидирует канонический дескриптор Identity.
/// \param bytes Байты фиксированного формата.
/// \return Валидированный дескриптор или `std::nullopt`, если длина/suite/ключи некорректны.
/// \post При ошибке частично прочитанные ключи наружу не возвращаются.
/// \thread_safety Потокобезопасна.
std::optional<IdentityAuthorization> DeserializeIdentityAuthorization(
    std::span<const unsigned char> bytes);
/// Вычисляет доменно-разделённый коммитмент дескриптора Identity.
/// \param authorization Валидный дескриптор Identity.
/// \return 32-байтовый digest или `std::nullopt`, если сериализация не удалась.
/// \pre Используется только с каноническим набором ролей Recovery/Authorization.
/// \post Коммитмент не раскрывает секреты и может логироваться.
/// \thread_safety Потокобезопасна.
std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitment(
    const IdentityAuthorization& authorization);

} // namespace cybou
#endif // CYBOU_IDENTITY_AUTHORIZATION_H
