// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_STORE_H
#define CYBOU_STORAGE_STORE_H

#include <cybou/crypto/sha256.h>
#include <cybou/kv_store.h>
#include <cybou/storage_crypto.h>

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace cybou {

enum class StorageWriteStatus : uint8_t {
    STORED,
    ALREADY_STORED,
    INVALID,
    CONFLICT,
    INCOMPLETE,
    CAPACITY_EXCEEDED,
    DISABLED,
};

struct StorageWriteResult {
    StorageWriteStatus status{StorageWriteStatus::INVALID};
    StorageChunkId commitment{};
    /** False when a remote request may have applied but its acknowledgment was lost. */
    bool response_received{true};
    explicit operator bool() const
    {
        return status == StorageWriteStatus::STORED || status == StorageWriteStatus::ALREADY_STORED;
    }
};

/** Local provider safeguards; these values are not part of CYP2 or consensus. */
struct StorageStagingPolicy {
    uint64_t max_bytes{0}; // zero selects a capacity-derived default
    uint32_t max_objects{1024};
    std::chrono::milliseconds ttl{std::chrono::hours{24}};
};

/** Client-facing operations shared by local and remote ciphertext providers. */
class StorageObjectProvider {
public:
    virtual ~StorageObjectProvider() = default;
    virtual bool SupportsAbortUncommittedUpload() const = 0;
    virtual StorageWriteResult PutChunk(const StorageObjectId& object_id,
        const StorageEncryptedChunk& chunk) = 0;
    virtual StorageWriteResult CommitManifest(const StoragePublicManifest& manifest) = 0;
    virtual bool AbortUncommittedObject(const StorageObjectId& object_id, uint32_t chunk_count) = 0;
    virtual std::optional<StoragePublicManifest> GetManifest(const StorageObjectId& object_id) const = 0;
    virtual std::optional<StorageEncryptedChunk> GetChunk(
        const StorageObjectId& object_id, uint32_t index) const = 0;
};

/** Durable, network-bound provider-side ciphertext storage. */
class StorageObjectStore final : public StorageObjectProvider
{
public:
    StorageObjectStore(
        const std::filesystem::path& path,
        std::span<const unsigned char, 32> network_id,
        uint64_t capacity_bytes,
        bool memory_only = false,
        bool wipe_data = false,
        StorageStagingPolicy staging_policy = {});

    StorageObjectStore(const StorageObjectStore&) = delete;
    StorageObjectStore& operator=(const StorageObjectStore&) = delete;

    bool SupportsAbortUncommittedUpload() const override { return true; }
    StorageWriteResult PutChunk(const StorageObjectId& object_id,
        const StorageEncryptedChunk& chunk) override;
    StorageWriteResult CommitManifest(const StoragePublicManifest& manifest) override;
    /** Remove only an uncommitted sequential upload after its client aborts. */
    bool AbortUncommittedObject(const StorageObjectId& object_id, uint32_t chunk_count) override;
    std::optional<StoragePublicManifest> GetManifest(const StorageObjectId& object_id) const override;
    std::optional<StorageEncryptedChunk> GetChunk(
        const StorageObjectId& object_id, uint32_t index) const override;
    uint64_t UsedBytes() const;
    uint64_t StagedBytes() const;
    size_t StagedObjectCount() const;
    /** Remove inactive uncommitted uploads. Called at startup and on writes. */
    uint64_t GarbageCollectExpiredStaging();
    uint64_t CapacityBytes() const { return m_capacity_bytes; }

private:
    struct StagingRecord {
        uint64_t received_bytes{0};
        int64_t last_activity_ms{0};
        uint32_t chunk_count{0};
        uint32_t highest_index{0};
    };

    static std::string ChunkKey(const StorageObjectId& object_id, uint32_t index);
    static std::string ManifestKey(const StorageObjectId& object_id);
    static std::string UsageKey();
    static std::string StagingKey();
    static std::vector<unsigned char> EncodeStagingRecords(
        const std::map<StorageObjectId, StagingRecord>& records);
    static std::optional<std::map<StorageObjectId, StagingRecord>> DecodeStagingRecords(
        const std::vector<unsigned char>& encoded);
    std::map<StorageObjectId, StagingRecord> ReadStagingRecords() const;
    void RecoverLegacyUncommittedChunks();
    uint64_t GarbageCollectExpiredStagingLocked(int64_t now_ms);
    std::optional<StorageEncryptedChunk> ReadChunk(const StorageObjectId& object_id, uint32_t index) const;
    std::optional<StoragePublicManifest> ReadManifest(const StorageObjectId& object_id) const;

    std::array<unsigned char, 32> m_network_id{};
    uint64_t m_capacity_bytes{0};
    uint64_t m_max_staging_bytes{0};
    uint32_t m_max_staging_objects{0};
    std::chrono::milliseconds m_staging_ttl{0};
    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_STORAGE_STORE_H
