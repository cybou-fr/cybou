// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_TEST_HELPERS_H
#define CYBOU_TEST_HELPERS_H

#include <cybou/identity_crypto.h>
#include <cybou/network_definition.h>
#include <cybou/validator.h>

#include <algorithm>
#include <array>
#include <optional>
#include <span>

namespace cybou {

inline IdentityHybridPublicKey TestPoaFinalizerPublicKey(unsigned char seed_byte = 0xA7)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    return DeriveIdentityPublicKey(seed, IdentityKeyPurpose::POA_FINALIZER).value();
}

inline std::optional<CybouState> CreateTestGenesisState(
    std::span<const IdentityHybridPublicKey> validator_public_keys)
{
    if (validator_public_keys.empty()) return std::nullopt;
    CybouState genesis = CreateDevGenesisState(validator_public_keys.front());
    auto& validators = genesis.validator_set.validators;
    for (size_t i = 1; i < validator_public_keys.size(); ++i) {
        validators.push_back(Validator{.validator_id = ComputeValidatorId(validator_public_keys[i]),
            .consensus_public_key = validator_public_keys[i], .weight = 1});
    }
    std::sort(validators.begin(), validators.end(), [](const Validator& left, const Validator& right) {
        return left.validator_id < right.validator_id;
    });
    if (ValidateValidatorSet(genesis.validator_set) != ValidatorSetValidationError::NONE) return std::nullopt;
    return genesis;
}

} // namespace cybou

#endif
