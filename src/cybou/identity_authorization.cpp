// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_authorization.h>

#include <openssl/evp.h>

#include <algorithm>
#include <memory>
#include <string_view>

namespace cybou {
namespace {
constexpr unsigned char VERSION{2};
constexpr unsigned char ROOT_SUITE{1}; // Ed25519 AND ML-DSA-65
constexpr unsigned char DEVICE_SUITE{1}; // Ed25519 AND ML-DSA-44
constexpr size_t ROOT_PQ_SIZE{1952};
constexpr size_t DEVICE_PQ_SIZE{1312};

bool HasNonzero(std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](unsigned char b) { return b != 0; });
}

bool Valid(const IdentityAuthorization& auth)
{
    return auth.recovery_root.purpose == IdentityKeyPurpose::RECOVERY_ROOT &&
        auth.initial_device.purpose == IdentityKeyPurpose::DEVICE &&
        auth.recovery_root.ml_dsa.size() == ROOT_PQ_SIZE &&
        auth.initial_device.ml_dsa.size() == DEVICE_PQ_SIZE &&
        HasNonzero(auth.recovery_root.ed25519) && HasNonzero(auth.recovery_root.ml_dsa) &&
        HasNonzero(auth.initial_device.ed25519) && HasNonzero(auth.initial_device.ml_dsa) &&
        auth.recovery_root.ed25519 != auth.initial_device.ed25519;
}
} // namespace

std::optional<IdentityAuthorizationBytes> SerializeIdentityAuthorization(
    const IdentityAuthorization& auth)
{
    if (!Valid(auth)) return std::nullopt;
    IdentityAuthorizationBytes bytes{};
    size_t pos{0};
    bytes[pos++] = VERSION;
    bytes[pos++] = ROOT_SUITE;
    std::copy(auth.recovery_root.ed25519.begin(), auth.recovery_root.ed25519.end(), bytes.begin() + pos);
    pos += auth.recovery_root.ed25519.size();
    std::copy(auth.recovery_root.ml_dsa.begin(), auth.recovery_root.ml_dsa.end(), bytes.begin() + pos);
    pos += ROOT_PQ_SIZE;
    bytes[pos++] = DEVICE_SUITE;
    std::copy(auth.initial_device.ed25519.begin(), auth.initial_device.ed25519.end(), bytes.begin() + pos);
    pos += auth.initial_device.ed25519.size();
    std::copy(auth.initial_device.ml_dsa.begin(), auth.initial_device.ml_dsa.end(), bytes.begin() + pos);
    pos += DEVICE_PQ_SIZE;
    if (pos != bytes.size()) return std::nullopt;
    return bytes;
}

std::optional<IdentityAuthorization> DeserializeIdentityAuthorization(
    std::span<const unsigned char> bytes)
{
    if (bytes.size() != IDENTITY_AUTHORIZATION_SIZE || bytes[0] != VERSION ||
        bytes[1] != ROOT_SUITE || bytes[1986] != DEVICE_SUITE) return std::nullopt;
    IdentityAuthorization auth{
        .recovery_root = IdentityHybridPublicKey{.purpose = IdentityKeyPurpose::RECOVERY_ROOT, .ed25519 = {}, .ml_dsa = {}},
        .initial_device = IdentityHybridPublicKey{.purpose = IdentityKeyPurpose::DEVICE, .ed25519 = {}, .ml_dsa = {}},
    };
    std::copy_n(bytes.begin() + 2, 32, auth.recovery_root.ed25519.begin());
    auth.recovery_root.ml_dsa.assign(bytes.begin() + 34, bytes.begin() + 1986);
    std::copy_n(bytes.begin() + 1987, 32, auth.initial_device.ed25519.begin());
    auth.initial_device.ml_dsa.assign(bytes.begin() + 2019, bytes.end());
    if (!Valid(auth)) return std::nullopt;
    return auth;
}

std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitment(
    const IdentityAuthorization& auth)
{
    const auto bytes = SerializeIdentityAuthorization(auth);
    if (!bytes) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/IDENTITY-AUTH-COMMIT/V2"};
    using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    Context ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    std::array<unsigned char, 32> digest{};
    unsigned int length{0};
    if (!ctx || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), domain.data(), domain.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), bytes->data(), bytes->size()) != 1 ||
        EVP_DigestFinal_ex(ctx.get(), digest.data(), &length) != 1 || length != digest.size()) return std::nullopt;
    return digest;
}

} // namespace cybou
