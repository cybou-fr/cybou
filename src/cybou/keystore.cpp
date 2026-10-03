// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/keystore.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/hkdf_sha256.h>
#include <cybou/root_publication.h>

#include <algorithm>
#include <map>

namespace cybou {

struct CybouKeyStore::Impl {
    std::optional<IdentityMaterial> material;
    std::optional<XWingSeed> identity_xwing_seed;
    std::optional<XWingPublicKey> identity_xwing_public_key;
    std::optional<IdentityHybridPublicKey> authorization_key;
    std::optional<IdentityHybridPublicKey> recovery_key;
    /** Pre-rotation KEM seeds recovered from a verified RecoveryBridge. */
    std::map<std::uint64_t, XWingSeed> historical_xwing_seeds;

    ~Impl() { Clear(); }

    void Clear()
    {
        material.reset();
        if (identity_xwing_seed) crypto::CleanseMemory(identity_xwing_seed->data(), identity_xwing_seed->size());
        identity_xwing_seed.reset();
        for (auto& [_, seed] : historical_xwing_seeds) crypto::CleanseMemory(seed.data(), seed.size());
        historical_xwing_seeds.clear();
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

bool CybouKeyStore::DerivesPublicKey(IdentityKeyPurpose purpose, const IdentityHybridPublicKey& key) const
{
    if (!m_impl->material) return false;
    const auto derived = DeriveIdentityPublicKey(m_impl->material->recovery_entropy, purpose);
    return derived && *derived == key;
}

std::optional<IdentityHybridPublicKey> CybouKeyStore::GetPoaFinalizerPublicKey() const
{
    if (!m_impl->material) return std::nullopt;
    return DeriveIdentityPublicKey(m_impl->material->recovery_entropy, IdentityKeyPurpose::POA_FINALIZER);
}

std::optional<IdentityHybridSignature> CybouKeyStore::SignPoaFinalizerMessage(
    const std::span<const unsigned char> message) const
{
    if (!m_impl->material || message.empty()) return std::nullopt;
    return SignIdentityMessage(m_impl->material->recovery_entropy, IdentityKeyPurpose::POA_FINALIZER, message);
}

std::optional<IdentityHybridPublicKey> CybouKeyStorePoaSigner::PublicKey() const
{
    return m_keystore.GetPoaFinalizerPublicKey();
}

std::optional<IdentityHybridSignature> CybouKeyStorePoaSigner::Sign(
    const std::span<const unsigned char> message) const
{
    return m_keystore.SignPoaFinalizerMessage(message);
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

std::optional<std::array<unsigned char, 32>> CybouKeyStore::DeriveApplicationStoreKey() const
{
    if (!m_impl->material) return std::nullopt;
    constexpr std::string_view salt{"CYBOU/LOCAL-APPLICATION-STORE"};
    const auto salt_bytes = std::span{reinterpret_cast<const unsigned char*>(salt.data()), salt.size()};
    // Bind the derived key to the stable AccountID, while keeping the recovery
    // entropy within the key store.
    std::array<unsigned char, 32> key{};
    if (!crypto::HkdfSha256(m_impl->material->recovery_entropy, salt_bytes,
            m_impl->material->account_id, key)) return std::nullopt;
    return key;
}

std::optional<ContentKey> CybouKeyStore::OpenRootCapsule(const std::span<const unsigned char, 32> network_binding,
    const AccountId& sender, const std::uint64_t sender_nonce, const std::uint64_t sender_key_epoch,
    const ChunkId& root_chunk_id, const RootRecipientCapsule& capsule, const std::uint64_t current_key_epoch) const
{
    const XWingSeed* seed{nullptr};
    if (capsule.key_epoch == current_key_epoch && m_impl->identity_xwing_seed) {
        seed = &*m_impl->identity_xwing_seed;
    } else if (const auto it = m_impl->historical_xwing_seeds.find(capsule.key_epoch);
               it != m_impl->historical_xwing_seeds.end()) {
        seed = &it->second;
    }
    if (!seed) return std::nullopt;
    return OpenRootRecipientCapsule(network_binding,
        std::span<const unsigned char, 32>{sender.Value().begin(), 32}, sender_nonce, sender_key_epoch,
        root_chunk_id, capsule, *seed);
}

bool CybouKeyStore::ImportHistoricalKemSeed(const std::uint64_t key_epoch, const XWingSeed& seed)
{
    if (!m_impl->material || !ValidateXWingKeyPair(seed)) return false;
    auto [it, inserted] = m_impl->historical_xwing_seeds.try_emplace(key_epoch, seed);
    return inserted || it->second == seed;
}

bool CybouKeyStore::HasKemSeedForEpoch(const std::uint64_t key_epoch, const std::uint64_t current_key_epoch) const
{
    if (key_epoch == current_key_epoch) return m_impl->identity_xwing_seed.has_value();
    return m_impl->historical_xwing_seeds.contains(key_epoch);
}

std::vector<std::pair<std::uint64_t, XWingSeed>> CybouKeyStore::KemSeedsForRecoveryBridge(
    const std::uint64_t current_key_epoch) const
{
    std::vector<std::pair<std::uint64_t, XWingSeed>> seeds;
    if (!m_impl->identity_xwing_seed) return seeds;
    for (const auto& [epoch, seed] : m_impl->historical_xwing_seeds) {
        if (epoch < current_key_epoch) seeds.emplace_back(epoch, seed);
    }
    seeds.emplace_back(current_key_epoch, *m_impl->identity_xwing_seed);
    return seeds;
}

} // namespace cybou
