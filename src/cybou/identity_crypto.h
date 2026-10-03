// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_CRYPTO_H
#define CYBOU_IDENTITY_CRYPTO_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

enum class IdentityKeyPurpose : uint8_t {
    RECOVERY_ROOT = 1,
    AUTHORIZATION = 2,
    POA_FINALIZER = 7,
    /** CYBOU P2P storage provider identity (StorageId = hash of this key). */
    STORAGE = 8,
    /** Strictly offline root authority that signs official network genesis. */
    NETWORK_ROOT = 9,
};

struct IdentityHybridPublicKey {
    IdentityKeyPurpose purpose;
    std::array<unsigned char, 32> ed25519{};
    std::vector<unsigned char> ml_dsa;

    friend bool operator==(const IdentityHybridPublicKey&, const IdentityHybridPublicKey&) = default;
};

struct IdentityHybridSignature {
    std::array<unsigned char, 64> ed25519{};
    std::vector<unsigned char> ml_dsa;

    friend bool operator==(const IdentityHybridSignature&, const IdentityHybridSignature&) = default;
};

// Hybrid key primitives. Consensus operations define their own signing domains.
std::optional<IdentityHybridPublicKey> DeriveIdentityPublicKey(
    std::span<const unsigned char, 32> secret,
    IdentityKeyPurpose purpose);
std::optional<IdentityHybridSignature> SignIdentityMessage(
    std::span<const unsigned char, 32> secret,
    IdentityKeyPurpose purpose,
    std::span<const unsigned char> message);
bool VerifyIdentityMessage(const IdentityHybridPublicKey& key,
    const IdentityHybridSignature& signature,
    std::span<const unsigned char> message);

// Domain- and suite-bound lookup identifier for an authorized recovery root.
// The ID changes when either public key changes; AccountID does not.
std::optional<std::array<unsigned char, 32>> ComputeRecoveryKeyId(
    const IdentityHybridPublicKey& recovery_key);
std::optional<std::array<unsigned char, 32>> ComputeAuthorizationKeyId(
    const IdentityHybridPublicKey& authorization_key);
std::optional<std::array<unsigned char, 32>> ComputePoaFinalizerKeyId(
    const IdentityHybridPublicKey& poa_finalizer_key);

} // namespace cybou
#endif
