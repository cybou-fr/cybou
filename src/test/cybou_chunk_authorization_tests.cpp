// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_authorization.h>
#include <cybou/kv_store.h>

#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>

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
    const auto ordered_chunks = chunks;
    const auto first = cybou::BuildChunkAuthorizationTree(chunks);
    BOOST_REQUIRE(first.has_value());
    const cybou::ChunkId expected_root{{
        0x95, 0x7f, 0xe7, 0xe3, 0x24, 0xd7, 0xa8, 0xe3,
        0xc6, 0x9d, 0x84, 0xd2, 0x0f, 0x12, 0x94, 0xee,
        0x6a, 0x40, 0x6c, 0x8e, 0x3d, 0xc9, 0x2b, 0x15,
        0x16, 0x07, 0x70, 0x8c, 0xd6, 0x39, 0x5c, 0x4e,
    }};
    BOOST_CHECK(first->root == expected_root);
    std::reverse(chunks.begin(), chunks.end());
    const auto reordered = cybou::BuildChunkAuthorizationTree(chunks);
    BOOST_REQUIRE(reordered.has_value());
    BOOST_CHECK(first->root != reordered->root);
    BOOST_CHECK(first->chunk_count == chunks.size());
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x55);
    publication.chunk_authorization_root = first->root;
    publication.chunk_count = first->chunk_count;
    for (std::size_t i = 0; i < first->chunk_count; ++i) {
        BOOST_CHECK(cybou::VerifyChunkAuthorizationProof(publication, ordered_chunks[i].id, first->Proof(i)));
    }
    std::reverse(chunks.begin(), chunks.end());
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
        const auto materialized = cybou::BuildChunkAuthorizationTree(chunks);
        BOOST_REQUIRE(streaming.has_value());
        BOOST_REQUIRE(materialized.has_value());
        BOOST_CHECK(streaming->root == materialized->root);
        BOOST_CHECK(streaming->chunk_count == count);
    }
}

BOOST_AUTO_TEST_CASE(chunk_authorization_rejects_bad_path_count_and_duplicate_ids)
{
    const auto commitment = cybou::BuildChunkAuthorizationTree(Chunks());
    BOOST_REQUIRE(commitment.has_value());
    cybou::RootPublication publication;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    auto bad_path = commitment->Proof(commitment->chunk_count - 1);
    bad_path.siblings[0][0] ^= 1;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, Chunks().back().id, bad_path));
    auto bad_count = publication;
    --bad_count.chunk_count;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(bad_count, Chunks().back().id, commitment->Proof(commitment->chunk_count - 1)));

    auto duplicate = Chunks();
    duplicate[2].id = duplicate[1].id;
    BOOST_CHECK(!cybou::BuildChunkAuthorizationTree(duplicate));
}

BOOST_AUTO_TEST_CASE(chunk_authorization_rejects_noncanonical_odd_duplication)
{
    auto chunks = Chunks();
    const auto commitment = cybou::BuildChunkAuthorizationTree(chunks);
    BOOST_REQUIRE(commitment.has_value());
    cybou::RootPublication publication;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    auto odd_leaf = commitment->Proof(commitment->chunk_count - 1);
    BOOST_REQUIRE(!odd_leaf.siblings.empty());
    odd_leaf.siblings[0][0] ^= 1;
    BOOST_CHECK(!cybou::VerifyChunkAuthorizationProof(publication, chunks.back().id, odd_leaf));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(cybou_chunk_authorization_disk_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(transient_tree_proofs_match_streaming_root_for_all_leaf_positions)
{
    for (std::size_t count = 1; count <= 65; ++count) {
        std::vector<cybou::AuthorizedChunk> chunks(count);
        cybou::ChunkAuthorizationAccumulator accumulator;
        for (std::size_t i = 0; i < count; ++i) {
            chunks[i].id.fill(static_cast<unsigned char>(i + 1));
            BOOST_REQUIRE(accumulator.Add(chunks[i]));
        }
        const auto tree = cybou::BuildChunkAuthorizationTree(chunks);
        const auto summary = accumulator.Finish();
        BOOST_REQUIRE(tree && summary);
        BOOST_CHECK(tree->Root() == summary->root);
        BOOST_CHECK_EQUAL(tree->ChunkCount(), count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto proof = tree->Proof(i);
            BOOST_CHECK(cybou::VerifyChunkAuthorizationPath(tree->Root(), chunks[i].id,
                proof.leaf_index, tree->ChunkCount(), proof.siblings));
        }
        BOOST_CHECK_THROW(tree->Proof(count), std::out_of_range);
    }
}

BOOST_AUTO_TEST_SUITE_END()
