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

std::vector<unsigned char> SerializeInviteVoucherPayload(const InviteVoucherPayloadV1& payload)
{
    std::vector<unsigned char> out;
    out.reserve(INVITE_VOUCHER_PAYLOAD_BASE_SIZE + (payload.organization_id ? uint256::size() : 0));
    const auto append_hash = [&out](const uint256& hash) {
        out.insert(out.end(), hash.data(), hash.data() + uint256::size());
    };
    const auto append_u64le = [&out](const uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };
    append_hash(payload.network_id);
    append_hash(payload.voucher_id);
    append_hash(payload.beneficiary_account_id);
    append_u64le(payload.grant_amount);
    append_u64le(payload.expiry_epoch);
    out.push_back(payload.organization_id ? 0x01 : 0x00);
    if (payload.organization_id) append_hash(*payload.organization_id);
    return out;
}

std::vector<unsigned char> InviteVoucherSigMessage(const InviteVoucherPayloadV1& payload)
{
    const std::string_view tag{ObjectSigningDomainTag(ObjectSigningDomain::INVITE_VOUCHER)};
    std::vector<unsigned char> message{tag.begin(), tag.end()};
    const auto payload_bytes{SerializeInviteVoucherPayload(payload)};
    message.insert(message.end(), payload_bytes.begin(), payload_bytes.end());
    return message;
}

InviteVoucherError ValidateInviteVoucher(
    const InviteVoucher& voucher,
    const InviteVoucherValidationContext& context)
{
    const auto& payload{voucher.payload};
    if (payload.voucher_id.IsNull()) return InviteVoucherError::NULL_VOUCHER_ID;
    if (payload.beneficiary_account_id.IsNull()) return InviteVoucherError::NULL_BENEFICIARY;
    if (payload.network_id != context.expected_network_id) return InviteVoucherError::NETWORK_MISMATCH;
    if (payload.beneficiary_account_id != context.redeemer_account_id) return InviteVoucherError::BENEFICIARY_MISMATCH;
    if (payload.grant_amount != WELCOME_GRANT) return InviteVoucherError::INVALID_GRANT_AMOUNT;
    if (payload.expiry_epoch < context.current_epoch) return InviteVoucherError::EXPIRED;
    if (payload.organization_id && payload.organization_id->IsNull()) return InviteVoucherError::NULL_ORGANIZATION_ID;
    if (!IsPresent(voucher.signature)) return InviteVoucherError::MISSING_AUTHORITY_SIGNATURE;
    if (!context.operator_authority_signature_valid) return InviteVoucherError::INVALID_AUTHORITY_SIGNATURE;
    if (context.voucher_already_consumed) return InviteVoucherError::ALREADY_CONSUMED;
    return InviteVoucherError::NONE;
}

} // namespace cybou
