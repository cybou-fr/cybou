// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <optional>
#include <set>
#include <string>

BOOST_AUTO_TEST_SUITE(cybou_identity_tests)

BOOST_AUTO_TEST_CASE(operator_key_domains_are_distinct)
{
    using cybou::OperatorKeyDomain;
    constexpr std::array domains{
        OperatorKeyDomain::AUTHORITY,
        OperatorKeyDomain::VALIDATOR,
        OperatorKeyDomain::RELEASE_SIGNING,
        OperatorKeyDomain::TREASURY,
    };

    std::set<std::string> tags;
    for (const auto domain : domains) tags.emplace(cybou::KeyDomainTag(domain));
    BOOST_CHECK_EQUAL(tags.size(), domains.size());
}

BOOST_AUTO_TEST_CASE(invite_voucher_requires_authority_and_is_single_use)
{
    cybou::InviteVoucher voucher{
        .voucher_id = uint256::ONE,
        .expiry_epoch = 10,
        .organization_id = std::nullopt,
        .operator_authority_signature = {0x01},
    };

    cybou::InviteVoucherValidationContext context{.current_epoch = 10};
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::INVALID_AUTHORITY_SIGNATURE);

    context.operator_authority_signature_valid = true;
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::NONE);

    context.voucher_already_consumed = true;
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::ALREADY_CONSUMED);
}

BOOST_AUTO_TEST_CASE(invite_voucher_enforces_frozen_grant_and_epoch)
{
    cybou::InviteVoucher voucher{
        .voucher_id = uint256::ONE,
        .grant_amount = cybou::WELCOME_GRANT - 1,
        .expiry_epoch = 4,
        .organization_id = std::nullopt,
        .operator_authority_signature = {0x01},
    };
    const cybou::InviteVoucherValidationContext context{
        .current_epoch = 5,
        .operator_authority_signature_valid = true,
    };

    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::INVALID_GRANT_AMOUNT);
    voucher.grant_amount = cybou::WELCOME_GRANT;
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::EXPIRED);
}

BOOST_AUTO_TEST_CASE(invite_voucher_rejects_unbounded_or_incomplete_envelopes)
{
    cybou::InviteVoucher voucher{
        .voucher_id = {},
        .expiry_epoch = 5,
        .organization_id = std::nullopt,
        .operator_authority_signature = {0x01},
    };
    const cybou::InviteVoucherValidationContext context{
        .current_epoch = 5,
        .operator_authority_signature_valid = true,
    };

    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::NULL_VOUCHER_ID);
    voucher.voucher_id = uint256::ONE;
    voucher.operator_authority_signature.clear();
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::MISSING_AUTHORITY_SIGNATURE);
    voucher.operator_authority_signature.resize(cybou::MAX_INVITE_AUTHORITY_SIGNATURE_SIZE + 1);
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::AUTHORITY_SIGNATURE_TOO_LARGE);
    voucher.operator_authority_signature = {0x01};
    voucher.organization_id = uint256{};
    BOOST_CHECK(cybou::ValidateInviteVoucher(voucher, context) == cybou::InviteVoucherError::NULL_ORGANIZATION_ID);
}

BOOST_AUTO_TEST_SUITE_END()
