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
#include <stdexcept>
#include <vector>

namespace {
class CommitAckLostProvider final : public cybou::StorageObjectProvider {
public:
    explicit CommitAckLostProvider(cybou::StorageObjectStore& store) : m_store{store} {}
    bool SupportsAbortUncommittedUpload() const override { return true; }
    cybou::StorageWriteResult PutChunk(const cybou::StorageObjectId& object_id,
        const cybou::StorageEncryptedChunk& chunk) override
    {
        return m_store.PutChunk(object_id, chunk);
    }
    cybou::StorageWriteResult CommitManifest(const cybou::StoragePublicManifest& manifest) override
    {
        auto result = m_store.CommitManifest(manifest);
        if (result && m_drop_commit_response) {
            result.response_received = false;
            m_drop_commit_response = false;
        }
        return result;
    }
    bool AbortUncommittedObject(const cybou::StorageObjectId& object_id, uint32_t chunk_count) override
    {
        return m_store.AbortUncommittedObject(object_id, chunk_count);
    }
    std::optional<cybou::StoragePublicManifest> GetManifest(
        const cybou::StorageObjectId& object_id) const override
    {
        return m_store.GetManifest(object_id);
    }
    std::optional<cybou::StorageEncryptedChunk> GetChunk(
        const cybou::StorageObjectId& object_id, uint32_t index) const override
    {
        return m_store.GetChunk(object_id, index);
    }

private:
    cybou::StorageObjectStore& m_store;
    bool m_drop_commit_response{true};
};

class CrashAfterFirstPutProvider final : public cybou::StorageObjectProvider {
public:
    explicit CrashAfterFirstPutProvider(cybou::StorageObjectStore& store) : m_store{store} {}
    bool SupportsAbortUncommittedUpload() const override { return true; }
    cybou::StorageWriteResult PutChunk(const cybou::StorageObjectId& object_id,
        const cybou::StorageEncryptedChunk& chunk) override
    {
        const auto result = m_store.PutChunk(object_id, chunk);
        if (m_throw_after_put) {
            m_throw_after_put = false;
            throw std::runtime_error("simulated process interruption after provider PUT");
        }
        return result;
    }
    cybou::StorageWriteResult CommitManifest(const cybou::StoragePublicManifest& manifest) override
    {
        return m_store.CommitManifest(manifest);
    }
    bool AbortUncommittedObject(const cybou::StorageObjectId& object_id, uint32_t chunk_count) override
    {
        return m_store.AbortUncommittedObject(object_id, chunk_count);
    }
    std::optional<cybou::StoragePublicManifest> GetManifest(
        const cybou::StorageObjectId& object_id) const override
    {
        return m_store.GetManifest(object_id);
    }
    std::optional<cybou::StorageEncryptedChunk> GetChunk(
        const cybou::StorageObjectId& object_id, uint32_t index) const override
    {
        return m_store.GetChunk(object_id, index);
    }

private:
    cybou::StorageObjectStore& m_store;
    bool m_throw_after_put{true};
};

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

        cybou::StorageObjectStore uncertain_store(root / "uncertain-provider", network_id,
            4U * cybou::STORAGE_OBJECT_CHUNK_SIZE);
        CommitAckLostProvider uncertain_provider{uncertain_store};
        cybou::StorageService uncertain_service(network_id, *account, keys, uncertain_provider,
            root / "uncertain-manifests");
        const auto uncertain_upload = uncertain_service.UploadFile(input_path, PASSWORD);
        BOOST_REQUIRE(uncertain_upload.status == cybou::StorageTransferStatus::COMMIT_UNCERTAIN);
        BOOST_CHECK(uncertain_upload.object_id != cybou::StorageObjectId{});
        const auto uncertain_download = uncertain_service.DownloadFile(uncertain_upload.object_id,
            root / "uncertain-output.bin", PASSWORD);
        BOOST_REQUIRE(uncertain_download.status == cybou::StorageTransferStatus::RETRIEVED);
        BOOST_CHECK(ReadFile(root / "uncertain-output.bin") == original);
        const auto reconciled_upload = uncertain_service.UploadFile(input_path, PASSWORD);
        BOOST_REQUIRE(reconciled_upload.status == cybou::StorageTransferStatus::STORED);
        BOOST_CHECK(uncertain_service.DownloadFile(uncertain_upload.object_id,
            root / "uncertain-output-after-reconcile.bin", PASSWORD).status ==
            cybou::StorageTransferStatus::RETRIEVED);

        cybou::StorageObjectStore crash_store(root / "crash-provider", network_id,
            4U * cybou::STORAGE_OBJECT_CHUNK_SIZE);
        CrashAfterFirstPutProvider crash_provider{crash_store};
        const auto crash_manifest_dir = root / "crash-manifests";
        cybou::StorageService interrupted_service(network_id, *account, keys, crash_provider, crash_manifest_dir);
        BOOST_CHECK_THROW(interrupted_service.UploadFile(input_path, PASSWORD), std::runtime_error);
        BOOST_CHECK_EQUAL(crash_store.StagedObjectCount(), 1U);
        cybou::StorageService recovered_service(network_id, *account, keys, crash_store, crash_manifest_dir);
        const auto recovered_upload = recovered_service.UploadFile(input_path, PASSWORD);
        BOOST_REQUIRE(recovered_upload.status == cybou::StorageTransferStatus::STORED);
        BOOST_CHECK_EQUAL(crash_store.StagedObjectCount(), 0U);
        BOOST_CHECK(!std::filesystem::exists(crash_manifest_dir / "storage-upload-journal.cybv2"));

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
