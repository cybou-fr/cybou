// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

// Test-only construction of exact encrypted publication chunks.
#ifndef CYBOU_TEST_PUBLICATION_BUILDER_H
#define CYBOU_TEST_PUBLICATION_BUILDER_H
#include <cybou/publication_service.h>
#include <cybou/chunk_blob_store.h>
#include <set>
namespace cybou::test {
class PublicationBuilder {
    ChunkBlobStore& m_blobs;
    std::array<unsigned char, 32> m_network{};
    std::vector<AuthorizedChunk> m_leaves;
    std::set<ChunkId> m_unique;
public:
    PublicationBuilder(ChunkBlobStore& blobs, std::span<const unsigned char, 32> network) : m_blobs{blobs} {
        std::copy(network.begin(), network.end(), m_network.begin());
    }
    std::optional<EncryptedTreeSummary> StageTree(const EncryptedTreeSource& source,
        std::span<const unsigned char> metadata = {}) {
        return BuildEncryptedChunkTree(m_network, source, [&](std::uint32_t, const EncryptedChunk& chunk) {
            if (!m_unique.insert(chunk.id).second) return false;
            const auto status = m_blobs.Put(chunk.id, chunk.stored_bytes);
            if (status != ChunkBlobPutStatus::STORED && status != ChunkBlobPutStatus::ALREADY_STORED) return false;
            m_leaves.push_back({chunk.id});
            return true;
        }, metadata);
    }
    std::optional<PreparedPublicationBundle> Finish(const EncryptedTreeSummary& main) const {
        const auto tree = BuildChunkAuthorizationTree(m_leaves);
        if (!tree || !m_unique.contains(main.root_chunk_id)) return std::nullopt;
        return PreparedPublicationBundle{main.root_chunk_id, main.content_key, tree->Root(), tree->ChunkCount()};
    }
    std::optional<ChunkId> GetLeafId(std::uint32_t index) const {
        return index < m_leaves.size() ? std::optional{m_leaves[index].id} : std::nullopt;
    }
};
}
#endif
