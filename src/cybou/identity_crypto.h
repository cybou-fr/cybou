// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Гибридные криптографические примитивы и идентификаторы ключевых ролей Identity.

#ifndef CYBOU_IDENTITY_CRYPTO_H
#define CYBOU_IDENTITY_CRYPTO_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// Назначение публичного ключа Identity в текущем протоколе.
enum class IdentityKeyPurpose : uint8_t {
    RECOVERY_ROOT = 1,
    AUTHORIZATION = 2,
    POA_FINALIZER = 7,
    /** CYBOU P2P storage provider identity (StorageId = hash of this key). */
    STORAGE = 8,
    /** Strictly offline root authority that signs official network genesis. */
    NETWORK_ROOT = 9,
};

/// Гибридный публичный ключ Ed25519 + ML-DSA для одной роли.
struct IdentityHybridPublicKey {
    IdentityKeyPurpose purpose;
    std::array<unsigned char, 32> ed25519{};
    std::vector<unsigned char> ml_dsa;

    friend bool operator==(const IdentityHybridPublicKey&, const IdentityHybridPublicKey&) = default;
};

/// Гибридная подпись Ed25519 + ML-DSA для одной роли.
struct IdentityHybridSignature {
    std::array<unsigned char, 64> ed25519{};
    std::vector<unsigned char> ml_dsa;

    friend bool operator==(const IdentityHybridSignature&, const IdentityHybridSignature&) = default;
};

/// Выводит публичный ключ заданной роли из 32-байтового секрета роли.
std::optional<IdentityHybridPublicKey> DeriveIdentityPublicKey(
    std::span<const unsigned char, 32> secret,
    IdentityKeyPurpose purpose);
/// Подписывает сообщение ключом заданной роли.
std::optional<IdentityHybridSignature> SignIdentityMessage(
    std::span<const unsigned char, 32> secret,
    IdentityKeyPurpose purpose,
    std::span<const unsigned char> message);
/// Проверяет гибридную подпись сообщения публичным ключом роли.
bool VerifyIdentityMessage(const IdentityHybridPublicKey& key,
    const IdentityHybridSignature& signature,
    std::span<const unsigned char> message);

/// Вычисляет идентификатор текущего recovery-ключа для поиска Identity в состоянии.
std::optional<std::array<unsigned char, 32>> ComputeRecoveryKeyId(
    const IdentityHybridPublicKey& recovery_key);
/// Вычисляет идентификатор текущего authorization-ключа.
std::optional<std::array<unsigned char, 32>> ComputeAuthorizationKeyId(
    const IdentityHybridPublicKey& authorization_key);
/// Вычисляет идентификатор PoA finalizer-ключа.
std::optional<std::array<unsigned char, 32>> ComputePoaFinalizerKeyId(
    const IdentityHybridPublicKey& poa_finalizer_key);

} // namespace cybou
#endif
