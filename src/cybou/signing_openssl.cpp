// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <support/cleanse.h>

#include <algorithm>
#include <memory>

namespace cybou {
namespace {

using PKey = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using MdContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using PKeyCtx = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

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
    const SignatureBundle& bundle,
    const std::span<const unsigned char> message) const
{
    if (bundle.suite_id != SignatureSuiteId::HYBRID_ED25519_MLDSA65 ||
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

std::optional<uint256> Ed25519PublicKeyToX25519(const uint256& ed25519_public_key)
{
    // Edwards to Montgomery map: u = (1 + y) / (1 - y) mod p, where p = 2^255 - 19.
    // ed25519_public_key is 32 bytes little-endian; high bit of byte 31 is the sign bit of x.
    std::array<unsigned char, 32> y_bytes;
    std::copy(ed25519_public_key.begin(), ed25519_public_key.end(), y_bytes.begin());
    y_bytes[31] &= 0x7F;

    BN_CTX* ctx = BN_CTX_new();
    if (!ctx) return std::nullopt;

    BIGNUM* p = nullptr;
    BN_hex2bn(&p, "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffed");
    BIGNUM* y = BN_new();
    BIGNUM* one = BN_new();
    BIGNUM* num = BN_new();
    BIGNUM* den = BN_new();
    BIGNUM* den_inv = BN_new();
    BIGNUM* u = BN_new();

    std::optional<uint256> result{std::nullopt};

    if (p && y && one && num && den && den_inv && u) {
        BN_lebin2bn(y_bytes.data(), 32, y);
        BN_set_word(one, 1);

        // num = (1 + y) mod p
        BN_add(num, one, y);
        BN_nnmod(num, num, p, ctx);

        // den = (1 - y) mod p
        BN_sub(den, one, y);
        BN_nnmod(den, den, p, ctx);

        if (!BN_is_zero(den) && BN_mod_inverse(den_inv, den, p, ctx)) {
            // u = num * den_inv mod p
            BN_mod_mul(u, num, den_inv, p, ctx);
            uint256 u_out;
            if (BN_bn2lebinpad(u, u_out.begin(), 32) == 32) {
                result = u_out;
            }
        }
    }

    BN_free(p);
    BN_free(y);
    BN_free(one);
    BN_free(num);
    BN_free(den);
    BN_free(den_inv);
    BN_free(u);
    BN_CTX_free(ctx);

    return result;
}

std::optional<std::array<unsigned char, 32>> Ed25519SeedToX25519PrivateKey(const std::span<const unsigned char, 32> seed)
{
    unsigned char hash[SHA512_DIGEST_LENGTH];
    SHA512(seed.data(), seed.size(), hash);

    std::array<unsigned char, 32> x25519_sk;
    std::copy(hash, hash + 32, x25519_sk.begin());
    x25519_sk[0] &= 248;
    x25519_sk[31] &= 127;
    x25519_sk[31] |= 64;

    memory_cleanse(hash, sizeof(hash));
    return x25519_sk;
}

bool GenerateX25519KeyPair(
    std::array<unsigned char, 32>& out_private_key,
    uint256& out_public_key)
{
    PKeyCtx ctx{EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr), EVP_PKEY_CTX_free};
    if (!ctx) return false;
    if (EVP_PKEY_keygen_init(ctx.get()) <= 0) return false;
    EVP_PKEY* raw_key{nullptr};
    if (EVP_PKEY_keygen(ctx.get(), &raw_key) <= 0 || !raw_key) return false;
    PKey key{raw_key, EVP_PKEY_free};

    size_t priv_len{out_private_key.size()};
    if (EVP_PKEY_get_raw_private_key(key.get(), out_private_key.data(), &priv_len) <= 0 || priv_len != 32) {
        return false;
    }
    size_t pub_len{out_public_key.size()};
    if (EVP_PKEY_get_raw_public_key(key.get(), out_public_key.begin(), &pub_len) <= 0 || pub_len != 32) {
        return false;
    }
    return true;
}

std::optional<std::array<unsigned char, 32>> X25519DeriveSharedSecret(
    const std::span<const unsigned char, 32> private_key,
    const uint256& peer_public_key)
{
    PKey priv{EVP_PKEY_new_raw_private_key_ex(nullptr, "X25519", nullptr, private_key.data(), private_key.size()), EVP_PKEY_free};
    if (!priv) return std::nullopt;
    PKey peer{EVP_PKEY_new_raw_public_key_ex(nullptr, "X25519", nullptr, peer_public_key.begin(), peer_public_key.size()), EVP_PKEY_free};
    if (!peer) return std::nullopt;

    PKeyCtx ctx{EVP_PKEY_CTX_new(priv.get(), nullptr), EVP_PKEY_CTX_free};
    if (!ctx) return std::nullopt;
    if (EVP_PKEY_derive_init(ctx.get()) <= 0) return std::nullopt;
    if (EVP_PKEY_derive_set_peer(ctx.get(), peer.get()) <= 0) return std::nullopt;

    size_t secret_len{32};
    std::array<unsigned char, 32> secret{};
    if (EVP_PKEY_derive(ctx.get(), secret.data(), &secret_len) <= 0 || secret_len != 32) {
        memory_cleanse(secret.data(), secret.size());
        return std::nullopt;
    }
    return secret;
}

} // namespace cybou
