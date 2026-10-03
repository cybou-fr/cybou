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

/// 32 = canonical X-Wing seed size used as the only persisted KEM secret for one epoch.
inline constexpr size_t XWING_SEED_SIZE{32};
/// 1216 = 1184 ML-KEM-768 public key + 32 X25519 public key.
inline constexpr size_t XWING_PUBLIC_KEY_SIZE{1216};
/// 1120 = 1088 ML-KEM-768 ciphertext + 32 ephemeral X25519 public key.
inline constexpr size_t XWING_CIPHERTEXT_SIZE{1120};
/// Draft-05 X-Wing profile tag stored in canonical IdentityKemPackage.
inline constexpr uint16_t IDENTITY_KEM_PROFILE_XWING{0x647a};
/// 1218 = 2-byte profile id + 1216-byte X-Wing public key.
inline constexpr size_t IDENTITY_KEM_PACKAGE_SIZE{2 + XWING_PUBLIC_KEY_SIZE};

using XWingSeed = std::array<unsigned char, XWING_SEED_SIZE>;
using XWingPublicKey = std::array<unsigned char, XWING_PUBLIC_KEY_SIZE>;
using XWingCiphertext = std::array<unsigned char, XWING_CIPHERTEXT_SIZE>;
using XWingSharedSecret = std::array<unsigned char, 32>;
/// Канонический пакет публичного KEM-ключа Identity.
using IdentityKemPackage = std::array<unsigned char, IDENTITY_KEM_PACKAGE_SIZE>;

/// Результат инкапсуляции X-Wing с автоматической очисткой секретов.
struct XWingEncapsulation {
    /// Шифртекст, публикуемый в RootRecipientCapsule.
    XWingCiphertext ciphertext{};
    /// Общий секрет; зануляется при перемещении и уничтожении.
    XWingSharedSecret shared_secret{};
    XWingEncapsulation() = default;
    XWingEncapsulation(const XWingEncapsulation&) = delete;
    XWingEncapsulation& operator=(const XWingEncapsulation&) = delete;
    XWingEncapsulation(XWingEncapsulation&& other) noexcept;
    XWingEncapsulation& operator=(XWingEncapsulation&& other) noexcept;
    ~XWingEncapsulation();
};

/// Генерирует новый seed X-Wing.
/// \return Новый seed или `std::nullopt`, если CSPRNG недоступен.
/// \post Возвращаемое значение содержит секрет; не логировать, хранить минимально и занулить после использования.
/// \thread_safety Потокобезопасна.
std::optional<XWingSeed> GenerateXWingSeed();
/// Выводит публичный X-Wing ключ из seed.
/// \param seed 32-байтовый секрет KEM-роли одного key epoch.
/// \return Публичный X-Wing ключ или `std::nullopt`, если seed некорректен.
/// \post `seed` не очищается автоматически.
/// \thread_safety Потокобезопасна.
std::optional<XWingPublicKey> DeriveXWingPublicKey(
    std::span<const unsigned char, XWING_SEED_SIZE> seed);
/// Проверяет seed X-Wing инкапсуляцией/декапсуляцией полного цикла.
/// \param seed Секрет KEM-роли.
/// \return `true`, если один и тот же seed стабильно даёт совместимые public key и decapsulation; иначе `false`.
/// \post Внутренние временные shared secret очищаются.
/// \thread_safety Потокобезопасна.
bool ValidateXWingKeyPair(std::span<const unsigned char, XWING_SEED_SIZE> seed);
/// Выполняет X-Wing инкапсуляцию для публичного ключа получателя.
/// \param public_key Публичный X-Wing ключ получателя.
/// \return Шифртекст и shared secret либо `std::nullopt`, если ключ некорректен или криптография отказала.
/// \post Возвращаемый shared secret должен быть использован сразу и не должен логироваться.
/// \thread_safety Потокобезопасна.
std::optional<XWingEncapsulation> EncapsulateXWing(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
#if defined(CYBOU_ENABLE_TEST_HOOKS)
/// Детерминированная инкапсуляция для тестовых векторов.
/// \param public_key Публичный X-Wing ключ.
/// \param randomness Тестовая энтропия; не использовать в production пути.
/// \return Детерминированный результат инкапсуляции или `std::nullopt`.
std::optional<XWingEncapsulation> EncapsulateXWingForTest(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key,
    std::span<const unsigned char, 64> randomness);
#endif
/// Выполняет X-Wing декапсуляцию ciphertext текущим seed получателя.
/// \param seed Секрет KEM-роли получателя.
/// \param ciphertext Канонический X-Wing ciphertext.
/// \return Shared secret или `std::nullopt`, если seed/ciphertext недопустимы.
/// \post Возвращаемый секрет не логировать и очистить после использования.
/// \thread_safety Потокобезопасна.
std::optional<XWingSharedSecret> DecapsulateXWing(
    std::span<const unsigned char, XWING_SEED_SIZE> seed,
    std::span<const unsigned char, XWING_CIPHERTEXT_SIZE> ciphertext);
/// Кодирует канонический пакет публичного KEM-ключа Identity.
/// \param public_key Публичный X-Wing ключ Identity.
/// \return Пакет профиля/ключа или `std::nullopt`, если ключ некорректен.
/// \thread_safety Потокобезопасна.
std::optional<IdentityKemPackage> EncodeIdentityKemPackage(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
/// Декодирует и валидирует канонический пакет KEM-ключа Identity.
/// \param package Канонический пакет Identity KEM.
/// \return Публичный X-Wing ключ или `std::nullopt`, если профиль/размер/содержимое неверны.
/// \thread_safety Потокобезопасна.
std::optional<XWingPublicKey> DecodeIdentityKemPackage(std::span<const unsigned char> package);
/// Выводит seed KEM-роли Identity из её recovery entropy.
/// \param identity_entropy Recovery entropy Identity; секрет не логировать.
/// \return Seed KEM-роли или `std::nullopt`, если KDF не сработал.
/// \post Вызывающий код отвечает за очистку возвращённого seed.
/// \thread_safety Потокобезопасна.
std::optional<XWingSeed> DeriveIdentityXWingSeed(std::span<const unsigned char, 32> identity_entropy);
/// Вычисляет коммитмент KEM-пакета, привязанный к сети, AccountID и epoch.
/// \param network_binding 32-байтовый NetworkBinding текущей официальной сети.
/// \param account_id 32-байтовый AccountID владельца.
/// \param key_epoch Epoch, к которому относится пакет.
/// \param package Канонический IdentityKemPackage.
/// \return 32-байтовый коммитмент или `std::nullopt`, если пакет некорректен.
/// \pre `package` относится к той же Identity и тому же `key_epoch`, что и вызывающий протокольный объект.
/// \post Коммитмент можно сохранять и сверять в финализированном состоянии.
/// \thread_safety Потокобезопасна.
std::optional<std::array<unsigned char, 32>> ComputeIdentityKemPackageCommitment(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> account_id,
    uint64_t key_epoch,
    std::span<const unsigned char> package);

} // namespace cybou

#endif // CYBOU_IDENTITY_KEM_H
