// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BFT_H
#define CYBOU_BFT_H

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

struct BftCommitVoteV1 {
    uint256 validator_id;
    std::array<unsigned char, USER_SIGNATURE_SIZE> signature{};

    friend bool operator==(const BftCommitVoteV1&, const BftCommitVoteV1&) = default;
};

struct BftFinalityCertificateV1 {
    uint8_t version{BFT_FINALITY_CERTIFICATE_VERSION};
    uint256 network_id;
    uint256 block_id;
    uint64_t height{0};
    uint256 validator_set_commitment;
    std::vector<BftCommitVoteV1> commit_votes;

    friend bool operator==(const BftFinalityCertificateV1&, const BftFinalityCertificateV1&) = default;
};

/**
 * Compute the domain-separated message digest for validator commit voting:
 * SHA256("CYBOU/BFT_COMMIT/V1" || network_id || block_id || height || validator_set_commitment)
 */
uint256 ComputeBftCommitDigest(
    const uint256& network_id,
    const uint256& block_id,
    uint64_t height,
    const uint256& validator_set_commitment);

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

} // namespace cybou

#endif // CYBOU_BFT_H
