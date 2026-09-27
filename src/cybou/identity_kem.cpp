// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_kem.h>

#include <cybou/crypto/cleanse.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <algorithm>
#include <memory>

namespace cybou {
namespace {

using PKey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PKeyCtx = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

bool IsZero(std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](unsigned char byte) { return byte == 0; });
}

PKey GenerateMlKem768Key(std::span<const unsigned char, ML_KEM_768_SEED_SIZE> seed)
{
    PKeyCtx context{EVP_PKEY_CTX_new_from_name(nullptr, "ML-KEM-768", nullptr), EVP_PKEY_CTX_free};
    if (!context || EVP_PKEY_keygen_init(context.get()) <= 0) return {nullptr, EVP_PKEY_free};
    MlKem768Seed seed_copy{};
    std::copy(seed.begin(), seed.end(), seed_copy.begin());
    OSSL_PARAM parameters[] = {
        OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_ML_KEM_SEED, seed_copy.data(), seed_copy.size()),
        OSSL_PARAM_construct_end(),
    };
    const bool configured = EVP_PKEY_CTX_set_params(context.get(), parameters) > 0;
    crypto::CleanseMemory(seed_copy.data(), seed_copy.size());
    if (!configured) return {nullptr, EVP_PKEY_free};
    EVP_PKEY* generated{nullptr};
    if (EVP_PKEY_generate(context.get(), &generated) <= 0) return {nullptr, EVP_PKEY_free};
    return PKey{generated, EVP_PKEY_free};
}

} // namespace

MlKem768Encapsulation::MlKem768Encapsulation(MlKem768Encapsulation&& other) noexcept
    : ciphertext{other.ciphertext}, shared_secret{other.shared_secret}
{
    crypto::CleanseMemory(other.ciphertext.data(), other.ciphertext.size());
    crypto::CleanseMemory(other.shared_secret.data(), other.shared_secret.size());
}

MlKem768Encapsulation& MlKem768Encapsulation::operator=(MlKem768Encapsulation&& other) noexcept
{
    if (this != &other) {
        crypto::CleanseMemory(ciphertext.data(), ciphertext.size());
        crypto::CleanseMemory(shared_secret.data(), shared_secret.size());
        ciphertext = other.ciphertext;
        shared_secret = other.shared_secret;
        crypto::CleanseMemory(other.ciphertext.data(), other.ciphertext.size());
        crypto::CleanseMemory(other.shared_secret.data(), other.shared_secret.size());
    }
    return *this;
}

MlKem768Encapsulation::~MlKem768Encapsulation()
{
    crypto::CleanseMemory(ciphertext.data(), ciphertext.size());
    crypto::CleanseMemory(shared_secret.data(), shared_secret.size());
}

MlKem768Decapsulation::MlKem768Decapsulation(MlKem768Decapsulation&& other) noexcept
    : shared_secret{other.shared_secret}
{
    crypto::CleanseMemory(other.shared_secret.data(), other.shared_secret.size());
}

MlKem768Decapsulation& MlKem768Decapsulation::operator=(MlKem768Decapsulation&& other) noexcept
{
    if (this != &other) {
        crypto::CleanseMemory(shared_secret.data(), shared_secret.size());
        shared_secret = other.shared_secret;
        crypto::CleanseMemory(other.shared_secret.data(), other.shared_secret.size());
    }
    return *this;
}

MlKem768Decapsulation::~MlKem768Decapsulation()
{
    crypto::CleanseMemory(shared_secret.data(), shared_secret.size());
}

std::optional<DeviceX25519PrivateKey> GenerateDeviceX25519PrivateKey()
{
    DeviceX25519PrivateKey key{};
    if (RAND_priv_bytes(key.data(), static_cast<int>(key.size())) != 1 || IsZero(key)) {
        crypto::CleanseMemory(key.data(), key.size());
        return std::nullopt;
    }
    return key;
}

