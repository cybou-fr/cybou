// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>

#include <limits>

namespace cybou {

InviteRedemptionResult RedeemInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context,
    const OperatorAuthoritySignatureVerifier& verifier,
    InviteRedemptionState& state)
{
    auto validation_context{context};
    validation_context.voucher_already_consumed = state.consumed_voucher_ids.contains(voucher.payload.voucher_id);
    const auto voucher_error{ValidateInviteVoucher(voucher, validation_context, verifier)};
    if (voucher_error != InviteVoucherError::NONE) {
        return {InviteRedemptionError::INVALID_VOUCHER, voucher_error};
    }

    const auto account_it{state.accounts.find(voucher.payload.beneficiary_account_id)};
    if (account_it == state.accounts.end()) return {InviteRedemptionError::ACCOUNT_NOT_FOUND};
    if (state.onboarding_pool < WELCOME_GRANT) return {InviteRedemptionError::INSUFFICIENT_ONBOARDING_POOL};
    if (account_it->second.system_balance > std::numeric_limits<uint64_t>::max() - WELCOME_GRANT) {
        return {InviteRedemptionError::SYSTEM_BALANCE_OVERFLOW};
    }

    // No mutation occurs before every failure path above has completed.
    state.onboarding_pool -= WELCOME_GRANT;
    account_it->second.system_balance += WELCOME_GRANT;
    state.consumed_voucher_ids.insert(voucher.payload.voucher_id);
    return {};
}

} // namespace cybou
