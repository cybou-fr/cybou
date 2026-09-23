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

private:
    CDBWrapper& m_db;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
