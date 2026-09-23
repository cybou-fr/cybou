// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity.h>
#include <cybou/signing.h>

#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <optional>
#include <set>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_signing_tests)

BOOST_AUTO_TEST_CASE(object_signing_domains_are_distinct_and_separate_from_key_domains)
{
    using cybou::ObjectSigningDomain;
    constexpr std::array domains{
        ObjectSigningDomain::INVITE_VOUCHER,
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

namespace {

cybou::InviteVoucherPayloadV1 ReferencePayload(bool with_organization)
{
    cybou::InviteVoucherPayloadV1 payload{
        .network_id = uint256::ONE,
        .voucher_id = uint256::FromUserHex("02").value(),
        .beneficiary_account_id = cybou::AccountId{uint256::FromUserHex("03").value()},
        .grant_amount = cybou::WELCOME_GRANT,
        .expiry_epoch = 10,
        .organization_id = std::nullopt,
    };
    if (with_organization) payload.organization_id = uint256::FromUserHex("04").value();
    return payload;
}

} // namespace

BOOST_AUTO_TEST_CASE(invite_voucher_payload_serialization_is_canonical)
{
    const auto payload{ReferencePayload(false)};
    const auto bytes{cybou::SerializeInviteVoucherPayload(payload)};

    // Fixed layout: version, three 32-byte hashes in internal
    // byte order, two uint64 little-endian, one presence flag byte.
    std::vector<unsigned char> expected;
    const auto append_hash = [&](const uint256& hash) {
        expected.insert(expected.end(), hash.data(), hash.data() + uint256::size());
    };
    const auto append_u64le = [&](const uint64_t value) {
        for (unsigned i = 0; i < 8; ++i) expected.push_back(static_cast<unsigned char>(value >> (8 * i)));
    };
    expected.push_back(cybou::INVITE_VOUCHER_PAYLOAD_VERSION);
    append_hash(payload.network_id);
    append_hash(payload.voucher_id);
    append_hash(payload.beneficiary_account_id.Value());
    append_u64le(payload.grant_amount);
    append_u64le(payload.expiry_epoch);
    expected.push_back(0x00);

    BOOST_CHECK_EQUAL(bytes.size(), 114);
    BOOST_CHECK(bytes == expected);

    const auto with_org{cybou::SerializeInviteVoucherPayload(ReferencePayload(true))};
    BOOST_CHECK_EQUAL(with_org.size(), 146);
    // Presence flag at offset 113; organization_id follows.
    BOOST_CHECK_EQUAL(with_org[113], 0x01);
    BOOST_CHECK(std::vector<unsigned char>(with_org.begin(), with_org.begin() + 113) ==
                std::vector<unsigned char>(bytes.begin(), bytes.begin() + 113));
}

BOOST_AUTO_TEST_CASE(invite_voucher_payload_serialization_matches_golden_vector)
{
    // Frozen in spec/invite_voucher_test_vectors.csv. Any change here is a
    // consensus-visible serialization change, not a refactor.
    static const std::string GOLDEN_NO_ORG{
        "01"
        "0100000000000000000000000000000000000000000000000000000000000000"
        "0200000000000000000000000000000000000000000000000000000000000000"
        "0300000000000000000000000000000000000000000000000000000000000000"
        "70170000000000000a0000000000000000"};
    static const std::string GOLDEN_WITH_ORG{
        GOLDEN_NO_ORG.substr(0, GOLDEN_NO_ORG.size() - 2) +
        "01"
        "0400000000000000000000000000000000000000000000000000000000000000"};

    BOOST_CHECK(HexStr(cybou::SerializeInviteVoucherPayload(ReferencePayload(false))) == GOLDEN_NO_ORG);
    BOOST_CHECK(HexStr(cybou::SerializeInviteVoucherPayload(ReferencePayload(true))) == GOLDEN_WITH_ORG);
}

BOOST_AUTO_TEST_CASE(invite_voucher_payload_serialization_is_deterministic_and_sensitive)
{
    const auto base{ReferencePayload(true)};
    const auto base_bytes{cybou::SerializeInviteVoucherPayload(base)};
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(base) == base_bytes);

    auto mutated{base};
    mutated.payload_version = 2;
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);

    mutated = base;
    mutated.network_id = uint256::FromUserHex("05").value();
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);

    mutated = base;
    mutated.voucher_id = uint256::FromUserHex("05").value();
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);

    mutated = base;
    mutated.beneficiary_account_id = cybou::AccountId{uint256::FromUserHex("05").value()};
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);

    mutated = base;
    mutated.grant_amount = cybou::WELCOME_GRANT + 1;
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);

    mutated = base;
    mutated.expiry_epoch = 11;
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);

    mutated = base;
    mutated.organization_id = std::nullopt;
    BOOST_CHECK(cybou::SerializeInviteVoucherPayload(mutated) != base_bytes);
}

BOOST_AUTO_TEST_CASE(invite_voucher_sig_message_is_domain_separated)
{
    const auto payload{ReferencePayload(false)};
    const uint256 keyset_id{uint256::FromUserHex("05").value()};
    const auto suite{cybou::SignatureSuiteId::HYBRID_ED25519_MLDSA65_V1};
    const auto message{cybou::InviteVoucherSigMessage(payload, suite, keyset_id)};
    const auto payload_bytes{cybou::SerializeInviteVoucherPayload(payload)};
    const std::string_view tag{cybou::ObjectSigningDomainTag(cybou::ObjectSigningDomain::INVITE_VOUCHER)};

    BOOST_CHECK_EQUAL(message.size(), tag.size() + 2 + uint256::size() + payload_bytes.size());
    BOOST_CHECK((std::string_view{reinterpret_cast<const char*>(message.data()), tag.size()} == tag));
    BOOST_CHECK_EQUAL(message[tag.size()], 0x01);
    BOOST_CHECK_EQUAL(message[tag.size() + 1], 0x00);
    BOOST_CHECK(std::equal(keyset_id.begin(), keyset_id.end(), message.begin() + tag.size() + 2));
    BOOST_CHECK(std::vector<unsigned char>(message.begin() + tag.size() + 2 + uint256::size(), message.end()) == payload_bytes);

    BOOST_CHECK(cybou::InviteVoucherSigMessage(payload, suite, uint256::ONE) != message);

    // A message signed for a different object domain can never collide.
    BOOST_CHECK((std::string_view{reinterpret_cast<const char*>(message.data()), tag.size()} !=
                cybou::ObjectSigningDomainTag(cybou::ObjectSigningDomain::VALIDATOR_ADMISSION)));
}

BOOST_AUTO_TEST_SUITE_END()
