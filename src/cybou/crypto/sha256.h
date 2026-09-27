// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_CRYPTO_SHA256_H
#define CYBOU_CRYPTO_SHA256_H

#include <cstddef>
#include <memory>

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

} // namespace cybou::crypto

#endif // CYBOU_CRYPTO_SHA256_H
