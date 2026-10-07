// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Гибридные криптографические примитивы и идентификаторы ключевых ролей Identity.

#ifndef CYBOU_IDENTITY_CRYPTO_H
#define CYBOU_IDENTITY_CRYPTO_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// Назначение публичного ключа Identity в текущем протоколе.
enum class IdentityKeyPurpose : uint8_t {
    /// Recovery role: поиск Identity по RecoveryKeyId и подписание IdentityRotate.
    RECOVERY_ROOT = 1,
    /// Authorization role: подписание обычных пользовательских кандидат-операций.
    AUTHORIZATION = 2,
    /// PoA role: локальный signer финализации для genesis-authorized ключа.
    POA_FINALIZER = 7,
    /** Storage provider role: CYBOU P2P StorageId = hash of этого ключа. */
    STORAGE = 8,
    /** Строго офлайн network root, подписывающий immutable genesis официальной сети. */
    NETWORK_ROOT = 9,
};

/// Гибридный публичный ключ Ed25519 + ML-DSA для одной роли.
struct IdentityHybridPublicKey {
    /// Роль ключа; должна совпадать с контекстом вывода и проверки подписи.
    IdentityKeyPurpose purpose;
    /// Ed25519-компонент фиксированной длины 32 байта.
    std::array<unsigned char, 32> ed25519{};
    /// ML-DSA-компонент; размер зависит от `purpose`.
    std::vector<unsigned char> ml_dsa;

    friend bool operator==(const IdentityHybridPublicKey&, const IdentityHybridPublicKey&) = default;
};

/// Гибридная подпись Ed25519 + ML-DSA для одной роли.
struct IdentityHybridSignature {
    /// Ed25519-подпись фиксированной длины 64 байта.
    std::array<unsigned char, 64> ed25519{};
    /// ML-DSA-подпись; размер зависит от `purpose`.
    std::vector<unsigned char> ml_dsa;

    friend bool operator==(const IdentityHybridSignature&, const IdentityHybridSignature&) = default;
};

/// Выводит публичный ключ заданной роли из 32-байтового секрета роли.
/// \param secret 32-байтовый секрет роли; вызывающий код не должен логировать или копировать его без нужды.
/// \param purpose Роль Identity, задающая криптографические suite и KDF context.
/// \return Канонический публичный ключ роли или `std::nullopt`, если роль/криптография недопустимы.
/// \pre `secret` содержит реальный секрет роли, а не нулевой буфер.
/// \post Секрет не зануляется этой функцией; ответственность за очистку остаётся у вызывающего кода.
/// \thread_safety Потокобезопасна при независимых аргументах; глобальное состояние Identity не изменяет.
std::optional<IdentityHybridPublicKey> DeriveIdentityPublicKey(
    std::span<const unsigned char, 32> secret,
    IdentityKeyPurpose purpose);
/// Подписывает сообщение ключом заданной роли.
/// \param secret 32-байтовый секрет роли; не логировать и очистить после последнего использования.
/// \param purpose Роль Identity, определяющая suite и KDF context.
/// \param message Подписываемые байты без дополнительной сериализации.
/// \return Гибридная подпись или `std::nullopt`, если секрет/роль/криптография недопустимы.
/// \pre `message` уже доменно-разделено вызывающим кодом, если это требуется протоколом.
/// \post Содержимое `message` не меняется; секрет не зануляется автоматически.
/// \thread_safety Потокобезопасна при независимых аргументах.
std::optional<IdentityHybridSignature> SignIdentityMessage(
    std::span<const unsigned char, 32> secret,
    IdentityKeyPurpose purpose,
    std::span<const unsigned char> message);
/// Signing key of one role, derived once from the role secret and kept on its own: the
/// secret it came from can be wiped while this key keeps signing (the PoA finalizer of a
/// locked desktop). Holds private keys only for this role.
class RetainedIdentityKey final {
public:
    ~RetainedIdentityKey();
    /// Returns the key, or nullptr if the role or cryptography is not available.
    static std::unique_ptr<RetainedIdentityKey> Derive(std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose);
    const IdentityHybridPublicKey& PublicKey() const;
    std::optional<IdentityHybridSignature> Sign(std::span<const unsigned char> message) const;
private:
    struct Impl;
    explicit RetainedIdentityKey(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

/// Проверяет гибридную подпись сообщения публичным ключом роли.
/// \param key Публичный ключ с корректно выставленным `purpose`.
/// \param signature Проверяемая гибридная подпись.
/// \param message Исходные байты сообщения.
/// \return `true`, если обе части подписи валидны для заданной роли; иначе `false`.
/// \pre `key.purpose` соответствует ожидаемой роли протокольного сообщения.
/// \post Секреты не создаются и не сохраняются.
/// \thread_safety Потокобезопасна.
bool VerifyIdentityMessage(const IdentityHybridPublicKey& key,
    const IdentityHybridSignature& signature,
    std::span<const unsigned char> message);

/// Вычисляет идентификатор текущего recovery-ключа для поиска Identity в состоянии.
/// \param recovery_key Канонический recovery public key.
/// \return 32-байтовый RecoveryKeyId или `std::nullopt`, если ключ некорректен по роли/формату.
/// \thread_safety Потокобезопасна.
std::optional<std::array<unsigned char, 32>> ComputeRecoveryKeyId(
    const IdentityHybridPublicKey& recovery_key);
/// Вычисляет идентификатор текущего authorization-ключа.
/// \param authorization_key Канонический authorization public key.
/// \return 32-байтовый AuthorizationKeyId или `std::nullopt`, если ключ некорректен.
/// \thread_safety Потокобезопасна.
std::optional<std::array<unsigned char, 32>> ComputeAuthorizationKeyId(
    const IdentityHybridPublicKey& authorization_key);
/// Вычисляет идентификатор PoA finalizer-ключа.
/// \param poa_finalizer_key Канонический публичный ключ локального PoA signer.
/// \return 32-байтовый PoA finalizer key id или `std::nullopt`, если ключ некорректен.
/// \thread_safety Потокобезопасна.
std::optional<std::array<unsigned char, 32>> ComputePoaFinalizerKeyId(
    const IdentityHybridPublicKey& poa_finalizer_key);

} // namespace cybou
#endif
