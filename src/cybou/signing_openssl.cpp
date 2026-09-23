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

} // namespace cybou
