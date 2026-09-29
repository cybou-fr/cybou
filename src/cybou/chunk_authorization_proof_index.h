// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CHUNK_AUTHORIZATION_PROOF_INDEX_H
#define CYBOU_CHUNK_AUTHORIZATION_PROOF_INDEX_H

#include <cybou/chunk_authorization.h>
#include <cybou/kv_store.h>

#include <mutex>
#include <string>

namespace cybou {

/** Disk-backed ordered leaf index and Merkle levels for large staged objects.
 * Add each leaf from the durable chunk staging callback, then call Finish and
 * GetProof as chunks are uploaded. One index owns its namespace in the store.
 */
class ChunkAuthorizationProofIndex final {
public:
    ChunkAuthorizationProofIndex(KVStore& db, std::string index_id);

    bool Add(std::uint32_t leaf_index, const AuthorizedChunk& chunk);
    std::uint32_t StagedCount() const;
    bool Contains(const ChunkId& id) const;
    std::optional<ChunkId> GetLeafId(std::uint32_t leaf_index) const;
    std::optional<ChunkAuthorizationSummary> Finish();
    std::optional<ChunkAuthorizationProof> GetProof(std::uint32_t leaf_index) const;
    bool Discard();

private:
    std::string LeafKey(std::uint32_t index) const;
    std::string SeenKey(const ChunkId& id) const;
    std::string NodeKey(std::uint32_t level, std::uint64_t index) const;
    bool ReadLeaf(std::uint32_t index, ChunkId& id) const;
    bool ReadNode(std::uint32_t level, std::uint64_t index, ChunkId& hash) const;
    std::vector<unsigned char> EncodeState(std::uint32_t count, bool finished,
        const ChunkId& root) const;

    KVStore& m_db;
    const std::string m_prefix;
    mutable std::mutex m_mutex;
    std::uint32_t m_chunk_count{0};
    bool m_finished{false};
    bool m_failed{false};
    bool m_discarded{false};
    ChunkId m_root{};
};

} // namespace cybou

#endif // CYBOU_CHUNK_AUTHORIZATION_PROOF_INDEX_H
