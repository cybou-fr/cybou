// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace cybou {
namespace {

const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};
const std::string HEAD_KEY{"cybou/head/v1"};
const std::string NETWORK_ID_KEY{"cybou/network-id/v1"};

} // namespace

GenesisInitResult CybouStateStore::InitializeGenesis(
    const CybouState& genesis_state,
    const bool sync)
{
    if (m_network_definition_error != NetworkDefinitionError::NONE) {
        return {GenesisInitError::INVALID_NETWORK_DEFINITION};
    }
    if (m_db.Exists(STATE_KEY) || m_db.Exists(HASH_KEY) || m_db.Exists(HEAD_KEY) ||
        m_db.Exists(NETWORK_ID_KEY)) {
        return {GenesisInitError::ALREADY_INITIALIZED};
    }
    if (CybouStateHash(genesis_state) != m_network_definition.genesis_state_root) {
        return {GenesisInitError::GENESIS_STATE_MISMATCH};
    }
    const FinalizedHeadV1 initial_head{
        .block_id = m_network_definition.genesis_block_id,
        .height = 0,
    };
    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(genesis_state));
    batch.Write(HASH_KEY, CybouStateHash(genesis_state));
    batch.Write(HEAD_KEY, initial_head);
    batch.Write(NETWORK_ID_KEY, m_network_id);
    m_db.WriteBatch(batch, sync);
    return {};
}

StateLoadResult CybouStateStore::LoadState() const
{
    if (m_network_definition_error != NetworkDefinitionError::NONE) {
        return {StateLoadError::INVALID_NETWORK_DEFINITION, std::nullopt};
    }
    std::vector<unsigned char> bytes;
    uint256 stored_hash;
    const bool state_exists{m_db.Exists(STATE_KEY)};
    const bool hash_exists{m_db.Exists(HASH_KEY)};
    const bool head_exists{m_db.Exists(HEAD_KEY)};
    const bool network_exists{m_db.Exists(NETWORK_ID_KEY)};
    if (!state_exists && !hash_exists && !head_exists && !network_exists) {
        return {StateLoadError::NOT_FOUND, std::nullopt};
    }
    if (!state_exists || !hash_exists || !head_exists || !network_exists) {
        return {StateLoadError::CORRUPT, std::nullopt};
    }
    const auto stored_network_id{GetStoredNetworkId()};
    if (!stored_network_id) return {StateLoadError::CORRUPT, std::nullopt};
    if (*stored_network_id != m_network_id) return {StateLoadError::NETWORK_MISMATCH, std::nullopt};

    const auto head{GetFinalizedHead()};
    if (!head) return {StateLoadError::CORRUPT, std::nullopt};

    const bool has_state{m_db.Read(STATE_KEY, bytes)};
    const bool has_hash{m_db.Read(HASH_KEY, stored_hash)};
    if (!has_state || !has_hash) return {StateLoadError::CORRUPT, std::nullopt};

    auto state{DeserializeCybouState(bytes)};
    if (!state || CybouStateHash(*state) != stored_hash) {
        return {StateLoadError::CORRUPT, std::nullopt};
    }
    return {StateLoadError::NONE, std::move(state)};
}

std::optional<uint256> CybouStateStore::GetStateRoot() const
{
    uint256 hash;
    if (!m_db.Read(HASH_KEY, hash)) return std::nullopt;
    return hash;
}

std::optional<FinalizedHeadV1> CybouStateStore::GetFinalizedHead() const
{
    FinalizedHeadV1 head;
    if (!m_db.Read(HEAD_KEY, head)) return std::nullopt;
    return head;
}

std::optional<uint256> CybouStateStore::GetFinalizedTip() const
{
    const auto head{GetFinalizedHead()};
    if (!head) return std::nullopt;
    return head->block_id;
}

std::optional<uint64_t> CybouStateStore::GetFinalizedHeight() const
{
    const auto head{GetFinalizedHead()};
    if (!head) return std::nullopt;
    return head->height;
}

std::optional<uint256> CybouStateStore::GetStoredNetworkId() const
{
    uint256 network_id;
    if (!m_db.Read(NETWORK_ID_KEY, network_id)) return std::nullopt;
    return network_id;
}

BlockTransitionResult CybouStateStore::CommitFinalizedBlock(
    const uint256& block_id,
    const uint256& previous_block_id,
    const std::vector<ProtocolOperationV1>& ops,
    const bool sync)
{
    const auto loaded{LoadState()};
    if (!loaded) {
        if (loaded.error == StateLoadError::INVALID_NETWORK_DEFINITION) {
            return {BlockTransitionError::INVALID_NETWORK_DEFINITION};
        }
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            return {BlockTransitionError::NETWORK_MISMATCH};
        }
        if (loaded.error == StateLoadError::CORRUPT) {
            return {BlockTransitionError::CORRUPT_STATE};
        }
        return {BlockTransitionError::STATE_NOT_INITIALIZED};
    }
    const auto& params{m_network_definition.protocol_parameters};
    if (block_id.IsNull()) return {BlockTransitionError::INVALID_BLOCK_ID};
    const size_t account_create_count{static_cast<size_t>(std::count_if(
        ops.begin(), ops.end(), [](const auto& operation) {
            return OperationType(operation) == ProtocolOperationType::ACCOUNT_CREATE;
        }))};
    if (account_create_count > params.max_account_creates_per_block) {
        return {BlockTransitionError::TOO_MANY_ACCOUNT_CREATES};
    }

    const auto head{GetFinalizedHead()};
    if (!head) return {BlockTransitionError::CORRUPT_HEAD};
    if (head->block_id == block_id) return {BlockTransitionError::BLOCK_ALREADY_APPLIED};
    if (head->block_id != previous_block_id) {
        return {BlockTransitionError::PARENT_MISMATCH};
    }
    if (head->height == std::numeric_limits<uint64_t>::max()) {
        return {BlockTransitionError::INVALID_HEIGHT};
    }
    const uint64_t next_height{head->height + 1};

    auto candidate{*loaded.state};
    const ProtocolExecutionContextV1 ctx{
        .network_id = m_network_id,
        .block_height = next_height,
        .params = params,
    };
    for (const auto& operation : ops) {
        const auto res{ApplyProtocolOperation(operation, ctx, candidate)};
        if (!res) return {BlockTransitionError::INVALID_OPERATION, res};
    }

    const FinalizedHeadV1 next_head{
        .block_id = block_id,
        .height = next_height,
    };

    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(candidate));
    batch.Write(HASH_KEY, CybouStateHash(candidate));
    batch.Write(HEAD_KEY, next_head);
    m_db.WriteBatch(batch, sync);
    return {};
}

} // namespace cybou
