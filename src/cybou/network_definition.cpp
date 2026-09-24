// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_definition.h>
#include <cybou/state.h>
#include <cybou/validator.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <string_view>

namespace cybou {

NetworkDefinitionError ValidateNetworkDefinition(const CybouNetworkDefinition& definition)
{
    if (definition.protocol_version != CYBOU_NETWORK_DEFINITION_VERSION) {
        return NetworkDefinitionError::UNSUPPORTED_VERSION;
    }
    if (definition.genesis_block_id.IsNull()) return NetworkDefinitionError::NULL_GENESIS_BLOCK_ID;
    if (definition.genesis_state_root.IsNull()) return NetworkDefinitionError::NULL_GENESIS_STATE_ROOT;
    if (definition.initial_validator_set_commitment.IsNull()) {
        return NetworkDefinitionError::NULL_VALIDATOR_SET_COMMITMENT;
    }
    if (definition.protocol_parameters.account_creation_work_bits > uint256::size() * 8) {
        return NetworkDefinitionError::INVALID_ACCOUNT_CREATION_WORK_BITS;
    }
    if (definition.protocol_parameters.max_account_creates_per_block == 0) {
        return NetworkDefinitionError::ZERO_MAX_ACCOUNT_CREATES_PER_BLOCK;
    }
    if (definition.protocol_parameters.epoch_blocks == 0) {
        return NetworkDefinitionError::ZERO_EPOCH_BLOCKS;
    }
    if (definition.operator_authority) {
        const auto& authority = *definition.operator_authority;
        if (authority.keyset_id.IsNull()) {
            return NetworkDefinitionError::NULL_OPERATOR_AUTHORITY_KEYSET_ID;
        }
        if (std::all_of(authority.ed25519_public_key.begin(), authority.ed25519_public_key.end(), [](unsigned char b) { return b == 0; })) {
            return NetworkDefinitionError::NULL_OPERATOR_AUTHORITY_KEY;
        }
        if (std::all_of(authority.mldsa65_public_key.begin(), authority.mldsa65_public_key.end(), [](unsigned char b) { return b == 0; })) {
            return NetworkDefinitionError::NULL_OPERATOR_AUTHORITY_KEY;
        }
        if (authority.active_from_epoch != 0 || authority.retired_from_epoch.has_value()) {
            return NetworkDefinitionError::INVALID_OPERATOR_AUTHORITY_EPOCH;
        }
    }
    return NetworkDefinitionError::NONE;
}

std::vector<unsigned char> SerializeNetworkDefinition(const CybouNetworkDefinition& definition)
{
    std::vector<unsigned char> out;
    const auto append_u32le = [&out](const uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };
    const auto append_u64le = [&out](const uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };
    const auto append_hash = [&out](const uint256& value) {
        out.insert(out.end(), value.begin(), value.end());
    };

    out.push_back(definition.protocol_version);
    append_hash(definition.genesis_block_id);
    append_hash(definition.genesis_state_root);
    append_u32le(static_cast<uint32_t>(definition.protocol_parameters.account_creation_work_bits));
    append_u64le(definition.protocol_parameters.account_creation_epoch_lag);
    append_u32le(definition.protocol_parameters.max_account_creates_per_block);
    append_u64le(definition.protocol_parameters.onboarding_bonus);
    append_u64le(definition.protocol_parameters.epoch_blocks);
    append_u64le(definition.protocol_parameters.payment_fee);
    append_u64le(definition.protocol_parameters.mail_base_fee);
    append_u64le(definition.protocol_parameters.mail_tier_bytes);
    append_u64le(definition.protocol_parameters.mail_tier_fee);
    append_u32le(definition.protocol_parameters.max_mail_ciphertext_size);
    append_u32le(definition.protocol_parameters.new_account_mail_limit_per_epoch);
    append_hash(definition.initial_validator_set_commitment);
    out.push_back(definition.operator_authority.has_value() ? 1 : 0);
    if (definition.operator_authority) {
        const auto& authority = *definition.operator_authority;
        append_hash(authority.keyset_id);
        out.insert(out.end(), authority.ed25519_public_key.begin(), authority.ed25519_public_key.end());
        out.insert(out.end(), authority.mldsa65_public_key.begin(), authority.mldsa65_public_key.end());
        append_u64le(authority.active_from_epoch);
        out.push_back(authority.retired_from_epoch.has_value() ? 1 : 0);
        if (authority.retired_from_epoch) append_u64le(*authority.retired_from_epoch);
    }
    return out;
}

std::optional<CybouNetworkDefinition> DeserializeNetworkDefinition(const std::span<const unsigned char> bytes)
{
    size_t pos{0};
    const auto read_u8 = [&]() -> std::optional<uint8_t> {
        if (pos >= bytes.size()) return std::nullopt;
        return bytes[pos++];
    };
    const auto read_u32le = [&]() -> std::optional<uint32_t> {
        if (pos + 4 > bytes.size()) return std::nullopt;
        uint32_t value{0};
        for (unsigned i = 0; i < 4; ++i) {
            value |= static_cast<uint32_t>(bytes[pos + i]) << (8 * i);
        }
        pos += 4;
        return value;
    };
    const auto read_u64le = [&]() -> std::optional<uint64_t> {
        if (pos + 8 > bytes.size()) return std::nullopt;
        uint64_t value{0};
        for (unsigned i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(bytes[pos + i]) << (8 * i);
        }
        pos += 8;
        return value;
    };
    const auto read_hash = [&]() -> std::optional<uint256> {
        if (pos + uint256::size() > bytes.size()) return std::nullopt;
        uint256 value;
        std::copy_n(bytes.begin() + pos, uint256::size(), value.begin());
        pos += uint256::size();
        return value;
    };

    const auto version = read_u8();
    if (!version || *version != CYBOU_NETWORK_DEFINITION_VERSION) return std::nullopt;

    CybouNetworkDefinition definition;
    definition.protocol_version = *version;

    const auto genesis_block_id = read_hash();
    const auto genesis_state_root = read_hash();
    const auto work_bits = read_u32le();
    const auto epoch_lag = read_u64le();
    const auto max_creates = read_u32le();
    const auto onboarding_bonus = read_u64le();
    const auto epoch_blocks = read_u64le();
    const auto payment_fee = read_u64le();
    const auto mail_base_fee = read_u64le();
    const auto mail_tier_bytes = read_u64le();
    const auto mail_tier_fee = read_u64le();
    const auto max_mail_size = read_u32le();
    const auto mail_limit = read_u32le();
    const auto validator_commitment = read_hash();
    const auto has_operator = read_u8();

    if (!genesis_block_id || !genesis_state_root || !work_bits || !epoch_lag || !max_creates ||
        !onboarding_bonus || !epoch_blocks || !payment_fee || !mail_base_fee || !mail_tier_bytes ||
        !mail_tier_fee || !max_mail_size || !mail_limit || !validator_commitment || !has_operator) {
        return std::nullopt;
    }

    definition.genesis_block_id = *genesis_block_id;
    definition.genesis_state_root = *genesis_state_root;
    definition.protocol_parameters.account_creation_work_bits = *work_bits;
    definition.protocol_parameters.account_creation_epoch_lag = *epoch_lag;
    definition.protocol_parameters.max_account_creates_per_block = *max_creates;
    definition.protocol_parameters.onboarding_bonus = *onboarding_bonus;
    definition.protocol_parameters.epoch_blocks = *epoch_blocks;
    definition.protocol_parameters.payment_fee = *payment_fee;
    definition.protocol_parameters.mail_base_fee = *mail_base_fee;
    definition.protocol_parameters.mail_tier_bytes = *mail_tier_bytes;
    definition.protocol_parameters.mail_tier_fee = *mail_tier_fee;
    definition.protocol_parameters.max_mail_ciphertext_size = *max_mail_size;
    definition.protocol_parameters.new_account_mail_limit_per_epoch = *mail_limit;
    definition.initial_validator_set_commitment = *validator_commitment;

    if (*has_operator == 1) {
        const auto keyset_id = read_hash();
        if (!keyset_id) return std::nullopt;

        OperatorAuthorityKeySet authority;
        authority.keyset_id = *keyset_id;
        if (pos + authority.ed25519_public_key.size() + authority.mldsa65_public_key.size() > bytes.size()) {
            return std::nullopt;
        }
        std::copy_n(bytes.begin() + pos, authority.ed25519_public_key.size(), authority.ed25519_public_key.begin());
        pos += authority.ed25519_public_key.size();
        std::copy_n(bytes.begin() + pos, authority.mldsa65_public_key.size(), authority.mldsa65_public_key.begin());
        pos += authority.mldsa65_public_key.size();

        const auto active_from = read_u64le();
        const auto has_retired = read_u8();
        if (!active_from || !has_retired) return std::nullopt;

        authority.active_from_epoch = *active_from;
        if (*has_retired == 1) {
            const auto retired_from = read_u64le();
            if (!retired_from) return std::nullopt;
            authority.retired_from_epoch = *retired_from;
        } else if (*has_retired != 0) {
            return std::nullopt;
        }

        definition.operator_authority = authority;
    } else if (*has_operator != 0) {
        return std::nullopt;
    }

    if (pos != bytes.size()) return std::nullopt;
    return definition;
}

uint256 NetworkId(const CybouNetworkDefinition& definition)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NETWORK-ID/V2"};
    const auto bytes = SerializeNetworkDefinition(definition);
    uint256 result;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(result.begin());
    return result;
}

