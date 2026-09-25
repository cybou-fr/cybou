// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_crypto.h>

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>

namespace cybou {
namespace {
using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using KeyCtx = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using MdCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using Kdf = std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)>;
using KdfCtx = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;

const char* Algorithm(IdentityKeyPurpose purpose)
{
    switch (purpose) {
    case IdentityKeyPurpose::RECOVERY_ROOT: return "ML-DSA-65";
    case IdentityKeyPurpose::DEVICE: return "ML-DSA-44";
    case IdentityKeyPurpose::VALIDATOR:
    case IdentityKeyPurpose::OPERATOR_AUTHORITY:
    case IdentityKeyPurpose::RELEASE_SIGNING:
    case IdentityKeyPurpose::TREASURY: return "ML-DSA-65";
    }
    return nullptr;
}

size_t PublicSize(IdentityKeyPurpose purpose)
{
    return purpose == IdentityKeyPurpose::DEVICE ? 1312 : 1952;
}

size_t SignatureSize(IdentityKeyPurpose purpose)
{
    return purpose == IdentityKeyPurpose::DEVICE ? 2420 : 3309;
}

std::optional<std::array<unsigned char, 32>> DeriveSeed(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose,
    std::string_view component)
{
    if (!Algorithm(purpose)) return std::nullopt;
    constexpr std::string_view salt{"CYBOU/IDENTITY-V2/HKDF-SHA256"};
    std::string_view purpose_label;
    switch (purpose) {
    case IdentityKeyPurpose::RECOVERY_ROOT: purpose_label = "ROOT"; break;
    case IdentityKeyPurpose::DEVICE: purpose_label = "DEVICE"; break;
    case IdentityKeyPurpose::VALIDATOR: purpose_label = "VALIDATOR"; break;
    case IdentityKeyPurpose::OPERATOR_AUTHORITY: purpose_label = "OPERATOR"; break;
    case IdentityKeyPurpose::RELEASE_SIGNING: purpose_label = "RELEASE"; break;
    case IdentityKeyPurpose::TREASURY: purpose_label = "TREASURY"; break;
    }
    const std::string info = std::string{"CYBOU/IDENTITY-V2/"} + std::string{purpose_label} + "/" +
        (component == "ED25519" ? "ED25519" : Algorithm(purpose));
    Kdf kdf{EVP_KDF_fetch(nullptr, "HKDF", nullptr), EVP_KDF_free};
    if (!kdf) return std::nullopt;
    KdfCtx ctx{EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free};
    if (!ctx) return std::nullopt;
    char digest[] = "SHA256";
    OSSL_PARAM params[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, const_cast<unsigned char*>(secret.data()), secret.size()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<char*>(salt.data()), salt.size()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, const_cast<char*>(info.data()), info.size()),
        OSSL_PARAM_construct_end(),
    };
    std::array<unsigned char, 32> seed{};
    if (EVP_KDF_derive(ctx.get(), seed.data(), seed.size(), params) != 1) return std::nullopt;
    return seed;
}

Key MakeKey(std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose,
    std::string_view component)
{
    auto seed = DeriveSeed(secret, purpose, component);
    if (!seed) return Key{nullptr, EVP_PKEY_free};
    Key key{nullptr, EVP_PKEY_free};
    if (component == "ED25519") {
        key.reset(EVP_PKEY_new_raw_private_key_ex(nullptr, "ED25519", nullptr, seed->data(), seed->size()));
    } else {
        KeyCtx ctx{EVP_PKEY_CTX_new_from_name(nullptr, Algorithm(purpose), nullptr), EVP_PKEY_CTX_free};
        if (ctx && EVP_PKEY_keygen_init(ctx.get()) == 1) {
            OSSL_PARAM params[] = {
                OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_ML_DSA_SEED, seed->data(), seed->size()),
                OSSL_PARAM_construct_end(),
            };
            EVP_PKEY* raw{nullptr};
            if (EVP_PKEY_CTX_set_params(ctx.get(), params) == 1 && EVP_PKEY_keygen(ctx.get(), &raw) == 1) key.reset(raw);
        }
    }
    OPENSSL_cleanse(seed->data(), seed->size());
    return key;
}

std::optional<std::vector<unsigned char>> Sign(Key& key, std::span<const unsigned char> message)
{
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    if (!ctx || EVP_DigestSignInit_ex(ctx.get(), nullptr, nullptr, nullptr, nullptr, key.get(), nullptr) != 1) return std::nullopt;
    size_t length{0};
    if (EVP_DigestSign(ctx.get(), nullptr, &length, message.data(), message.size()) != 1) return std::nullopt;
    std::vector<unsigned char> signature(length);
    if (EVP_DigestSign(ctx.get(), signature.data(), &length, message.data(), message.size()) != 1) return std::nullopt;
    signature.resize(length);
    return signature;
}

bool Verify(const char* algorithm, std::span<const unsigned char> public_key,
    std::span<const unsigned char> signature, std::span<const unsigned char> message)
{
    Key key{EVP_PKEY_new_raw_public_key_ex(nullptr, algorithm, nullptr, public_key.data(), public_key.size()), EVP_PKEY_free};
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    return key && ctx && EVP_DigestVerifyInit_ex(ctx.get(), nullptr, nullptr, nullptr, nullptr, key.get(), nullptr) == 1 &&
        EVP_DigestVerify(ctx.get(), signature.data(), signature.size(), message.data(), message.size()) == 1;
}
} // namespace

