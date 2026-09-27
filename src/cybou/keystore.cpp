// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/keystore.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/hkdf_sha256.h>
#include <cybou/signing.h>

#include <openssl/evp.h>

#include <algorithm>
#include <string_view>

namespace cybou {
namespace {

std::optional<std::array<unsigned char, 32>> DeriveMailSeed(std::span<const unsigned char, 32> device_secret)
{
    char salt[] = "CYBOU/MAIL-KEY-HKDF/V2";
    char info[] = "X25519";
    std::array<unsigned char, 32> seed{};
    if (!crypto::HkdfSha256(device_secret,
            std::span<const unsigned char>{reinterpret_cast<const unsigned char*>(salt), sizeof(salt) - 1},
            std::span<const unsigned char>{reinterpret_cast<const unsigned char*>(info), sizeof(info) - 1},
            seed)) return std::nullopt;
    return seed;
}

} // namespace

struct CybouKeyStore::Impl {
    std::optional<IdentityMaterial> material;
    std::optional<StorageKeyRing> storage_key_ring;
    std::optional<std::array<unsigned char, 32>> mail_seed;
    std::optional<uint256> mail_public_key;
    std::optional<uint256> x25519_public_key;
    std::optional<DeviceX25519PublicKey> device_x25519_public_key;
    std::optional<MlKem768PublicKey> device_mlkem768_public_key;
    std::optional<IdentityHybridPublicKey> device_key;
    std::optional<IdentityHybridPublicKey> recovery_root;
    std::optional<std::array<unsigned char, 32>> device_id;

    ~Impl() { Clear(); }

    void Clear()
    {
        material.reset();
        storage_key_ring.reset();
        if (mail_seed) {
            crypto::CleanseMemory(mail_seed->data(), mail_seed->size());
            mail_seed.reset();
        }
        mail_public_key.reset();
        x25519_public_key.reset();
        device_x25519_public_key.reset();
        device_mlkem768_public_key.reset();
        device_key.reset();
        recovery_root.reset();
        device_id.reset();
    }

    bool SetMaterial(IdentityMaterial value)
    {
        Clear();
        if (!AccountId::FromBytes(value.account_id)) return false;
        const auto device = DeriveIdentityPublicKey(value.device_secret, IdentityKeyPurpose::DEVICE);
        const auto root = DeriveIdentityPublicKey(value.recovery_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
        if (!device || !root) return false;
        const auto id = ComputeDeviceKeyId(*device);
        if (!id) return false;

        auto device_x25519 = DeriveDeviceX25519PublicKey(value.device_x25519_private_key);
        auto device_mlkem768 = DeriveMlKem768PublicKey(value.device_mlkem768_seed);
        if (!device_x25519 || !device_mlkem768) return false;

        auto derived_mail_seed = DeriveMailSeed(value.device_secret);
        if (!derived_mail_seed) return false;
        const auto public_key = DeriveEd25519PublicKey(*derived_mail_seed);
        const auto x25519 = public_key ? Ed25519PublicKeyToX25519(*public_key) : std::nullopt;
        if (!public_key || !x25519) {
            crypto::CleanseMemory(derived_mail_seed->data(), derived_mail_seed->size());
            return false;
        }

        material.emplace(std::move(value));
        mail_seed = *derived_mail_seed;
        crypto::CleanseMemory(derived_mail_seed->data(), derived_mail_seed->size());
        mail_public_key = *public_key;
        x25519_public_key = *x25519;
        device_x25519_public_key = *device_x25519;
        device_mlkem768_public_key = *device_mlkem768;
        device_key = *device;
        recovery_root = *root;
        device_id = *id;
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

bool CybouKeyStore::LoadMaterial(IdentityMaterial material)
{
    return m_impl->SetMaterial(std::move(material));
}

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

std::optional<IdentityMaterial> CybouKeyStore::CreateRecoveryRotationMaterial(
    std::span<const unsigned char, 32> new_recovery_entropy) const
{
    if (!m_impl->material || !std::any_of(new_recovery_entropy.begin(), new_recovery_entropy.end(),
            [](unsigned char byte) { return byte != 0; })) return std::nullopt;
    IdentityMaterial material;
    material.account_id = m_impl->material->account_id;
    material.recovery_entropy = std::array<unsigned char, 32>{};
    std::copy(new_recovery_entropy.begin(), new_recovery_entropy.end(), material.recovery_entropy.begin());
    material.device_secret = m_impl->material->device_secret;
    material.device_x25519_private_key = m_impl->material->device_x25519_private_key;
    material.device_mlkem768_seed = m_impl->material->device_mlkem768_seed;
    return material;
}

std::optional<RecoveryWords> CybouKeyStore::GetRecoveryWords() const
{
    if (!m_impl->material) return std::nullopt;
    return EncodeRecoveryWords(m_impl->material->recovery_entropy);
}

void CybouKeyStore::Clear() { m_impl->Clear(); }
bool CybouKeyStore::HasKey() const { return m_impl->material.has_value(); }
std::optional<uint256> CybouKeyStore::GetPublicKey() const { return m_impl->mail_public_key; }
std::optional<uint256> CybouKeyStore::GetX25519PublicKey() const { return m_impl->x25519_public_key; }
std::optional<DeviceX25519PublicKey> CybouKeyStore::GetDeviceX25519PublicKey() const
{
    return m_impl->device_x25519_public_key;
}
std::optional<MlKem768PublicKey> CybouKeyStore::GetDeviceMlKem768PublicKey() const
{
    return m_impl->device_mlkem768_public_key;
}

std::optional<AccountId> CybouKeyStore::GetAccountId() const
{
    if (!m_impl->material) return std::nullopt;
    return AccountId::FromBytes(m_impl->material->account_id);
}

std::optional<std::array<unsigned char, 32>> CybouKeyStore::DeriveX25519SharedSecret(const uint256& peer_x25519_pubkey) const
{
    if (!m_impl->mail_seed) return std::nullopt;
    const auto private_key = Ed25519SeedToX25519PrivateKey(*m_impl->mail_seed);
    if (!private_key) return std::nullopt;
    return X25519DeriveSharedSecret(*private_key, peer_x25519_pubkey);
}

std::optional<IdentityHybridPublicKey> CybouKeyStore::GetDevicePublicKey() const { return m_impl->device_key; }
std::optional<IdentityHybridPublicKey> CybouKeyStore::GetRecoveryPublicKey() const { return m_impl->recovery_root; }
std::optional<std::array<unsigned char, 32>> CybouKeyStore::GetDeviceId() const { return m_impl->device_id; }

std::optional<IdentityHybridSignature> CybouKeyStore::SignDevice(std::span<const unsigned char> digest) const
{
    if (!m_impl->material) return std::nullopt;
    return SignIdentityMessage(m_impl->material->device_secret, IdentityKeyPurpose::DEVICE, digest);
}

std::optional<IdentityHybridSignature> CybouKeyStore::SignRecovery(std::span<const unsigned char> digest) const
{
    if (!m_impl->material) return std::nullopt;
    return SignIdentityMessage(m_impl->material->recovery_entropy, IdentityKeyPurpose::RECOVERY_ROOT, digest);
}

} // namespace cybou
