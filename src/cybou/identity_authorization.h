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

inline constexpr size_t IDENTITY_AUTHORIZATION_SIZE{3330};
/// Фиксированное бинарное представление дескриптора авторизации Identity.
using IdentityAuthorizationBytes = std::array<unsigned char, IDENTITY_AUTHORIZATION_SIZE>;

/// Публичные ключи, публикуемые при создании Identity.
struct IdentityAuthorization {
    IdentityHybridPublicKey recovery_root;
    IdentityHybridPublicKey authorization_key;

    friend bool operator==(const IdentityAuthorization&, const IdentityAuthorization&) = default;
};

/// Сериализует дескриптор Identity в канонический фиксированный формат.
std::optional<IdentityAuthorizationBytes> SerializeIdentityAuthorization(
    const IdentityAuthorization& authorization);
/// Десериализует и валидирует канонический дескриптор Identity.
std::optional<IdentityAuthorization> DeserializeIdentityAuthorization(
    std::span<const unsigned char> bytes);
/// Вычисляет доменно-разделённый коммитмент дескриптора Identity.
std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitment(
    const IdentityAuthorization& authorization);

} // namespace cybou
#endif // CYBOU_IDENTITY_AUTHORIZATION_H
