// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/keystore.h>
#include <cybou/storage_service.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
std::vector<unsigned char> ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

bool WriteFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    return output.good();
}
} // namespace

BOOST_AUTO_TEST_SUITE(cybou_storage_service_tests)

BOOST_AUTO_TEST_CASE(file_roundtrip_survives_restart_and_aborted_upload_is_cleaned)
{
    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::temp_directory_path() /
        ("cybou-storage-service-" + std::to_string(unique));
    std::filesystem::create_directories(root);
    const auto identity_path = root / "identity.cybou";
    const auto key_ring_path = root / "identity.storage-keys.cybou";
    const auto provider_path = root / "provider";
    const auto manifest_dir = root / "private-manifests";
    const auto input_path = root / "input.bin";
    const auto output_path = root / "output.bin";
    const auto empty_path = root / "empty.bin";
    const auto empty_output_path = root / "empty-output.bin";
    const auto interrupted_path = root / "interrupted.bin";
    constexpr std::string_view PASSWORD{"correct horse battery staple"};

    std::vector<unsigned char> original(cybou::STORAGE_OBJECT_CHUNK_SIZE + 73);
    for (size_t i = 0; i < original.size(); ++i) original[i] = static_cast<unsigned char>((i * 37) & 0xff);
    BOOST_REQUIRE(WriteFile(input_path, original));
    BOOST_REQUIRE(WriteFile(empty_path, {}));
    std::vector<unsigned char> interrupted(cybou::STORAGE_OBJECT_CHUNK_SIZE * 2, 0x5a);
    BOOST_REQUIRE(WriteFile(interrupted_path, interrupted));

    std::array<unsigned char, 32> network_id{};
    network_id[0] = 0x42;
    cybou::CybouKeyStore keys;
    BOOST_REQUIRE(keys.GenerateNew());
    const auto account = keys.GetAccountId();
    BOOST_REQUIRE(account);
    BOOST_REQUIRE(keys.SaveToFile(identity_path, PASSWORD));
    BOOST_REQUIRE(keys.CreateStorageKeyRing(key_ring_path, PASSWORD));

    cybou::StorageObjectId object_id{};
    cybou::StorageObjectId empty_object_id{};
    cybou::StorageChunkId expected_commitment{};
    {
        cybou::StorageObjectStore store(provider_path, network_id,
            cybou::STORAGE_OBJECT_CHUNK_SIZE + (256U << 10));
        cybou::StorageService service(network_id, *account, keys, store, manifest_dir);
        const auto uploaded = service.UploadFile(input_path, PASSWORD);
        BOOST_REQUIRE(uploaded.status == cybou::StorageTransferStatus::STORED);
        object_id = uploaded.object_id;
        expected_commitment = uploaded.manifest_commitment;
        const auto manifest = store.GetManifest(object_id);
        BOOST_REQUIRE(manifest);
        BOOST_CHECK_EQUAL(manifest->chunk_count, 2U);
        BOOST_CHECK(manifest->commitment == expected_commitment);
        const auto first_chunk = store.GetChunk(object_id, 0);
        BOOST_REQUIRE(first_chunk);
        BOOST_CHECK(first_chunk->ciphertext_and_tag.size() == cybou::STORAGE_OBJECT_CHUNK_SIZE + 16);
        BOOST_CHECK(!std::equal(original.begin(), original.begin() + cybou::STORAGE_OBJECT_CHUNK_SIZE,
            first_chunk->ciphertext_and_tag.begin()));

        const auto empty_upload = service.UploadFile(empty_path, PASSWORD);
        BOOST_REQUIRE(empty_upload.status == cybou::StorageTransferStatus::STORED);
        empty_object_id = empty_upload.object_id;
        const auto empty_manifest = store.GetManifest(empty_object_id);
        BOOST_REQUIRE(empty_manifest);
        BOOST_CHECK_EQUAL(empty_manifest->chunk_count, 1U);
    }

    cybou::CybouKeyStore restored_keys;
    BOOST_REQUIRE(restored_keys.LoadFromFile(identity_path, PASSWORD));
    BOOST_REQUIRE(restored_keys.LoadStorageKeyRing(key_ring_path, PASSWORD));
    {
        cybou::StorageObjectStore reopened_store(provider_path, network_id,
            cybou::STORAGE_OBJECT_CHUNK_SIZE + (256U << 10));
        cybou::StorageService restored_service(network_id, *account, restored_keys,
            reopened_store, manifest_dir);
        const auto downloaded = restored_service.DownloadFile(object_id, output_path, PASSWORD);
        BOOST_REQUIRE(downloaded.status == cybou::StorageTransferStatus::RETRIEVED);
        BOOST_CHECK(downloaded.manifest_commitment == expected_commitment);
        BOOST_CHECK(ReadFile(output_path) == original);
        BOOST_CHECK(restored_service.DownloadFile(object_id, output_path, PASSWORD).status ==
            cybou::StorageTransferStatus::IO_ERROR);
        BOOST_CHECK(restored_service.DownloadFile(empty_object_id, empty_output_path, PASSWORD).status ==
            cybou::StorageTransferStatus::RETRIEVED);
        BOOST_CHECK(ReadFile(empty_output_path).empty());
    }

    {
        const auto interrupted_provider = root / "interrupted-provider";
        cybou::StorageObjectStore small_store(interrupted_provider, network_id,
            cybou::STORAGE_OBJECT_CHUNK_SIZE + 1024);
        cybou::StorageService service(network_id, *account, restored_keys, small_store,
            root / "interrupted-manifests");
        const auto failed = service.UploadFile(interrupted_path, PASSWORD);
        BOOST_CHECK(failed.status == cybou::StorageTransferStatus::PROVIDER_ERROR);
        BOOST_CHECK_EQUAL(small_store.UsedBytes(), 0U);
    }

    std::filesystem::remove_all(root);
}

BOOST_AUTO_TEST_SUITE_END()
