// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_blob_store.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/network_definition.h>
#include <cybou/node_runtime.h>
#include <test/cybou_test_helpers.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(cybou_chunk_blob_store_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(local_blob_survives_restart_and_provider_admission_without_duplicate_bytes)
{
    const auto provider_path = m_data_dir / "storage";
    const auto blob_root = provider_path / "chunks";
    const std::vector<unsigned char> bytes(1100, 0x5a);
    const auto id = cybou::ComputeChunkId(bytes);
    auto wrong_id = id;
    wrong_id[0] ^= 1;

    {
        cybou::ChunkBlobStore blobs(blob_root);
        BOOST_CHECK(blobs.Put(wrong_id, bytes) == cybou::ChunkBlobPutStatus::INVALID);
        BOOST_CHECK(blobs.Put(id, bytes) == cybou::ChunkBlobPutStatus::STORED);
        BOOST_CHECK(blobs.Put(id, bytes) == cybou::ChunkBlobPutStatus::ALREADY_STORED);
        BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());
    }

    cybou::ChunkBlobStore blobs(blob_root);
    BOOST_CHECK(blobs.Has(id));
    BOOST_CHECK(blobs.Get(id) == bytes);
    BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());

    std::array<unsigned char, 32> network_id{};
    network_id.fill(0x7c);
    cybou::AuthorizedChunk authorized{id};
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(
        std::span<const cybou::AuthorizedChunk>{&authorized, 1});
    BOOST_REQUIRE(commitment);
    cybou::RootPublication publication;
    publication.root_chunk_id = id;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    const uint256 operation_id{uint8_t{1}};
    const auto lookup = [operation_id, publication](const uint256& candidate)
        -> std::optional<cybou::RootPublication> {
        return candidate == operation_id ? std::optional{publication} : std::nullopt;
    };

    {
        cybou::FinalizedChunkStore provider(blobs, provider_path, network_id, 4096);
        BOOST_CHECK(!provider.HasChunk(id));
        BOOST_CHECK(provider.PutChunk(operation_id, id, bytes, commitment->proofs.front(), lookup).status ==
            cybou::ChunkAdmissionStatus::STORED);
        BOOST_CHECK(provider.GetChunk(id) == bytes);
        BOOST_CHECK_EQUAL(provider.UsedBytes(), bytes.size());
        BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());
    }

    std::size_t blob_files{0};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(blob_root)) {
        if (entry.is_regular_file()) ++blob_files;
    }
    BOOST_CHECK_EQUAL(blob_files, 1U);

    // Resetting provider metadata must not delete a local staged/cache blob.
    {
        cybou::FinalizedChunkStore provider(blobs, provider_path, network_id, 4096, true);
        BOOST_CHECK(!provider.HasChunk(id));
    }
    BOOST_CHECK(blobs.Get(id) == bytes);
    BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());
}

BOOST_AUTO_TEST_CASE(runtime_exposes_local_blobs_without_provider_admission)
{
    const auto genesis = cybou::CreateDevGenesisState();
    cybou::NodeRuntimeConfig config{
        .network_definition = cybou::CreateDevNetworkDefinition(
            genesis, cybou::TestPoaFinalizerPublicKey(), cybou::TestNetworkPublicKey()),
        .data_dir = m_data_dir / "runtime",
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));
    const std::vector<unsigned char> bytes(1100, 0x6b);
    const auto id = cybou::ComputeChunkId(bytes);
    BOOST_CHECK(!runtime.HasStorageProvider());
    BOOST_CHECK(runtime.GetChunkBlobStore().Put(id, bytes) == cybou::ChunkBlobPutStatus::STORED);
    BOOST_CHECK(runtime.GetChunkBlobStore().Get(id) == bytes);
    BOOST_CHECK(!runtime.GetFinalizedChunk(id));
}

BOOST_AUTO_TEST_CASE(provider_startup_checks_size_not_content_and_put_heals_damage)
{
    const auto provider_path = m_data_dir / "storage-heal";
    const auto blob_root = provider_path / "chunks";
    const std::vector<unsigned char> bytes(1200, 0x3c);
    const auto id = cybou::ComputeChunkId(bytes);
    std::array<unsigned char, 32> network_id{};
    network_id.fill(0x6d);
    cybou::AuthorizedChunk authorized{id};
    const auto commitment = cybou::BuildChunkAuthorizationCommitment(
        std::span<const cybou::AuthorizedChunk>{&authorized, 1});
    BOOST_REQUIRE(commitment);
    cybou::RootPublication publication;
    publication.root_chunk_id = id;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    const uint256 operation_id{uint8_t{7}};
    const auto lookup = [operation_id, publication](const uint256& candidate)
        -> std::optional<cybou::RootPublication> {
        return candidate == operation_id ? std::optional{publication} : std::nullopt;
    };
    std::filesystem::path blob_file;
    {
        cybou::ChunkBlobStore blobs(blob_root);
        cybou::FinalizedChunkStore provider(blobs, provider_path, network_id, 4096);
        BOOST_REQUIRE(provider.PutChunk(operation_id, id, bytes, commitment->proofs.front(), lookup).status ==
            cybou::ChunkAdmissionStatus::STORED);
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(blob_root)) {
        if (entry.is_regular_file()) blob_file = entry.path();
    }
    BOOST_REQUIRE(!blob_file.empty());
    // Silent bit rot, same size: a size-only startup must not reread content.
    {
        std::fstream file(blob_file, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(100);
        file.put(static_cast<char>(0x00));
    }
    cybou::ChunkBlobStore blobs(blob_root);
    BOOST_CHECK_EQUAL(*blobs.StoredSize(id), bytes.size());
    cybou::FinalizedChunkStore provider(blobs, provider_path, network_id, 4096); // no throw, no hashing
    BOOST_CHECK(provider.HasChunk(id));
    // Content is verified where it is served: damaged bytes never leave.
    BOOST_CHECK(!provider.GetChunk(id));
    BOOST_CHECK(!blobs.Get(id));
    // An owner repairing with the valid bytes heals the provider copy.
    BOOST_CHECK(provider.PutChunk(operation_id, id, bytes, commitment->proofs.front(), lookup).status ==
        cybou::ChunkAdmissionStatus::ALREADY_STORED);
    BOOST_CHECK(provider.GetChunk(id) == bytes);
    BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());
    // A wrong-size file fails startup instead of being served.
    std::filesystem::resize_file(blob_file, bytes.size() - 1);
    cybou::ChunkBlobStore truncated(blob_root);
    BOOST_CHECK_THROW(cybou::FinalizedChunkStore(truncated, provider_path, network_id, 4096), std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()
