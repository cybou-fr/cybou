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
    chunks[1].id.fill(0x42);
    chunks[2].id.fill(0x93);
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
        0x95, 0x7f, 0xe7, 0xe3, 0x24, 0xd7, 0xa8, 0xe3,
        0xc6, 0x9d, 0x84, 0xd2, 0x0f, 0x12, 0x94, 0xee,
        0x6a, 0x40, 0x6c, 0x8e, 0x3d, 0xc9, 0x2b, 0x15,
        0x16, 0x07, 0x70, 0x8c, 0xd6, 0x39, 0x5c, 0x4e,
    }};
    BOOST_CHECK(first->root == expected_root);
    std::reverse(chunks.begin(), chunks.end());
    const auto reordered = cybou::BuildChunkAuthorizationCommitment(chunks);
    BOOST_REQUIRE(reordered.has_value());
    BOOST_CHECK(first->root != reordered->root);
    BOOST_CHECK(first->chunk_count == chunks.size());

    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x55);
    publication.chunk_authorization_root = first->root;
    publication.chunk_count = first->chunk_count;
    for (const auto& proof : first->proofs) BOOST_CHECK(cybou::VerifyChunkAuthorizationProof(publication, proof));
}

BOOST_AUTO_TEST_CASE(chunk_authorization_streaming_accumulator_matches_tree_for_odd_counts)
{
    for (std::size_t count = 1; count <= 33; ++count) {
        std::vector<cybou::AuthorizedChunk> chunks(count);
        cybou::ChunkAuthorizationAccumulator accumulator;
        for (std::size_t i = 0; i < count; ++i) {
            chunks[i].id.fill(static_cast<unsigned char>(i + 1));
            BOOST_REQUIRE(accumulator.Add(chunks[i]));
        }
        const auto streaming = accumulator.Finish();
        const auto materialized = cybou::BuildChunkAuthorizationCommitment(chunks);
        BOOST_REQUIRE(streaming.has_value());
        BOOST_REQUIRE(materialized.has_value());
        BOOST_CHECK(streaming->root == materialized->root);
        BOOST_CHECK(streaming->chunk_count == count);
    }
}

BOOST_AUTO_TEST_CASE(chunk_authorization_rejects_bad_path_count_and_duplicate_ids)
{
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(Chunks());
    BOOST_REQUIRE(commitment.has_value());
    cybou::RootPublication publication;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    auto bad_path = commitment->proofs.back();
    bad_path.siblings[0][0] ^= 1;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, bad_path));
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
    auto odd_leaf = commitment->proofs.back();
    BOOST_REQUIRE(!odd_leaf.siblings.empty());
    odd_leaf.siblings[0][0] ^= 1;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, odd_leaf));
}

BOOST_AUTO_TEST_SUITE_END()
