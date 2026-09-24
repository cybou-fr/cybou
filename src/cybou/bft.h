// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BFT_H
#define CYBOU_BFT_H

#include <cybou/identity_crypto.h>
#include <cybou/validator.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t BFT_FINALITY_CERTIFICATE_VERSION{2};
inline constexpr size_t BFT_COMMIT_VOTE_SIZE{32 + 64 + 3309}; // 3405 bytes

// Post-Quantum BFT Commit Vote and Certificate (Ed25519 + ML-DSA-65)
struct BftCommitVote {
    uint256 validator_id;
    IdentityHybridSignature signature;

    friend bool operator==(const BftCommitVote&, const BftCommitVote&) = default;
};

struct BftFinalityCertificate {
    uint8_t version{BFT_FINALITY_CERTIFICATE_VERSION};
    uint256 network_id;
    uint256 block_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 validator_set_commitment;
    std::vector<BftCommitVote> commit_votes;

    friend bool operator==(const BftFinalityCertificate&, const BftFinalityCertificate&) = default;
};

/**
 * Compute the domain-separated message digest for validator commit voting:
 * SHA256("CYBOU/BFT_COMMIT/V2" || network_id || block_id || height || round || validator_set_commitment)
 */
uint256 ComputeBftCommitDigest(
    const uint256& network_id,
    const uint256& block_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_set_commitment);

std::optional<IdentityHybridSignature> SignValidatorVote(
    std::span<const unsigned char, 32> private_key_seed,
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
    const BftFinalityCertificate& cert,
    const ValidatorSet& validator_set,
    const uint256& expected_network_id);

std::optional<std::vector<unsigned char>> SerializeFinalityCertificate(const BftFinalityCertificate& cert);
std::optional<BftFinalityCertificate> DeserializeFinalityCertificate(std::span<const unsigned char> bytes);

// Temporary transition aliases
using BftCommitVoteV2 = BftCommitVote;
using BftFinalityCertificateV2 = BftFinalityCertificate;
inline constexpr uint8_t BFT_FINALITY_CERTIFICATE_VERSION_V2{BFT_FINALITY_CERTIFICATE_VERSION};
inline constexpr size_t BFT_COMMIT_VOTE_V2_SIZE{BFT_COMMIT_VOTE_SIZE};
inline uint256 ComputeBftCommitDigestV2(const uint256& nid, const uint256& bid, uint64_t h, uint32_t r, const uint256& vc) { return ComputeBftCommitDigest(nid, bid, h, r, vc); }
inline FinalityVerificationError VerifyFinalityCertificateV2(const BftFinalityCertificate& c, const ValidatorSet& vs, const uint256& enid) { return VerifyFinalityCertificate(c, vs, enid); }
inline std::optional<std::vector<unsigned char>> SerializeFinalityCertificateV2(const BftFinalityCertificate& c) { return SerializeFinalityCertificate(c); }
inline std::optional<BftFinalityCertificate> DeserializeFinalityCertificateV2(std::span<const unsigned char> b) { return DeserializeFinalityCertificate(b); }

} // namespace cybou

#endif // CYBOU_BFT_H
