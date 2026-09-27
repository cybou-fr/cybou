// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_KEM_H
#define CYBOU_IDENTITY_KEM_H

#include <array>
#include <optional>
#include <span>

namespace cybou {

inline constexpr size_t X25519_PRIVATE_KEY_SIZE{32};
inline constexpr size_t X25519_PUBLIC_KEY_SIZE{32};
inline constexpr size_t ML_KEM_768_SEED_SIZE{64};
inline constexpr size_t ML_KEM_768_PUBLIC_KEY_SIZE{1184};
inline constexpr size_t ML_KEM_768_CIPHERTEXT_SIZE{1088};
inline constexpr size_t ML_KEM_768_SHARED_SECRET_SIZE{32};

using DeviceX25519PrivateKey = std::array<unsigned char, X25519_PRIVATE_KEY_SIZE>;
using DeviceX25519PublicKey = std::array<unsigned char, X25519_PUBLIC_KEY_SIZE>;
using MlKem768Seed = std::array<unsigned char, ML_KEM_768_SEED_SIZE>;
using MlKem768PublicKey = std::array<unsigned char, ML_KEM_768_PUBLIC_KEY_SIZE>;
using MlKem768Ciphertext = std::array<unsigned char, ML_KEM_768_CIPHERTEXT_SIZE>;
using MlKem768SharedSecret = std::array<unsigned char, ML_KEM_768_SHARED_SECRET_SIZE>;

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

} // namespace cybou

#endif // CYBOU_IDENTITY_KEM_H
