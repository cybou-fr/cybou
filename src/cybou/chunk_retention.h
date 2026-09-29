// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CHUNK_RETENTION_H
#define CYBOU_CHUNK_RETENTION_H

#include <cybou/chunk_id.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

class ChunkBlobStore;
class KVStore;

/**
 * Opaque local retention reference: a holder (for example one local
 * Identity) and one of its references (for example a publication job).
 * Both are 32-byte hashes; the registry never learns what they mean.
 */
struct RetentionKey {
    std::array<unsigned char, 32> holder{};
    std::array<unsigned char, 32> reference{};
};

/** holder = BLAKE3(domain || bytes); reference = BLAKE3(domain || bytes). */
std::array<unsigned char, 32> RetentionTag(std::string_view domain, std::span<const unsigned char> bytes);

/**
 * Node-local pin/reference registry for the common encrypted ChunkStore.
 *
 * The ChunkStore stays a plain map of ChunkID -> encrypted bytes. This
 * registry records, separately, why a local blob must stay:
 *   - pins: (holder, reference) -> ChunkIDs that must not be evicted
 *     (staged content not yet remotely durable, content the user keeps
 *     offline);
 *   - cache entries: blobs that may be evicted, with their last use.
 * Provider obligations stay in FinalizedChunkStore admission records.
 *
 * Garbage collection only ever deletes a blob that is a cache entry, is not
 * pinned by anyone, is not provider-admitted and was not used within the
 * grace period. A blob the registry has never heard of is never deleted.
 */
class ChunkRetentionRegistry final {
public:
    /** memory_only keeps the registry in memory (tests, ephemeral nodes). */
    ChunkRetentionRegistry(const std::filesystem::path& path, bool memory_only, bool wipe_data);
    ~ChunkRetentionRegistry();
    ChunkRetentionRegistry(const ChunkRetentionRegistry&) = delete;
    ChunkRetentionRegistry& operator=(const ChunkRetentionRegistry&) = delete;

    /** Adds chunks to a reference (idempotent, atomic). */
    bool Pin(const RetentionKey& key, std::span<const ChunkId> chunks);
    /** Drops every pin of a reference; its chunks become evictable cache. */
    bool Release(const RetentionKey& key, std::uint64_t now_ms);
    bool IsPinned(const ChunkId& chunk) const;
    std::vector<ChunkId> Pinned(const RetentionKey& key) const;
    /** Records a (re)used local cache blob. */
    bool NoteCacheUse(const ChunkId& chunk, std::uint64_t now_ms);

    struct CollectResult {
        std::uint64_t cache_bytes{0};
        std::size_t removed{0};
        std::uint64_t removed_bytes{0};
    };
    /**
     * Evicts least-recently-used unpinned cache blobs until the evictable
     * cache fits cache_budget_bytes, at most max_removals per call.
     * remove(chunk) deletes the blob only if no provider obligation exists
     * and reports whether it did.
     */
    CollectResult Collect(const ChunkBlobStore& blobs, std::uint64_t cache_budget_bytes, std::uint64_t now_ms,
        std::uint64_t grace_ms, std::size_t max_removals, const std::function<bool(const ChunkId&)>& remove);

private:
    bool IsPinnedLocked(const ChunkId& chunk) const;

    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_CHUNK_RETENTION_H
