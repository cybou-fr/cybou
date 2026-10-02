// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_SIGNING_H
#define CYBOU_SIGNING_H

#include <uint256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

inline constexpr size_t ED25519_PUBLIC_KEY_SIZE{32};
inline constexpr size_t MLDSA44_PUBLIC_KEY_SIZE{1312};
inline constexpr size_t MLDSA65_PUBLIC_KEY_SIZE{1952};

inline constexpr size_t USER_SIGNATURE_SIZE{64};

/** Verify a classical Ed25519 user signature against a 32-byte public key. */
bool VerifyUserSignature(
    const uint256& public_key,
    std::span<const unsigned char> signature,
    std::span<const unsigned char> message);

/** Derive 32-byte Ed25519 public key from a 32-byte private key seed. */
std::optional<uint256> DeriveEd25519PublicKey(std::span<const unsigned char, 32> private_key);

/** Sign a message using a 32-byte Ed25519 private key seed, producing a 64-byte signature. */
std::optional<std::array<unsigned char, USER_SIGNATURE_SIZE>> SignUserMessage(
    std::span<const unsigned char, 32> private_key,
    std::span<const unsigned char> message);

} // namespace cybou

#endif // CYBOU_SIGNING_H
