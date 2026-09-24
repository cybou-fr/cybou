// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_SIGNING_H
#define CYBOU_SIGNING_H

#include <uint256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

/**
 * Per-object signing domains. A signature is always computed over
 * ObjectSigningDomainTag(domain) || canonical(object), so a valid signature
 * for one protocol object can never be replayed as another object type.
 * These are distinct from OperatorKeyDomain tags, which separate keys.
 */
enum class ObjectSigningDomain : uint8_t {
    VALIDATOR_ADMISSION,
    VALIDATOR_REMOVAL,
    PROTOCOL_PARAMETER,
};

std::string_view ObjectSigningDomainTag(ObjectSigningDomain domain);

/**
 * Frozen signature suite identifiers. The suite fixes both algorithms and
 * their combination rule; new suites get new identifiers, never a reinterpretation.
 */
enum class SignatureSuiteId : uint16_t {
    HYBRID_ED25519_MLDSA65_V1 = 1,
};

inline constexpr size_t ED25519_SIGNATURE_SIZE{64};
inline constexpr size_t MLDSA65_SIGNATURE_SIZE{3309};
inline constexpr size_t ED25519_PUBLIC_KEY_SIZE{32};
inline constexpr size_t MLDSA65_PUBLIC_KEY_SIZE{1952};

struct OperatorAuthorityKeySet {
    uint256 keyset_id;
    std::array<unsigned char, ED25519_PUBLIC_KEY_SIZE> ed25519_public_key{};
    std::array<unsigned char, MLDSA65_PUBLIC_KEY_SIZE> mldsa65_public_key{};
    uint64_t active_from_epoch{0};
    std::optional<uint64_t> retired_from_epoch;
};

bool IsActiveAtEpoch(const OperatorAuthorityKeySet& keyset, uint64_t epoch);

struct SignatureBundleV1;

/**
 * Cryptographic boundary for Operator Authority signatures.
 * Implementations must verify BOTH component signatures over the exact message.
 */
class OperatorAuthoritySignatureVerifier
{
public:
    virtual ~OperatorAuthoritySignatureVerifier() = default;
    virtual bool Verify(
        const OperatorAuthorityKeySet& keyset,
        const SignatureBundleV1& bundle,
        std::span<const unsigned char> message) const = 0;
};

/** Production Ed25519 + ML-DSA-65 verifier backed by OpenSSL >= 3.5. */
class OpenSslOperatorAuthoritySignatureVerifier final : public OperatorAuthoritySignatureVerifier
{
public:
    bool Verify(
        const OperatorAuthorityKeySet& keyset,
        const SignatureBundleV1& bundle,
        std::span<const unsigned char> message) const override;
};

/**
 * Hybrid signature bundle for HYBRID_ED25519_MLDSA65_V1.
 *
 * Both signatures independently cover the same domain-separated canonical
 * message. The bundle is valid only if BOTH verify (AND, not OR). Fixed-size
 * fields: no arbitrary-length vectors on any consensus-adjacent path.
 */
struct SignatureBundleV1 {
    SignatureSuiteId suite_id{SignatureSuiteId::HYBRID_ED25519_MLDSA65_V1};
    uint256 authority_keyset_id;
    std::array<unsigned char, ED25519_SIGNATURE_SIZE> classical_signature{};
    std::array<unsigned char, MLDSA65_SIGNATURE_SIZE> pq_signature{};

    friend bool operator==(const SignatureBundleV1&, const SignatureBundleV1&) = default;
};

inline constexpr size_t SIGNATURE_BUNDLE_V1_SIZE{
    2 + 32 + ED25519_SIGNATURE_SIZE + MLDSA65_SIGNATURE_SIZE
};

std::vector<unsigned char> SerializeSignatureBundle(const SignatureBundleV1& bundle);
std::optional<SignatureBundleV1> DeserializeSignatureBundle(std::span<const unsigned char> bytes);

/** Structural gate: known suite, non-null keyset, and BOTH signature parts. */
bool IsPresent(const SignatureBundleV1& bundle);

inline constexpr size_t USER_SIGNATURE_SIZE{64};

/** Verify a classical Ed25519 user signature against a 32-byte public key. */
bool VerifyUserSignature(
    const uint256& public_key,
    std::span<const unsigned char> signature,
    std::span<const unsigned char> message);

/** Derive 32-byte Ed25519 public key from a 32-byte private key seed. */
std::optional<uint256> DeriveEd25519PublicKey(std::span<const unsigned char, 32> private_key);

/** Sign a message using a 32-byte Ed25519 private key seed, producing a 64-byte signature. */
std::optional<std::array<unsigned char, USER_SIGNATURE_SIZE>> SignUserMessage(
    std::span<const unsigned char, 32> private_key,
    std::span<const unsigned char> message);

/** Convert a 32-byte Ed25519 public key to a 32-byte X25519 (Curve25519) Montgomery public key. */
std::optional<uint256> Ed25519PublicKeyToX25519(const uint256& ed25519_public_key);

/** Derive 32-byte X25519 private scalar from a 32-byte Ed25519 private seed. */
std::optional<std::array<unsigned char, 32>> Ed25519SeedToX25519PrivateKey(std::span<const unsigned char, 32> seed);

/** Generate an ephemeral X25519 key pair. */
bool GenerateX25519KeyPair(
    std::array<unsigned char, 32>& out_private_key,
    uint256& out_public_key);

/** Perform X25519 Diffie-Hellman key agreement to produce a 32-byte shared secret. */
std::optional<std::array<unsigned char, 32>> X25519DeriveSharedSecret(
    std::span<const unsigned char, 32> private_key,
    const uint256& peer_public_key);

} // namespace cybou

#endif // CYBOU_SIGNING_H
