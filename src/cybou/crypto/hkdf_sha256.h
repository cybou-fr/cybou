// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_CRYPTO_HKDF_SHA256_H
#define CYBOU_CRYPTO_HKDF_SHA256_H

#include <span>

namespace cybou::crypto {

/** Derive output with RFC 5869 HKDF-SHA256. Returns false if EVP cannot derive it. */
[[nodiscard]] bool HkdfSha256(
    std::span<const unsigned char> input_key_material,
    std::span<const unsigned char> salt,
    std::span<const unsigned char> info,
    std::span<unsigned char> output);

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_HKDF_SHA256_H
