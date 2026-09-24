// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BFT_H
#define CYBOU_BFT_H

#include <cybou/identity_crypto.h>
#include <cybou/signing.h>
#include <cybou/validator.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t BFT_FINALITY_CERTIFICATE_VERSION{1};
inline constexpr uint8_t BFT_FINALITY_CERTIFICATE_VERSION_V2{2};
inline constexpr size_t BFT_COMMIT_VOTE_V2_SIZE{32 + 64 + 3309}; // 3405 bytes

using ValidatorSignature = std::array<unsigned char, USER_SIGNATURE_SIZE>;

struct BftCommitVoteV1 {
    uint256 validator_id;
    ValidatorSignature signature{};

    friend bool operator==(const BftCommitVoteV1&, const BftCommitVoteV1&) = default;
};

struct BftFinalityCertificateV1 {
    uint8_t version{BFT_FINALITY_CERTIFICATE_VERSION};
    uint256 network_id;
    uint256 block_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 validator_set_commitment;
    std::vector<BftCommitVoteV1> commit_votes;

    friend bool operator==(const BftFinalityCertificateV1&, const BftFinalityCertificateV1&) = default;
};

// Post-Quantum BFT Commit Vote and Certificate (Ed25519 + ML-DSA-65)
struct BftCommitVoteV2 {
    uint256 validator_id;
    IdentityHybridSignature signature;

    friend bool operator==(const BftCommitVoteV2&, const BftCommitVoteV2&) = default;
};

struct BftFinalityCertificateV2 {
    uint8_t version{BFT_FINALITY_CERTIFICATE_VERSION_V2};
    uint256 network_id;
    uint256 block_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 validator_set_commitment;
    std::vector<BftCommitVoteV2> commit_votes;

    friend bool operator==(const BftFinalityCertificateV2&, const BftFinalityCertificateV2&) = default;
};

/**
 * Compute the domain-separated message digest for validator commit voting:
 * SHA256("CYBOU/BFT_COMMIT/V1" || network_id || block_id || height || round || validator_set_commitment)
 */
uint256 ComputeBftCommitDigest(
    const uint256& network_id,
    const uint256& block_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_set_commitment);

uint256 ComputeBftCommitDigestV2(
    const uint256& network_id,
    const uint256& block_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_set_commitment);

/** Sign a validator BFT vote digest with validator consensus private key seed. */
std::optional<ValidatorSignature> SignValidatorVote(
    std::span<const unsigned char, 32> private_key,
    const uint256& digest);

/** Verify a validator BFT vote signature against the validator consensus public key. */
bool VerifyValidatorSignature(
    const uint256& public_key,
    const ValidatorSignature& signature,
    const uint256& digest);

enum class FinalityVerificationError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    NETWORK_MISMATCH,
    VALIDATOR_SET_MISMATCH,
    INSUFFICIENT_VOTES,
    DUPLICATE_VOTE,
    UNKNOWN_VALIDATOR,
    INVALID_SIGNATURE,
};

FinalityVerificationError VerifyFinalityCertificate(
    const BftFinalityCertificateV1& cert,
    const ValidatorSetV1& validator_set,
    const uint256& expected_network_id);

std::vector<unsigned char> SerializeFinalityCertificate(const BftFinalityCertificateV1& cert);
std::optional<BftFinalityCertificateV1> DeserializeFinalityCertificate(std::span<const unsigned char> bytes);

FinalityVerificationError VerifyFinalityCertificateV2(
    const BftFinalityCertificateV2& cert,
    const ValidatorSetV2& validator_set,
    const uint256& expected_network_id);

std::vector<unsigned char> SerializeFinalityCertificateV2(const BftFinalityCertificateV2& cert);
std::optional<BftFinalityCertificateV2> DeserializeFinalityCertificateV2(std::span<const unsigned char> bytes);

// Unversioned aliases
using BftCommitVote = BftCommitVoteV2;
using BftFinalityCertificate = BftFinalityCertificateV2;

} // namespace cybou

#endif // CYBOU_BFT_H
