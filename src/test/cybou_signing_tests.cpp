// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>
#include <cybou/signing.h>

#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <set>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_signing_tests)

BOOST_AUTO_TEST_CASE(object_signing_domains_are_distinct_and_separate_from_key_domains)
{
    using cybou::ObjectSigningDomain;
    constexpr std::array domains{
        ObjectSigningDomain::VALIDATOR_ADMISSION,
        ObjectSigningDomain::VALIDATOR_REMOVAL,
        ObjectSigningDomain::PROTOCOL_PARAMETER,
    };

    std::set<std::string> tags;
    for (const auto domain : domains) {
        const auto tag{cybou::ObjectSigningDomainTag(domain)};
        BOOST_CHECK(tag.starts_with("CYBOU/SIG/"));
        tags.emplace(tag);
    }
    BOOST_CHECK_EQUAL(tags.size(), domains.size());

    using cybou::OperatorKeyDomain;
    constexpr std::array key_domains{
        OperatorKeyDomain::AUTHORITY,
        OperatorKeyDomain::VALIDATOR,
        OperatorKeyDomain::RELEASE_SIGNING,
        OperatorKeyDomain::TREASURY,
    };
    for (const auto key_domain : key_domains) {
        BOOST_CHECK(!tags.contains(std::string{cybou::KeyDomainTag(key_domain)}));
    }
}

BOOST_AUTO_TEST_CASE(signature_bundle_v1_has_frozen_hybrid_layout)
{
    BOOST_CHECK_EQUAL(cybou::ED25519_SIGNATURE_SIZE, 64);
    BOOST_CHECK_EQUAL(cybou::MLDSA65_SIGNATURE_SIZE, 3309);
    BOOST_CHECK_EQUAL(sizeof(cybou::SignatureBundleV1::classical_signature), cybou::ED25519_SIGNATURE_SIZE);
    BOOST_CHECK_EQUAL(sizeof(cybou::SignatureBundleV1::pq_signature), cybou::MLDSA65_SIGNATURE_SIZE);

    const cybou::SignatureBundleV1 empty{};
    BOOST_CHECK(empty.suite_id == cybou::SignatureSuiteId::HYBRID_ED25519_MLDSA65_V1);
    BOOST_CHECK(!cybou::IsPresent(empty));

    cybou::SignatureBundleV1 classical_only{};
    classical_only.authority_keyset_id = uint256::ONE;
    classical_only.classical_signature[0] = 0x01;
    BOOST_CHECK(!cybou::IsPresent(classical_only));

    cybou::SignatureBundleV1 pq_only{};
    pq_only.authority_keyset_id = uint256::ONE;
    pq_only.pq_signature[cybou::MLDSA65_SIGNATURE_SIZE - 1] = 0x01;
    BOOST_CHECK(!cybou::IsPresent(pq_only));

    auto complete{classical_only};
    complete.pq_signature[0] = 0x01;
    BOOST_CHECK(cybou::IsPresent(complete));

    complete.suite_id = static_cast<cybou::SignatureSuiteId>(0xffff);
    BOOST_CHECK(!cybou::IsPresent(complete));
}

BOOST_AUTO_TEST_CASE(operator_authority_keyset_has_frozen_layout_and_epoch_window)
{
    BOOST_CHECK_EQUAL(cybou::ED25519_PUBLIC_KEY_SIZE, 32);
    BOOST_CHECK_EQUAL(cybou::MLDSA65_PUBLIC_KEY_SIZE, 1952);
    cybou::OperatorAuthorityKeySet keyset{
        .keyset_id = uint256::ONE,
        .active_from_epoch = 10,
        .retired_from_epoch = 20,
    };
    BOOST_CHECK(!cybou::IsActiveAtEpoch(keyset, 9));
    BOOST_CHECK(cybou::IsActiveAtEpoch(keyset, 10));
    BOOST_CHECK(cybou::IsActiveAtEpoch(keyset, 19));
    BOOST_CHECK(!cybou::IsActiveAtEpoch(keyset, 20));
}

BOOST_AUTO_TEST_SUITE_END()
