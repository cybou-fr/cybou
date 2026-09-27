// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_STORE_H
#define CYBOU_STORAGE_STORE_H

#include <cybou/crypto/sha256.h>
#include <cybou/kv_store.h>
#include <cybou/storage_crypto.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

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
    explicit operator bool() const
    {
        return status == StorageWriteStatus::STORED || status == StorageWriteStatus::ALREADY_STORED;
    }
};

/** Durable, network-bound provider-side ciphertext storage. */
class StorageObjectStore final
{
public:
    StorageObjectStore(
        const std::filesystem::path& path,
        std::span<const unsigned char, 32> network_id,
        uint64_t capacity_bytes,
        bool memory_only = false,
        bool wipe_data = false);

    StorageObjectStore(const StorageObjectStore&) = delete;
    StorageObjectStore& operator=(const StorageObjectStore&) = delete;

    StorageWriteResult PutChunk(const StorageObjectId& object_id, const StorageEncryptedChunk& chunk);
    StorageWriteResult CommitManifest(const StoragePublicManifest& manifest);
    /** Remove only an uncommitted sequential upload after its client aborts. */
    bool AbortUncommittedObject(const StorageObjectId& object_id, uint32_t chunk_count);
    std::optional<StoragePublicManifest> GetManifest(const StorageObjectId& object_id) const;
    std::optional<StorageEncryptedChunk> GetChunk(const StorageObjectId& object_id, uint32_t index) const;
    uint64_t UsedBytes() const;
    uint64_t CapacityBytes() const { return m_capacity_bytes; }

private:
    static std::string ChunkKey(const StorageObjectId& object_id, uint32_t index);
    static std::string ManifestKey(const StorageObjectId& object_id);
    static std::string UsageKey();
    std::optional<StorageEncryptedChunk> ReadChunk(const StorageObjectId& object_id, uint32_t index) const;
    std::optional<StoragePublicManifest> ReadManifest(const StorageObjectId& object_id) const;

    std::array<unsigned char, 32> m_network_id{};
    uint64_t m_capacity_bytes{0};
    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_STORAGE_STORE_H
