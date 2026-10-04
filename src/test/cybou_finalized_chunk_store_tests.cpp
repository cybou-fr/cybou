// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/finalized_chunk_store.h>
#include <cybou/protocol_limits.h>
#include <cybou/storage_audit.h>
#include <cybou/hex.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif


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
    BOOST_CHECK_THROW((cybou::FinalizedChunkStore{blobs, path, other_network_id, 4096}), cybou::NetworkMismatchError);
    {
        cybou::FinalizedChunkStore store(blobs, path, network_binding, 4096);
        BOOST_CHECK(store.UsedBytes() == 0);
    }
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(finalized_chunk_store_prunes_admitted_chunk_on_object_revocation)
{
    AuthorizedFixture fixture;
    cybou::ChunkBlobStore blobs({}, true);
    cybou::FinalizedChunkStore store(blobs, {}, fixture.network_binding, 4096);
    BOOST_REQUIRE(store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, fixture.Lookup()).status ==
        cybou::ChunkAdmissionStatus::STORED);
    BOOST_CHECK(store.HasChunk(fixture.chunk_id));
    BOOST_CHECK_EQUAL(store.UsedBytes(), fixture.bytes.size());

    // Prune admitted chunk (DEC-271)
    BOOST_CHECK_EQUAL(store.PurgePublication(fixture.operation_id), 1U);
    BOOST_CHECK(!store.HasChunk(fixture.chunk_id));
    BOOST_CHECK_EQUAL(store.UsedBytes(), 0U);
    BOOST_CHECK(!blobs.Has(fixture.chunk_id));

    // Pruning an unknown or already pruned chunk returns false
    BOOST_CHECK_EQUAL(store.PurgePublication(fixture.operation_id), 0U);
}

BOOST_AUTO_TEST_CASE(purge_preserves_shared_chunks_until_last_publication_is_revoked)
{
    AuthorizedFixture fixture;
    const cybou::Hash256 other{uint8_t{2}};
    const auto lookup = [&](const cybou::Hash256& id) -> std::optional<cybou::RootPublication> {
        return id == fixture.operation_id || id == other ? std::optional{fixture.publication} : std::nullopt;
    };
    cybou::ChunkBlobStore blobs({}, true);
    cybou::FinalizedChunkStore store(blobs, {}, fixture.network_binding, 4096);
    BOOST_REQUIRE(store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, lookup));
    BOOST_REQUIRE(store.PutChunk(other, fixture.chunk_id, fixture.bytes, fixture.proof, lookup));
    BOOST_CHECK_EQUAL(store.PurgePublication(fixture.operation_id), 0U);
    BOOST_CHECK_EQUAL(store.UsedBytes(), fixture.bytes.size());
    BOOST_CHECK_EQUAL(store.RetryPendingPurges(), 0U);
    BOOST_CHECK(store.GetChunkAuthorizationProof(other, fixture.chunk_id, lookup));
    BOOST_CHECK_EQUAL(store.PurgePublication(other), 1U);
    BOOST_CHECK_EQUAL(store.UsedBytes(), 0U);
    BOOST_CHECK(!blobs.Has(fixture.chunk_id));
    BOOST_CHECK_EQUAL(store.PurgePublication(other), 0U);
}

BOOST_AUTO_TEST_CASE(purge_recovers_unlink_before_metadata_commit_and_missed_revocation_event)
{
    AuthorizedFixture fixture;
    const auto path = std::filesystem::temp_directory_path() / "cybou-purge-crash-recovery";
    const auto ns = "chunk-store/" + cybou::HexEncode(fixture.network_binding);
    const auto hex = cybou::HexEncode(fixture.chunk_id);
    for (bool unlinked : {false, true}) {
        {
            cybou::ChunkBlobStore blobs(path / "chunks", false, true);
            cybou::FinalizedChunkStore store(blobs, path, fixture.network_binding, 4096, true);
            BOOST_REQUIRE(store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, fixture.Lookup()));
            if (unlinked) BOOST_REQUIRE(blobs.Remove(fixture.chunk_id));
        }
        if (unlinked) {
            // Durable intent survived; process died after unlink, before its accounting commit.
            cybou::KVStore db({.path = path / "metadata"});
            cybou::KVStore::Batch batch;
            batch.Erase(ns + "/publication-chunk/" + fixture.operation_id.GetHex() + "/" + hex);
            batch.Write(ns + "/purge/" + hex, static_cast<uint64_t>(fixture.bytes.size()));
            db.WriteBatch(batch, true);
        }
        {
            cybou::ChunkBlobStore blobs(path / "chunks", false);
            cybou::FinalizedChunkStore store(blobs, path, fixture.network_binding, 4096);
            if (!unlinked) {
                // Finalized revocation persisted, but its local event never ran.
                store.PurgeRevokedPublications([](const cybou::Hash256&) -> std::optional<cybou::RootPublication> {
                    return std::nullopt;
                });
            }
            BOOST_CHECK_EQUAL(store.UsedBytes(), 0U);
            BOOST_CHECK(!blobs.Has(fixture.chunk_id));
            BOOST_CHECK_EQUAL(store.RetryPendingPurges(), 0U);
        }
    }
    std::filesystem::remove_all(path);
}

