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

inline constexpr size_t X25519_PRIVATE_KEY_SIZE{32};
inline constexpr size_t X25519_PUBLIC_KEY_SIZE{32};
inline constexpr size_t ML_KEM_768_SEED_SIZE{64};
inline constexpr size_t ML_KEM_768_PUBLIC_KEY_SIZE{1184};
inline constexpr size_t ML_KEM_768_CIPHERTEXT_SIZE{1088};
inline constexpr size_t ML_KEM_768_SHARED_SECRET_SIZE{32};
inline constexpr size_t XWING_SEED_SIZE{32};
inline constexpr size_t XWING_PUBLIC_KEY_SIZE{1216};
inline constexpr size_t XWING_CIPHERTEXT_SIZE{1120};
inline constexpr uint16_t IDENTITY_KEM_PROFILE_XWING{0x647a};
inline constexpr size_t IDENTITY_KEM_PACKAGE_SIZE{3 + XWING_PUBLIC_KEY_SIZE};

using DeviceX25519PrivateKey = std::array<unsigned char, X25519_PRIVATE_KEY_SIZE>;
using DeviceX25519PublicKey = std::array<unsigned char, X25519_PUBLIC_KEY_SIZE>;
using MlKem768Seed = std::array<unsigned char, ML_KEM_768_SEED_SIZE>;
using MlKem768PublicKey = std::array<unsigned char, ML_KEM_768_PUBLIC_KEY_SIZE>;
using MlKem768Ciphertext = std::array<unsigned char, ML_KEM_768_CIPHERTEXT_SIZE>;
using MlKem768SharedSecret = std::array<unsigned char, ML_KEM_768_SHARED_SECRET_SIZE>;
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

struct MlKem768Encapsulation {
    MlKem768Ciphertext ciphertext{};
    MlKem768SharedSecret shared_secret{};

    MlKem768Encapsulation() = default;
    MlKem768Encapsulation(const MlKem768Encapsulation&) = delete;
    MlKem768Encapsulation& operator=(const MlKem768Encapsulation&) = delete;
    MlKem768Encapsulation(MlKem768Encapsulation&& other) noexcept;
    MlKem768Encapsulation& operator=(MlKem768Encapsulation&& other) noexcept;
    ~MlKem768Encapsulation();
};

struct MlKem768Decapsulation {
    MlKem768SharedSecret shared_secret{};

    MlKem768Decapsulation() = default;
    MlKem768Decapsulation(const MlKem768Decapsulation&) = delete;
    MlKem768Decapsulation& operator=(const MlKem768Decapsulation&) = delete;
    MlKem768Decapsulation(MlKem768Decapsulation&& other) noexcept;
    MlKem768Decapsulation& operator=(MlKem768Decapsulation&& other) noexcept;
    ~MlKem768Decapsulation();
};

std::optional<DeviceX25519PrivateKey> GenerateDeviceX25519PrivateKey();
std::optional<DeviceX25519PublicKey> DeriveDeviceX25519PublicKey(
    std::span<const unsigned char, X25519_PRIVATE_KEY_SIZE> private_key);
std::optional<MlKem768Seed> GenerateMlKem768Seed();
std::optional<MlKem768PublicKey> DeriveMlKem768PublicKey(
    std::span<const unsigned char, ML_KEM_768_SEED_SIZE> seed);
std::optional<MlKem768Encapsulation> EncapsulateMlKem768(
    std::span<const unsigned char, ML_KEM_768_PUBLIC_KEY_SIZE> public_key);
std::optional<MlKem768Decapsulation> DecapsulateMlKem768(
    std::span<const unsigned char, ML_KEM_768_SEED_SIZE> seed,
    std::span<const unsigned char, ML_KEM_768_CIPHERTEXT_SIZE> ciphertext);

std::optional<XWingSeed> GenerateXWingSeed();
std::optional<XWingPublicKey> DeriveXWingPublicKey(
    std::span<const unsigned char, XWING_SEED_SIZE> seed);
/** Verify the seed's X-Wing public key with an encapsulation/decapsulation round trip. */
bool ValidateXWingKeyPair(std::span<const unsigned char, XWING_SEED_SIZE> seed);
std::optional<XWingEncapsulation> EncapsulateXWing(
    std::span<const unsigned char, XWING_PUBLIC_KEY_SIZE> public_key);
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
