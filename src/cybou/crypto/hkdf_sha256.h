// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Русский публичный API RFC 5869 HKDF-SHA256.

#ifndef CYBOU_CRYPTO_HKDF_SHA256_H
#define CYBOU_CRYPTO_HKDF_SHA256_H

#include <span>

namespace cybou::crypto {

/// \brief Выполняет RFC 5869 HKDF-SHA256 над входным ключевым материалом.
/// \param input_key_material Входной ключевой материал IKM.
/// \param salt Необязательная соль; пустой span трактуется как RFC 5869 zero-salt длиной 32 байта.
/// \param info Контекстная строка domain separation.
/// \param output Выходной буфер длиной от 1 до `255 * 32` байт.
/// \return false, если OpenSSL EVP не смог выполнить derivation.
/// \post При `false` `output` очищен fail-closed.
[[nodiscard]] bool HkdfSha256(
    std::span<const unsigned char> input_key_material,
    std::span<const unsigned char> salt,
    std::span<const unsigned char> info,
    std::span<unsigned char> output);

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_HKDF_SHA256_H
