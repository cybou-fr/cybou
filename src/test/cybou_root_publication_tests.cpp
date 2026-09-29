// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/root_publication.h>
#include <cybou/canonical_cbor.h>

#include <boost/test/unit_test.hpp>

#include <limits>

namespace {

cybou::RootPublication ValidPublication()
{
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x11);
    publication.chunk_authorization_root.fill(0x22);
    publication.chunk_count = 1;
    cybou::RootRecipientCapsule capsule;
    capsule.key_epoch = 3;
    for (std::size_t i = 0; i < capsule.encapsulation.size(); ++i) {
        capsule.encapsulation[i] = static_cast<unsigned char>(i & 0xff);
    }
    capsule.wrapped_content_key.fill(0x44);
    publication.recipient_capsules.push_back(capsule);
    return publication;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_root_publication_tests)

BOOST_AUTO_TEST_CASE(root_publication_round_trips_canonical_cbor)
{
    const auto publication = ValidPublication();
    const auto encoded = cybou::SerializeRootPublication(publication);
    BOOST_REQUIRE(encoded.has_value());
    const auto decoded_cbor = cybou::DecodeCanonicalCbor(*encoded);
    const auto* public_fields = std::get_if<cybou::CborValue::Map>(&decoded_cbor.value);
    BOOST_REQUIRE(public_fields != nullptr);
    BOOST_CHECK(public_fields->size() == 5); // version + four frozen public fields
    const auto decoded = cybou::DeserializeRootPublication(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == publication);
    BOOST_CHECK(cybou::SerializeRootPublication(*decoded) == encoded);
}

BOOST_AUTO_TEST_CASE(root_publication_rejects_unknown_version_and_invalid_fields)
{
    const auto encoded = cybou::SerializeRootPublication(ValidPublication());
    BOOST_REQUIRE(encoded.has_value());
    auto unknown_version = *encoded;
    BOOST_REQUIRE(unknown_version.size() > 2);
    unknown_version[2] = 2;
    BOOST_CHECK(!cybou::DeserializeRootPublication(unknown_version));

    auto bad_accounting = ValidPublication();
    bad_accounting.chunk_count = 0;
    BOOST_CHECK(!cybou::SerializeRootPublication(bad_accounting));
    bad_accounting = ValidPublication();
    bad_accounting.root_chunk_id.fill(0);
    BOOST_CHECK(!cybou::SerializeRootPublication(bad_accounting));
}

BOOST_AUTO_TEST_CASE(root_publication_fee_is_integer_size_aware_and_has_no_priority_input)
{
    auto params = cybou::DevProtocolParameters();
    BOOST_CHECK(cybou::ComputeRootPublicationFee(params, 1, 1) == 8);
    BOOST_CHECK(cybou::ComputeRootPublicationFee(params, 1024, 1) == 8);
    BOOST_CHECK(cybou::ComputeRootPublicationFee(params, 1025, 1) == 12);
    BOOST_CHECK(cybou::ComputeRootPublicationFee(params, 1, 2) == 12);
    BOOST_CHECK(!cybou::ComputeRootPublicationFee(params, 0, 1));
    BOOST_CHECK(!cybou::ComputeRootPublicationFee(params,
        cybou::ROOT_PUBLICATION_MAX_OPERATION_BYTES + 1, 1));
    BOOST_CHECK(!cybou::ComputeRootPublicationFee(params, 1, 0));

    params.root_publication_fee_per_started_kib = 7;
    params.root_publication_fee_per_chunk = 11;
    BOOST_CHECK(cybou::ComputeRootPublicationFee(params, 1025, 2) == 36);
    params.root_publication_fee_per_chunk = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK(!cybou::ComputeRootPublicationFee(params, 1, 2));
}

BOOST_AUTO_TEST_CASE(root_publication_maximum_profile_fits_the_frozen_body_limit)
{
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x31);
    publication.chunk_authorization_root.fill(0x42);
    publication.chunk_count = cybou::MAX_PUBLICATION_CHUNKS;
    publication.recipient_capsules.resize(cybou::ROOT_PUBLICATION_MAX_CAPSULES);

    const auto encoded = cybou::SerializeRootPublication(publication);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK(encoded->size() <= cybou::ROOT_PUBLICATION_MAX_BYTES);

    publication.recipient_capsules.emplace_back();
    BOOST_CHECK(!cybou::SerializeRootPublication(publication));
}

BOOST_AUTO_TEST_CASE(root_publication_chunk_count_is_the_shared_storage_invariant)
{
    BOOST_CHECK_EQUAL(cybou::MAX_PUBLICATION_CHUNKS, 1U << 20);
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x31);
    publication.chunk_authorization_root.fill(0x42);
    publication.chunk_count = cybou::MAX_PUBLICATION_CHUNKS + 1;
    // A publication storage could not serve is never valid on the network.
    const auto encoded = cybou::SerializeRootPublication(publication);
    BOOST_CHECK(!encoded || !cybou::DeserializeRootPublication(*encoded));
}

BOOST_AUTO_TEST_SUITE_END()
