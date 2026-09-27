// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_CRYPTO_SHA256_H
#define CYBOU_CRYPTO_SHA256_H

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>

namespace cybou::crypto {

/** Incremental SHA-256 using OpenSSL EVP, with byte-identical output. */
class Sha256 final
{
public:
    static constexpr std::size_t OUTPUT_SIZE{32};

    Sha256();
    ~Sha256();

    Sha256(Sha256&&) noexcept;
    Sha256& operator=(Sha256&&) noexcept;
    Sha256(const Sha256&) = delete;
    Sha256& operator=(const Sha256&) = delete;

    Sha256& Write(const unsigned char* data, std::size_t size);
    void Finalize(unsigned char* output);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

/** Compute a digest over ordered byte spans. Returns false on provider failure. */
bool ComputeSha256(std::initializer_list<std::span<const unsigned char>> parts, unsigned char* output) noexcept;

/** View string bytes without changing their encoding. */
std::span<const unsigned char> Sha256Bytes(std::string_view text) noexcept;

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_SHA256_H
