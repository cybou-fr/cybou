// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_H
#define CYBOU_IDENTITY_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cybou {

/** Security domains whose keys must never be interchangeable. */
enum class OperatorKeyDomain : uint8_t {
    AUTHORITY,
    VALIDATOR,
    RELEASE_SIGNING,
    TREASURY,
};

std::string_view KeyDomainTag(OperatorKeyDomain domain);

inline constexpr uint64_t WELCOME_GRANT{6000};

// Development envelope bound. The signature suite and final consensus encoding
// remain gated on the protocol-format and crypto-suite decisions.
inline constexpr size_t MAX_INVITE_AUTHORITY_SIGNATURE_SIZE{8192};

struct InviteVoucher {
    uint256 voucher_id;
    uint64_t grant_amount{WELCOME_GRANT};
    uint64_t expiry_epoch{0};
    std::optional<uint256> organization_id;
    std::vector<unsigned char> operator_authority_signature;
};

struct InviteVoucherValidationContext {
    uint64_t current_epoch{0};
    bool operator_authority_signature_valid{false};
    bool voucher_already_consumed{false};
};

enum class InviteVoucherError : uint8_t {
    NONE,
    NULL_VOUCHER_ID,
    INVALID_GRANT_AMOUNT,
    EXPIRED,
    NULL_ORGANIZATION_ID,
    MISSING_AUTHORITY_SIGNATURE,
    AUTHORITY_SIGNATURE_TOO_LARGE,
    INVALID_AUTHORITY_SIGNATURE,
    ALREADY_CONSUMED,
};

/**
 * Validate the development Invite Voucher envelope.
 *
 * The caller must provide the result of cryptographic verification under the
 * Operator Authority domain. Structural validity alone can never authorize a
 * Welcome Grant.
 */
InviteVoucherError ValidateInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context);

} // namespace cybou

#endif // CYBOU_IDENTITY_H
