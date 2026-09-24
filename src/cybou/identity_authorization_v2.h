// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_AUTHORIZATION_V2_H
#define CYBOU_IDENTITY_AUTHORIZATION_V2_H

#include <cybou/identity_crypto.h>

#include <array>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t IDENTITY_AUTHORIZATION_V2_SIZE{3331};
using IdentityAuthorizationBytesV2 = std::array<unsigned char, IDENTITY_AUTHORIZATION_V2_SIZE>;

struct IdentityAuthorizationV2 {
    IdentityHybridPublicKey recovery_root;
    IdentityHybridPublicKey initial_device;
};

// Bounded canonical pre-consensus representation for AccountCreate V2.
// This does not activate V2 operations in the current DEV network.
std::optional<IdentityAuthorizationBytesV2> SerializeIdentityAuthorizationV2(
    const IdentityAuthorizationV2& authorization);
std::optional<IdentityAuthorizationV2> DeserializeIdentityAuthorizationV2(
    std::span<const unsigned char> bytes);
std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitmentV2(
    const IdentityAuthorizationV2& authorization);

inline constexpr size_t IDENTITY_AUTHORIZATION_SIZE{IDENTITY_AUTHORIZATION_V2_SIZE};
using IdentityAuthorizationBytes = std::array<unsigned char, IDENTITY_AUTHORIZATION_SIZE>;
using IdentityAuthorization = IdentityAuthorizationV2;

inline std::optional<IdentityAuthorizationBytes> SerializeIdentityAuthorization(
    const IdentityAuthorization& authorization)
{
    return SerializeIdentityAuthorizationV2(authorization);
}

inline std::optional<IdentityAuthorization> DeserializeIdentityAuthorization(
    std::span<const unsigned char> bytes)
{
    return DeserializeIdentityAuthorizationV2(bytes);
}

inline std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitment(
    const IdentityAuthorization& authorization)
{
    return ComputeIdentityAuthorizationCommitmentV2(authorization);
}

} // namespace cybou
#endif
