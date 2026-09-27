// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_crypto.h>
#include <cybou/storage_store.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_storage_crypto_tests)

BOOST_AUTO_TEST_CASE(storage_chunk_crypto_is_chunked_randomized_and_network_bound)
{
    std::array<unsigned char, 32> network_id{};
    std::array<unsigned char, 32> storage_master_key{};
    network_id[0] = 0x41;
    storage_master_key[0] = 0x93;

    constexpr uint64_t plaintext_size = cybou::STORAGE_OBJECT_CHUNK_SIZE + 37;
    const auto metadata = cybou::CreateStorageObjectMetadata(plaintext_size, 7);
    BOOST_REQUIRE(metadata);
    const auto second_metadata = cybou::CreateStorageObjectMetadata(plaintext_size, 7);
    BOOST_REQUIRE(second_metadata);
    BOOST_CHECK(metadata->object_id != second_metadata->object_id);
    BOOST_CHECK(metadata->salt != second_metadata->salt);
    BOOST_CHECK(!cybou::CreateStorageObjectMetadata(cybou::STORAGE_OBJECT_MAX_BYTES + 1, 0));
    std::array<unsigned char, 32> zero_value{};
    BOOST_CHECK(!cybou::StorageObjectCryptoContext::Create(zero_value, storage_master_key, *metadata));
    BOOST_CHECK(!cybou::StorageObjectCryptoContext::Create(network_id, zero_value, *metadata));

    auto context = cybou::StorageObjectCryptoContext::Create(network_id, storage_master_key, *metadata);
    BOOST_REQUIRE(context);
    BOOST_CHECK_EQUAL(context->ChunkCount(), 2U);
    BOOST_CHECK_EQUAL(context->ExpectedPlaintextChunkSize(0).value(), cybou::STORAGE_OBJECT_CHUNK_SIZE);
    BOOST_CHECK_EQUAL(context->ExpectedPlaintextChunkSize(1).value(), 37U);
    BOOST_CHECK(!context->ExpectedPlaintextChunkSize(2));

    std::vector<unsigned char> first(cybou::STORAGE_OBJECT_CHUNK_SIZE);
    std::vector<unsigned char> last(37);
    for (size_t i = 0; i < first.size(); ++i) first[i] = static_cast<unsigned char>((i * 13 + 5) % 251);
    std::fill(last.begin(), last.end(), 0x6d);
    const auto encrypted_first = context->EncryptChunk(0, first);
    const auto encrypted_last = context->EncryptChunk(1, last);
    BOOST_REQUIRE(encrypted_first);
    BOOST_REQUIRE(encrypted_last);
    BOOST_CHECK(encrypted_first->ciphertext_and_tag != first);
    BOOST_CHECK_EQUAL(encrypted_first->ciphertext_and_tag.size(), first.size() + 16);
    BOOST_CHECK_EQUAL(encrypted_last->ciphertext_and_tag.size(), last.size() + 16);
    BOOST_CHECK(context->DecryptChunk(*encrypted_first) == first);
    BOOST_CHECK(context->DecryptChunk(*encrypted_last) == last);

    std::array<unsigned char, 32> wrong_network = network_id;
    wrong_network[1] ^= 0x80;
    auto wrong_context = cybou::StorageObjectCryptoContext::Create(wrong_network, storage_master_key, *metadata);
    BOOST_REQUIRE(wrong_context);
    BOOST_CHECK(!wrong_context->DecryptChunk(*encrypted_first));

    std::array<cybou::StorageEncryptedChunk, 2> chunks{*encrypted_first, *encrypted_last};
    auto manifest = cybou::BuildStoragePublicManifest(network_id, metadata->object_id, chunks);
    BOOST_REQUIRE(manifest);
    BOOST_CHECK(cybou::VerifyStoragePublicManifest(network_id, *manifest));
    BOOST_CHECK(!cybou::VerifyStoragePublicManifest(zero_value, *manifest));
    BOOST_CHECK(!cybou::VerifyStoragePublicManifest(wrong_network, *manifest));
    BOOST_CHECK_EQUAL(manifest->chunks.size(), 2U);
    BOOST_CHECK_EQUAL(manifest->chunks[0].ciphertext_size, first.size() + 16);

    auto modified_manifest = *manifest;
    modified_manifest.chunks[0].ciphertext_size--;
    BOOST_CHECK(!cybou::VerifyStoragePublicManifest(network_id, modified_manifest));

    auto tampered_chunk = *encrypted_first;
    tampered_chunk.ciphertext_and_tag[0] ^= 1;
    BOOST_CHECK(!context->DecryptChunk(tampered_chunk));
    chunks[0] = tampered_chunk;
    BOOST_CHECK(!cybou::BuildStoragePublicManifest(network_id, metadata->object_id, chunks));
}

