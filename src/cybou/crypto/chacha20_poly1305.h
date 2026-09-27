// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_CRYPTO_CHACHA20_POLY1305_H
#define CYBOU_CRYPTO_CHACHA20_POLY1305_H

#include <span>

namespace cybou::crypto {

inline constexpr std::size_t CHACHA20_POLY1305_KEY_SIZE{32};
inline constexpr std::size_t CHACHA20_POLY1305_NONCE_SIZE{12};
inline constexpr std::size_t CHACHA20_POLY1305_TAG_SIZE{16};

/** Encrypt and append the 16-byte RFC 8439 authentication tag. */
[[nodiscard]] bool ChaCha20Poly1305Encrypt(
    std::span<const unsigned char, CHACHA20_POLY1305_KEY_SIZE> key,
    std::span<const unsigned char, CHACHA20_POLY1305_NONCE_SIZE> nonce,
    std::span<const unsigned char> associated_data,
    std::span<const unsigned char> plaintext,
    std::span<unsigned char> ciphertext_and_tag);

/** Authenticate and decrypt ciphertext whose final 16 bytes are the tag. */
[[nodiscard]] bool ChaCha20Poly1305Decrypt(
    std::span<const unsigned char, CHACHA20_POLY1305_KEY_SIZE> key,
    std::span<const unsigned char, CHACHA20_POLY1305_NONCE_SIZE> nonce,
    std::span<const unsigned char> associated_data,
    std::span<const unsigned char> ciphertext_and_tag,
    std::span<unsigned char> plaintext);

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_CHACHA20_POLY1305_H
