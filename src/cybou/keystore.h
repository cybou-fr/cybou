// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// Локальное хранилище секретов Identity и производных ролей CYBOU.

#ifndef CYBOU_KEYSTORE_H
#define CYBOU_KEYSTORE_H

#include <cybou/account_id.h>
#include <cybou/chunk_id.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/identity_crypto.h>
#include <cybou/identity_kem.h>
#include <cybou/identity_material.h>
#include <cybou/poa_signer.h>
#include <cybou/validation_attestation.h>
#include <cybou/hash256.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace cybou {

struct RootRecipientCapsule;

/// Разблокированные локальные секреты Identity поверх переносимого vault CYBV.
class CybouKeyStore {
public:
    CybouKeyStore();
    ~CybouKeyStore();

    CybouKeyStore(const CybouKeyStore&) = delete;
    CybouKeyStore& operator=(const CybouKeyStore&) = delete;
    CybouKeyStore(CybouKeyStore&&) noexcept;
    CybouKeyStore& operator=(CybouKeyStore&&) noexcept;

    /// Генерирует новый случайный AccountID и recovery entropy.
    bool GenerateNew();
    /// Загружает уже подготовленный переносимый материал Identity.
    bool LoadMaterial(IdentityMaterial material);
    /// Загружает vault Identity с диска.
    bool LoadFromFile(const std::filesystem::path& path, std::string_view password);
    /// Сохраняет новый vault Identity на диск.
    bool SaveToFile(const std::filesystem::path& path, std::string_view password) const;
    /// Готовит новый переносимый материал для ротации без смены AccountID.
    std::optional<IdentityMaterial> CreateIdentityRotationMaterial(
        std::span<const unsigned char, 32> new_recovery_entropy) const;
    /// Возвращает 24 слова текущей recovery phrase.
    std::optional<RecoveryWords> GetRecoveryWords() const;
    /// Проверяет, что загруженная recovery entropy выводит ровно этот публичный ключ роли.
    bool DerivesPublicKey(IdentityKeyPurpose purpose, const IdentityHybridPublicKey& key) const;
    /// Возвращает публичный PoA finalizer-ключ текущей Identity.
    std::optional<IdentityHybridPublicKey> GetPoaFinalizerPublicKey() const;
    /// Подписывает сообщение локальным PoA finalizer-ключом.
    std::optional<IdentityHybridSignature> SignPoaFinalizerMessage(
        std::span<const unsigned char> message) const;

    /// Безопасно стирает весь секретный материал из памяти.
    void Clear();

    /// Возвращает true, если в памяти загружен материал Identity.
    bool HasKey() const;
    /// Возвращает текущий AccountID.
    std::optional<AccountId> GetAccountId() const;
    /// Возвращает публичный X-Wing ключ текущей Identity.
    std::optional<XWingPublicKey> GetIdentityXWingPublicKey() const;
    /// Выполняет самопроверку текущей X-Wing пары.
    bool ValidateIdentityXWingKeyPair() const;

    /// Возвращает authorization-публичный ключ.
    std::optional<IdentityHybridPublicKey> GetAuthorizationPublicKey() const;
    /// Возвращает recovery-публичный ключ.
    std::optional<IdentityHybridPublicKey> GetRecoveryPublicKey() const;
    /// Подписывает digest authorization-ключом.
    std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const;
    /// Подписывает digest recovery-ключом.
    std::optional<IdentityHybridSignature> SignRecovery(std::span<const unsigned char> digest) const;

    /// Выводит ключ локальной rebuildable Application DB этой Identity.
    std::optional<std::array<unsigned char, 32>> DeriveApplicationStoreKey() const;

    /// Открывает адресованную этой Identity капсулу RootPublication.
    std::optional<ContentKey> OpenRootCapsule(std::span<const unsigned char, 32> network_binding,
        const AccountId& sender, std::uint64_t sender_nonce, std::uint64_t sender_key_epoch,
        const ChunkId& root_chunk_id, const RootRecipientCapsule& capsule,
        std::uint64_t current_key_epoch) const;
    /// Импортирует исторический KEM seed после его проверки против канонического коммитмента.
    bool ImportHistoricalKemSeed(std::uint64_t key_epoch, const XWingSeed& seed);
    /// Возвращает true, если ключевой материал этого epoch доступен локально.
    bool HasKemSeedForEpoch(std::uint64_t key_epoch, std::uint64_t current_key_epoch) const;
    /// Возвращает все известные KEM seed для упаковки RecoveryBridge.
    std::vector<std::pair<std::uint64_t, XWingSeed>> KemSeedsForRecoveryBridge(std::uint64_t current_key_epoch) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/// Представление PoA signer поверх разблокированного vault.
class CybouKeyStorePoaSigner final : public PoaSigner {
public:
    explicit CybouKeyStorePoaSigner(const CybouKeyStore& keystore) : m_keystore{keystore} {}
    std::optional<IdentityHybridPublicKey> PublicKey() const override;
    std::optional<IdentityHybridSignature> Sign(std::span<const unsigned char> message) const override;

private:
    const CybouKeyStore& m_keystore;
};

/// Представление Validation signer поверх разблокированного vault.
class CybouKeyStoreValidationSigner final : public ValidationSigner {
public:
    explicit CybouKeyStoreValidationSigner(const CybouKeyStore& keystore) : m_keystore{keystore} {}
    std::optional<AccountId> Account() const override { return m_keystore.GetAccountId(); }
    std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const override
    {
        return m_keystore.SignAuthorization(digest);
    }

private:
    const CybouKeyStore& m_keystore;
};

} // namespace cybou

#endif // CYBOU_KEYSTORE_H
