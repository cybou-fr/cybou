// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_SERVICE_H
#define CYBOU_STORAGE_SERVICE_H

#include <cybou/keystore.h>
#include <cybou/storage_store.h>

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cybou::p2p { class PeerManager; }

namespace cybou {

enum class StorageTransferStatus : uint8_t {
    STORED,
    RETRIEVED,
    INVALID,
    KEY_UNAVAILABLE,
    NOT_FOUND,
    IO_ERROR,
    PROVIDER_ERROR,
    INTEGRITY_ERROR,
    CLEANUP_FAILED,
    COMMIT_UNCERTAIN,
};

struct StorageTransferResult {
    StorageTransferStatus status{StorageTransferStatus::INVALID};
    StorageObjectId object_id{};
    StorageChunkId manifest_commitment{};
    size_t target_replicas{1};
    size_t committed_replicas{0};
    explicit operator bool() const
    {
        return status == StorageTransferStatus::STORED || status == StorageTransferStatus::RETRIEVED;
    }
};

/** Locally decrypted file metadata. The filename is never sent to providers. */
struct StorageFileInfo {
    StorageObjectId object_id{};
    StorageChunkId manifest_commitment{};
    std::string filename;
    uint64_t size{0};
    uint32_t key_epoch{0};
    bool pending_verification{false};
};

/** One connected CYP2 storage provider, used by a caller-owned peer worker. */
class PeerStorageProvider final : public StorageObjectProvider {
public:
    PeerStorageProvider(p2p::PeerManager& peers, std::string address, uint16_t port);
    bool SupportsAbortUncommittedUpload() const override;
    StorageWriteResult PutChunk(const StorageObjectId& object_id,
        const StorageEncryptedChunk& chunk) override;
    StorageWriteResult CommitManifest(const StoragePublicManifest& manifest) override;
    bool AbortUncommittedObject(const StorageObjectId& object_id, uint32_t chunk_count) override;
    std::optional<StoragePublicManifest> GetManifest(const StorageObjectId& object_id) const override;
    std::optional<StorageEncryptedChunk> GetChunk(
        const StorageObjectId& object_id, uint32_t index) const override;

private:
    p2p::PeerManager& m_peers;
    std::string m_address;
    uint16_t m_port{0};
};

/** Bounded client-side replica placement over already connected CAP_STORAGE peers. */
class StoragePlacement final : public StorageObjectProvider {
public:
    static constexpr size_t MAX_REPLICAS{3};

    explicit StoragePlacement(p2p::PeerManager& peers, size_t desired_replicas = MAX_REPLICAS);
    bool SupportsAbortUncommittedUpload() const override;
    StorageWriteResult PutChunk(const StorageObjectId& object_id,
        const StorageEncryptedChunk& chunk) override;
    StorageWriteResult CommitManifest(const StoragePublicManifest& manifest) override;
    bool AbortUncommittedObject(const StorageObjectId& object_id, uint32_t chunk_count) override;
    std::optional<StoragePublicManifest> GetManifest(const StorageObjectId& object_id) const override;
    std::optional<StorageEncryptedChunk> GetChunk(
        const StorageObjectId& object_id, uint32_t index) const override;

    /** Current placement size and confirmed manifest acknowledgements. */
    std::pair<size_t, size_t> DurabilityState(const StorageObjectId& object_id) const override;

private:
    using Endpoint = std::pair<std::string, uint16_t>;
    std::vector<Endpoint> EndpointsForObject(const StorageObjectId& object_id, bool pin);
    std::vector<Endpoint> ReadEndpoints(const StorageObjectId& object_id) const;

    p2p::PeerManager& m_peers;
    size_t m_desired_replicas{MAX_REPLICAS};
    mutable std::mutex m_mutex;
    std::map<StorageObjectId, std::vector<Endpoint>> m_placements;
    std::map<StorageObjectId, size_t> m_committed_replicas;
};

/** Client-side file transfer through either a local or a connected peer provider. */
class StorageService final {
public:
    StorageService(std::span<const unsigned char, 32> network_id, AccountId account_id,
        const CybouKeyStore& keystore, StorageObjectProvider& store,
        std::filesystem::path private_manifest_dir);

    StorageTransferResult UploadFile(
        const std::filesystem::path& source, std::string_view vault_password);
    /** List locally indexed uploads without contacting Storage providers. */
    std::optional<std::vector<StorageFileInfo>> ListFiles(
        std::string_view vault_password) const;
    StorageTransferResult DownloadFile(const StorageObjectId& object_id,
        const std::filesystem::path& destination, std::string_view vault_password) const;

private:
    std::filesystem::path PrivateManifestPath(const StorageObjectId& object_id) const;
    std::optional<StorageTransferStatus> RecoverPendingUpload(std::string_view vault_password);

    std::array<unsigned char, 32> m_network_id{};
    AccountId m_account_id;
    const CybouKeyStore& m_keystore;
    StorageObjectProvider& m_store;
    std::filesystem::path m_private_manifest_dir;
    std::mutex m_upload_mutex;
};

} // namespace cybou

#endif // CYBOU_STORAGE_SERVICE_H
