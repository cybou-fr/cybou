// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <cybou/chunk_blob_store.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/network_genesis.h>
#include <cybou/node_runtime.h>
#include <test/cybou_test_helpers.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(cybou_chunk_blob_store_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(available_disk_is_an_os_measurement_not_storage_policy)
{
    cybou::ChunkBlobStore memory({}, true, false, 4096);
    BOOST_CHECK(!memory.AvailableDiskBytes());
    const auto parent = m_data_dir / "disk-observation";
    std::filesystem::create_directories(parent);
    cybou::ChunkBlobStore disk(parent / "chunks", false, false, 4096);
    BOOST_CHECK(disk.AvailableDiskBytes().has_value()); // parent exists before first blob
    std::filesystem::create_directories(parent / "chunks");
    BOOST_CHECK(disk.AvailableDiskBytes().has_value());
    BOOST_CHECK_EQUAL(disk.UsedBytes(), 0U);
    BOOST_CHECK_EQUAL(disk.CapacityBytes(), 4096U);
    std::filesystem::remove(parent / "chunks"); // empty fixture directories only
    std::filesystem::remove(parent);
    BOOST_CHECK(!disk.AvailableDiskBytes());
}

BOOST_AUTO_TEST_CASE(local_blob_survives_restart_and_storage_admission_without_duplicate_bytes)
{
    const auto storage_path = m_data_dir / "storage";
    const auto blob_root = storage_path / "chunks";
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

    std::array<unsigned char, 32> network_binding{};
    network_binding.fill(0x7c);
    cybou::AuthorizedChunk authorized{id};
    const auto commitment = cybou::BuildChunkAuthorizationTree(
        std::span<const cybou::AuthorizedChunk>{&authorized, 1});
    BOOST_REQUIRE(commitment);
    cybou::RootPublication publication;
    publication.root_chunk_id = id;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    const cybou::Hash256 operation_id{uint8_t{1}};
    const auto lookup = [operation_id, publication](const cybou::Hash256& candidate)
        -> std::optional<cybou::RootPublication> {
        return candidate == operation_id ? std::optional{publication} : std::nullopt;
    };

    {
        cybou::FinalizedChunkStore provider(blobs, storage_path, network_binding, 4096);
        BOOST_CHECK(!provider.HasChunk(id));
        BOOST_CHECK(provider.PutChunk(operation_id, id, bytes, commitment->Proof(0), lookup).status ==
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
        cybou::FinalizedChunkStore provider(blobs, storage_path, network_binding, 4096, true);
        BOOST_CHECK(!provider.HasChunk(id));
    }
    BOOST_CHECK(blobs.Get(id) == bytes);
    BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());
}

BOOST_AUTO_TEST_CASE(runtime_exposes_local_blobs_without_storage_admission)
{
    const auto genesis = cybou::CreateTestGenesisState();
    cybou::NodeRuntimeConfig config{
        .network_genesis = cybou::CreateTestNetworkGenesis(
            genesis, cybou::TestPoaFinalizerPublicKey(), cybou::TestNetworkPublicKey()),
        .data_dir = m_data_dir / "runtime",
        .memory_only = true,
        .wipe_data = true,
        .storage_capacity_bytes = 0, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));
    const std::vector<unsigned char> bytes(1100, 0x6b);
    const auto id = cybou::ComputeChunkId(bytes);
    BOOST_REQUIRE(runtime.LocalStorageId());
    BOOST_CHECK_EQUAL(runtime.GetDiagnostics().storage_capacity, 0U);
    BOOST_CHECK(runtime.GetChunkBlobStore().Put(id, bytes) == cybou::ChunkBlobPutStatus::STORED);
    BOOST_CHECK(runtime.GetChunkBlobStore().Get(id) == bytes);
    BOOST_CHECK(!runtime.GetFinalizedChunk(id));
}

