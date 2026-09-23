// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>
#include <cybou/signing.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <optional>
#include <set>
#include <string>

BOOST_AUTO_TEST_SUITE(cybou_identity_tests)

namespace {

const uint256 NETWORK_ID{uint256::ONE};
const cybou::AccountId BENEFICIARY{uint256::FromUserHex("0a").value()};
const cybou::AccountId OTHER_ACCOUNT{uint256::FromUserHex("0b").value()};

const cybou::OperatorAuthorityKeySet& AuthorityKeySet()
{
    static const cybou::OperatorAuthorityKeySet keyset{
        .keyset_id = uint256::FromUserHex("04").value(),
        .active_from_epoch = 1,
        .retired_from_epoch = 20,
    };
    return keyset;
}

class TestAuthorityVerifier final : public cybou::OperatorAuthoritySignatureVerifier
{
public:
    explicit TestAuthorityVerifier(bool result) : m_result{result} {}

    bool Verify(
        const cybou::OperatorAuthorityKeySet&,
        const cybou::SignatureBundleV1&,
        std::span<const unsigned char>) const override
    {
        return m_result;
    }

private:
    bool m_result;
};

const TestAuthorityVerifier ACCEPT_SIGNATURE{true};
const TestAuthorityVerifier REJECT_SIGNATURE{false};

cybou::InviteVoucher ValidVoucher()
{
    cybou::InviteVoucher voucher{
        .payload{
            .network_id = NETWORK_ID,
            .voucher_id = uint256::FromUserHex("02").value(),
            .beneficiary_account_id = BENEFICIARY,
            .expiry_epoch = 10,
            .organization_id = std::nullopt,
        },
        .signature{},
    };
    voucher.signature.authority_keyset_id = AuthorityKeySet().keyset_id;
    voucher.signature.classical_signature[0] = 0x01;
    voucher.signature.pq_signature[0] = 0x01;
    return voucher;
}

cybou::InviteVoucherValidationContext ValidContext()
{
    return {
        .expected_network_id = NETWORK_ID,
        .redeemer_account_id = BENEFICIARY,
        .current_epoch = 10,
        .authority_keyset = &AuthorityKeySet(),
        .voucher_already_consumed = false,
    };
}

cybou::InviteVoucherError Validate(
    const cybou::InviteVoucher& voucher,
    const cybou::InviteVoucherValidationContext& context,
    const cybou::OperatorAuthoritySignatureVerifier& verifier = ACCEPT_SIGNATURE)
{
    return cybou::ValidateInviteVoucher(voucher, context, verifier);
}

} // namespace

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

BOOST_AUTO_TEST_CASE(account_id_has_one_canonical_fixed_width_encoding)
{
    std::array<unsigned char, cybou::AccountId::SIZE> bytes{};
    bytes.front() = 0x2a;

    const auto id{cybou::AccountId::FromBytes(bytes)};
    BOOST_REQUIRE(id);
    BOOST_CHECK_EQUAL(*id->Value().begin(), 0x2a);

    BOOST_CHECK(!cybou::AccountId::FromBytes(std::span{bytes}.first(bytes.size() - 1)));
    bytes.fill(0);
    BOOST_CHECK(!cybou::AccountId::FromBytes(bytes));
    BOOST_CHECK(cybou::AccountId{}.IsNull());
}

BOOST_AUTO_TEST_CASE(invite_voucher_requires_authority_and_is_single_use)
{
    const auto voucher{ValidVoucher()};

    auto context{ValidContext()};
    BOOST_CHECK(Validate(voucher, context, REJECT_SIGNATURE) == cybou::InviteVoucherError::INVALID_AUTHORITY_SIGNATURE);

    context = ValidContext();
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::NONE);

    context.voucher_already_consumed = true;
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::ALREADY_CONSUMED);
}

BOOST_AUTO_TEST_CASE(invite_voucher_requires_matching_active_authority_keyset)
{
    const auto voucher{ValidVoucher()};
    auto context{ValidContext()};

    context.authority_keyset = nullptr;
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::UNKNOWN_AUTHORITY_KEYSET);

    auto wrong_keyset{AuthorityKeySet()};
    wrong_keyset.keyset_id = uint256::FromUserHex("ff").value();
    context.authority_keyset = &wrong_keyset;
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::UNKNOWN_AUTHORITY_KEYSET);

    auto inactive_keyset{AuthorityKeySet()};
    inactive_keyset.retired_from_epoch = context.current_epoch;
    context.authority_keyset = &inactive_keyset;
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::INACTIVE_AUTHORITY_KEYSET);
}

BOOST_AUTO_TEST_CASE(invite_voucher_is_bound_to_network)
{
    const auto voucher{ValidVoucher()};

    auto context{ValidContext()};
    context.expected_network_id = uint256::FromUserHex("ff").value();
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::NETWORK_MISMATCH);

    auto wrong_network{ValidVoucher()};
    wrong_network.payload.network_id.SetNull();
    BOOST_CHECK(Validate(ValidVoucher(), ValidContext()) == cybou::InviteVoucherError::NONE);
    BOOST_CHECK(Validate(wrong_network, ValidContext()) == cybou::InviteVoucherError::NETWORK_MISMATCH);
}

BOOST_AUTO_TEST_CASE(invite_voucher_is_bound_to_beneficiary)
{
    const auto voucher{ValidVoucher()};

    auto context{ValidContext()};
    context.redeemer_account_id = OTHER_ACCOUNT;
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::BENEFICIARY_MISMATCH);

    auto null_beneficiary{ValidVoucher()};
    null_beneficiary.payload.beneficiary_account_id = cybou::AccountId{};
    BOOST_CHECK(Validate(null_beneficiary, ValidContext()) == cybou::InviteVoucherError::NULL_BENEFICIARY);
}

BOOST_AUTO_TEST_CASE(invite_voucher_enforces_frozen_grant_and_epoch)
{
    auto voucher{ValidVoucher()};
    voucher.payload.grant_amount = cybou::WELCOME_GRANT - 1;
    const auto context{ValidContext()};

    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::INVALID_GRANT_AMOUNT);
    voucher.payload.grant_amount = cybou::WELCOME_GRANT;

    auto expired_context{ValidContext()};
    expired_context.current_epoch = 11;
    BOOST_CHECK(Validate(voucher, expired_context) == cybou::InviteVoucherError::EXPIRED);
}

BOOST_AUTO_TEST_CASE(invite_voucher_rejects_incomplete_envelopes)
{
    auto voucher{ValidVoucher()};
    const auto context{ValidContext()};

    voucher.payload.voucher_id.SetNull();
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::NULL_VOUCHER_ID);

    voucher = ValidVoucher();
    voucher.payload.payload_version = 2;
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::UNSUPPORTED_PAYLOAD_VERSION);

    voucher = ValidVoucher();
    voucher.signature = cybou::SignatureBundleV1{};
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::MISSING_AUTHORITY_SIGNATURE);

    voucher = ValidVoucher();
    voucher.signature.pq_signature.fill(0);
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::MISSING_AUTHORITY_SIGNATURE);

    voucher = ValidVoucher();
    voucher.payload.organization_id = uint256{};
    BOOST_CHECK(Validate(voucher, context) == cybou::InviteVoucherError::NULL_ORGANIZATION_ID);
}

BOOST_AUTO_TEST_SUITE_END()
