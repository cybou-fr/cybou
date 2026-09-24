// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/keystore.h>

#include <cybou/signing.h>
#include <support/cleanse.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <algorithm>
#include <memory>
#include <string_view>

namespace cybou {
namespace {

std::optional<std::array<unsigned char, 32>> DeriveMailSeed(std::span<const unsigned char, 32> device_secret)
{
    using Kdf = std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)>;
    using Context = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;
    Kdf kdf{EVP_KDF_fetch(nullptr, "HKDF", nullptr), EVP_KDF_free};
    if (!kdf) return std::nullopt;
    Context context{EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free};
    if (!context) return std::nullopt;
    char digest[] = "SHA256";
    char salt[] = "CYBOU/MAIL-KEY-HKDF/V2";
    char info[] = "X25519";
    OSSL_PARAM parameters[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, const_cast<unsigned char*>(device_secret.data()), device_secret.size()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, salt, sizeof(salt) - 1),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, info, sizeof(info) - 1),
        OSSL_PARAM_construct_end(),
    };
    std::array<unsigned char, 32> seed{};
    if (EVP_KDF_derive(context.get(), seed.data(), seed.size(), parameters) != 1) return std::nullopt;
    return seed;
}

} // namespace

struct CybouKeyStore::Impl {
    std::optional<IdentityMaterial> material;
    std::optional<std::array<unsigned char, 32>> mail_seed;
    std::optional<uint256> mail_public_key;
    std::optional<uint256> x25519_public_key;
    std::optional<IdentityHybridPublicKey> device_key;
    std::optional<IdentityHybridPublicKey> recovery_root;
    std::optional<std::array<unsigned char, 32>> device_id;

    ~Impl() { Clear(); }

    void Clear()
    {
        material.reset();
        if (mail_seed) {
            memory_cleanse(mail_seed->data(), mail_seed->size());
            mail_seed.reset();
        }
        mail_public_key.reset();
        x25519_public_key.reset();
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

        auto derived_mail_seed = DeriveMailSeed(value.device_secret);
        if (!derived_mail_seed) return false;
        const auto public_key = DeriveEd25519PublicKey(*derived_mail_seed);
        const auto x25519 = public_key ? Ed25519PublicKeyToX25519(*public_key) : std::nullopt;
        if (!public_key || !x25519) {
            memory_cleanse(derived_mail_seed->data(), derived_mail_seed->size());
            return false;
        }

        material.emplace(std::move(value));
        mail_seed = *derived_mail_seed;
        memory_cleanse(derived_mail_seed->data(), derived_mail_seed->size());
        mail_public_key = *public_key;
        x25519_public_key = *x25519;
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

std::optional<RecoveryWords> CybouKeyStore::GetRecoveryWords() const
{
    if (!m_impl->material) return std::nullopt;
    return EncodeRecoveryWords(m_impl->material->recovery_entropy);
}

void CybouKeyStore::Clear() { m_impl->Clear(); }
bool CybouKeyStore::HasKey() const { return m_impl->material.has_value(); }
std::optional<uint256> CybouKeyStore::GetPublicKey() const { return m_impl->mail_public_key; }
std::optional<uint256> CybouKeyStore::GetX25519PublicKey() const { return m_impl->x25519_public_key; }

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
