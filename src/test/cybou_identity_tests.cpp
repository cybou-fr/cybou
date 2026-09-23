// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>

#include <boost/test/unit_test.hpp>

#include <array>
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

BOOST_AUTO_TEST_CASE(account_authorization_serialization_and_commitment)
{
    cybou::AccountAuthorizationV1 auth{
        .authorization_descriptor = uint256::FromUserHex("42").value(),
    };

    const auto bytes{cybou::SerializeAccountAuthorization(auth)};
    BOOST_CHECK_EQUAL(bytes.size(), 32);

    const auto commitment{cybou::ComputeAuthCommitment(auth)};
    BOOST_CHECK(!commitment.IsNull());

    cybou::AccountAuthorizationV1 other_auth{
        .authorization_descriptor = uint256::FromUserHex("43").value(),
    };
    BOOST_CHECK(cybou::ComputeAuthCommitment(other_auth) != commitment);
}

BOOST_AUTO_TEST_SUITE_END()
