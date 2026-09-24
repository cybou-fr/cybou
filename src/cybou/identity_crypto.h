// Copyright (c) 2026 The CYBOU developers
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

enum class IdentityKeyPurpose : uint8_t { RECOVERY_ROOT = 1, DEVICE = 2 };

struct IdentityHybridPublicKey {
    IdentityKeyPurpose purpose;
    std::array<unsigned char, 32> ed25519{};
    std::vector<unsigned char> ml_dsa;
};

struct IdentityHybridSignature {
    std::array<unsigned char, 64> ed25519{};
    std::vector<unsigned char> ml_dsa;
};

// Local V2 foundation only. Consensus encoding and operation-specific signing
// domains are intentionally separate and have not yet been activated.
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
std::optional<std::array<unsigned char, 32>> ComputeDeviceKeyId(
    const IdentityHybridPublicKey& device_key);

} // namespace cybou
#endif
