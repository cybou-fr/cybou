// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Реализация потокового SHA-256 на OpenSSL EVP.

#include <cybou/crypto/sha256.h>
#include <cybou/crypto/cleanse.h>

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <stdexcept>

namespace cybou::crypto {
namespace {

/// \brief Освобождает `EVP_MD_CTX` в unique_ptr без выброса исключений.
struct EvpMdCtxDeleter {
    void operator()(EVP_MD_CTX* context) const noexcept { EVP_MD_CTX_free(context); }
};

/// \brief Централизует fail-closed исключение для нарушений жизненного цикла SHA-256 контекста.
[[noreturn]] void ThrowSha256Failure()
{
    throw std::runtime_error{"OpenSSL EVP SHA-256 operation failed"};
}

} // namespace

struct Sha256::Impl {
    std::unique_ptr<EVP_MD_CTX, EvpMdCtxDeleter> context{EVP_MD_CTX_new()};
    bool finalized{false};

    Impl()
    {
        if (!context || EVP_DigestInit_ex2(context.get(), EVP_sha256(), nullptr) != 1) {
            ThrowSha256Failure();
        }
    }
};

Sha256::Sha256() : m_impl{std::make_unique<Impl>()} {}
Sha256::~Sha256() = default;
Sha256::Sha256(Sha256&&) noexcept = default;
Sha256& Sha256::operator=(Sha256&&) noexcept = default;

Sha256& Sha256::Write(const unsigned char* data, const std::size_t size)
{
    if (!m_impl || m_impl->finalized || (size != 0 && data == nullptr)) ThrowSha256Failure();
    if (size != 0 && EVP_DigestUpdate(m_impl->context.get(), data, size) != 1) ThrowSha256Failure();
    return *this;
}

void Sha256::Finalize(unsigned char* output)
{
    if (!m_impl || m_impl->finalized || output == nullptr) ThrowSha256Failure();

    unsigned int output_size{0};
    // Сначала помечаем контекст как завершенный: повторная попытка после
    // частичного сбоя тоже считается нарушением жизненного цикла.
    m_impl->finalized = true;
    if (EVP_DigestFinal_ex(m_impl->context.get(), output, &output_size) != 1 || output_size != OUTPUT_SIZE) {
        ThrowSha256Failure();
    }
}

bool ComputeSha256(const std::initializer_list<std::span<const unsigned char>> parts, unsigned char* output) noexcept
{
    if (output == nullptr) return false;
    try {
        Sha256 hasher;
        for (const auto part : parts) hasher.Write(part.data(), part.size());
        hasher.Finalize(output);
        return true;
    } catch (...) {
        CleanseMemory(output, Sha256::OUTPUT_SIZE);
        return false;
    }
}

std::span<const unsigned char> Sha256Bytes(const std::string_view text) noexcept
{
    return {reinterpret_cast<const unsigned char*>(text.data()), text.size()};
}

} // namespace cybou::crypto
