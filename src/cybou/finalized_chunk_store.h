// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_FINALIZED_CHUNK_STORE_H
#define CYBOU_FINALIZED_CHUNK_STORE_H

#include <cybou/chunk_authorization.h>
#include <cybou/kv_store.h>

#include <filesystem>
#include <functional>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <uint256.h>

namespace cybou {

enum class ChunkAdmissionStatus {
    STORED,
    ALREADY_STORED,
    NOT_FINALIZED,
    INVALID,
    NOT_AUTHORIZED,
    CONFLICT,
    CAPACITY_EXCEEDED,
    STORAGE_ERROR,
};

struct ChunkAdmissionResult {
    ChunkAdmissionStatus status{ChunkAdmissionStatus::INVALID};
    explicit operator bool() const
    {
        return status == ChunkAdmissionStatus::STORED || status == ChunkAdmissionStatus::ALREADY_STORED;
    }
};

/** Must resolve only RootPublications found in this full node's canonical finalized history. */
using FinalizedPublicationLookup = std::function<std::optional<RootPublication>(const uint256& operation_id)>;

/** Content-addressed immutable provider store for finalized RootPublication chunks. */
class FinalizedChunkStore final {
public:
    FinalizedChunkStore(const std::filesystem::path& path,
        std::span<const unsigned char, 32> network_id, std::uint64_t capacity_bytes,
        bool memory_only = false, bool wipe_data = false);
    ~FinalizedChunkStore();
    FinalizedChunkStore(const FinalizedChunkStore&) = delete;
    FinalizedChunkStore& operator=(const FinalizedChunkStore&) = delete;

    ChunkAdmissionResult PutChunk(const uint256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof, const FinalizedPublicationLookup& lookup);
    std::optional<std::vector<unsigned char>> GetChunk(const ChunkId& chunk_id) const;
    bool HasChunk(const ChunkId& chunk_id) const;
    std::uint64_t UsedBytes() const;
    std::uint64_t CapacityBytes() const { return m_capacity_bytes; }

private:
    std::optional<std::uint64_t> ReadCounter(const std::string& key) const;

    const std::string m_namespace;
    const std::filesystem::path m_blob_root;
    const bool m_memory_only;
    const std::uint64_t m_capacity_bytes;
    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_FINALIZED_CHUNK_STORE_H
