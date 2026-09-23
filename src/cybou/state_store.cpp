// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>

#include <string>
#include <vector>

namespace cybou {
namespace {

const std::string STATE_KEY{"cybou/invite-redemption/state/v1"};
const std::string HASH_KEY{"cybou/invite-redemption/hash/v1"};

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

} // namespace cybou
