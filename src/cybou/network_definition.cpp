// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_definition.h>
#include <cybou/network_genesis.h>
#include <cybou/state.h>
#include <cybou/root_publication.h>

#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <limits>
#include <string_view>

namespace cybou {

bool ValidateProtocolParameters(const CybouProtocolParameters& params)
{
    if (params.account_creation_work_bits > uint256::size() * 8) {
        return false;
    }
    if (params.max_account_creates_per_block == 0) {
        return false;
    }
    if (params.epoch_blocks == 0) {
        return false;
    }
    const auto max_fee_kib = (ROOT_PUBLICATION_MAX_OPERATION_BYTES + 1023) / 1024;
    const auto per_kib = params.root_publication_fee_per_started_kib;
    const auto per_chunk = params.root_publication_fee_per_chunk;
    if ((per_kib != 0 && max_fee_kib > std::numeric_limits<uint64_t>::max() / per_kib) ||
        (per_chunk != 0 && MAX_PUBLICATION_CHUNKS > std::numeric_limits<uint64_t>::max() / per_chunk)) {
        return false;
    }
    const auto max_byte_fee = static_cast<uint64_t>(max_fee_kib) * per_kib;
    const auto max_chunk_fee = static_cast<uint64_t>(MAX_PUBLICATION_CHUNKS) * per_chunk;
    if (max_chunk_fee > std::numeric_limits<uint64_t>::max() - max_byte_fee) {
        return false;
    }
    if (params.name_claim_work_bits > uint256::size() * 8 ||
        params.name_commit_min_depth == 0 ||
        params.name_commit_max_lifetime < params.name_commit_min_depth ||
        params.max_pending_name_commits == 0 ||
        params.max_pending_name_commits > DEFAULT_MAX_PENDING_NAME_COMMITS) {
        return false;
    }
    return true;
}

NetworkDefinitionError ValidateNetworkDefinition(const CybouNetworkDefinition& definition)
{
    if (definition.network_public_key.purpose != IdentityKeyPurpose::NETWORK_ROOT ||
        definition.network_public_key.ml_dsa.size() != MLDSA65_PUBLIC_KEY_SIZE ||
        std::all_of(definition.network_public_key.ed25519.begin(), definition.network_public_key.ed25519.end(),
            [](unsigned char b) { return b == 0; })) {
        return NetworkDefinitionError::INVALID_NETWORK_KEY;
    }
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
        (per_chunk != 0 && MAX_PUBLICATION_CHUNKS > std::numeric_limits<uint64_t>::max() / per_chunk)) {
        return NetworkDefinitionError::INVALID_ROOT_PUBLICATION_FEES;
    }
    const auto max_byte_fee = static_cast<uint64_t>(max_fee_kib) * per_kib;
    const auto max_chunk_fee = static_cast<uint64_t>(MAX_PUBLICATION_CHUNKS) * per_chunk;
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

uint256 ComputeNetworkBinding(const IdentityHybridPublicKey& network_public_key)
{
    static constexpr std::string_view DOMAIN{"CYBOU/NETWORK-ID/V6"};
    const auto key = CanonicalSerializeNetworkPublicKey(network_public_key);
    uint256 result;
    ::cybou::crypto::Sha256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(key.data(), key.size());
    hasher.Finalize(result.begin());
    return result;
}

CybouState CreateDevGenesisState()
{
    return CybouState{
        .onboarding_pool = 10'000'000,
        .security_reward_pool = 0,
        .pending_fee_pool = 0,
        .accounts = {},
        .identities = {},
        .names = {},
    };
}

uint256 ComputeGenesisBlockId(const uint256& state_root, const IdentityHybridPublicKey& poa_finalizer_public_key)
{
    static constexpr std::string_view DOMAIN{"CYBOU/GENESIS-BLOCK/V4"};
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
    const IdentityHybridPublicKey& poa_finalizer_public_key,
    const IdentityHybridPublicKey& network_public_key)
{
    const auto state_root_opt = CybouStateHash(genesis);
    const uint256 state_root = state_root_opt.value_or(uint256{});
    return CybouNetworkDefinition{
        .protocol_version = CYBOU_NETWORK_DEFINITION_VERSION,
        .network_public_key = network_public_key,
        .genesis_block_id = ComputeGenesisBlockId(state_root, poa_finalizer_public_key),
        .genesis_state_root = state_root,
        .poa_finalizer_public_key = poa_finalizer_public_key,
        .protocol_parameters = DevProtocolParameters(),
    };
}

} // namespace cybou
