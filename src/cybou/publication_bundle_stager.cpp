// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/publication_bundle_stager.h>
#include <cybou/crypto/cleanse.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace cybou {

PublicationBundleStager::PublicationBundleStager(ChunkBlobStore& blobs, KVStore& proof_db,
    std::string local_index_id, const std::span<const unsigned char, 32> network_id)
    : m_blobs{blobs}, m_db{proof_db}, m_index{proof_db, local_index_id},
      m_binding_key{"publication-stager/" + local_index_id + "/network-id"},
      m_in_progress_key{"publication-stager/" + local_index_id + "/in-progress"},
      m_next_leaf{m_index.StagedCount()}
{
    std::copy(network_id.begin(), network_id.end(), m_network_id.begin());
    if (std::all_of(m_network_id.begin(), m_network_id.end(), [](unsigned char byte) { return byte == 0; })) {
        throw std::invalid_argument{"publication stager requires a network ID"};
    }
    std::vector<unsigned char> saved_network;
    if (m_db.Read(m_binding_key, saved_network)) {
        if (!std::equal(saved_network.begin(), saved_network.end(), m_network_id.begin(), m_network_id.end())) {
            throw std::runtime_error{"publication proof index belongs to another network"};
        }
    } else {
        if (m_db.Exists(m_binding_key) || m_next_leaf != 0) {
            throw std::runtime_error{"publication proof index has no valid network binding"};
        }
        m_db.Write(m_binding_key, std::vector<unsigned char>{m_network_id.begin(), m_network_id.end()}, true);
    }
    m_failed = m_db.Exists(m_in_progress_key);
}

std::optional<StagedApplicationTree> PublicationBundleStager::StageTree(
    const EncryptedTreeSource& source, const std::span<const unsigned char> private_root_metadata)
{
    std::lock_guard lock{m_mutex};
    if (m_failed || m_finished || !source) return std::nullopt;
    const std::uint32_t first_leaf = m_next_leaf;
    try {
        m_db.Write(m_in_progress_key, uint8_t{1}, true);
    } catch (...) {
        m_failed = true;
        return std::nullopt;
    }
    const auto staged = BuildEncryptedChunkTree(m_network_id, source,
        [&](const std::uint32_t, const EncryptedChunk& chunk) {
            if (m_next_leaf == MAX_PUBLICATION_CHUNKS) return false;
            const auto status = m_blobs.Put(chunk.id, chunk.stored_bytes);
            if (status != ChunkBlobPutStatus::STORED && status != ChunkBlobPutStatus::ALREADY_STORED) return false;
            if (!m_index.Add(m_next_leaf, AuthorizedChunk{chunk.id})) return false;
            ++m_next_leaf;
            return true;
        }, private_root_metadata);
    if (!staged || m_next_leaf == first_leaf) {
        m_failed = true;
        return std::nullopt;
    }
    try {
        m_db.Erase(m_in_progress_key, true);
    } catch (...) {
        m_failed = true;
        return std::nullopt;
    }
    return StagedApplicationTree{*staged, first_leaf, m_next_leaf - first_leaf};
}

std::optional<PreparedPublicationBundle> PublicationBundleStager::Finish(
    const StagedApplicationTree& main_tree)
{
    std::lock_guard lock{m_mutex};
    if (m_failed || m_finished || main_tree.leaf_count == 0 ||
        static_cast<std::uint64_t>(main_tree.first_leaf) + main_tree.leaf_count > m_next_leaf ||
        !m_index.Contains(main_tree.tree.root_chunk_id)) return std::nullopt;
    bool root_in_main_tree{false};
    for (std::uint64_t i = main_tree.first_leaf;
         i < static_cast<std::uint64_t>(main_tree.first_leaf) + main_tree.leaf_count; ++i) {
        if (m_index.GetLeafId(static_cast<std::uint32_t>(i)) == main_tree.tree.root_chunk_id) {
            root_in_main_tree = true;
            break;
        }
    }
    if (!root_in_main_tree) return std::nullopt;
    if (main_tree.tree.chunk_count != main_tree.leaf_count) return std::nullopt;
    const auto root_bytes = m_blobs.Get(main_tree.tree.root_chunk_id);
    auto opened_root = root_bytes ? DecryptChunk(m_network_id, main_tree.tree.content_key,
        main_tree.tree.root_chunk_id, *root_bytes) : std::nullopt;
    if (!opened_root) return std::nullopt;
    crypto::CleanseMemory(opened_root->data(), opened_root->size());
    const auto commitment = m_index.Finish();
    if (!commitment || commitment->chunk_count != m_next_leaf) return std::nullopt;
    m_finished = true;
    return PreparedPublicationBundle{main_tree.tree.root_chunk_id, main_tree.tree.content_key,
        commitment->root, commitment->chunk_count};
}

std::optional<ChunkId> PublicationBundleStager::GetLeafId(const std::uint32_t leaf_index) const
{
    std::lock_guard lock{m_mutex};
    return m_finished ? m_index.GetLeafId(leaf_index) : std::nullopt;
}

std::optional<ChunkAuthorizationProof> PublicationBundleStager::GetProof(const std::uint32_t leaf_index) const
{
    std::lock_guard lock{m_mutex};
    return m_finished ? m_index.GetProof(leaf_index) : std::nullopt;
}

bool PublicationBundleStager::Discard()
{
    std::lock_guard lock{m_mutex};
    if (!m_index.Discard()) return false;
    try {
        m_db.Erase(m_in_progress_key, true);
        m_db.Erase(m_binding_key, true);
        m_failed = true;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace cybou
