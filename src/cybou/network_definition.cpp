// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_definition.h>
#include <cybou/state.h>
#include <cybou/validator.h>
#include <cybou/root_publication.h>

#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <string_view>

namespace cybou {

NetworkDefinitionError ValidateNetworkDefinition(const CybouNetworkDefinition& definition)
{
    if (definition.protocol_version != CYBOU_NETWORK_DEFINITION_VERSION) {
        return NetworkDefinitionError::UNSUPPORTED_VERSION;
    }
    if (definition.genesis_block_id.IsNull()) return NetworkDefinitionError::NULL_GENESIS_BLOCK_ID;
    if (definition.genesis_state_root.IsNull()) return NetworkDefinitionError::NULL_GENESIS_STATE_ROOT;
    const auto& poa_key = definition.poa_finalizer_public_key;
    if (poa_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        poa_key.ml_dsa.size() != MLDSA65_PUBLIC_KEY_SIZE ||
        std::all_of(poa_key.ed25519.begin(), poa_key.ed25519.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(poa_key.ml_dsa.begin(), poa_key.ml_dsa.end(), [](unsigned char b) { return b == 0; }) ||
        !ComputePoaFinalizerKeyId(poa_key)) {
        return NetworkDefinitionError::INVALID_POA_FINALIZER_KEY;
    }
    if (ComputeGenesisBlockId(definition.genesis_state_root, poa_key) != definition.genesis_block_id) {
        return NetworkDefinitionError::GENESIS_BLOCK_ID_MISMATCH;
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
    const auto max_fee_kib = (ROOT_PUBLICATION_MAX_OPERATION_BYTES + 1023) / 1024;
    const auto per_kib = definition.protocol_parameters.root_publication_fee_per_started_kib;
    const auto per_chunk = definition.protocol_parameters.root_publication_fee_per_chunk;
    if ((per_kib != 0 && max_fee_kib > std::numeric_limits<uint64_t>::max() / per_kib) ||
        (per_chunk != 0 && ROOT_PUBLICATION_MAX_CHUNKS > std::numeric_limits<uint64_t>::max() / per_chunk)) {
        return NetworkDefinitionError::INVALID_ROOT_PUBLICATION_FEES;
    }
    const auto max_byte_fee = static_cast<uint64_t>(max_fee_kib) * per_kib;
    const auto max_chunk_fee = static_cast<uint64_t>(ROOT_PUBLICATION_MAX_CHUNKS) * per_chunk;
    if (max_chunk_fee > std::numeric_limits<uint64_t>::max() - max_byte_fee) {
        return NetworkDefinitionError::INVALID_ROOT_PUBLICATION_FEES;
    }
    if (definition.protocol_parameters.name_claim_work_bits > uint256::size() * 8 ||
        definition.protocol_parameters.name_commit_min_depth == 0 ||
        definition.protocol_parameters.name_commit_max_lifetime < definition.protocol_parameters.name_commit_min_depth ||
        definition.protocol_parameters.max_pending_name_commits == 0 ||
        definition.protocol_parameters.max_pending_name_commits > DEFAULT_MAX_PENDING_NAME_COMMITS) {
        return NetworkDefinitionError::INVALID_NAME_PARAMETERS;
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
    out.insert(out.end(), definition.poa_finalizer_public_key.ed25519.begin(),
        definition.poa_finalizer_public_key.ed25519.end());
    out.insert(out.end(), definition.poa_finalizer_public_key.ml_dsa.begin(),
        definition.poa_finalizer_public_key.ml_dsa.end());
    append_u32le(static_cast<uint32_t>(definition.protocol_parameters.account_creation_work_bits));
    append_u64le(definition.protocol_parameters.account_creation_epoch_lag);
    append_u32le(definition.protocol_parameters.max_account_creates_per_block);
    append_u64le(definition.protocol_parameters.onboarding_bonus);
    append_u64le(definition.protocol_parameters.epoch_blocks);
    append_u64le(definition.protocol_parameters.payment_fee);
    append_u64le(definition.protocol_parameters.root_publication_fee_per_started_kib);
    append_u64le(definition.protocol_parameters.root_publication_fee_per_chunk);
    append_u32le(definition.protocol_parameters.name_claim_work_bits);
    append_u64le(definition.protocol_parameters.name_commit_min_depth);
    append_u64le(definition.protocol_parameters.name_commit_max_lifetime);
    append_u32le(definition.protocol_parameters.max_pending_name_commits);
    out.push_back(definition.protocol_parameters.identity_kem_xwing_enabled ? 1 : 0);
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
    if (pos + ED25519_PUBLIC_KEY_SIZE + MLDSA65_PUBLIC_KEY_SIZE > bytes.size()) return std::nullopt;
    definition.poa_finalizer_public_key.purpose = IdentityKeyPurpose::POA_FINALIZER;
    std::copy_n(bytes.begin() + pos, ED25519_PUBLIC_KEY_SIZE,
        definition.poa_finalizer_public_key.ed25519.begin());
    pos += ED25519_PUBLIC_KEY_SIZE;
    definition.poa_finalizer_public_key.ml_dsa.assign(
        bytes.begin() + pos, bytes.begin() + pos + MLDSA65_PUBLIC_KEY_SIZE);
    pos += MLDSA65_PUBLIC_KEY_SIZE;
    const auto work_bits = read_u32le();
    const auto epoch_lag = read_u64le();
    const auto max_creates = read_u32le();
    const auto onboarding_bonus = read_u64le();
    const auto epoch_blocks = read_u64le();
    const auto payment_fee = read_u64le();
    const auto root_publication_fee_per_kib = read_u64le();
    const auto root_publication_fee_per_chunk = read_u64le();
    const auto name_work_bits = read_u32le();
    const auto name_min_depth = read_u64le();
    const auto name_max_lifetime = read_u64le();
    const auto max_pending_names = read_u32le();
    const auto kem_enabled = read_u8();
    if (!genesis_block_id || !genesis_state_root || !work_bits || !epoch_lag || !max_creates ||
        !onboarding_bonus || !epoch_blocks || !payment_fee || !root_publication_fee_per_kib ||
        !root_publication_fee_per_chunk || !name_work_bits || !name_min_depth ||
        !name_max_lifetime || !max_pending_names || !kem_enabled || *kem_enabled > 1) {
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
    definition.protocol_parameters.root_publication_fee_per_started_kib = *root_publication_fee_per_kib;
    definition.protocol_parameters.root_publication_fee_per_chunk = *root_publication_fee_per_chunk;
    definition.protocol_parameters.name_claim_work_bits = *name_work_bits;
    definition.protocol_parameters.name_commit_min_depth = *name_min_depth;
    definition.protocol_parameters.name_commit_max_lifetime = *name_max_lifetime;
    definition.protocol_parameters.max_pending_name_commits = *max_pending_names;
    definition.protocol_parameters.identity_kem_xwing_enabled = *kem_enabled == 1;
    if (pos != bytes.size() || ValidateNetworkDefinition(definition) != NetworkDefinitionError::NONE) return std::nullopt;
    return definition;
}

uint256 NetworkId(const CybouNetworkDefinition& definition)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NETWORK-ID/V4"};
    const auto bytes = SerializeNetworkDefinition(definition);
    uint256 result;
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(bytes.data(), bytes.size());
    hasher.Finalize(result.begin());
    return result;
}

std::optional<CybouNetworkFile> LoadCybouNetworkFile(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size < 12 || size > 16 * 1024 * 1024) return std::nullopt;
    std::vector<unsigned char> bytes(size);
    std::ifstream file(path, std::ios::binary);
    if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), bytes.size()) ||
        !std::equal(bytes.begin(), bytes.begin() + 4, "CYN1")) return std::nullopt;
    const auto read_u32 = [&bytes](size_t offset) {
        uint32_t value{0};
        for (unsigned i{0}; i < 4; ++i) value |= uint32_t{bytes[offset + i]} << (8 * i);
        return value;
    };
    const auto definition_size = read_u32(4);
    if (definition_size > bytes.size() - 12) return std::nullopt;
    const size_t state_offset = 8 + definition_size;
    const auto state_size = read_u32(state_offset);
    if (state_size != bytes.size() - state_offset - 4) return std::nullopt;
    const auto definition = DeserializeNetworkDefinition(
        std::span<const unsigned char>{bytes.data() + 8, definition_size});
    const auto genesis = DeserializeCybouState(
        std::span<const unsigned char>{bytes.data() + state_offset + 4, state_size});
    if (!definition || !genesis || CybouStateHash(*genesis) != definition->genesis_state_root ||
        ComputeGenesisBlockId(definition->genesis_state_root, definition->poa_finalizer_public_key) != definition->genesis_block_id) {
        return std::nullopt;
    }
    return CybouNetworkFile{*definition, *genesis};
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

uint256 ComputeGenesisBlockId(const uint256& state_root, const IdentityHybridPublicKey& poa_finalizer_public_key)
{
    static constexpr std::string_view DOMAIN{"CYBOU/GENESIS-BLOCK/V3"};
    const auto key_id = ComputePoaFinalizerKeyId(poa_finalizer_public_key);
    if (!key_id) return {};
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(state_root.begin(), state_root.size());
    hasher.Write(key_id->data(), key_id->size());
    uint256 out;
    hasher.Finalize(out.begin());
    return out;
}

CybouNetworkDefinition CreateDevNetworkDefinition(
    const CybouState& genesis,
    const IdentityHybridPublicKey& poa_finalizer_public_key)
{
    const auto state_root_opt = CybouStateHash(genesis);
    const uint256 state_root = state_root_opt.value_or(uint256{});
    return CybouNetworkDefinition{
        .protocol_version = CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = ComputeGenesisBlockId(state_root, poa_finalizer_public_key),
        .genesis_state_root = state_root,
        .poa_finalizer_public_key = poa_finalizer_public_key,
        .protocol_parameters = DevProtocolParameters(),
    };
}

} // namespace cybou
