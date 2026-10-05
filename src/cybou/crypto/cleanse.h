// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Русская обертка над гарантированной очисткой секретной памяти.

#ifndef CYBOU_CRYPTO_CLEANSE_H
#define CYBOU_CRYPTO_CLEANSE_H

#include <openssl/crypto.h>

#include <cstddef>

namespace cybou::crypto {

/// \brief Очищает чувствительную память через примитив OpenSSL, который нельзя безопасно выкинуть оптимизацией.
/// \param data Начало диапазона для очистки; допускается `nullptr` только при `size == 0`.
/// \param size Длина диапазона в байтах.
/// \post Если диапазон ненулевой, его прежнее содержимое больше нельзя считать допустимым для чтения.
inline void CleanseMemory(void* data, std::size_t size) noexcept
{
    if (data != nullptr && size != 0) OPENSSL_cleanse(data, size);
}

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_CLEANSE_H
