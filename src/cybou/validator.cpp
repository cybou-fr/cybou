// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/validator.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <set>
#include <string_view>

namespace cybou {

size_t ValidatorSet::TotalWeight() const
{
    size_t sum{0};
    for (const auto& val : validators) {
        sum += val.weight;
    }
    return sum;
}

const Validator* ValidatorSet::FindValidator(const uint256& id) const
{
    for (const auto& val : validators) {
        if (val.validator_id == id) return &val;
    }
    return nullptr;
}

const Validator* ValidatorSet::FindValidatorByPublicKey(const IdentityHybridPublicKey& pubkey) const
{
    for (const auto& val : validators) {
        if (val.consensus_public_key == pubkey) return &val;
    }
    return nullptr;
}

ValidatorSetValidationError ValidateValidatorSet(const ValidatorSet& val_set)
{
    if (val_set.version != VALIDATOR_SET_VERSION) {
        return ValidatorSetValidationError::UNSUPPORTED_VERSION;
    }
    if (val_set.validators.size() < MIN_VALIDATORS) {
        return ValidatorSetValidationError::INVALID_VALIDATOR_COUNT;
    }
    std::set<uint256> seen_ids;
    std::set<std::array<unsigned char, 32>> seen_ed25519;
    std::set<std::vector<unsigned char>> seen_ml_dsa;
    for (const auto& val : val_set.validators) {
        if (val.weight != 1) {
            return ValidatorSetValidationError::INVALID_WEIGHT;
        }
        if (val.validator_id.IsNull()) {
            return ValidatorSetValidationError::NULL_VALIDATOR_ID;
        }
        if (val.consensus_public_key.purpose != IdentityKeyPurpose::VALIDATOR ||
            val.consensus_public_key.ml_dsa.size() != 1952 ||
            std::all_of(val.consensus_public_key.ed25519.begin(), val.consensus_public_key.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
            std::all_of(val.consensus_public_key.ml_dsa.begin(), val.consensus_public_key.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
            return ValidatorSetValidationError::NULL_CONSENSUS_KEY;
        }
        const auto expected_id = ComputeValidatorKeyId(val.consensus_public_key);
        if (!expected_id || !std::equal(expected_id->begin(), expected_id->end(), val.validator_id.begin())) {
            return ValidatorSetValidationError::INVALID_VALIDATOR_ID;
        }
        if (!seen_ids.insert(val.validator_id).second) {
            return ValidatorSetValidationError::DUPLICATE_VALIDATOR_ID;
        }
        if (!seen_ed25519.insert(val.consensus_public_key.ed25519).second) {
            return ValidatorSetValidationError::DUPLICATE_CONSENSUS_KEY;
        }
        if (!seen_ml_dsa.insert(val.consensus_public_key.ml_dsa).second) {
            return ValidatorSetValidationError::DUPLICATE_CONSENSUS_KEY;
        }
    }
    return ValidatorSetValidationError::NONE;
}

std::vector<unsigned char> SerializeValidatorSet(const ValidatorSet& val_set)
{
    std::vector<unsigned char> out;
    out.push_back(val_set.version);
    const uint32_t count = static_cast<uint32_t>(val_set.validators.size());
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(count >> (8 * i)));
    for (const auto& val : val_set.validators) {
        out.insert(out.end(), val.validator_id.begin(), val.validator_id.end());
        out.insert(out.end(), val.consensus_public_key.ed25519.begin(), val.consensus_public_key.ed25519.end());
        out.insert(out.end(), val.consensus_public_key.ml_dsa.begin(), val.consensus_public_key.ml_dsa.end());
        for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(val.weight >> (8 * i)));
    }
    return out;
}

std::optional<ValidatorSet> DeserializeValidatorSet(const std::span<const unsigned char> bytes)
{
    if (bytes.size() < 1 + 4) return std::nullopt;
    if (bytes[0] != VALIDATOR_SET_VERSION) return std::nullopt;
    uint32_t count{0};
    for (int i = 0; i < 4; ++i) count |= uint32_t{bytes[1 + i]} << (8 * i);
    if (bytes.size() != 5 + count * VALIDATOR_ENTRY_SIZE) return std::nullopt;

    ValidatorSet set{.version = bytes[0], .validators{}};
    set.validators.reserve(count);
    size_t offset{5};
    for (uint32_t i = 0; i < count; ++i) {
        uint256 id;
        std::copy_n(bytes.begin() + offset, 32, id.begin());
        offset += 32;

        IdentityHybridPublicKey pubkey{.purpose = IdentityKeyPurpose::VALIDATOR, .ed25519 = {}, .ml_dsa = {}};
        std::copy_n(bytes.begin() + offset, 32, pubkey.ed25519.begin());
        offset += 32;
        pubkey.ml_dsa.assign(bytes.begin() + offset, bytes.begin() + offset + 1952);
        offset += 1952;

        uint32_t weight{0};
        for (int j = 0; j < 4; ++j) weight |= uint32_t{bytes[offset + j]} << (8 * j);
        offset += 4;

        if (std::all_of(pubkey.ed25519.begin(), pubkey.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
            std::all_of(pubkey.ml_dsa.begin(), pubkey.ml_dsa.end(), [](unsigned char b) { return b == 0; })) {
            return std::nullopt;
        }

        set.validators.push_back(Validator{
            .validator_id = id,
            .consensus_public_key = std::move(pubkey),
            .weight = weight,
        });
    }
    return set;
}

uint256 ComputeValidatorSetCommitment(const ValidatorSet& val_set)
{
    static constexpr std::string_view DOMAIN{"CYBOU/VALIDATOR_SET/V2"};
    const auto bytes{SerializeValidatorSet(val_set)};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    uint256 commitment;
    hasher.Finalize(commitment.begin());
    return commitment;
}

} // namespace cybou