BOOST_AUTO_TEST_CASE(storage_startup_checks_size_not_content_and_put_heals_damage)
{
    const auto storage_path = m_data_dir / "storage-heal";
    const auto blob_root = storage_path / "chunks";
    const std::vector<unsigned char> bytes(1200, 0x3c);
    const auto id = cybou::ComputeChunkId(bytes);
    std::array<unsigned char, 32> network_binding{};
    network_binding.fill(0x6d);
    cybou::AuthorizedChunk authorized{id};
    const auto commitment = cybou::BuildChunkAuthorizationTree(
        std::span<const cybou::AuthorizedChunk>{&authorized, 1});
    BOOST_REQUIRE(commitment);
    cybou::RootPublication publication;
    publication.root_chunk_id = id;
    publication.chunk_authorization_root = commitment->root;
    publication.chunk_count = commitment->chunk_count;
    const cybou::Hash256 operation_id{uint8_t{7}};
    const auto lookup = [operation_id, publication](const cybou::Hash256& candidate)
        -> std::optional<cybou::RootPublication> {
        return candidate == operation_id ? std::optional{publication} : std::nullopt;
    };
    std::filesystem::path blob_file;
    {
        cybou::ChunkBlobStore blobs(blob_root);
        cybou::FinalizedChunkStore provider(blobs, storage_path, network_binding, 4096);
        BOOST_REQUIRE(provider.PutChunk(operation_id, id, bytes, commitment->Proof(0), lookup).status ==
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
    cybou::FinalizedChunkStore provider(blobs, storage_path, network_binding, 4096); // no throw, no hashing
    BOOST_CHECK(provider.HasChunk(id));
    // Content is verified where it is served: damaged bytes never leave.
    BOOST_CHECK(!provider.GetChunk(id));
    BOOST_CHECK(!blobs.Get(id));
    // An owner repairing with the valid bytes heals the provider copy.
    BOOST_CHECK(provider.PutChunk(operation_id, id, bytes, commitment->Proof(0), lookup).status ==
        cybou::ChunkAdmissionStatus::ALREADY_STORED);
    BOOST_CHECK(provider.GetChunk(id) == bytes);
    BOOST_CHECK_EQUAL(blobs.UsedBytes(), bytes.size());
    // A wrong-size file fails startup instead of being served.
    std::filesystem::resize_file(blob_file, bytes.size() - 1);
    cybou::ChunkBlobStore truncated(blob_root);
    BOOST_CHECK_THROW(cybou::FinalizedChunkStore(truncated, storage_path, network_binding, 4096), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(explicit_capacity_bounds_whole_store_and_provider_budget)
{
    // DEC-275: provider obligations <= floor(2V/3); the rest of V is a local reserve.
    BOOST_CHECK_EQUAL(cybou::ProviderBudgetBytes(0), 0U);
    BOOST_CHECK_EQUAL(cybou::ProviderBudgetBytes(3), 2U);
    BOOST_CHECK_EQUAL(cybou::ProviderBudgetBytes(5), 3U);
    BOOST_CHECK_EQUAL(cybou::ProviderBudgetBytes(150ULL << 30), 100ULL << 30);
    BOOST_CHECK_EQUAL(cybou::ProviderBudgetBytes(std::numeric_limits<uint64_t>::max()),
        std::numeric_limits<uint64_t>::max() / 3 * 2);

    const std::vector<unsigned char> first(1100, 0x11), second(1100, 0x22);
    for (const bool memory_only : {true, false}) {
        cybou::ChunkBlobStore blobs(m_data_dir / (memory_only ? "cap-mem" : "cap-disk"), memory_only, true, 2000);
        BOOST_CHECK(blobs.Put(cybou::ComputeChunkId(first), first) == cybou::ChunkBlobPutStatus::STORED);
        BOOST_CHECK(blobs.Put(cybou::ComputeChunkId(second), second) == cybou::ChunkBlobPutStatus::CAPACITY_EXCEEDED);
        BOOST_CHECK(blobs.Put(cybou::ComputeChunkId(first), first) == cybou::ChunkBlobPutStatus::ALREADY_STORED);
        BOOST_CHECK_EQUAL(blobs.UsedBytes(), first.size());
        BOOST_CHECK(!blobs.Has(cybou::ComputeChunkId(second)));
    }
}

BOOST_AUTO_TEST_CASE(runtime_requires_minimum_capacity_and_defaults_to_it)
{
    const auto genesis = cybou::CreateTestGenesisState();
    const auto make = [&](std::optional<uint64_t> capacity, bool memory_only) {
        return cybou::NodeRuntimeConfig{
            .network_genesis = cybou::CreateTestNetworkGenesis(
                genesis, cybou::TestPoaFinalizerPublicKey(), cybou::TestNetworkPublicKey()),
            .data_dir = m_data_dir / "capacity-runtime",
            .memory_only = memory_only,
            .wipe_data = true,
            .storage_capacity_bytes = capacity, .operation_work_bits = 0};
    };
    BOOST_CHECK_THROW(cybou::CybouNodeRuntime{make(cybou::MIN_STORAGE_CAPACITY_BYTES - 1, false)},
        std::invalid_argument);
    BOOST_CHECK_THROW(cybou::CybouNodeRuntime{make(0, false)}, std::invalid_argument);

    cybou::CybouNodeRuntime runtime{make(std::nullopt, true)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));
    const auto diagnostics = runtime.GetDiagnostics();
    BOOST_CHECK_EQUAL(diagnostics.local_storage_capacity, cybou::DEFAULT_STORAGE_CAPACITY_BYTES);
    BOOST_CHECK_EQUAL(diagnostics.storage_capacity, cybou::ProviderBudgetBytes(cybou::DEFAULT_STORAGE_CAPACITY_BYTES));
    BOOST_CHECK_EQUAL(runtime.GetChunkBlobStore().CapacityBytes(), cybou::DEFAULT_STORAGE_CAPACITY_BYTES);
}

BOOST_AUTO_TEST_SUITE_END()
