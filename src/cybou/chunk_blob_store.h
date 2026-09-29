// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CHUNK_BLOB_STORE_H
#define CYBOU_CHUNK_BLOB_STORE_H

#include <cybou/chunk_id.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

enum class ChunkBlobPutStatus {
    STORED,
    ALREADY_STORED,
    INVALID,
    CONFLICT,
    STORAGE_ERROR,
};

/** Exact encrypted bytes shared by local staging/cache and provider retention. */
class ChunkBlobStore final {
public:
    ChunkBlobStore(std::filesystem::path root, bool memory_only = false, bool wipe_data = false);
    ChunkBlobStore(const ChunkBlobStore&) = delete;
    ChunkBlobStore& operator=(const ChunkBlobStore&) = delete;

    ChunkBlobPutStatus Put(const ChunkId& id, std::span<const unsigned char> stored_bytes);
    std::optional<std::vector<unsigned char>> Get(const ChunkId& id) const;
    bool Has(const ChunkId& id) const;
    /** Caller must first release every local and provider retention obligation. */
    bool Remove(const ChunkId& id);
    std::uint64_t UsedBytes() const;
    bool MemoryOnly() const { return m_memory_only; }

private:
    const std::filesystem::path m_root;
    const bool m_memory_only;
    mutable std::mutex m_mutex;
    std::map<ChunkId, std::vector<unsigned char>> m_memory_blobs;
    std::uint64_t m_used_bytes{0};
};

} // namespace cybou

#endif // CYBOU_CHUNK_BLOB_STORE_H
