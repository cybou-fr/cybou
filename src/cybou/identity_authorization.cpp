// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Реализация канонической сериализации IdentityAuthorization.

#include <cybou/identity_authorization.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <string_view>

namespace cybou {
namespace {
constexpr unsigned char ROOT_SUITE{1}; // Ed25519 AND ML-DSA-65
constexpr unsigned char AUTHORIZATION_SUITE{1}; // Ed25519 AND ML-DSA-44
constexpr size_t ROOT_PQ_SIZE{1952};
constexpr size_t AUTHORIZATION_PQ_SIZE{1312};

bool HasNonzero(std::span<const unsigned char> bytes)
{
    return std::any_of(bytes.begin(), bytes.end(), [](unsigned char b) { return b != 0; });
}

// Дескриптор жёстко фиксирует только Recovery и Authorization роли:
// любые нулевые/совпадающие ключи отвергаются до сериализации и коммитмента.
bool Valid(const IdentityAuthorization& auth)
{
    return auth.recovery_root.purpose == IdentityKeyPurpose::RECOVERY_ROOT &&
        auth.authorization_key.purpose == IdentityKeyPurpose::AUTHORIZATION &&
        auth.recovery_root.ml_dsa.size() == ROOT_PQ_SIZE &&
        auth.authorization_key.ml_dsa.size() == AUTHORIZATION_PQ_SIZE &&
        HasNonzero(auth.recovery_root.ed25519) && HasNonzero(auth.recovery_root.ml_dsa) &&
        HasNonzero(auth.authorization_key.ed25519) && HasNonzero(auth.authorization_key.ml_dsa) &&
        auth.recovery_root.ed25519 != auth.authorization_key.ed25519;
}
} // namespace

std::optional<IdentityAuthorizationBytes> SerializeIdentityAuthorization(
    const IdentityAuthorization& auth)
{
    if (!Valid(auth)) return std::nullopt;
    IdentityAuthorizationBytes bytes{};
    size_t pos{0};
    bytes[pos++] = ROOT_SUITE;
    std::copy(auth.recovery_root.ed25519.begin(), auth.recovery_root.ed25519.end(), bytes.begin() + pos);
    pos += auth.recovery_root.ed25519.size();
    std::copy(auth.recovery_root.ml_dsa.begin(), auth.recovery_root.ml_dsa.end(), bytes.begin() + pos);
    pos += ROOT_PQ_SIZE;
    bytes[pos++] = AUTHORIZATION_SUITE;
    std::copy(auth.authorization_key.ed25519.begin(), auth.authorization_key.ed25519.end(), bytes.begin() + pos);
    pos += auth.authorization_key.ed25519.size();
    std::copy(auth.authorization_key.ml_dsa.begin(), auth.authorization_key.ml_dsa.end(), bytes.begin() + pos);
    pos += AUTHORIZATION_PQ_SIZE;
    if (pos != bytes.size()) return std::nullopt;
    return bytes;
}

std::optional<IdentityAuthorization> DeserializeIdentityAuthorization(
    std::span<const unsigned char> bytes)
{
    if (bytes.size() != IDENTITY_AUTHORIZATION_SIZE ||
        bytes[0] != ROOT_SUITE || bytes[1985] != AUTHORIZATION_SUITE) return std::nullopt;
    IdentityAuthorization auth{
        .recovery_root = IdentityHybridPublicKey{.purpose = IdentityKeyPurpose::RECOVERY_ROOT, .ed25519 = {}, .ml_dsa = {}},
        .authorization_key = IdentityHybridPublicKey{.purpose = IdentityKeyPurpose::AUTHORIZATION, .ed25519 = {}, .ml_dsa = {}},
    };
    std::copy_n(bytes.begin() + 1, 32, auth.recovery_root.ed25519.begin());
    auth.recovery_root.ml_dsa.assign(bytes.begin() + 33, bytes.begin() + 1985);
    std::copy_n(bytes.begin() + 1986, 32, auth.authorization_key.ed25519.begin());
    auth.authorization_key.ml_dsa.assign(bytes.begin() + 2018, bytes.end());
    if (!Valid(auth)) return std::nullopt;
    return auth;
}

std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitment(
    const IdentityAuthorization& auth)
{
    const auto bytes = SerializeIdentityAuthorization(auth);
    if (!bytes) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/IDENTITY-AUTH-COMMIT"};
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*bytes}}, digest.data())) return std::nullopt;
    return digest;
}

} // namespace cybou
