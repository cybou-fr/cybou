// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_kem.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/sha256.h>

#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <algorithm>
#include <memory>
#include <string_view>

namespace cybou {
namespace {

using PKey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PKeyCtx = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using MdCtx = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

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

bool XWingExpandSeed(std::span<const unsigned char, XWING_SEED_SIZE> seed,
    MlKem768Seed& mlkem_seed, DeviceX25519PrivateKey& x25519_seed)
{
    std::array<unsigned char, ML_KEM_768_SEED_SIZE + X25519_PRIVATE_KEY_SIZE> expanded{};
    MdCtx context{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    const bool ok = context && EVP_DigestInit_ex2(context.get(), EVP_shake256(), nullptr) == 1 &&
        EVP_DigestUpdate(context.get(), seed.data(), seed.size()) == 1 &&
        EVP_DigestFinalXOF(context.get(), expanded.data(), expanded.size()) == 1;
    if (ok) {
        std::copy_n(expanded.begin(), mlkem_seed.size(), mlkem_seed.begin());
        std::copy_n(expanded.begin() + mlkem_seed.size(), x25519_seed.size(), x25519_seed.begin());
    }
    crypto::CleanseMemory(expanded.data(), expanded.size());
    return ok;
}

std::optional<DeviceX25519PublicKey> X25519PublicFromSeed(
    std::span<const unsigned char, X25519_PRIVATE_KEY_SIZE> private_key)
{
    return DeriveDeviceX25519PublicKey(private_key);
}

std::optional<MlKem768SharedSecret> X25519SharedSecret(
    std::span<const unsigned char, X25519_PRIVATE_KEY_SIZE> private_key,
    std::span<const unsigned char, X25519_PUBLIC_KEY_SIZE> public_key)
{
    if (IsZero(private_key) || IsZero(public_key)) return std::nullopt;
    PKey local{EVP_PKEY_new_raw_private_key_ex(nullptr, "X25519", nullptr,
        private_key.data(), private_key.size()), EVP_PKEY_free};
    PKey peer{EVP_PKEY_new_raw_public_key_ex(nullptr, "X25519", nullptr,
        public_key.data(), public_key.size()), EVP_PKEY_free};
    PKeyCtx context{local ? EVP_PKEY_CTX_new_from_pkey(nullptr, local.get(), nullptr) : nullptr,
        EVP_PKEY_CTX_free};
    MlKem768SharedSecret secret{};
    size_t length = secret.size();
    if (!context || !peer || EVP_PKEY_derive_init(context.get()) != 1 ||
        EVP_PKEY_derive_set_peer(context.get(), peer.get()) != 1 ||
        EVP_PKEY_derive(context.get(), secret.data(), &length) != 1 || length != secret.size() ||
        IsZero(secret)) {
        crypto::CleanseMemory(secret.data(), secret.size());
        return std::nullopt;
    }
    return secret;
}

std::optional<XWingSharedSecret> CombineXWingSecrets(
    std::span<const unsigned char, 32> pq_secret,
    std::span<const unsigned char, 32> x25519_secret,
    std::span<const unsigned char, 32> ephemeral_public,
    std::span<const unsigned char, 32> recipient_public)
{
    static constexpr std::array<unsigned char, 6> LABEL{0x5c, 0x2e, 0x2f, 0x2f, 0x5e, 0x5c};
    std::array<unsigned char, 134> input{};
    size_t offset{0};
    const auto append = [&](std::span<const unsigned char> part) {
        std::copy(part.begin(), part.end(), input.begin() + offset);
        offset += part.size();
    };
    append(pq_secret);
    append(x25519_secret);
    append(ephemeral_public);
    append(recipient_public);
    append(LABEL);
    XWingSharedSecret result{};
    size_t result_size{0};
    const bool ok = EVP_Q_digest(nullptr, "SHA3-256", nullptr, input.data(), input.size(),
        result.data(), &result_size) == 1 && result_size == result.size();
    crypto::CleanseMemory(input.data(), input.size());
    if (!ok) {
        crypto::CleanseMemory(result.data(), result.size());
        return std::nullopt;
    }
    return result;
}

} // namespace

XWingEncapsulation::XWingEncapsulation(XWingEncapsulation&& other) noexcept
    : ciphertext{other.ciphertext}, shared_secret{other.shared_secret}
{
    crypto::CleanseMemory(other.ciphertext.data(), other.ciphertext.size());
    crypto::CleanseMemory(other.shared_secret.data(), other.shared_secret.size());
}

XWingEncapsulation& XWingEncapsulation::operator=(XWingEncapsulation&& other) noexcept
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

XWingEncapsulation::~XWingEncapsulation()
{
    crypto::CleanseMemory(ciphertext.data(), ciphertext.size());
    crypto::CleanseMemory(shared_secret.data(), shared_secret.size());
}

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

std::optional<XWingSeed> GenerateXWingSeed()
{
    XWingSeed seed{};
    if (RAND_priv_bytes(seed.data(), static_cast<int>(seed.size())) != 1 || IsZero(seed)) {
        crypto::CleanseMemory(seed.data(), seed.size());
        return std::nullopt;
    }
    return seed;
}

std::optional<XWingPublicKey> DeriveXWingPublicKey(
    std::span<const unsigned char, XWING_SEED_SIZE> seed)
{
    MlKem768Seed mlkem_seed{};
    DeviceX25519PrivateKey x25519_seed{};
    if (!XWingExpandSeed(seed, mlkem_seed, x25519_seed)) return std::nullopt;
    const auto pq_public = DeriveMlKem768PublicKey(mlkem_seed);
    const auto classical_public = X25519PublicFromSeed(x25519_seed);
    crypto::CleanseMemory(mlkem_seed.data(), mlkem_seed.size());
    crypto::CleanseMemory(x25519_seed.data(), x25519_seed.size());
    if (!pq_public || !classical_public) return std::nullopt;
    XWingPublicKey result{};
    std::copy(pq_public->begin(), pq_public->end(), result.begin());
    std::copy(classical_public->begin(), classical_public->end(), result.begin() + pq_public->size());
    return result;
}

std::optional<XWingEncapsulation> EncapsulateXWing(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key)
{
    if (IsZero(public_key)) return std::nullopt;
    const auto pq_public = public_key.first<ML_KEM_768_PUBLIC_KEY_SIZE>();
    const auto recipient_x25519 = public_key.last<X25519_PUBLIC_KEY_SIZE>();
    auto pq_encapsulation = EncapsulateMlKem768(pq_public);
    auto ephemeral_seed = GenerateDeviceX25519PrivateKey();
    const auto ephemeral_public = ephemeral_seed ? DeriveDeviceX25519PublicKey(*ephemeral_seed) : std::nullopt;
    auto x25519_secret = ephemeral_seed && ephemeral_public ?
        X25519SharedSecret(*ephemeral_seed, recipient_x25519) : std::nullopt;
    if (!pq_encapsulation || !ephemeral_seed || !ephemeral_public || !x25519_secret) {
        if (ephemeral_seed) crypto::CleanseMemory(ephemeral_seed->data(), ephemeral_seed->size());
        if (x25519_secret) crypto::CleanseMemory(x25519_secret->data(), x25519_secret->size());
        return std::nullopt;
    }
    XWingEncapsulation result;
    std::copy(pq_encapsulation->ciphertext.begin(), pq_encapsulation->ciphertext.end(), result.ciphertext.begin());
    std::copy(ephemeral_public->begin(), ephemeral_public->end(), result.ciphertext.begin() + ML_KEM_768_CIPHERTEXT_SIZE);
    auto combined = CombineXWingSecrets(pq_encapsulation->shared_secret, *x25519_secret,
        *ephemeral_public, recipient_x25519);
    crypto::CleanseMemory(ephemeral_seed->data(), ephemeral_seed->size());
    crypto::CleanseMemory(x25519_secret->data(), x25519_secret->size());
    if (!combined) return std::nullopt;
    result.shared_secret = *combined;
    crypto::CleanseMemory(pq_encapsulation->shared_secret.data(), pq_encapsulation->shared_secret.size());
    crypto::CleanseMemory(combined->data(), combined->size());
    return result;
}

std::optional<XWingSharedSecret> DecapsulateXWing(
    std::span<const unsigned char, XWING_SEED_SIZE> seed,
    std::span<const unsigned char, XWING_CIPHERTEXT_SIZE> ciphertext)
{
    MlKem768Seed mlkem_seed{};
    DeviceX25519PrivateKey x25519_seed{};
    if (!XWingExpandSeed(seed, mlkem_seed, x25519_seed)) return std::nullopt;
    const auto public_key = DeriveXWingPublicKey(seed);
    const auto pq_ciphertext = ciphertext.first<ML_KEM_768_CIPHERTEXT_SIZE>();
    const auto ephemeral_public = ciphertext.last<X25519_PUBLIC_KEY_SIZE>();
    auto pq_secret = DecapsulateMlKem768(mlkem_seed, pq_ciphertext);
    auto x25519_secret = X25519SharedSecret(x25519_seed, ephemeral_public);
    crypto::CleanseMemory(mlkem_seed.data(), mlkem_seed.size());
    crypto::CleanseMemory(x25519_seed.data(), x25519_seed.size());
    if (!public_key || !pq_secret || !x25519_secret) {
        if (x25519_secret) crypto::CleanseMemory(x25519_secret->data(), x25519_secret->size());
        return std::nullopt;
    }
    auto combined = CombineXWingSecrets(pq_secret->shared_secret, *x25519_secret,
        ephemeral_public, std::span<const unsigned char, X25519_PUBLIC_KEY_SIZE>{
            public_key->data() + ML_KEM_768_PUBLIC_KEY_SIZE, X25519_PUBLIC_KEY_SIZE});
    crypto::CleanseMemory(pq_secret->shared_secret.data(), pq_secret->shared_secret.size());
    crypto::CleanseMemory(x25519_secret->data(), x25519_secret->size());
    return combined;
}

std::optional<IdentityKemPackage> EncodeIdentityKemPackage(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key)
{
    if (IsZero(public_key)) return std::nullopt;
    IdentityKemPackage package{};
    package[0] = 1;
    package[1] = static_cast<unsigned char>(IDENTITY_KEM_PROFILE_XWING);
    package[2] = static_cast<unsigned char>(IDENTITY_KEM_PROFILE_XWING >> 8);
    std::copy(public_key.begin(), public_key.end(), package.begin() + 3);
    if (!DecodeIdentityKemPackage(package)) return std::nullopt;
    return package;
}

std::optional<XWingPublicKey> DecodeIdentityKemPackage(std::span<const unsigned char> package)
{
    if (package.size() != IDENTITY_KEM_PACKAGE_SIZE || package[0] != 1 ||
        package[1] != static_cast<unsigned char>(IDENTITY_KEM_PROFILE_XWING) ||
        package[2] != static_cast<unsigned char>(IDENTITY_KEM_PROFILE_XWING >> 8)) return std::nullopt;
    XWingPublicKey public_key{};
    std::copy_n(package.begin() + 3, public_key.size(), public_key.begin());
    const auto pq_public = std::span<const unsigned char, ML_KEM_768_PUBLIC_KEY_SIZE>{public_key.data(), ML_KEM_768_PUBLIC_KEY_SIZE};
    const auto classical_public = std::span<const unsigned char, X25519_PUBLIC_KEY_SIZE>{
        public_key.data() + ML_KEM_768_PUBLIC_KEY_SIZE, X25519_PUBLIC_KEY_SIZE};
    if (IsZero(pq_public) || IsZero(classical_public)) return std::nullopt;
    PKey pq{EVP_PKEY_new_raw_public_key_ex(nullptr, "ML-KEM-768", nullptr,
        pq_public.data(), pq_public.size()), EVP_PKEY_free};
    PKey classical{EVP_PKEY_new_raw_public_key_ex(nullptr, "X25519", nullptr,
        classical_public.data(), classical_public.size()), EVP_PKEY_free};
    if (!pq || !classical) return std::nullopt;
    DeviceX25519PrivateKey probe{};
    probe.fill(0x42);
    const auto ecdh_probe = X25519SharedSecret(probe, classical_public);
    crypto::CleanseMemory(probe.data(), probe.size());
    if (!ecdh_probe) return std::nullopt;
    auto probe_secret = *ecdh_probe;
    crypto::CleanseMemory(probe_secret.data(), probe_secret.size());
    return public_key;
}

std::optional<std::array<unsigned char, 32>> ComputeIdentityKemPackageCommitment(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char, 32> account_id,
    std::span<const unsigned char, 32> device_key_id,
    uint64_t activation_nonce,
    std::span<const unsigned char> package)
{
    if (IsZero(network_id) || IsZero(account_id) || IsZero(device_key_id) ||
        !DecodeIdentityKemPackage(package)) return std::nullopt;
    std::array<unsigned char, 8> activation{};
    for (size_t i{0}; i < activation.size(); ++i) {
        activation[i] = static_cast<unsigned char>(activation_nonce >> (8 * i));
    }
    const auto length = static_cast<uint16_t>(package.size());
    const std::array<unsigned char, 2> length_le{
        static_cast<unsigned char>(length), static_cast<unsigned char>(length >> 8)};
    constexpr std::string_view domain{"CYBOU/IDENTITY-KEM-PACKAGE/V1"};
    constexpr std::array<unsigned char, 1> separator{0};
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), separator, network_id, account_id,
            device_key_id, activation, length_le, package}, digest.data())) return std::nullopt;
    return digest;
}

} // namespace cybou
