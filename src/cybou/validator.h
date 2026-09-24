// Copyright (c) 2026 Stanislav Saveliev
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

inline constexpr uint8_t VALIDATOR_SET_VERSION{1};
inline constexpr uint8_t VALIDATOR_SET_VERSION_V2{2};
inline constexpr size_t MIN_VALIDATORS{1};
inline constexpr size_t VALIDATOR_V2_ENTRY_SIZE{32 + 32 + 1952 + 4}; // 2020 bytes

// Backwards-compatibility alias
inline constexpr size_t BFT_MIN_VALIDATORS{MIN_VALIDATORS};

enum class ConsensusMode : uint8_t {
    AUTHORITY = 1,    // N = 1 (1/1 quorum, f = 0)
    INTEGRATION = 2,  // N = 2, 3 (2/2 or 3/3 quorum, f = 0)
    BFT = 3,          // N >= 4 (floor(2N/3) + 1, f >= 1)
};

struct ValidatorV1 {
    uint256 validator_id;
    uint256 consensus_public_key;
    uint32_t weight{1};

    friend bool operator==(const ValidatorV1&, const ValidatorV1&) = default;
};

struct ValidatorSetV1 {
    uint8_t version{VALIDATOR_SET_VERSION};
    std::vector<ValidatorV1> validators;

    friend bool operator==(const ValidatorSetV1&, const ValidatorSetV1&) = default;

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

    const ValidatorV1* FindValidator(const uint256& id) const;
    const ValidatorV1* FindValidatorByPublicKey(const uint256& pubkey) const;
};

// Post-Quantum Validator representation (Ed25519 + ML-DSA-65)
struct ValidatorV2 {
    uint256 validator_id;
    IdentityHybridPublicKey consensus_public_key;
    uint32_t weight{1};

    friend bool operator==(const ValidatorV2&, const ValidatorV2&) = default;
};

struct ValidatorSetV2 {
    uint8_t version{VALIDATOR_SET_VERSION_V2};
    std::vector<ValidatorV2> validators;

    friend bool operator==(const ValidatorSetV2&, const ValidatorSetV2&) = default;

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

    const ValidatorV2* FindValidator(const uint256& id) const;
    const ValidatorV2* FindValidatorByPublicKey(const IdentityHybridPublicKey& pubkey) const;
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

ValidatorSetValidationError ValidateValidatorSet(const ValidatorSetV1& val_set);
std::vector<unsigned char> SerializeValidatorSet(const ValidatorSetV1& val_set);
std::optional<ValidatorSetV1> DeserializeValidatorSet(std::span<const unsigned char> bytes);
uint256 ComputeValidatorSetCommitment(const ValidatorSetV1& val_set);

ValidatorSetValidationError ValidateValidatorSetV2(const ValidatorSetV2& val_set);
std::vector<unsigned char> SerializeValidatorSetV2(const ValidatorSetV2& val_set);
std::optional<ValidatorSetV2> DeserializeValidatorSetV2(std::span<const unsigned char> bytes);
uint256 ComputeValidatorSetCommitmentV2(const ValidatorSetV2& val_set);

// Unversioned aliases
using Validator = ValidatorV2;
using ValidatorSet = ValidatorSetV2;

} // namespace cybou

#endif // CYBOU_VALIDATOR_H
