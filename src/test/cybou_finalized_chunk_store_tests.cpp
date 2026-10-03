// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/finalized_chunk_store.h>


#include <boost/test/unit_test.hpp>

#include <array>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace {

struct AuthorizedFixture {
    std::array<unsigned char, 32> network_binding{};
    std::vector<unsigned char> bytes = std::vector<unsigned char>(1100, 0x5a);
    cybou::ChunkId chunk_id = cybou::ComputeChunkId(bytes);
    cybou::AuthorizedChunk authorized_chunk{chunk_id};
    cybou::ChunkAuthorizationTree commitment = *cybou::BuildChunkAuthorizationTree(
        std::span<const cybou::AuthorizedChunk>{&authorized_chunk, 1});
    cybou::RootPublication publication{};
    cybou::ChunkAuthorizationProof proof = commitment.Proof(0);
    cybou::Hash256 operation_id{uint8_t{1}};

    AuthorizedFixture()
    {
        network_binding.fill(0x7c);
        publication.root_chunk_id.fill(0x31);
        publication.chunk_authorization_root = commitment.root;
        publication.chunk_count = commitment.chunk_count;
    }

    cybou::FinalizedPublicationLookup Lookup() const
    {
        return [this](const cybou::Hash256& id) -> std::optional<cybou::RootPublication> {
            if (id != operation_id) return std::nullopt;
            return publication;
        };
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_finalized_chunk_store_tests)

BOOST_AUTO_TEST_CASE(finalized_chunk_store_requires_finality_and_valid_proof)
{
    AuthorizedFixture fixture;
    cybou::ChunkBlobStore blobs({}, true);
    cybou::FinalizedChunkStore store(blobs, {}, fixture.network_binding, 4096);
    const auto unavailable = [](const cybou::Hash256&) -> std::optional<cybou::RootPublication> { return std::nullopt; };
    BOOST_CHECK(store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, unavailable).status ==
        cybou::ChunkAdmissionStatus::NOT_FINALIZED);
    BOOST_CHECK(!store.HasChunk(fixture.chunk_id));

    auto bad_proof = fixture.proof;
    bad_proof.siblings.push_back(cybou::ChunkId{});
    BOOST_CHECK(store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, bad_proof, fixture.Lookup()).status ==
        cybou::ChunkAdmissionStatus::NOT_AUTHORIZED);
    BOOST_CHECK(!store.HasChunk(fixture.chunk_id));

    auto bad_id = fixture.chunk_id;
    bad_id[0] ^= 1;
    BOOST_CHECK(store.PutChunk(fixture.operation_id, bad_id, fixture.bytes, fixture.proof, fixture.Lookup()).status ==
        cybou::ChunkAdmissionStatus::INVALID);
}

