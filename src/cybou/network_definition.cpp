// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_definition.h>

#include <crypto/sha256.h>

#include <algorithm>
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

std::optional<CybouNetworkDefinitionV1> DeserializeNetworkDefinition(const std::span<const unsigned char> bytes)
{
    size_t pos{0};
    const auto read_u8 = [&]() -> std::optional<uint8_t> {
        if (pos >= bytes.size()) return std::nullopt;
        return bytes[pos++];
    };
    const auto read_u32 = [&]() -> std::optional<uint32_t> {
        if (bytes.size() - pos < 4) return std::nullopt;
        uint32_t value{0};
        for (unsigned i = 0; i < 4; ++i) value |= uint32_t{bytes[pos++]} << (8 * i);
        return value;
    };
    const auto read_u64 = [&]() -> std::optional<uint64_t> {
        if (bytes.size() - pos < 8) return std::nullopt;
        uint64_t value{0};
        for (unsigned i = 0; i < 8; ++i) value |= uint64_t{bytes[pos++]} << (8 * i);
        return value;
    };
    const auto read_hash = [&]() -> std::optional<uint256> {
        if (bytes.size() - pos < 32) return std::nullopt;
        uint256 value;
        std::copy_n(bytes.begin() + pos, 32, value.begin());
        pos += 32;
        return value;
    };

    CybouNetworkDefinitionV1 result;
    const auto version = read_u8();
    const auto genesis_id = read_hash();
    const auto genesis_root = read_hash();
    const auto work_bits = read_u32();
    const auto epoch_lag = read_u64();
    const auto max_creates = read_u32();
    const auto bonus = read_u64();
    const auto epoch_blocks = read_u64();
    const auto payment_fee = read_u64();
    const auto mail_base_fee = read_u64();
    const auto mail_tier_bytes = read_u64();
    const auto mail_tier_fee = read_u64();
    const auto max_mail_size = read_u32();
    const auto mail_limit = read_u32();
    const auto set_commitment = read_hash();
    const auto has_authority = read_u8();
    if (!version || !genesis_id || !genesis_root || !work_bits || !epoch_lag ||
        !max_creates || !bonus || !epoch_blocks || !payment_fee || !mail_base_fee ||
        !mail_tier_bytes || !mail_tier_fee || !max_mail_size || !mail_limit ||
        !set_commitment || !has_authority || *has_authority > 1) return std::nullopt;
    result.protocol_version = *version;
    result.genesis_block_id = *genesis_id;
    result.genesis_state_root = *genesis_root;
    result.protocol_parameters = {
        .account_creation_work_bits = *work_bits,
        .account_creation_epoch_lag = *epoch_lag,
        .max_account_creates_per_block = *max_creates,
        .onboarding_bonus = *bonus,
        .epoch_blocks = *epoch_blocks,
        .payment_fee = *payment_fee,
        .mail_base_fee = *mail_base_fee,
        .mail_tier_bytes = *mail_tier_bytes,
        .mail_tier_fee = *mail_tier_fee,
        .max_mail_ciphertext_size = *max_mail_size,
        .new_account_mail_limit_per_epoch = *mail_limit,
    };
    result.initial_validator_set_commitment = *set_commitment;
    if (*has_authority) {
        OperatorAuthorityKeySet authority;
        const auto keyset_id = read_hash();
        if (!keyset_id || bytes.size() - pos < authority.ed25519_public_key.size() + authority.mldsa65_public_key.size()) {
            return std::nullopt;
        }
        authority.keyset_id = *keyset_id;
        std::copy_n(bytes.begin() + pos, authority.ed25519_public_key.size(), authority.ed25519_public_key.begin());
        pos += authority.ed25519_public_key.size();
        std::copy_n(bytes.begin() + pos, authority.mldsa65_public_key.size(), authority.mldsa65_public_key.begin());
        pos += authority.mldsa65_public_key.size();
        const auto active_from = read_u64();
        const auto has_retirement = read_u8();
        if (!active_from || !has_retirement || *has_retirement > 1) return std::nullopt;
        authority.active_from_epoch = *active_from;
        if (*has_retirement) {
            authority.retired_from_epoch = read_u64();
            if (!authority.retired_from_epoch) return std::nullopt;
        }
        result.operator_authority = authority;
    }
    if (pos != bytes.size() || ValidateNetworkDefinition(result) != NetworkDefinitionError::NONE) return std::nullopt;
    return result;
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
