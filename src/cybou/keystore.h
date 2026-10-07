// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

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
#include <cybou/identity_signer.h>
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
    /// \return `true`, если keystore загружен новым локальным материалом.
    /// \post Старые секреты зануляются перед заменой новым материалом.
    bool GenerateNew();
    /// Загружает уже подготовленный переносимый материал Identity.
    /// \param material Секретный материал; после передачи объект-источник следует считать очищаемым владельцем секрета.
    /// \return `true`, если все Recovery/Authorization/KEM роли успешно выведены и самопроверены.
    /// \post При ошибке keystore остаётся очищенным.
    bool LoadMaterial(IdentityMaterial material);
    /// Загружает vault Identity с диска.
    /// \param path Путь к vault.
    /// \param password Пароль vault; не логировать.
    /// \return `true`, если файл открыт и роли успешно выведены.
    bool LoadFromFile(const std::filesystem::path& path, std::string_view password);
    /// Сохраняет новый vault Identity на диск.
    /// \return `true`, если новый vault записан и повторно проверен.
    /// \pre Keystore уже разблокирован и `path` не занят существующим файлом.
    bool SaveToFile(const std::filesystem::path& path, std::string_view password) const;
    /// Готовит новый переносимый материал для ротации без смены AccountID.
    /// \param new_recovery_entropy Новый recovery entropy; секрет не логировать.
    /// \return Новый `IdentityMaterial` либо `std::nullopt`, если keystore пуст или entropy нулевой.
    /// \post Возвращаемый материал содержит секрет и должен быть очищен после упаковки candidate vault.
    std::optional<IdentityMaterial> CreateIdentityRotationMaterial(
        std::span<const unsigned char, 32> new_recovery_entropy) const;
    /// Возвращает 24 слова текущей recovery phrase.
    /// \return Recovery words либо `std::nullopt`, если keystore пуст.
    /// \warning Возвращаемые слова являются секретом.
    std::optional<RecoveryWords> GetRecoveryWords() const;
    /// Проверяет, что загруженная recovery entropy выводит ровно этот публичный ключ роли.
    /// \return `true`, если локальный секрет действительно соответствует данному публичному ключу роли.
    bool DerivesPublicKey(IdentityKeyPurpose purpose, const IdentityHybridPublicKey& key) const;
    /// Возвращает публичный PoA finalizer-ключ текущей Identity.
    /// \return Публичный ключ либо `std::nullopt`, если keystore пуст.
    std::optional<IdentityHybridPublicKey> GetPoaFinalizerPublicKey() const;
    /// rief PoA signer holding only the derived PoA key (see MakeRetainedPoaSigner).
    std::shared_ptr<PoaSigner> MakeRetainedPoaSigner() const;
    /// Подписывает сообщение локальным PoA finalizer-ключом.
    /// \return Подпись или `std::nullopt`, если keystore пуст либо сообщение недопустимо.
    /// \post Секретный ключ не покидает keystore.
    std::optional<IdentityHybridSignature> SignPoaFinalizerMessage(
        std::span<const unsigned char> message) const;

    /// Безопасно стирает весь секретный материал из памяти.
    /// \post Recovery entropy, KEM seed и исторические seed занулены.
    void Clear();

    /// Возвращает true, если в памяти загружен материал Identity.
    bool HasKey() const;
    /// Возвращает текущий AccountID.
    /// \return AccountID либо `std::nullopt`, если keystore пуст.
    std::optional<AccountId> GetAccountId() const;
    /// Возвращает публичный X-Wing ключ текущей Identity.
    /// \return Публичный KEM-ключ либо `std::nullopt`, если keystore пуст.
    std::optional<XWingPublicKey> GetIdentityXWingPublicKey() const;
    /// Выполняет самопроверку текущей X-Wing пары.
    /// \return `true`, если текущий seed и public key согласованы.
    bool ValidateIdentityXWingKeyPair() const;

    /// Возвращает authorization-публичный ключ.
    /// \return Ключ либо `std::nullopt`, если keystore пуст.
    std::optional<IdentityHybridPublicKey> GetAuthorizationPublicKey() const;
    /// Возвращает recovery-публичный ключ.
    /// \return Ключ либо `std::nullopt`, если keystore пуст.
    std::optional<IdentityHybridPublicKey> GetRecoveryPublicKey() const;
    /// Подписывает digest authorization-ключом.
    /// \return Подпись или `std::nullopt`, если keystore пуст.
    std::optional<IdentityHybridSignature> SignAuthorization(std::span<const unsigned char> digest) const;
    /// Подписывает digest recovery-ключом.
    /// \return Подпись или `std::nullopt`, если keystore пуст.
    std::optional<IdentityHybridSignature> SignRecovery(std::span<const unsigned char> digest) const;

    /// Выводит ключ локальной rebuildable Application DB этой Identity.
    /// \return 32-байтовый симметричный ключ или `std::nullopt`, если keystore пуст.
    /// \post Возвращённый ключ считается секретом; вызывающий код обязан очистить копии.
    std::optional<std::array<unsigned char, 32>> DeriveApplicationStoreKey() const;

    /// Открывает адресованную этой Identity капсулу RootPublication.
    /// \return Раскрытый ContentKey либо `std::nullopt`, если нет подходящего текущего/исторического KEM seed или проверка не прошла.
    /// \pre `current_key_epoch` соответствует текущему финализированному состоянию Identity.
    /// \post Возвращённый ContentKey является секретом и не должен логироваться.
    std::optional<ContentKey> OpenRootCapsule(std::span<const unsigned char, 32> network_binding,
        const AccountId& sender, std::uint64_t sender_nonce, std::uint64_t sender_key_epoch,
        const ChunkId& root_chunk_id, const RootRecipientCapsule& capsule,
        std::uint64_t current_key_epoch) const;
    /// Импортирует исторический KEM seed после его проверки против канонического коммитмента.
    /// \return `true`, если seed прошёл коммитмент и сохранён локально.
    /// \post Seed хранится только в памяти keystore и зануляется при `Clear()`.
    bool ImportHistoricalKemSeed(std::uint64_t key_epoch, const XWingSeed& seed);
    /// Возвращает true, если ключевой материал этого epoch доступен локально.
    bool HasKemSeedForEpoch(std::uint64_t key_epoch, std::uint64_t current_key_epoch) const;
    /// Возвращает все известные KEM seed для упаковки RecoveryBridge.
    /// \return Пары `(key_epoch, seed)`; seeds являются секретами и должны очищаться владельцем копий.
    std::vector<std::pair<std::uint64_t, XWingSeed>> KemSeedsForRecoveryBridge(std::uint64_t current_key_epoch) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/// Представление PoA signer поверх разблокированного vault.
/// PoA signer that keeps only the derived PoA finalizer key: the keystore (and its
/// recovery secret) may be locked and cleared while finalization continues.
/// Returns nullptr if the keystore is locked or the key cannot be derived.
std::shared_ptr<PoaSigner> MakeRetainedPoaSigner(const CybouKeyStore& keystore);

class CybouKeyStorePoaSigner final : public PoaSigner {
public:
    /// \param keystore Разблокированный keystore, владеющий PoA секретом локально.
    explicit CybouKeyStorePoaSigner(const CybouKeyStore& keystore) : m_keystore{keystore} {}
    std::optional<IdentityHybridPublicKey> PublicKey() const override;
    std::optional<IdentityHybridSignature> Sign(std::span<const unsigned char> message) const override;

private:
    const CybouKeyStore& m_keystore;
};

/// Identity Authorization signer поверх разблокированного vault (storage payout binding).
class CybouKeyStoreIdentitySigner final : public IdentitySigner {
public:
    /// \param keystore Разблокированный keystore текущей Identity.
    explicit CybouKeyStoreIdentitySigner(const CybouKeyStore& keystore) : m_keystore{keystore} {}
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
