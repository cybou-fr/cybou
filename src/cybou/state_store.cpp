// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>

#include <limits>
#include <string>
#include <vector>

namespace cybou {
namespace {

const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};
const std::string TIP_KEY{"cybou/tip/v1"};
const std::string HEIGHT_KEY{"cybou/height/v1"};
const std::string NETWORK_ID_KEY{"cybou/network-id/v1"};

} // namespace

GenesisInitResult CybouStateStore::InitializeGenesis(
    const CybouState& genesis_state,
    const bool sync,
    const uint64_t genesis_height)
{
    if (m_network_definition_error != NetworkDefinitionError::NONE) {
        return {GenesisInitError::INVALID_NETWORK_DEFINITION};
    }
    if (m_db.Exists(STATE_KEY) || m_db.Exists(HASH_KEY) || m_db.Exists(TIP_KEY) ||
        m_db.Exists(HEIGHT_KEY) || m_db.Exists(NETWORK_ID_KEY)) {
        return {GenesisInitError::ALREADY_INITIALIZED};
    }
    if (CybouStateHash(genesis_state) != m_network_definition.genesis_state_root) {
        return {GenesisInitError::GENESIS_STATE_MISMATCH};
    }
    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(genesis_state));
    batch.Write(HASH_KEY, CybouStateHash(genesis_state));
    batch.Write(HEIGHT_KEY, genesis_height);
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
    const bool tip_exists{m_db.Exists(TIP_KEY)};
    const bool height_exists{m_db.Exists(HEIGHT_KEY)};
    const bool network_exists{m_db.Exists(NETWORK_ID_KEY)};
    if (!state_exists && !hash_exists && !tip_exists && !height_exists && !network_exists) {
        return {StateLoadError::NOT_FOUND, std::nullopt};
    }
    if (!state_exists || !hash_exists || !height_exists || !network_exists) {
        return {StateLoadError::CORRUPT, std::nullopt};
    }
    const auto stored_network_id{GetStoredNetworkId()};
    if (!stored_network_id) return {StateLoadError::CORRUPT, std::nullopt};
    if (*stored_network_id != m_network_id) return {StateLoadError::NETWORK_MISMATCH, std::nullopt};
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

std::optional<uint256> CybouStateStore::GetFinalizedTip() const
{
    uint256 tip;
    if (!m_db.Read(TIP_KEY, tip)) return std::nullopt;
    return tip;
}

std::optional<uint64_t> CybouStateStore::GetFinalizedHeight() const
{
    uint64_t height;
    if (!m_db.Read(HEIGHT_KEY, height)) return std::nullopt;
    return height;
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
    const std::vector<AccountCreateOpV1>& ops,
    const uint64_t block_height,
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
    if (ops.size() > params.max_account_creates_per_block) {
        return {BlockTransitionError::TOO_MANY_ACCOUNT_CREATES};
    }

    const auto tip{GetFinalizedTip()};
    const auto finalized_height{GetFinalizedHeight()};
    if (!finalized_height) return {BlockTransitionError::CORRUPT_HEAD};
    if (tip == block_id) return {BlockTransitionError::BLOCK_ALREADY_APPLIED};
    if ((tip.has_value() && *tip != previous_block_id) ||
        (!tip.has_value() && !previous_block_id.IsNull())) {
        return {BlockTransitionError::PARENT_MISMATCH};
    }
    if (*finalized_height == std::numeric_limits<uint64_t>::max() || block_height != *finalized_height + 1) {
        return {BlockTransitionError::INVALID_HEIGHT};
    }

    auto candidate{*loaded.state};
    for (const auto& op : ops) {
        const auto res{ApplyAccountCreate(op, m_network_id, block_height, params, candidate)};
        if (!res) return {BlockTransitionError::INVALID_OPERATION, res};
    }

    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(candidate));
    batch.Write(HASH_KEY, CybouStateHash(candidate));
    batch.Write(TIP_KEY, block_id);
    batch.Write(HEIGHT_KEY, block_height);
    m_db.WriteBatch(batch, sync);
    return {};
}

} // namespace cybou