#ifdef _WIN32
BOOST_AUTO_TEST_CASE(purge_keeps_quota_and_retries_locked_blob_across_restart)
{
    AuthorizedFixture fixture;
    for (bool readmit : {false, true}) {
        const auto path = std::filesystem::temp_directory_path() / "cybou-purge-locked-blob";
        const auto hex = cybou::HexEncode(fixture.chunk_id);
        const auto blob_path = path / "chunks" / hex.substr(0, 2) / hex.substr(2, 2) / hex;
        cybou::ChunkBlobStore blobs(path / "chunks", false, true);
        {
            cybou::FinalizedChunkStore store(blobs, path, fixture.network_binding, 4096, true);
            BOOST_REQUIRE(store.PutChunk(fixture.operation_id, fixture.chunk_id, fixture.bytes, fixture.proof, fixture.Lookup()));
        }
        const HANDLE locked = CreateFileW(blob_path.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        BOOST_REQUIRE(locked != INVALID_HANDLE_VALUE);
        struct CloseHandleOnExit {
            HANDLE handle;
            ~CloseHandleOnExit() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
        } guard{locked};
        {
            cybou::FinalizedChunkStore store(blobs, path, fixture.network_binding, 4096);
            BOOST_CHECK_EQUAL(store.PurgePublication(fixture.operation_id), 0U);
            BOOST_CHECK_EQUAL(store.UsedBytes(), fixture.bytes.size());
            BOOST_CHECK(blobs.Has(fixture.chunk_id));
            BOOST_CHECK_EQUAL(store.RetryPendingPurges(), 0U);
        }
        {
            cybou::FinalizedChunkStore store(blobs, path, fixture.network_binding, 4096);
            BOOST_CHECK_EQUAL(store.UsedBytes(), fixture.bytes.size());
            const cybou::Hash256 other{uint8_t{2}};
            if (readmit) {
                const auto lookup = [&](const cybou::Hash256& id) -> std::optional<cybou::RootPublication> {
                    return id == other ? std::optional{fixture.publication} : std::nullopt;
                };
                BOOST_REQUIRE(store.PutChunk(other, fixture.chunk_id, fixture.bytes, fixture.proof, lookup));
                BOOST_CHECK_EQUAL(store.RetryPendingPurges(), 0U);
            }
            BOOST_REQUIRE(CloseHandle(guard.handle));
            guard.handle = INVALID_HANDLE_VALUE;
            if (readmit) {
                BOOST_CHECK_EQUAL(store.RetryPendingPurges(), 0U);
                BOOST_CHECK(store.HasChunk(fixture.chunk_id));
                BOOST_CHECK_EQUAL(store.UsedBytes(), fixture.bytes.size());
                BOOST_CHECK_EQUAL(store.PurgePublication(other), 1U);
            } else {
                BOOST_CHECK_EQUAL(store.RetryPendingPurges(), 1U);
            }
            BOOST_CHECK_EQUAL(store.UsedBytes(), 0U);
            BOOST_CHECK(!blobs.Has(fixture.chunk_id));
        }
        std::filesystem::remove_all(path);
    }
}
#endif

BOOST_AUTO_TEST_CASE(storage_audit_challenge_and_proof_verification)
{
    std::vector<unsigned char> data(1200);
    for (size_t i{0}; i < data.size(); ++i) data[i] = static_cast<unsigned char>(i * 37 + 13);
    const auto chunk_id = cybou::ComputeChunkId(data);

    cybou::StorageAuditChallenge challenge{
        .chunk_id = chunk_id,
        .byte_offset = 100,
        .nonce = {0x01, 0x02, 0x03},
    };

    const auto proof = cybou::CreateStorageAuditProof(challenge, data);
    BOOST_REQUIRE(proof.has_value());
    BOOST_CHECK(proof->chunk_id == chunk_id);
    BOOST_CHECK_EQUAL(proof->byte_offset, 100U);
    BOOST_CHECK(cybou::VerifyStorageAuditProof(*proof, data));

    // Tampered data fails verification
    auto tampered_data = data;
    tampered_data[100] ^= 0xff;
    BOOST_CHECK(!cybou::VerifyStorageAuditProof(*proof, tampered_data));

    // Tampered nonce fails verification
    auto tampered_proof = *proof;
    tampered_proof.nonce[0] ^= 0xff;
    BOOST_CHECK(!cybou::VerifyStorageAuditProof(tampered_proof, data));

    // Tampered offset fails verification
    auto tampered_offset_proof = *proof;
    tampered_offset_proof.byte_offset = 200;
    BOOST_CHECK(!cybou::VerifyStorageAuditProof(tampered_offset_proof, data));

    // Challenge with wrong chunk_id fails to create proof
    auto invalid_challenge = challenge;
    invalid_challenge.chunk_id.fill(0xee);
    BOOST_CHECK(!cybou::CreateStorageAuditProof(invalid_challenge, data).has_value());
}

BOOST_AUTO_TEST_SUITE_END()
