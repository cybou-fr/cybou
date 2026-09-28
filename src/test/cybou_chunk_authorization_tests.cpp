// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_authorization.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>

namespace {

std::array<cybou::AuthorizedChunk, 3> Chunks()
{
    std::array<cybou::AuthorizedChunk, 3> chunks{};
    chunks[0].id.fill(0x11);
    chunks[0].stored_bytes = 4096;
    chunks[1].id.fill(0x42);
    chunks[1].stored_bytes = 16384;
    chunks[2].id.fill(0x93);
    chunks[2].stored_bytes = 1100;
    return chunks;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_chunk_authorization_tests)

BOOST_AUTO_TEST_CASE(chunk_authorization_builds_ordered_proofs)
{
    auto chunks = Chunks();
    const auto first = cybou::BuildChunkAuthorizationCommitment(chunks);
    BOOST_REQUIRE(first.has_value());
    const cybou::ChunkId expected_root{{
        0x21, 0x17, 0xd0, 0x8e, 0xe5, 0x5b, 0x42, 0xe9,
        0xb8, 0xaa, 0x63, 0xaf, 0x00, 0xa2, 0xec, 0x57,
        0xcd, 0x86, 0x2b, 0xf9, 0xcb, 0x7b, 0x6b, 0x08,
        0xab, 0x6c, 0x7c, 0xce, 0xb6, 0x94, 0x1e, 0x19,
    }};
    BOOST_CHECK(first->root == expected_root);
    std::reverse(chunks.begin(), chunks.end());
    const auto reordered = cybou::BuildChunkAuthorizationCommitment(chunks);
    BOOST_REQUIRE(reordered.has_value());
    BOOST_CHECK(first->root != reordered->root);
    BOOST_CHECK(first->chunk_count == chunks.size());
    BOOST_CHECK(first->authorized_stored_bytes == 1100 + 4096 + 16384);

    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x55);
    publication.chunk_authorization_root = first->root;
    publication.chunk_count = first->chunk_count;
    publication.authorized_stored_bytes = first->authorized_stored_bytes;
    for (const auto& proof : first->proofs) BOOST_CHECK(cybou::VerifyChunkAuthorizationProof(publication, proof));
}

BOOST_AUTO_TEST_CASE(chunk_authorization_streaming_accumulator_matches_tree_for_odd_counts)
{
    for (std::size_t count = 1; count <= 33; ++count) {
        std::vector<cybou::AuthorizedChunk> chunks(count);
        cybou::ChunkAuthorizationAccumulator accumulator;
        for (std::size_t i = 0; i < count; ++i) {
            chunks[i].id.fill(static_cast<unsigned char>(i + 1));
            chunks[i].stored_bytes = 1089 + i;
            BOOST_REQUIRE(accumulator.Add(chunks[i]));
        }
        const auto streaming = accumulator.Finish();
        const auto materialized = cybou::BuildChunkAuthorizationCommitment(chunks);
        BOOST_REQUIRE(streaming.has_value());
        BOOST_REQUIRE(materialized.has_value());
        BOOST_CHECK(streaming->root == materialized->root);
        BOOST_CHECK(streaming->chunk_count == count);
        BOOST_CHECK(streaming->authorized_stored_bytes == materialized->authorized_stored_bytes);
    }
}

BOOST_AUTO_TEST_CASE(chunk_authorization_rejects_bad_path_count_size_and_duplicate_ids)
{
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(Chunks());
    BOOST_REQUIRE(commitment.has_value());
    cybou::RootPublication publication;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    publication.authorized_stored_bytes = commitment->authorized_stored_bytes;
    auto bad_path = commitment->proofs.back();
    bad_path.siblings[0][0] ^= 1;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, bad_path));
    auto bad_size = commitment->proofs.front();
    ++bad_size.stored_bytes;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, bad_size));
    auto bad_count = commitment->proofs.front();
    ++bad_count.chunk_count;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, bad_count));

    auto duplicate = Chunks();
    duplicate[2].id = duplicate[1].id;
    BOOST_CHECK(!cybou::BuildChunkAuthorizationCommitment(duplicate));
}

BOOST_AUTO_TEST_CASE(chunk_authorization_rejects_noncanonical_odd_duplication)
{
    auto chunks = Chunks();
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(chunks);
    BOOST_REQUIRE(commitment.has_value());
    cybou::RootPublication publication;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    publication.authorized_stored_bytes = commitment->authorized_stored_bytes;
    auto odd_leaf = commitment->proofs.back();
    BOOST_REQUIRE(!odd_leaf.siblings.empty());
    odd_leaf.siblings[0][0] ^= 1;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, odd_leaf));
}

BOOST_AUTO_TEST_SUITE_END()
