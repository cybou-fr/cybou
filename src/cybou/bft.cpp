// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <set>
#include <string_view>

namespace cybou {

namespace {

inline void AppendUint64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline uint64_t ReadUint64LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint64_t val{0};
    for (int i = 0; i < 8; ++i) {
        val |= uint64_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

inline uint32_t ReadUint32LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint32_t val{0};
    for (int i = 0; i < 4; ++i) {
        val |= uint32_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

} // namespace

uint256 ComputeBftCommitDigest(
    const uint256& network_id,
    const uint256& block_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_set_commitment)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_COMMIT/V2"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());
    hasher.Write(block_id.begin(), block_id.size());

    unsigned char height_bytes[8];
    for (int i = 0; i < 8; ++i) {
        height_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    }
    hasher.Write(height_bytes, sizeof(height_bytes));

    unsigned char round_bytes[4];
    for (int i = 0; i < 4; ++i) {
        round_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    }
    hasher.Write(round_bytes, sizeof(round_bytes));

    hasher.Write(validator_set_commitment.begin(), validator_set_commitment.size());

    uint256 result;
    hasher.Finalize(result.begin());
    return result;
}

std::optional<IdentityHybridSignature> SignValidatorVote(
    std::span<const unsigned char, 32> private_key_seed,
    const uint256& digest)
{
    return SignIdentityMessage(private_key_seed, IdentityKeyPurpose::VALIDATOR, digest);
}

FinalityVerificationError VerifyFinalityCertificate(
    const BftFinalityCertificate& cert,
    const ValidatorSet& validator_set,
    const uint256& expected_network_id)
{
    if (cert.version != BFT_FINALITY_CERTIFICATE_VERSION) {
        return FinalityVerificationError::UNSUPPORTED_VERSION;
    }

    if (cert.network_id != expected_network_id) {
        return FinalityVerificationError::NETWORK_MISMATCH;
    }

    const uint256 expected_val_commitment = ComputeValidatorSetCommitment(validator_set);
    if (cert.validator_set_commitment != expected_val_commitment) {
        return FinalityVerificationError::VALIDATOR_SET_MISMATCH;
    }

    const size_t quorum = validator_set.QuorumThreshold();
    if (cert.commit_votes.size() < quorum) {
        return FinalityVerificationError::INSUFFICIENT_VOTES;
    }

    const uint256 digest = ComputeBftCommitDigest(
        cert.network_id,
        cert.block_id,
        cert.height,
        cert.round,
        cert.validator_set_commitment);

    std::set<uint256> seen_validators;
    for (const auto& vote : cert.commit_votes) {
        if (!seen_validators.insert(vote.validator_id).second) {
            return FinalityVerificationError::DUPLICATE_VOTE;
        }

        const auto* val = validator_set.FindValidator(vote.validator_id);
        if (!val) {
            return FinalityVerificationError::UNKNOWN_VALIDATOR;
        }

        std::array<unsigned char, 32> digest_bytes;
        std::copy_n(digest.begin(), 32, digest_bytes.begin());
        if (!VerifyIdentityMessage(val->consensus_public_key, vote.signature, digest_bytes)) {
            return FinalityVerificationError::INVALID_SIGNATURE;
        }
    }

    return FinalityVerificationError::NONE;
}

std::optional<std::vector<unsigned char>> SerializeFinalityCertificate(const BftFinalityCertificate& cert)
{
    if (cert.network_id.IsNull() || cert.block_id.IsNull() || cert.validator_set_commitment.IsNull()) {
        return std::nullopt;
    }
    std::vector<unsigned char> out;
    out.push_back(cert.version);
    out.insert(out.end(), cert.network_id.begin(), cert.network_id.end());
    out.insert(out.end(), cert.block_id.begin(), cert.block_id.end());
    AppendUint64LE(out, cert.height);
    AppendUint32LE(out, cert.round);
    out.insert(out.end(), cert.validator_set_commitment.begin(), cert.validator_set_commitment.end());

    AppendUint32LE(out, static_cast<uint32_t>(cert.commit_votes.size()));
    for (const auto& vote : cert.commit_votes) {
        if (vote.validator_id.IsNull()) {
            return std::nullopt;
        }
        if (vote.signature.ml_dsa.size() != 3309) {
            return std::nullopt;
        }
        if (std::all_of(vote.signature.ed25519.begin(), vote.signature.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
            std::all_of(vote.signature.ml_dsa.begin(), vote.signature.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
            return std::nullopt;
        }
        out.insert(out.end(), vote.validator_id.begin(), vote.validator_id.end());
        out.insert(out.end(), vote.signature.ed25519.begin(), vote.signature.ed25519.end());
        out.insert(out.end(), vote.signature.ml_dsa.begin(), vote.signature.ml_dsa.end());
    }
    return out;
}

std::optional<BftFinalityCertificate> DeserializeFinalityCertificate(std::span<const unsigned char> bytes)
{
    static constexpr size_t HEADER_SIZE{1 + 32 + 32 + 8 + 4 + 32 + 4};
    if (bytes.size() < HEADER_SIZE) return std::nullopt;

    size_t offset{0};
    const uint8_t version = bytes[offset++];
    if (version != BFT_FINALITY_CERTIFICATE_VERSION) return std::nullopt;

    BftFinalityCertificate cert;
    cert.version = version;

    std::copy_n(bytes.begin() + offset, 32, cert.network_id.begin());
    offset += 32;

    std::copy_n(bytes.begin() + offset, 32, cert.block_id.begin());
    offset += 32;

    cert.height = ReadUint64LE(bytes, offset);
    offset += 8;

    cert.round = ReadUint32LE(bytes, offset);
    offset += 4;

    std::copy_n(bytes.begin() + offset, 32, cert.validator_set_commitment.begin());
    offset += 32;

    const uint32_t vote_count = ReadUint32LE(bytes, offset);
    offset += 4;

    if (bytes.size() != HEADER_SIZE + vote_count * BFT_COMMIT_VOTE_SIZE) {
        return std::nullopt;
    }

    cert.commit_votes.reserve(vote_count);
    for (uint32_t i = 0; i < vote_count; ++i) {
        BftCommitVote vote;
        std::copy_n(bytes.begin() + offset, 32, vote.validator_id.begin());
        offset += 32;

        std::copy_n(bytes.begin() + offset, 64, vote.signature.ed25519.begin());
        offset += 64;

        vote.signature.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 3309);
        offset += 3309;

        cert.commit_votes.push_back(std::move(vote));
    }

    return cert;
}

} // namespace cybou