std::optional<IdentityHybridPublicKey> DeriveIdentityPublicKey(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose)
{
    if (!Algorithm(purpose)) return std::nullopt;
    auto ed = MakeKey(secret, purpose, "ED25519");
    auto pq = MakeKey(secret, purpose, "ML-DSA");
    if (!ed || !pq) return std::nullopt;
    IdentityHybridPublicKey result{.purpose = purpose, .ed25519 = {}, .ml_dsa = {}};
    size_t ed_size{result.ed25519.size()};
    result.ml_dsa.resize(PublicSize(purpose));
    size_t pq_size{result.ml_dsa.size()};
    if (EVP_PKEY_get_raw_public_key(ed.get(), result.ed25519.data(), &ed_size) != 1 || ed_size != result.ed25519.size() ||
        EVP_PKEY_get_raw_public_key(pq.get(), result.ml_dsa.data(), &pq_size) != 1 || pq_size != result.ml_dsa.size()) return std::nullopt;
    return result;
}

std::optional<IdentityHybridSignature> SignIdentityMessage(
    std::span<const unsigned char, 32> secret, IdentityKeyPurpose purpose,
    std::span<const unsigned char> message)
{
    if (!Algorithm(purpose)) return std::nullopt;
    auto ed = MakeKey(secret, purpose, "ED25519");
    auto pq = MakeKey(secret, purpose, "ML-DSA");
    if (!ed || !pq) return std::nullopt;
    auto ed_sig = Sign(ed, message);
    auto pq_sig = Sign(pq, message);
    if (!ed_sig || !pq_sig || ed_sig->size() != 64 || pq_sig->size() != SignatureSize(purpose)) return std::nullopt;
    IdentityHybridSignature result;
    std::copy(ed_sig->begin(), ed_sig->end(), result.ed25519.begin());
    result.ml_dsa = std::move(*pq_sig);
    return result;
}

bool VerifyIdentityMessage(const IdentityHybridPublicKey& key,
    const IdentityHybridSignature& signature, std::span<const unsigned char> message)
{
    const char* algorithm = Algorithm(key.purpose);
    if (!algorithm || key.ml_dsa.size() != PublicSize(key.purpose) || signature.ml_dsa.size() != SignatureSize(key.purpose)) return false;
    return Verify("ED25519", key.ed25519, signature.ed25519, message) &&
        Verify(algorithm, key.ml_dsa, signature.ml_dsa, message);
}

std::optional<std::array<unsigned char, 32>> ComputeRecoveryKeyId(
    const IdentityHybridPublicKey& recovery_key)
{
    if (recovery_key.purpose != IdentityKeyPurpose::RECOVERY_ROOT ||
        recovery_key.ml_dsa.size() != PublicSize(IdentityKeyPurpose::RECOVERY_ROOT) ||
        std::all_of(recovery_key.ed25519.begin(), recovery_key.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(recovery_key.ml_dsa.begin(), recovery_key.ml_dsa.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;

    constexpr std::string_view domain{"CYBOU/RECOVERY-KEY-ID/V2"};
    constexpr std::array<unsigned char, 2> suite{2, 1}; // identifier version 2, hybrid root suite 1
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    std::array<unsigned char, 32> id{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), suite.data(), suite.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), recovery_key.ed25519.data(), recovery_key.ed25519.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), recovery_key.ml_dsa.data(), recovery_key.ml_dsa.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), id.data(), &size) != 1 || size != id.size()) return std::nullopt;
    return id;
}

std::optional<std::array<unsigned char, 32>> ComputeDeviceKeyId(
    const IdentityHybridPublicKey& device_key)
{
    if (device_key.purpose != IdentityKeyPurpose::DEVICE ||
        device_key.ml_dsa.size() != PublicSize(IdentityKeyPurpose::DEVICE) ||
        std::all_of(device_key.ed25519.begin(), device_key.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(device_key.ml_dsa.begin(), device_key.ml_dsa.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/DEVICE-KEY-ID/V2"};
    constexpr std::array<unsigned char, 2> suite{2, 1};
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    std::array<unsigned char, 32> id{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), suite.data(), suite.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), device_key.ed25519.data(), device_key.ed25519.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), device_key.ml_dsa.data(), device_key.ml_dsa.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), id.data(), &size) != 1 || size != id.size()) return std::nullopt;
    return id;
}

std::optional<std::array<unsigned char, 32>> ComputeValidatorKeyId(
    const IdentityHybridPublicKey& validator_key)
{
    if (validator_key.purpose != IdentityKeyPurpose::VALIDATOR ||
        validator_key.ml_dsa.size() != PublicSize(IdentityKeyPurpose::VALIDATOR) ||
        std::all_of(validator_key.ed25519.begin(), validator_key.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(validator_key.ml_dsa.begin(), validator_key.ml_dsa.end(), [](unsigned char b) { return b == 0; })) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/VALIDATOR-KEY-ID/V2"};
    constexpr std::array<unsigned char, 2> suite{3, 1};
    MdCtx ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    std::array<unsigned char, 32> id{};
    unsigned int size{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), suite.data(), suite.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), validator_key.ed25519.data(), validator_key.ed25519.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), validator_key.ml_dsa.data(), validator_key.ml_dsa.size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), id.data(), &size) != 1 || size != id.size()) return std::nullopt;
    return id;
}

} // namespace cybou