IdentityHybridPublicKey CreateDevValidatorKey(const uint256& seed)
{
    IdentityHybridPublicKey key;
    key.purpose = IdentityKeyPurpose::VALIDATOR;
    std::copy_n(seed.begin(), 32, key.ed25519.begin());
    if (std::all_of(key.ed25519.begin(), key.ed25519.end(), [](unsigned char b) { return b == 0; })) {
        key.ed25519[0] = 1;
    }
    key.ml_dsa.resize(1952);
    // Expand deterministically with SHA256
    uint256 block = seed;
    size_t written{0};
    while (written < key.ml_dsa.size()) {
        CSHA256 hasher;
        hasher.Write(block.begin(), 32);
        hasher.Finalize(block.begin());
        size_t to_copy = std::min(size_t{32}, key.ml_dsa.size() - written);
        std::copy_n(block.begin(), to_copy, key.ml_dsa.begin() + written);
        written += to_copy;
    }
    return key;
}

CybouState CreateDevGenesisState(const IdentityHybridPublicKey& validator_public_key)
{
    const auto val_id = ComputeValidatorId(validator_public_key);
    return CybouState{
        .onboarding_pool = 10'000'000,
        .security_reward_pool = 0,
        .pending_fee_pool = 0,
        .accounts = {},
        .identities = {},
        .validator_set = {
            .version = VALIDATOR_SET_VERSION,
            .validators = {
                Validator{
                    .validator_id = val_id,
                    .consensus_public_key = validator_public_key,
                    .weight = 1,
                },
            },
        },
        .names = {},
    };
}

CybouState CreateDevGenesisState(const uint256& validator_seed_or_key)
{
    return CreateDevGenesisState(CreateDevValidatorKey(validator_seed_or_key));
}

uint256 ComputeGenesisBlockId(const uint256& state_root, const uint256& validator_set_commitment)
{
    static constexpr std::string_view DOMAIN{"CYBOU/GENESIS-BLOCK/V2"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(state_root.begin(), state_root.size());
    hasher.Write(validator_set_commitment.begin(), validator_set_commitment.size());
    uint256 out;
    hasher.Finalize(out.begin());
    return out;
}

CybouNetworkDefinition CreateDevNetworkDefinition(const CybouState& genesis)
{
    const auto state_root_opt = CybouStateHash(genesis);
    const uint256 state_root = state_root_opt.value_or(uint256{});
    const uint256 val_commitment = ComputeValidatorSetCommitment(genesis.validator_set);
    return CybouNetworkDefinition{
        .protocol_version = CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = ComputeGenesisBlockId(state_root, val_commitment),
        .genesis_state_root = state_root,
        .protocol_parameters = DevProtocolParameters(),
        .initial_validator_set_commitment = val_commitment,
        .operator_authority = std::nullopt,
    };
}

} // namespace cybou
