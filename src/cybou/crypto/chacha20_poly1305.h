// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API AEAD ChaCha20-Poly1305.

#ifndef CYBOU_CRYPTO_CHACHA20_POLY1305_H
#define CYBOU_CRYPTO_CHACHA20_POLY1305_H

#include <span>

namespace cybou::crypto {

/// \brief Размер ключа ChaCha20-Poly1305: `32` байта = `256` бит по RFC 8439.
inline constexpr std::size_t CHACHA20_POLY1305_KEY_SIZE{32};
/// \brief Размер nonce: `12` байт = `96` бит, как требует стандартный режим AEAD RFC 8439.
inline constexpr std::size_t CHACHA20_POLY1305_NONCE_SIZE{12};
/// \brief Размер тега аутентичности: `16` байт = `128` бит.
inline constexpr std::size_t CHACHA20_POLY1305_TAG_SIZE{16};

/// \brief Шифрует данные и дописывает 16-байтовый тег аутентичности RFC 8439.
/// \param key Ключ шифрования.
/// \param nonce Одноразовый nonce для данного ключа.
/// \param associated_data Аутентифицируемые, но не шифруемые байты.
/// \param plaintext Открытый текст.
/// \param ciphertext_and_tag Выходной буфер длиной `plaintext.size() + CHACHA20_POLY1305_TAG_SIZE`.
/// \return `true` при полном успехе; `false` при нарушении размеров или ошибке OpenSSL.
/// \post При `false` выходной буфер очищен fail-closed.
[[nodiscard]] bool ChaCha20Poly1305Encrypt(
    std::span<const unsigned char, CHACHA20_POLY1305_KEY_SIZE> key,
    std::span<const unsigned char, CHACHA20_POLY1305_NONCE_SIZE> nonce,
    std::span<const unsigned char> associated_data,
    std::span<const unsigned char> plaintext,
    std::span<unsigned char> ciphertext_and_tag);

/// \brief Проверяет тег и расшифровывает буфер, где последние 16 байт — тег.
/// \param key Ключ шифрования.
/// \param nonce Nonce, использованный при шифровании.
/// \param associated_data Аутентифицируемые сопутствующие байты.
/// \param ciphertext_and_tag Входной буфер, где последние `CHACHA20_POLY1305_TAG_SIZE` байт составляют тег.
/// \param plaintext Выходной буфер длиной `ciphertext_and_tag.size() - CHACHA20_POLY1305_TAG_SIZE`.
/// \return `true` только если тег корректен и размеры согласованы.
/// \post При `false` `plaintext` очищен fail-closed.
[[nodiscard]] bool ChaCha20Poly1305Decrypt(
    std::span<const unsigned char, CHACHA20_POLY1305_KEY_SIZE> key,
    std::span<const unsigned char, CHACHA20_POLY1305_NONCE_SIZE> nonce,
    std::span<const unsigned char> associated_data,
    std::span<const unsigned char> ciphertext_and_tag,
    std::span<unsigned char> plaintext);

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_CHACHA20_POLY1305_H
