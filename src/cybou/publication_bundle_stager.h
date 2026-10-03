// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PUBLICATION_BUNDLE_STAGER_H
#define CYBOU_PUBLICATION_BUNDLE_STAGER_H

#include <cybou/chunk_authorization_proof_index.h>
#include <cybou/chunk_blob_store.h>
#include <cybou/encrypted_chunk_tree.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>

namespace cybou {

struct StagedApplicationTree {
    EncryptedTreeSummary tree;
    std::uint32_t first_leaf{0};
    std::uint32_t leaf_count{0};
};

struct PreparedPublicationBundle {
    ChunkId root_chunk_id{};
    ContentKey content_key{};
    ChunkId chunk_authorization_root{};
    std::uint32_t chunk_count{0};
};

/** Local implementation helper for one future RootPublication.
 *
 * The caller stages child trees before the main application tree. No protocol
 * bundle ID or wire format is created. If a tree is interrupted, this index
 * fails closed on reopen; the caller must discard it and prepare anew.
 */
class PublicationBundleStager final {
public:
    PublicationBundleStager(ChunkBlobStore& blobs, KVStore& proof_db,
        std::string local_index_id, std::span<const unsigned char, 32> network_binding);

    std::optional<StagedApplicationTree> StageTree(const EncryptedTreeSource& source,
        std::span<const unsigned char> private_root_metadata = {});
    std::optional<PreparedPublicationBundle> Finish(const StagedApplicationTree& main_tree);
    std::optional<ChunkId> GetLeafId(std::uint32_t leaf_index) const;
    std::optional<ChunkAuthorizationProof> GetProof(std::uint32_t leaf_index) const;
    bool Discard();

private:
    ChunkBlobStore& m_blobs;
    KVStore& m_db;
    ChunkAuthorizationProofIndex m_index;
    std::string m_binding_key;
    std::string m_in_progress_key;
    std::array<unsigned char, 32> m_network_binding{};
    std::uint32_t m_next_leaf{0};
    bool m_failed{false};
    bool m_finished{false};
    mutable std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_PUBLICATION_BUNDLE_STAGER_H