BOOST_AUTO_TEST_CASE(finalized_chunk_store_is_content_addressed_idempotent_and_capacity_bounded)
{
    AuthorizedFixture fixture;
    cybou::ChunkBlobStore blobs({}, true);
    cybou::FinalizedChunkStore store(blobs, {}, fixture.network_binding, fixture.bytes.size());
    const auto lookup = fixture.Lookup();
    const auto first = store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, lookup);
    BOOST_REQUIRE(first.status == cybou::ChunkAdmissionStatus::STORED);
    BOOST_CHECK(store.UsedBytes() == fixture.bytes.size());
    BOOST_CHECK(store.HasChunk(fixture.chunk_id));
    BOOST_CHECK(store.GetChunk(fixture.chunk_id) == fixture.bytes);

    const auto repeated = store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, lookup);
    BOOST_CHECK(repeated.status == cybou::ChunkAdmissionStatus::ALREADY_STORED);
    BOOST_CHECK(store.UsedBytes() == fixture.bytes.size());

    auto second_bytes = fixture.bytes;
    second_bytes[0] ^= 1;
    const auto second_id = cybou::ComputeChunkId(second_bytes);
    const cybou::AuthorizedChunk second_chunk{second_id};
    const auto second_commitment = cybou::BuildChunkAuthorizationTree(
        std::span<const cybou::AuthorizedChunk>{&second_chunk, 1});
    BOOST_REQUIRE(second_commitment.has_value());
    auto second_publication = fixture.publication;
    second_publication.chunk_authorization_root = second_commitment->root;
    auto second_id_op = cybou::Hash256{uint8_t{2}};
    const auto second_lookup = [second_id_op, second_publication](const cybou::Hash256& id)
        -> std::optional<cybou::RootPublication> {
        if (id == second_id_op) return second_publication;
        return std::nullopt;
    };
    BOOST_CHECK(store.PutChunk(second_id_op, second_id, second_bytes,
        second_commitment->Proof(0), second_lookup).status == cybou::ChunkAdmissionStatus::CAPACITY_EXCEEDED);

    // The same physical chunk can be authorized by another finalized publication without doubling storage use.
    const auto second_publication_id = cybou::Hash256{uint8_t{3}};
    const auto shared_lookup = [second_publication_id, publication = fixture.publication](const cybou::Hash256& id)
        -> std::optional<cybou::RootPublication> {
        if (id == second_publication_id) return publication;
        return std::nullopt;
    };
    BOOST_CHECK(store.PutChunk(second_publication_id, fixture.chunk_id, fixture.bytes,
        fixture.proof, shared_lookup).status == cybou::ChunkAdmissionStatus::STORED);
    BOOST_CHECK(store.UsedBytes() == fixture.bytes.size());
}

BOOST_AUTO_TEST_CASE(finalized_chunk_store_allows_proven_chunks_until_storage_capacity)
{
    auto first_bytes = std::vector<unsigned char>(1100, 0x11);
    auto second_bytes = std::vector<unsigned char>(1100, 0x22);
    const cybou::AuthorizedChunk chunks[] = {
        {cybou::ComputeChunkId(first_bytes)},
        {cybou::ComputeChunkId(second_bytes)},
    };
    const auto commitment = cybou::BuildChunkAuthorizationTree(chunks);
    BOOST_REQUIRE(commitment.has_value());

    const auto publication_id = cybou::Hash256{uint8_t{4}};
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x42);
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    const auto lookup = [publication_id, publication](const cybou::Hash256& id)
        -> std::optional<cybou::RootPublication> {
        if (id == publication_id) return publication;
        return std::nullopt;
    };

    const std::array<unsigned char, 32> network_binding = [] {
        std::array<unsigned char, 32> id{};
        id.fill(0x7c);
        return id;
    }();
    cybou::ChunkBlobStore blobs({}, true);
    cybou::FinalizedChunkStore store(blobs, {}, network_binding, 4096);
    BOOST_CHECK(store.PutChunk(publication_id, chunks[0].id, first_bytes, commitment->Proof(0), lookup).status ==
        cybou::ChunkAdmissionStatus::STORED);
    BOOST_CHECK(store.PutChunk(publication_id, chunks[1].id, second_bytes, commitment->Proof(1), lookup).status ==
        cybou::ChunkAdmissionStatus::STORED);
    BOOST_CHECK(store.UsedBytes() == first_bytes.size() + second_bytes.size());
}

BOOST_AUTO_TEST_CASE(finalized_chunk_store_binds_persistent_database_to_network)
{
    const auto path = std::filesystem::temp_directory_path() / "cybou-finalized-chunk-store-network-binding";
    std::array<unsigned char, 32> network_binding{};
    network_binding.fill(0x7c);
    auto other_network_id = network_binding;
    other_network_id[0] ^= 1;
    cybou::ChunkBlobStore blobs(path / "chunks", false, true);
    {
        cybou::FinalizedChunkStore store(blobs, path, network_binding, 4096, true);
        BOOST_CHECK(store.UsedBytes() == 0);
    }
    BOOST_CHECK_THROW((cybou::FinalizedChunkStore{blobs, path, other_network_id, 4096}), std::invalid_argument);
    {
        cybou::FinalizedChunkStore store(blobs, path, network_binding, 4096);
        BOOST_CHECK(store.UsedBytes() == 0);
    }
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_SUITE_END()
