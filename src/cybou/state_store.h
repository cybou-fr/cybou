// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_STORE_H
#define CYBOU_STATE_STORE_H

#include <cybou/state.h>

#include <cstdint>
#include <optional>

class CDBWrapper;

namespace cybou {

enum class StateLoadError : uint8_t {
    NONE,
    NOT_FOUND,
    CORRUPT,
};

struct StateLoadResult {
    StateLoadError error{StateLoadError::NONE};
    std::optional<InviteRedemptionState> state;

    explicit operator bool() const { return error == StateLoadError::NONE && state.has_value(); }
};

enum class BlockTransitionError : uint8_t {
    NONE,
    INVALID_BLOCK_ID,
    STATE_NOT_INITIALIZED,
    STATE_MISMATCH,
    PARENT_MISMATCH,
    BLOCK_ALREADY_APPLIED,
    INVALID_REDEMPTION,
    NOT_CURRENT_TIP,
    MISSING_OR_CORRUPT_UNDO,
};

struct BlockTransitionResult {
    BlockTransitionError error{BlockTransitionError::NONE};
    InviteRedemptionResult redemption{};

    explicit operator bool() const { return error == BlockTransitionError::NONE; }
};

/** Atomic LevelDB snapshot persistence for the canonical redemption state. */
class InviteRedemptionStateStore
{
public:
    explicit InviteRedemptionStateStore(CDBWrapper& db) : m_db{db} {}

    void Write(const InviteRedemptionState& state, bool sync = true);
    StateLoadResult Load() const;
    InviteRedemptionResult RedeemAndWrite(
        const InviteVoucher& voucher,
        const InviteVoucherValidationContext& context,
        const OperatorAuthoritySignatureVerifier& verifier,
        InviteRedemptionState& state,
        bool sync = true);
    BlockTransitionResult ApplyFinalizedRedemption(
        const uint256& block_id,
        const uint256& previous_block_id,
        const InviteVoucher& voucher,
        const InviteVoucherValidationContext& context,
        const OperatorAuthoritySignatureVerifier& verifier,
        InviteRedemptionState& state,
        bool sync = true);
    BlockTransitionResult RollbackFinalizedRedemption(
        const uint256& block_id,
        InviteRedemptionState& state,
        bool sync = true);

private:
    CDBWrapper& m_db;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
