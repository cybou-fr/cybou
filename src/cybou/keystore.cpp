// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/keystore.h>

#include <cybou/crypto/cleanse.h>

#include <algorithm>

namespace cybou {

struct CybouKeyStore::Impl {
    std::optional<IdentityMaterial> material;
    std::optional<StorageKeyRing> storage_key_ring;
    std::optional<XWingSeed> identity_xwing_seed;
    std::optional<XWingPublicKey> identity_xwing_public_key;
    std::optional<IdentityHybridPublicKey> authorization_key;
    std::optional<IdentityHybridPublicKey> recovery_key;

    ~Impl() { Clear(); }

    void Clear()
    {
        material.reset();
        storage_key_ring.reset();
        if (identity_xwing_seed) crypto::CleanseMemory(identity_xwing_seed->data(), identity_xwing_seed->size());
        identity_xwing_seed.reset();
        identity_xwing_public_key.reset();
        authorization_key.reset();
        recovery_key.reset();
    }

    bool SetMaterial(IdentityMaterial value)
    {
        Clear();
        if (!AccountId::FromBytes(value.account_id)) return false;
        auto authorization = DeriveIdentityPublicKey(value.recovery_entropy, IdentityKeyPurpose::AUTHORIZATION);
        auto recovery = DeriveIdentityPublicKey(value.recovery_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
        auto kem_seed = DeriveIdentityXWingSeed(value.recovery_entropy);
        if (!authorization || !recovery || !kem_seed || !ValidateXWingKeyPair(*kem_seed)) {
            if (kem_seed) crypto::CleanseMemory(kem_seed->data(), kem_seed->size());
            return false;
        }
        auto kem_public = DeriveXWingPublicKey(*kem_seed);
        if (!kem_public) {
            crypto::CleanseMemory(kem_seed->data(), kem_seed->size());
            return false;
        }

        material.emplace(std::move(value));
        identity_xwing_seed = *kem_seed;
        crypto::CleanseMemory(kem_seed->data(), kem_seed->size());
        identity_xwing_public_key = *kem_public;
        authorization_key = std::move(*authorization);
        recovery_key = std::move(*recovery);
        return true;
    }
};

CybouKeyStore::CybouKeyStore() : m_impl{std::make_unique<Impl>()} {}
CybouKeyStore::~CybouKeyStore() = default;
CybouKeyStore::CybouKeyStore(CybouKeyStore&&) noexcept = default;
CybouKeyStore& CybouKeyStore::operator=(CybouKeyStore&&) noexcept = default;

bool CybouKeyStore::GenerateNew()
{
    auto material = GenerateIdentityMaterial();
    return material && m_impl->SetMaterial(std::move(*material));
}

bool CybouKeyStore::LoadMaterial(IdentityMaterial material) { return m_impl->SetMaterial(std::move(material)); }

bool CybouKeyStore::LoadFromFile(const std::filesystem::path& path, std::string_view password)
{
    auto material = LoadIdentityMaterial(path, password);
    return material && m_impl->SetMaterial(std::move(*material));
}

bool CybouKeyStore::SaveToFile(const std::filesystem::path& path, std::string_view password) const
{
    return m_impl->material && SaveNewIdentityMaterial(path, password, *m_impl->material);
}

bool CybouKeyStore::CreateStorageKeyRing(const std::filesystem::path& path, std::string_view password)
{
    if (!m_impl->material || m_impl->storage_key_ring) return false;
    const auto account = AccountId::FromBytes(m_impl->material->account_id);
    auto ring = account ? StorageKeyRing::Create(*account) : std::nullopt;
    if (!ring || !ring->SaveNewToFile(path, password)) return false;
    m_impl->storage_key_ring.emplace(std::move(*ring));
    return true;
}

bool CybouKeyStore::LoadStorageKeyRing(const std::filesystem::path& path, std::string_view password)
{
    m_impl->storage_key_ring.reset();
    if (!m_impl->material) return false;
    const auto account = AccountId::FromBytes(m_impl->material->account_id);
    auto ring = StorageKeyRing::LoadFromFile(path, password);
    if (!account || !ring || ring->BoundAccount() != *account) return false;
    m_impl->storage_key_ring.emplace(std::move(*ring));
    return true;
}

bool CybouKeyStore::RotateStorageKeyRing(const std::filesystem::path& path, std::string_view password)
{
    if (!m_impl->material || !m_impl->storage_key_ring) return false;
    const auto account = AccountId::FromBytes(m_impl->material->account_id);
    return account && m_impl->storage_key_ring->BoundAccount() == *account &&
        m_impl->storage_key_ring->RotateAndSave(path, password);
}

std::optional<uint32_t> CybouKeyStore::GetCurrentStorageKeyEpoch() const
{
    if (!m_impl->storage_key_ring) return std::nullopt;
    return m_impl->storage_key_ring->CurrentEpoch();
}

bool CybouKeyStore::CopyStorageMasterKey(const uint32_t epoch, const std::span<unsigned char, 32> out) const
{
    return m_impl->storage_key_ring && m_impl->storage_key_ring->CopyMasterKey(epoch, out);
}

std::optional<IdentityMaterial> CybouKeyStore::CreateIdentityRotationMaterial(
    std::span<const unsigned char, 32> new_recovery_entropy) const
{
    if (!m_impl->material || std::all_of(new_recovery_entropy.begin(), new_recovery_entropy.end(),
            [](unsigned char byte) { return byte == 0; })) return std::nullopt;
    IdentityMaterial material;
    material.account_id = m_impl->material->account_id;
    std::copy(new_recovery_entropy.begin(), new_recovery_entropy.end(), material.recovery_entropy.begin());
    return material;
}

std::optional<RecoveryWords> CybouKeyStore::GetRecoveryWords() const
{
    if (!m_impl->material) return std::nullopt;
    return EncodeRecoveryWords(m_impl->material->recovery_entropy);
}

void CybouKeyStore::Clear() { m_impl->Clear(); }
bool CybouKeyStore::HasKey() const { return m_impl->material.has_value(); }
std::optional<XWingPublicKey> CybouKeyStore::GetIdentityXWingPublicKey() const { return m_impl->identity_xwing_public_key; }
bool CybouKeyStore::ValidateIdentityXWingKeyPair() const
{
    return m_impl->identity_xwing_seed && ValidateXWingKeyPair(*m_impl->identity_xwing_seed);
}

std::optional<AccountId> CybouKeyStore::GetAccountId() const
{
    if (!m_impl->material) return std::nullopt;
    return AccountId::FromBytes(m_impl->material->account_id);
}

std::optional<IdentityHybridPublicKey> CybouKeyStore::GetAuthorizationPublicKey() const { return m_impl->authorization_key; }
std::optional<IdentityHybridPublicKey> CybouKeyStore::GetRecoveryPublicKey() const { return m_impl->recovery_key; }

std::optional<IdentityHybridSignature> CybouKeyStore::SignAuthorization(std::span<const unsigned char> digest) const
{
    if (!m_impl->material) return std::nullopt;
    return SignIdentityMessage(m_impl->material->recovery_entropy, IdentityKeyPurpose::AUTHORIZATION, digest);
}

std::optional<IdentityHybridSignature> CybouKeyStore::SignRecovery(std::span<const unsigned char> digest) const
{
    if (!m_impl->material) return std::nullopt;
    return SignIdentityMessage(m_impl->material->recovery_entropy, IdentityKeyPurpose::RECOVERY_ROOT, digest);
}

} // namespace cybou