std::optional<DeviceX25519PublicKey> DeriveDeviceX25519PublicKey(
    const std::span<const unsigned char, X25519_PRIVATE_KEY_SIZE> private_key)
{
    if (IsZero(private_key)) return std::nullopt;
    PKey key{EVP_PKEY_new_raw_private_key_ex(nullptr, "X25519", nullptr,
        private_key.data(), private_key.size()), EVP_PKEY_free};
    if (!key) return std::nullopt;
    DeviceX25519PublicKey public_key{};
    size_t length = public_key.size();
    if (EVP_PKEY_get_raw_public_key(key.get(), public_key.data(), &length) != 1 ||
        length != public_key.size() || IsZero(public_key)) return std::nullopt;
    return public_key;
}

std::optional<MlKem768Seed> GenerateMlKem768Seed()
{
    MlKem768Seed seed{};
    if (RAND_priv_bytes(seed.data(), static_cast<int>(seed.size())) != 1 || IsZero(seed)) {
        crypto::CleanseMemory(seed.data(), seed.size());
        return std::nullopt;
    }
    const auto key = GenerateMlKem768Key(seed);
    if (!key) {
        crypto::CleanseMemory(seed.data(), seed.size());
        return std::nullopt;
    }
    return seed;
}

std::optional<MlKem768PublicKey> DeriveMlKem768PublicKey(
    const std::span<const unsigned char, ML_KEM_768_SEED_SIZE> seed)
{
    if (IsZero(seed)) return std::nullopt;
    const auto key = GenerateMlKem768Key(seed);
    if (!key) return std::nullopt;
    MlKem768PublicKey public_key{};
    size_t length = public_key.size();
    if (EVP_PKEY_get_raw_public_key(key.get(), public_key.data(), &length) != 1 ||
        length != public_key.size() || IsZero(public_key)) return std::nullopt;
    return public_key;
}

std::optional<MlKem768Encapsulation> EncapsulateMlKem768(
    const std::span<const unsigned char, ML_KEM_768_PUBLIC_KEY_SIZE> public_key)
{
    if (IsZero(public_key)) return std::nullopt;
    PKey key{EVP_PKEY_new_raw_public_key_ex(nullptr, "ML-KEM-768", nullptr,
        public_key.data(), public_key.size()), EVP_PKEY_free};
    if (!key) return std::nullopt;
    PKeyCtx context{EVP_PKEY_CTX_new_from_pkey(nullptr, key.get(), nullptr), EVP_PKEY_CTX_free};
    if (!context || EVP_PKEY_encapsulate_init(context.get(), nullptr) <= 0) return std::nullopt;
    MlKem768Encapsulation result;
    size_t ciphertext_length = result.ciphertext.size();
    size_t secret_length = result.shared_secret.size();
    if (EVP_PKEY_encapsulate(context.get(), result.ciphertext.data(), &ciphertext_length,
            result.shared_secret.data(), &secret_length) <= 0 ||
        ciphertext_length != result.ciphertext.size() || secret_length != result.shared_secret.size()) {
        return std::nullopt;
    }
    return result;
}

std::optional<MlKem768Decapsulation> DecapsulateMlKem768(
    const std::span<const unsigned char, ML_KEM_768_SEED_SIZE> seed,
    const std::span<const unsigned char, ML_KEM_768_CIPHERTEXT_SIZE> ciphertext)
{
    if (IsZero(seed)) return std::nullopt;
    const auto key = GenerateMlKem768Key(seed);
    if (!key) return std::nullopt;
    PKeyCtx context{EVP_PKEY_CTX_new_from_pkey(nullptr, key.get(), nullptr), EVP_PKEY_CTX_free};
    if (!context || EVP_PKEY_decapsulate_init(context.get(), nullptr) <= 0) return std::nullopt;
    MlKem768Decapsulation result;
    size_t secret_length = result.shared_secret.size();
    if (EVP_PKEY_decapsulate(context.get(), result.shared_secret.data(), &secret_length,
            ciphertext.data(), ciphertext.size()) <= 0 || secret_length != result.shared_secret.size()) {
        crypto::CleanseMemory(result.shared_secret.data(), result.shared_secret.size());
        return std::nullopt;
    }
    return result;
}

} // namespace cybou
