// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/root_publication.h>
#include <cybou/chunk_authorization.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace {

cybou::RootPublication ValidPublication()
{
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x11);
    const std::array<cybou::AuthorizedChunk, 1> chunks{{
        {publication.root_chunk_id, cybou::ROOT_PUBLICATION_MIN_CHUNK_STORED_BYTES},
    }};
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(chunks);
    BOOST_REQUIRE(commitment.has_value());
    publication.chunk_authorization_root = commitment->root;
    publication.authorized_chunks.assign(chunks.begin(), chunks.end());
    publication.chunk_count = 1;
    publication.authorized_stored_bytes = cybou::ROOT_PUBLICATION_MIN_CHUNK_STORED_BYTES;
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
    const auto decoded = cybou::DeserializeRootPublication(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == publication);
    BOOST_CHECK(cybou::SerializeRootPublication(*decoded) == encoded);
}

BOOST_AUTO_TEST_CASE(root_publication_rejects_unknown_version_and_bad_accounting)
{
    const auto encoded = cybou::SerializeRootPublication(ValidPublication());
    BOOST_REQUIRE(encoded.has_value());
    auto unknown_version = *encoded;
    BOOST_REQUIRE(unknown_version.size() > 2);
    unknown_version[2] = 2;
    BOOST_CHECK(!cybou::DeserializeRootPublication(unknown_version));

    auto bad_accounting = ValidPublication();
    bad_accounting.authorized_stored_bytes = cybou::ROOT_PUBLICATION_MIN_CHUNK_STORED_BYTES - 1;
    BOOST_CHECK(!cybou::SerializeRootPublication(bad_accounting));
    bad_accounting = ValidPublication();
    bad_accounting.chunk_count = static_cast<std::uint32_t>(cybou::ROOT_PUBLICATION_MAX_CHUNKS + 1);
    BOOST_CHECK(!cybou::SerializeRootPublication(bad_accounting));
    bad_accounting = ValidPublication();
    bad_accounting.authorized_stored_bytes += 1;
    BOOST_CHECK(!cybou::SerializeRootPublication(bad_accounting));
    bad_accounting = ValidPublication();
    bad_accounting.root_chunk_id[0] ^= 0xff;
    BOOST_CHECK(!cybou::SerializeRootPublication(bad_accounting));
}

BOOST_AUTO_TEST_CASE(root_publication_fee_is_integer_size_aware_and_has_no_priority_input)
{
    BOOST_CHECK(cybou::ComputeRootPublicationFee(1) == 4);
    BOOST_CHECK(cybou::ComputeRootPublicationFee(1024) == 4);
    BOOST_CHECK(cybou::ComputeRootPublicationFee(1025) == 8);
    BOOST_CHECK(!cybou::ComputeRootPublicationFee(0));
    BOOST_CHECK(!cybou::ComputeRootPublicationFee(cybou::ROOT_PUBLICATION_MAX_OPERATION_BYTES + 1));
}

BOOST_AUTO_TEST_CASE(root_publication_maximum_profile_fits_the_frozen_body_limit)
{
    cybou::RootPublication publication;
    publication.authorized_chunks.reserve(cybou::ROOT_PUBLICATION_MAX_CHUNKS);
    for (std::size_t i = 0; i < cybou::ROOT_PUBLICATION_MAX_CHUNKS; ++i) {
        cybou::AuthorizedChunk chunk;
        chunk.id[0] = 1;
        chunk.id[30] = static_cast<unsigned char>(i >> 8);
        chunk.id[31] = static_cast<unsigned char>(i);
        chunk.stored_bytes = 256 * 1024;
        publication.authorized_chunks.push_back(chunk);
    }
    publication.root_chunk_id = publication.authorized_chunks.front().id;
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(publication.authorized_chunks);
    BOOST_REQUIRE(commitment.has_value());
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    publication.authorized_stored_bytes = commitment->authorized_stored_bytes;
    publication.recipient_capsules.resize(cybou::ROOT_PUBLICATION_MAX_CAPSULES);

    const auto encoded = cybou::SerializeRootPublication(publication);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK(encoded->size() <= cybou::ROOT_PUBLICATION_MAX_BYTES);

    publication.recipient_capsules.emplace_back();
    BOOST_CHECK(!cybou::SerializeRootPublication(publication));
}

BOOST_AUTO_TEST_SUITE_END()
