// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_AUTHORIZATION_H
#define CYBOU_IDENTITY_AUTHORIZATION_H

#include <cybou/identity_crypto.h>

#include <array>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t IDENTITY_AUTHORIZATION_SIZE{3331};
using IdentityAuthorizationBytes = std::array<unsigned char, IDENTITY_AUTHORIZATION_SIZE>;

struct IdentityAuthorization {
    IdentityHybridPublicKey recovery_root;
    IdentityHybridPublicKey initial_device;

    friend bool operator==(const IdentityAuthorization&, const IdentityAuthorization&) = default;
};

std::optional<IdentityAuthorizationBytes> SerializeIdentityAuthorization(
    const IdentityAuthorization& authorization);
std::optional<IdentityAuthorization> DeserializeIdentityAuthorization(
    std::span<const unsigned char> bytes);
std::optional<std::array<unsigned char, 32>> ComputeIdentityAuthorizationCommitment(
    const IdentityAuthorization& authorization);

} // namespace cybou
#endif // CYBOU_IDENTITY_AUTHORIZATION_H
