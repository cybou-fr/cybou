// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_H
#define CYBOU_STATE_H

#include <cybou/identity.h>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace cybou {

struct AccountBalanceState {
    uint64_t balance{0};
    uint64_t system_balance{0};

    friend bool operator==(const AccountBalanceState&, const AccountBalanceState&) = default;
};

/** Minimal validation-relevant state for atomic Invite Voucher redemption. */
struct InviteRedemptionState {
    uint64_t onboarding_pool{0};
    std::map<uint256, AccountBalanceState> accounts;
    std::set<uint256> consumed_voucher_ids;

    friend bool operator==(const InviteRedemptionState&, const InviteRedemptionState&) = default;
};

inline constexpr uint8_t INVITE_REDEMPTION_STATE_VERSION{1};
inline constexpr uint32_t MAX_SERIALIZED_ACCOUNTS{1'000'000};
inline constexpr uint32_t MAX_SERIALIZED_CONSUMED_VOUCHERS{1'000'000};

std::vector<unsigned char> SerializeInviteRedemptionState(const InviteRedemptionState& state);
std::optional<InviteRedemptionState> DeserializeInviteRedemptionState(std::span<const unsigned char> bytes);
uint256 InviteRedemptionStateHash(const InviteRedemptionState& state);

enum class InviteRedemptionError : uint8_t {
    NONE,
    INVALID_VOUCHER,
    ACCOUNT_NOT_FOUND,
    INSUFFICIENT_ONBOARDING_POOL,
    SYSTEM_BALANCE_OVERFLOW,
};

struct InviteRedemptionResult {
    InviteRedemptionError error{InviteRedemptionError::NONE};
    InviteVoucherError voucher_error{InviteVoucherError::NONE};

    explicit operator bool() const { return error == InviteRedemptionError::NONE; }
};

/** Validate first, then atomically move the grant and consume the voucher id. */
InviteRedemptionResult RedeemInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context,
    const OperatorAuthoritySignatureVerifier& verifier,
    InviteRedemptionState& state);

} // namespace cybou

#endif // CYBOU_STATE_H
