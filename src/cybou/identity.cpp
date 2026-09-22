// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>

#include <cassert>

namespace cybou {

std::string_view KeyDomainTag(const OperatorKeyDomain domain)
{
    switch (domain) {
    case OperatorKeyDomain::AUTHORITY:
        return "CYBOU/OPERATOR-AUTHORITY/V1";
    case OperatorKeyDomain::VALIDATOR:
        return "CYBOU/OPERATOR-VALIDATOR/V1";
    case OperatorKeyDomain::RELEASE_SIGNING:
        return "CYBOU/RELEASE-SIGNING/V1";
    case OperatorKeyDomain::TREASURY:
        return "CYBOU/TREASURY/V1";
    }
    assert(false);
    return {};
}

InviteVoucherError ValidateInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context)
{
    if (voucher.voucher_id.IsNull()) return InviteVoucherError::NULL_VOUCHER_ID;
    if (voucher.grant_amount != WELCOME_GRANT) return InviteVoucherError::INVALID_GRANT_AMOUNT;
    if (voucher.expiry_epoch < context.current_epoch) return InviteVoucherError::EXPIRED;
    if (voucher.organization_id && voucher.organization_id->IsNull()) return InviteVoucherError::NULL_ORGANIZATION_ID;
    if (voucher.operator_authority_signature.empty()) return InviteVoucherError::MISSING_AUTHORITY_SIGNATURE;
    if (voucher.operator_authority_signature.size() > MAX_INVITE_AUTHORITY_SIGNATURE_SIZE) {
        return InviteVoucherError::AUTHORITY_SIGNATURE_TOO_LARGE;
    }
    if (!context.operator_authority_signature_valid) return InviteVoucherError::INVALID_AUTHORITY_SIGNATURE;
    if (context.voucher_already_consumed) return InviteVoucherError::ALREADY_CONSUMED;
    return InviteVoucherError::NONE;
}

} // namespace cybou
