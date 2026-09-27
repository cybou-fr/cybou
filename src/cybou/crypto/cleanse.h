// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_CRYPTO_CLEANSE_H
#define CYBOU_CRYPTO_CLEANSE_H

#include <openssl/crypto.h>

#include <cstddef>

namespace cybou::crypto {

/** Clear sensitive memory using OpenSSL's non-optimizable cleanse primitive. */
inline void CleanseMemory(void* data, std::size_t size) noexcept
{
    if (data != nullptr && size != 0) OPENSSL_cleanse(data, size);
}

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_CLEANSE_H
