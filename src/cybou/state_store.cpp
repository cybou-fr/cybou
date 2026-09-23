// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>

#include <string>
#include <vector>

namespace cybou {
namespace {

const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};
const std::string TIP_KEY{"cybou/tip/v1"};

} // namespace

void CybouStateStore::Write(const CybouState& state, const bool sync)
{
    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(state));
    batch.Write(HASH_KEY, CybouStateHash(state));
    m_db.WriteBatch(batch, sync);
}

GenesisInitResult CybouStateStore::InitializeGenesis(const CybouState& genesis_state, const bool sync)
{
    if (m_db.Exists(STATE_KEY) || m_db.Exists(HASH_KEY)) {
        return {GenesisInitError::ALREADY_INITIALIZED};
    }
    Write(genesis_state, sync);
    return {};
}

StateLoadResult CybouStateStore::LoadState() const
{
    std::vector<unsigned char> bytes;
    uint256 stored_hash;
    const bool state_exists{m_db.Exists(STATE_KEY)};
    const bool hash_exists{m_db.Exists(HASH_KEY)};
    if (!state_exists && !hash_exists) return {StateLoadError::NOT_FOUND, std::nullopt};
    if (!state_exists || !hash_exists) return {StateLoadError::CORRUPT, std::nullopt};
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

BlockTransitionResult CybouStateStore::CommitFinalizedBlock(
    const uint256& block_id,
    const uint256& previous_block_id,
    const std::vector<AccountCreateOpV1>& ops,
    const uint256& network_id,
    const uint64_t block_height,
    const CybouProtocolParameters& params,
    const bool sync)
{
    if (block_id.IsNull()) return {BlockTransitionError::INVALID_BLOCK_ID};
    if (ops.size() > params.max_account_creates_per_block) {
        return {BlockTransitionError::TOO_MANY_ACCOUNT_CREATES};
    }
    const auto loaded{LoadState()};
    if (!loaded) return {BlockTransitionError::STATE_NOT_INITIALIZED};

    const auto tip{GetFinalizedTip()};
    if (tip == block_id) return {BlockTransitionError::BLOCK_ALREADY_APPLIED};
    if ((tip.has_value() && *tip != previous_block_id) ||
        (!tip.has_value() && !previous_block_id.IsNull())) {
        return {BlockTransitionError::PARENT_MISMATCH};
    }

    auto candidate{*loaded.state};
    for (const auto& op : ops) {
        const auto res{ApplyAccountCreate(op, network_id, block_height, params, candidate)};
        if (!res) return {BlockTransitionError::INVALID_OPERATION, res};
    }

    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(candidate));
    batch.Write(HASH_KEY, CybouStateHash(candidate));
    batch.Write(TIP_KEY, block_id);
    m_db.WriteBatch(batch, sync);
    return {};
}

} // namespace cybou
