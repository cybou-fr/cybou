// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>

#include <openssl/evp.h>

#include <memory>

namespace cybou {
namespace {

using PKey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using MdContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

bool VerifyComponent(
    const char* algorithm,
    const std::span<const unsigned char> public_key,
    const std::span<const unsigned char> signature,
    const std::span<const unsigned char> message)
{
    PKey key{EVP_PKEY_new_raw_public_key_ex(
        nullptr, algorithm, nullptr, public_key.data(), public_key.size()), EVP_PKEY_free};
    if (!key) return false;
    MdContext context{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    if (!context) return false;
    if (EVP_DigestVerifyInit_ex(context.get(), nullptr, nullptr, nullptr, nullptr, key.get(), nullptr) != 1) return false;
    return EVP_DigestVerify(
        context.get(), signature.data(), signature.size(), message.data(), message.size()) == 1;
}

} // namespace

bool OpenSslOperatorAuthoritySignatureVerifier::Verify(
    const OperatorAuthorityKeySet& keyset,
    const SignatureBundleV1& bundle,
    const std::span<const unsigned char> message) const
{
    if (bundle.suite_id != SignatureSuiteId::HYBRID_ED25519_MLDSA65_V1 ||
        bundle.authority_keyset_id != keyset.keyset_id) return false;
    return VerifyComponent("ED25519", keyset.ed25519_public_key, bundle.classical_signature, message) &&
        VerifyComponent("ML-DSA-65", keyset.mldsa65_public_key, bundle.pq_signature, message);
}

bool VerifyUserSignature(
    const uint256& public_key,
    const std::span<const unsigned char> signature,
    const std::span<const unsigned char> message)
{
    if (public_key.IsNull() || signature.size() != USER_SIGNATURE_SIZE) return false;
    return VerifyComponent(
        "ED25519",
        std::span<const unsigned char>{public_key.begin(), public_key.size()},
        signature,
        message);
}

std::optional<uint256> DeriveEd25519PublicKey(const std::span<const unsigned char, 32> private_key)
{
    PKey key{EVP_PKEY_new_raw_private_key_ex(
        nullptr, "ED25519", nullptr, private_key.data(), private_key.size()), EVP_PKEY_free};
    if (!key) return std::nullopt;
    uint256 pub;
    size_t len{pub.size()};
    if (EVP_PKEY_get_raw_public_key(key.get(), pub.begin(), &len) != 1 || len != pub.size()) {
        return std::nullopt;
    }
    return pub;
}

std::optional<std::array<unsigned char, USER_SIGNATURE_SIZE>> SignUserMessage(
    const std::span<const unsigned char, 32> private_key,
    const std::span<const unsigned char> message)
{
    PKey key{EVP_PKEY_new_raw_private_key_ex(
        nullptr, "ED25519", nullptr, private_key.data(), private_key.size()), EVP_PKEY_free};
    if (!key) return std::nullopt;
    MdContext context{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    if (!context) return std::nullopt;
    if (EVP_DigestSignInit_ex(context.get(), nullptr, nullptr, nullptr, nullptr, key.get(), nullptr) != 1) {
        return std::nullopt;
    }
    std::array<unsigned char, USER_SIGNATURE_SIZE> sig{};
    size_t sig_len{sig.size()};
    if (EVP_DigestSign(context.get(), sig.data(), &sig_len, message.data(), message.size()) != 1 ||
        sig_len != USER_SIGNATURE_SIZE) {
        return std::nullopt;
    }
    return sig;
}

} // namespace cybou
