// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>

#include <algorithm>
#include <string>
#include <vector>

namespace cybou {
namespace {

const std::string STATE_KEY{"cybou/invite-redemption/state/v1"};
const std::string HASH_KEY{"cybou/invite-redemption/hash/v1"};
const std::string TIP_KEY{"cybou/invite-redemption/tip/v1"};
const std::string UNDO_PREFIX{"cybou/invite-redemption/undo/v1/"};

std::string UndoKey(const uint256& block_id) { return UNDO_PREFIX + block_id.GetHex(); }

std::vector<unsigned char> SerializeUndo(
    const std::optional<uint256>& previous_tip,
    const uint256& before_hash,
    const uint256& after_hash,
    const InviteRedemptionState& before)
{
    std::vector<unsigned char> out{1, static_cast<unsigned char>(previous_tip.has_value())};
    if (previous_tip) out.insert(out.end(), previous_tip->begin(), previous_tip->end());
    out.insert(out.end(), before_hash.begin(), before_hash.end());
    out.insert(out.end(), after_hash.begin(), after_hash.end());
    const auto state_bytes{SerializeInviteRedemptionState(before)};
    out.insert(out.end(), state_bytes.begin(), state_bytes.end());
    return out;
}

struct ParsedUndo {
    std::optional<uint256> previous_tip;
    uint256 before_hash;
    uint256 after_hash;
    InviteRedemptionState before;
};

std::optional<ParsedUndo> ParseUndo(const std::vector<unsigned char>& bytes)
{
    if (bytes.size() < 2 + 2 * uint256::size() || bytes[0] != 1 || bytes[1] > 1) return std::nullopt;
    size_t offset{2};
    const auto read_hash = [&]() -> std::optional<uint256> {
        if (bytes.size() - offset < uint256::size()) return std::nullopt;
        uint256 value;
        std::copy_n(bytes.begin() + offset, uint256::size(), value.begin());
        offset += uint256::size();
        return value;
    };
    std::optional<uint256> previous_tip;
    if (bytes[1] == 1) previous_tip = read_hash();
    const auto before_hash{read_hash()};
    const auto after_hash{read_hash()};
    if ((bytes[1] == 1 && !previous_tip) || !before_hash || !after_hash) return std::nullopt;
    const auto before{DeserializeInviteRedemptionState(std::span{bytes}.subspan(offset))};
    if (!before || InviteRedemptionStateHash(*before) != *before_hash) return std::nullopt;
    return ParsedUndo{previous_tip, *before_hash, *after_hash, *before};
}

} // namespace

void InviteRedemptionStateStore::Write(const InviteRedemptionState& state, const bool sync)
{
    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeInviteRedemptionState(state));
    batch.Write(HASH_KEY, InviteRedemptionStateHash(state));
    m_db.WriteBatch(batch, sync);
}

StateLoadResult InviteRedemptionStateStore::Load() const
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

    auto state{DeserializeInviteRedemptionState(bytes)};
    if (!state || InviteRedemptionStateHash(*state) != stored_hash) {
        return {StateLoadError::CORRUPT, std::nullopt};
    }
    return {StateLoadError::NONE, std::move(state)};
}

InviteRedemptionResult InviteRedemptionStateStore::RedeemAndWrite(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context,
    const OperatorAuthoritySignatureVerifier& verifier,
    InviteRedemptionState& state,
    const bool sync)
{
    auto candidate{state};
    auto result{RedeemInviteVoucher(voucher, context, verifier, candidate)};
    if (!result) return result;
    Write(candidate, sync);
    state = std::move(candidate);
    return result;
}

BlockTransitionResult InviteRedemptionStateStore::ApplyFinalizedRedemption(
    const uint256& block_id,
    const uint256& previous_block_id,
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context,
    const OperatorAuthoritySignatureVerifier& verifier,
    InviteRedemptionState& state,
    const bool sync)
{
    if (block_id.IsNull()) return {BlockTransitionError::INVALID_BLOCK_ID};
    const auto loaded{Load()};
    if (!loaded) return {BlockTransitionError::STATE_NOT_INITIALIZED};
    if (*loaded.state != state) return {BlockTransitionError::STATE_MISMATCH};
    const std::string undo_key{UndoKey(block_id)};
    if (m_db.Exists(undo_key)) return {BlockTransitionError::BLOCK_ALREADY_APPLIED};

    const bool tip_exists{m_db.Exists(TIP_KEY)};
    uint256 tip;
    if ((tip_exists && (!m_db.Read(TIP_KEY, tip) || tip != previous_block_id)) ||
        (!tip_exists && !previous_block_id.IsNull())) {
        return {BlockTransitionError::PARENT_MISMATCH};
    }

    auto candidate{state};
    const auto redemption{RedeemInviteVoucher(voucher, context, verifier, candidate)};
    if (!redemption) return {BlockTransitionError::INVALID_REDEMPTION, redemption};

    std::optional<uint256> previous_tip;
    if (tip_exists) previous_tip = tip;
    const uint256 before_hash{InviteRedemptionStateHash(state)};
    const uint256 after_hash{InviteRedemptionStateHash(candidate)};
    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeInviteRedemptionState(candidate));
    batch.Write(HASH_KEY, after_hash);
    batch.Write(undo_key, SerializeUndo(previous_tip, before_hash, after_hash, state));
    batch.Write(TIP_KEY, block_id);
    m_db.WriteBatch(batch, sync);
    state = std::move(candidate);
    return {};
}

BlockTransitionResult InviteRedemptionStateStore::RollbackFinalizedRedemption(
    const uint256& block_id,
    InviteRedemptionState& state,
    const bool sync)
{
    if (block_id.IsNull()) return {BlockTransitionError::INVALID_BLOCK_ID};
    uint256 tip;
    if (!m_db.Read(TIP_KEY, tip) || tip != block_id) return {BlockTransitionError::NOT_CURRENT_TIP};
    std::vector<unsigned char> undo_bytes;
    const std::string undo_key{UndoKey(block_id)};
    if (!m_db.Read(undo_key, undo_bytes)) return {BlockTransitionError::MISSING_OR_CORRUPT_UNDO};
    const auto undo{ParseUndo(undo_bytes)};
    if (!undo) return {BlockTransitionError::MISSING_OR_CORRUPT_UNDO};
    const auto loaded{Load()};
    if (!loaded || *loaded.state != state || InviteRedemptionStateHash(state) != undo->after_hash) {
        return {BlockTransitionError::STATE_MISMATCH};
    }

    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeInviteRedemptionState(undo->before));
    batch.Write(HASH_KEY, undo->before_hash);
    batch.Erase(undo_key);
    if (undo->previous_tip) batch.Write(TIP_KEY, *undo->previous_tip);
    else batch.Erase(TIP_KEY);
    m_db.WriteBatch(batch, sync);
    state = undo->before;
    return {};
}

} // namespace cybou
