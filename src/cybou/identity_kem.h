// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_KEM_H
#define CYBOU_IDENTITY_KEM_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t XWING_SEED_SIZE{32};
inline constexpr size_t XWING_PUBLIC_KEY_SIZE{1216};
inline constexpr size_t XWING_CIPHERTEXT_SIZE{1120};
inline constexpr uint16_t IDENTITY_KEM_PROFILE_XWING{0x647a};
inline constexpr size_t IDENTITY_KEM_PACKAGE_SIZE{3 + XWING_PUBLIC_KEY_SIZE};

using XWingSeed = std::array<unsigned char, XWING_SEED_SIZE>;
using XWingPublicKey = std::array<unsigned char, XWING_PUBLIC_KEY_SIZE>;
using XWingCiphertext = std::array<unsigned char, XWING_CIPHERTEXT_SIZE>;
using XWingSharedSecret = std::array<unsigned char, 32>;
using IdentityKemPackage = std::array<unsigned char, IDENTITY_KEM_PACKAGE_SIZE>;

struct XWingEncapsulation {
    XWingCiphertext ciphertext{};
    XWingSharedSecret shared_secret{};
    XWingEncapsulation() = default;
    XWingEncapsulation(const XWingEncapsulation&) = delete;
    XWingEncapsulation& operator=(const XWingEncapsulation&) = delete;
    XWingEncapsulation(XWingEncapsulation&& other) noexcept;
    XWingEncapsulation& operator=(XWingEncapsulation&& other) noexcept;
    ~XWingEncapsulation();
};

std::optional<XWingSeed> GenerateXWingSeed();
std::optional<XWingPublicKey> DeriveXWingPublicKey(
    std::span<const unsigned char, XWING_SEED_SIZE> seed);
/** Verify the seed's X-Wing public key with an encapsulation/decapsulation round trip. */
bool ValidateXWingKeyPair(std::span<const unsigned char, XWING_SEED_SIZE> seed);
std::optional<XWingEncapsulation> EncapsulateXWing(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
#if defined(CYBOU_ENABLE_TEST_HOOKS)
/** Deterministic draft-vector entry point. Available only in test-hook builds. */
std::optional<XWingEncapsulation> EncapsulateXWingForTest(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key,
    std::span<const unsigned char, 64> randomness);
#endif
std::optional<XWingSharedSecret> DecapsulateXWing(
    std::span<const unsigned char, XWING_SEED_SIZE> seed,
    std::span<const unsigned char, XWING_CIPHERTEXT_SIZE> ciphertext);
std::optional<IdentityKemPackage> EncodeIdentityKemPackage(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
std::optional<XWingPublicKey> DecodeIdentityKemPackage(std::span<const unsigned char> package);
std::optional<std::array<unsigned char, 32>> ComputeIdentityKemPackageCommitment(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char, 32> account_id,
    std::span<const unsigned char, 32> device_key_id,
    uint64_t activation_nonce,
    std::span<const unsigned char> package);

} // namespace cybou

#endif // CYBOU_IDENTITY_KEM_H