BOOST_AUTO_TEST_CASE(storage_empty_object_is_an_authenticated_empty_chunk)
{
    std::array<unsigned char, 32> network_id{};
    std::array<unsigned char, 32> storage_master_key{};
    network_id[0] = 1;
    storage_master_key[0] = 2;
    const auto metadata = cybou::CreateStorageObjectMetadata(0, 0);
    BOOST_REQUIRE(metadata);
    auto context = cybou::StorageObjectCryptoContext::Create(network_id, storage_master_key, *metadata);
    BOOST_REQUIRE(context);
    BOOST_CHECK_EQUAL(context->ChunkCount(), 1U);
    const auto chunk = context->EncryptChunk(0, {});
    BOOST_REQUIRE(chunk);
    BOOST_CHECK_EQUAL(chunk->ciphertext_and_tag.size(), 16U);
    const auto plaintext = context->DecryptChunk(*chunk);
    BOOST_REQUIRE(plaintext);
    BOOST_CHECK(plaintext->empty());
    const std::array<cybou::StorageEncryptedChunk, 1> chunks{*chunk};
    const auto manifest = cybou::BuildStoragePublicManifest(network_id, metadata->object_id, chunks);
    BOOST_REQUIRE(manifest);
    BOOST_CHECK(cybou::VerifyStoragePublicManifest(network_id, *manifest));
}

BOOST_AUTO_TEST_CASE(storage_provider_persists_only_manifest_committed_chunks)
{
    std::array<unsigned char, 32> network_id{};
    std::array<unsigned char, 32> storage_master_key{};
    network_id[0] = 0x51;
    storage_master_key[0] = 0xa7;
    const auto metadata = cybou::CreateStorageObjectMetadata(64, 3);
    BOOST_REQUIRE(metadata);
    auto context = cybou::StorageObjectCryptoContext::Create(network_id, storage_master_key, *metadata);
    BOOST_REQUIRE(context);
    std::vector<unsigned char> plaintext(64, 0x4c);
    const auto chunk = context->EncryptChunk(0, plaintext);
    const auto alternate_chunk = context->EncryptChunk(0, plaintext);
    BOOST_REQUIRE(chunk);
    BOOST_REQUIRE(alternate_chunk);
    const std::array<cybou::StorageEncryptedChunk, 1> chunks{*chunk};
    const auto manifest = cybou::BuildStoragePublicManifest(network_id, metadata->object_id, chunks);
    BOOST_REQUIRE(manifest);

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("cybou-storage-store-" + std::to_string(unique));
    std::filesystem::remove_all(path);
    {
        cybou::StorageObjectStore store(path, network_id, 1 << 20);
        BOOST_CHECK(store.PutChunk(metadata->object_id, *chunk));
        BOOST_CHECK_EQUAL(static_cast<int>(store.PutChunk(metadata->object_id, *chunk).status),
            static_cast<int>(cybou::StorageWriteStatus::ALREADY_STORED));
        BOOST_CHECK_EQUAL(static_cast<int>(store.PutChunk(metadata->object_id, *alternate_chunk).status),
            static_cast<int>(cybou::StorageWriteStatus::CONFLICT));
        BOOST_CHECK(!store.GetChunk(metadata->object_id, 0));
        BOOST_CHECK_EQUAL(static_cast<int>(store.CommitManifest(*manifest).status),
            static_cast<int>(cybou::StorageWriteStatus::STORED));
        BOOST_CHECK_EQUAL(static_cast<int>(store.CommitManifest(*manifest).status),
            static_cast<int>(cybou::StorageWriteStatus::ALREADY_STORED));
        const auto fetched = store.GetChunk(metadata->object_id, 0);
        BOOST_REQUIRE(fetched);
        BOOST_CHECK(fetched->ciphertext_and_tag == chunk->ciphertext_and_tag);
    }
    {
        cybou::StorageObjectStore reopened(path, network_id, 1 << 20);
        BOOST_CHECK_EQUAL(reopened.UsedBytes(),
            chunk->ciphertext_and_tag.size() + 52 + cybou::EncodeStoragePublicManifest(network_id, *manifest)->size());
        const auto fetched_manifest = reopened.GetManifest(metadata->object_id);
        BOOST_REQUIRE(fetched_manifest);
        BOOST_CHECK(fetched_manifest->commitment == manifest->commitment);
        BOOST_CHECK(reopened.GetChunk(metadata->object_id, 0));
    }
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_CASE(storage_provider_enforces_capacity_and_network_binding)
{
    std::array<unsigned char, 32> network_id{};
    std::array<unsigned char, 32> other_network{};
    network_id[0] = 1;
    other_network[0] = 2;
    std::array<unsigned char, 32> storage_master_key{};
    storage_master_key[0] = 3;
    const auto metadata = cybou::CreateStorageObjectMetadata(1, 0);
    BOOST_REQUIRE(metadata);
    auto context = cybou::StorageObjectCryptoContext::Create(network_id, storage_master_key, *metadata);
    BOOST_REQUIRE(context);
    const auto chunk = context->EncryptChunk(0, std::array<unsigned char, 1>{0x42});
    BOOST_REQUIRE(chunk);
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
        ("cybou-storage-capacity-" + std::to_string(unique));
    std::filesystem::remove_all(path);
    {
        cybou::StorageObjectStore store(path, network_id, 1);
        BOOST_CHECK_EQUAL(static_cast<int>(store.PutChunk(metadata->object_id, *chunk).status),
            static_cast<int>(cybou::StorageWriteStatus::CAPACITY_EXCEEDED));
        BOOST_CHECK_EQUAL(store.UsedBytes(), 0U);
    }
    {
        cybou::StorageObjectStore store(path, other_network, 1 << 20);
        BOOST_CHECK_EQUAL(static_cast<int>(store.PutChunk(metadata->object_id, *chunk).status),
            static_cast<int>(cybou::StorageWriteStatus::INVALID));
    }
    std::filesystem::remove_all(path);
}

BOOST_AUTO_TEST_SUITE_END()
