// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// KEM-профиль Identity и канонические операции с X-Wing пакетами.

#ifndef CYBOU_IDENTITY_KEM_H
#define CYBOU_IDENTITY_KEM_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t XWING_SEED_SIZE{32};
inline constexpr size_t XWING_PUBLIC_KEY_SIZE{1216};
inline constexpr size_t XWING_CIPHERTEXT_SIZE{1120};
inline constexpr uint16_t IDENTITY_KEM_PROFILE_XWING{0x647a};
inline constexpr size_t IDENTITY_KEM_PACKAGE_SIZE{2 + XWING_PUBLIC_KEY_SIZE};

using XWingSeed = std::array<unsigned char, XWING_SEED_SIZE>;
using XWingPublicKey = std::array<unsigned char, XWING_PUBLIC_KEY_SIZE>;
using XWingCiphertext = std::array<unsigned char, XWING_CIPHERTEXT_SIZE>;
using XWingSharedSecret = std::array<unsigned char, 32>;
/// Канонический пакет публичного KEM-ключа Identity.
using IdentityKemPackage = std::array<unsigned char, IDENTITY_KEM_PACKAGE_SIZE>;

/// Результат инкапсуляции X-Wing с автоматической очисткой секретов.
struct XWingEncapsulation {
    XWingCiphertext ciphertext{};
    XWingSharedSecret shared_secret{};
    XWingEncapsulation() = default;
    XWingEncapsulation(const XWingEncapsulation&) = delete;
    XWingEncapsulation& operator=(const XWingEncapsulation&) = delete;
    XWingEncapsulation(XWingEncapsulation&& other) noexcept;
    XWingEncapsulation& operator=(XWingEncapsulation&& other) noexcept;
    ~XWingEncapsulation();
};

/// Генерирует новый seed X-Wing.
std::optional<XWingSeed> GenerateXWingSeed();
/// Выводит публичный X-Wing ключ из seed.
std::optional<XWingPublicKey> DeriveXWingPublicKey(
    std::span<const unsigned char, XWING_SEED_SIZE> seed);
/// Проверяет seed X-Wing инкапсуляцией/декапсуляцией полного цикла.
bool ValidateXWingKeyPair(std::span<const unsigned char, XWING_SEED_SIZE> seed);
/// Выполняет X-Wing инкапсуляцию для публичного ключа получателя.
std::optional<XWingEncapsulation> EncapsulateXWing(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
#if defined(CYBOU_ENABLE_TEST_HOOKS)
/// Детерминированная инкапсуляция для тестовых векторов.
std::optional<XWingEncapsulation> EncapsulateXWingForTest(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key,
    std::span<const unsigned char, 64> randomness);
#endif
/// Выполняет X-Wing декапсуляцию ciphertext текущим seed получателя.
std::optional<XWingSharedSecret> DecapsulateXWing(
    std::span<const unsigned char, XWING_SEED_SIZE> seed,
    std::span<const unsigned char, XWING_CIPHERTEXT_SIZE> ciphertext);
/// Кодирует канонический пакет публичного KEM-ключа Identity.
std::optional<IdentityKemPackage> EncodeIdentityKemPackage(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
/// Декодирует и валидирует канонический пакет KEM-ключа Identity.
std::optional<XWingPublicKey> DecodeIdentityKemPackage(std::span<const unsigned char> package);
/// Выводит seed KEM-роли Identity из её recovery entropy.
std::optional<XWingSeed> DeriveIdentityXWingSeed(std::span<const unsigned char, 32> identity_entropy);
/// Вычисляет коммитмент KEM-пакета, привязанный к сети, AccountID и epoch.
std::optional<std::array<unsigned char, 32>> ComputeIdentityKemPackageCommitment(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> account_id,
    uint64_t key_epoch,
    std::span<const unsigned char> package);

} // namespace cybou

#endif // CYBOU_IDENTITY_KEM_H
