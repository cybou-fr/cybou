// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_definition.h>

#include <crypto/sha256.h>

#include <string_view>

namespace cybou {

NetworkDefinitionError ValidateNetworkDefinition(const CybouNetworkDefinitionV1& definition)
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
    return NetworkDefinitionError::NONE;
}

std::vector<unsigned char> SerializeNetworkDefinition(const CybouNetworkDefinitionV1& definition)
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
    append_hash(definition.initial_validator_set_commitment);
    return out;
}

uint256 NetworkId(const CybouNetworkDefinitionV1& definition)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NETWORK-DEFINITION/V1"};
    const auto bytes{SerializeNetworkDefinition(definition)};
    uint256 result;
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(result.begin());
    return result;
}

} // namespace cybou
