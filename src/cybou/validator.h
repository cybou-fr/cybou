// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_VALIDATOR_H
#define CYBOU_VALIDATOR_H

#include <cybou/identity_crypto.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t VALIDATOR_SET_VERSION{2};
inline constexpr size_t MIN_VALIDATORS{1};
inline constexpr size_t VALIDATOR_ENTRY_SIZE{32 + 32 + 1952 + 4}; // 2020 bytes
inline constexpr size_t BFT_MIN_VALIDATORS{MIN_VALIDATORS};

enum class ConsensusMode : uint8_t {
    AUTHORITY = 1,    // N = 1 (1/1 quorum, f = 0)
    INTEGRATION = 2,  // N = 2, 3 (2/2 or 3/3 quorum, f = 0)
    BFT = 3,          // N >= 4 (floor(2N/3) + 1, f >= 1)
};

using ValidatorSignature = IdentityHybridSignature;

struct ValidatorHybridKeyPair {
    IdentityHybridPublicKey public_key;
    std::array<unsigned char, 32> seed{};
};

inline std::optional<ValidatorHybridKeyPair> GenerateValidatorKeyPair(std::span<const unsigned char, 32> seed)
{
    const auto pub = DeriveIdentityPublicKey(seed, IdentityKeyPurpose::VALIDATOR);
    if (!pub) return std::nullopt;
    ValidatorHybridKeyPair pair;
    pair.public_key = *pub;
    std::copy(seed.begin(), seed.end(), pair.seed.begin());
    return pair;
}

inline std::optional<ValidatorHybridKeyPair> GenerateValidatorKeyPair(const std::array<unsigned char, 32>& seed)
{
    return GenerateValidatorKeyPair(std::span<const unsigned char, 32>(seed));
}

inline uint256 ComputeValidatorId(const IdentityHybridPublicKey& pubkey)
{
    const auto id = ComputeValidatorKeyId(pubkey);
    if (!id) return uint256{};
    uint256 val_id;
    std::copy_n(id->begin(), 32, val_id.begin());
    return val_id;
}

inline bool VerifyValidatorSignature(
    const IdentityHybridPublicKey& pubkey,
    const IdentityHybridSignature& signature,
    const uint256& digest)
{
    return VerifyIdentityMessage(pubkey, signature,
        std::span<const unsigned char>(digest.begin(), digest.size()));
}

// Post-Quantum Validator representation (Ed25519 + ML-DSA-65)
struct Validator {
    uint256 validator_id;
    IdentityHybridPublicKey consensus_public_key;
    uint32_t weight{1};

    friend bool operator==(const Validator&, const Validator&) = default;
};

struct ValidatorSet {
    uint8_t version{VALIDATOR_SET_VERSION};
    std::vector<Validator> validators;

    friend bool operator==(const ValidatorSet&, const ValidatorSet&) = default;

    size_t Size() const { return validators.size(); }
    size_t TotalWeight() const;
    size_t FaultTolerance() const { return validators.empty() ? 0 : (validators.size() - 1) / 3; }
    size_t QuorumThreshold() const { return (2 * validators.size()) / 3 + 1; }

    ConsensusMode Mode() const
    {
        if (validators.size() == 1) return ConsensusMode::AUTHORITY;
        if (validators.size() <= 3) return ConsensusMode::INTEGRATION;
        return ConsensusMode::BFT;
    }

    const Validator* FindValidator(const uint256& id) const;
    const Validator* FindValidatorByPublicKey(const IdentityHybridPublicKey& pubkey) const;
};

enum class ValidatorSetValidationError : uint8_t {
    NONE,
    UNSUPPORTED_VERSION,
    INVALID_VALIDATOR_COUNT,
    INVALID_WEIGHT,
    NULL_VALIDATOR_ID,
    NULL_CONSENSUS_KEY,
    DUPLICATE_VALIDATOR_ID,
    DUPLICATE_CONSENSUS_KEY,
    INVALID_VALIDATOR_ID,
};

ValidatorSetValidationError ValidateValidatorSet(const ValidatorSet& val_set);
std::vector<unsigned char> SerializeValidatorSet(const ValidatorSet& val_set);
std::optional<ValidatorSet> DeserializeValidatorSet(std::span<const unsigned char> bytes);
uint256 ComputeValidatorSetCommitment(const ValidatorSet& val_set);

} // namespace cybou

#endif // CYBOU_VALIDATOR_H
