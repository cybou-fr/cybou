// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_VALIDATOR_H
#define CYBOU_VALIDATOR_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr uint8_t VALIDATOR_SET_VERSION{1};
inline constexpr size_t BFT_STAGE1_VALIDATOR_COUNT{4};
inline constexpr size_t BFT_STAGE1_QUORUM{3};
inline constexpr size_t BFT_STAGE1_FAULT_TOLERANCE{1};

// Backwards-compatibility alias
inline constexpr size_t BFT_MIN_VALIDATORS{BFT_STAGE1_VALIDATOR_COUNT};

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
    size_t FaultTolerance() const { return BFT_STAGE1_FAULT_TOLERANCE; }
    size_t QuorumThreshold() const { return BFT_STAGE1_QUORUM; }

    const ValidatorV1* FindValidator(const uint256& id) const;
    const ValidatorV1* FindValidatorByPublicKey(const uint256& pubkey) const;
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
};

ValidatorSetValidationError ValidateValidatorSet(const ValidatorSetV1& val_set);
std::vector<unsigned char> SerializeValidatorSet(const ValidatorSetV1& val_set);
std::optional<ValidatorSetV1> DeserializeValidatorSet(std::span<const unsigned char> bytes);
uint256 ComputeValidatorSetCommitment(const ValidatorSetV1& val_set);

} // namespace cybou

#endif // CYBOU_VALIDATOR_H
