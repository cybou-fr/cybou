// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/crypto/hkdf_sha256.h>

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>

#include <array>
#include <memory>
#include <span>

namespace cybou::crypto {
namespace {

using Kdf = std::unique_ptr<EVP_KDF, decltype(&EVP_KDF_free)>;
using KdfContext = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;
constexpr std::array<unsigned char, 32> DEFAULT_SALT{};
unsigned char EMPTY_OCTET{0};

} // namespace

bool HkdfSha256(
    const std::span<const unsigned char> input_key_material,
    const std::span<const unsigned char> salt,
    const std::span<const unsigned char> info,
    const std::span<unsigned char> output)
{
    // RFC 5869 limits output to 255 SHA-256 blocks.
    if (output.empty() || output.size() > 255 * 32) return false;

    Kdf kdf{EVP_KDF_fetch(nullptr, "HKDF", nullptr), EVP_KDF_free};
    if (!kdf) return false;
    KdfContext context{EVP_KDF_CTX_new(kdf.get()), EVP_KDF_CTX_free};
    if (!context) return false;

    char digest[] = "SHA256";
    // Пустую соль приводим к RFC 5869 zero-salt длиной HashLen, чтобы поведение
    // не зависело от трактовки пустого span внутри конкретной OpenSSL версии.
    const auto effective_salt = salt.empty() ? std::span<const unsigned char>{DEFAULT_SALT} : salt;
    auto* key_data = const_cast<unsigned char*>(input_key_material.empty() ? &EMPTY_OCTET : input_key_material.data());
    auto* info_data = const_cast<unsigned char*>(info.empty() ? &EMPTY_OCTET : info.data());
    OSSL_PARAM parameters[] = {
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, digest, 0),
        OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_KEY, key_data, input_key_material.size()),
        OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_SALT, const_cast<unsigned char*>(effective_salt.data()), effective_salt.size()),
        OSSL_PARAM_construct_octet_string(
            OSSL_KDF_PARAM_INFO, info_data, info.size()),
        OSSL_PARAM_construct_end(),
    };
    if (EVP_KDF_derive(context.get(), output.data(), output.size(), parameters) == 1) return true;
    // На ошибке очищаем весь буфер derivation: частично записанный ключевой
    // материал нельзя оставлять вызывающему коду.
    OPENSSL_cleanse(output.data(), output.size());
    return false;
}

} // namespace cybou::crypto
