// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_authorization.h>

#include <algorithm>
#include <limits>
#include <set>
#include <string_view>

namespace cybou {
namespace {

constexpr std::string_view LEAF_DOMAIN{"CYBOU/CHUNK-AUTH/LEAF"};
constexpr std::string_view NODE_DOMAIN{"CYBOU/CHUNK-AUTH/NODE"};
bool IsZero(const ChunkId& id)
{
    return std::all_of(id.begin(), id.end(), [](const auto byte) { return byte == 0; });
}

void AppendU64Be(std::vector<unsigned char>& bytes, const std::uint64_t value)
{
    for (int shift = 56; shift >= 0; shift -= 8) bytes.push_back(static_cast<unsigned char>(value >> shift));
}

ChunkId HashLeaf(const AuthorizedChunk& chunk)
{
    std::vector<unsigned char> preimage(LEAF_DOMAIN.begin(), LEAF_DOMAIN.end());
    preimage.insert(preimage.end(), chunk.id.begin(), chunk.id.end());
    AppendU64Be(preimage, chunk.stored_bytes);
    return ComputeBlake3Digest(preimage);
}

ChunkId HashNode(const ChunkId& left, const ChunkId& right)
{
    std::vector<unsigned char> preimage(NODE_DOMAIN.begin(), NODE_DOMAIN.end());
    preimage.insert(preimage.end(), left.begin(), left.end());
    preimage.insert(preimage.end(), right.begin(), right.end());
    return ComputeBlake3Digest(preimage);
}

bool ValidChunkSet(const std::span<const AuthorizedChunk> chunks, std::uint64_t& total_bytes)
{
    if (chunks.empty() || chunks.size() > ROOT_PUBLICATION_MAX_CHUNKS) return false;
    total_bytes = 0;
    for (const auto& chunk : chunks) {
        if (IsZero(chunk.id) || chunk.stored_bytes < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
            chunk.stored_bytes > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
            total_bytes > ROOT_PUBLICATION_MAX_STORED_BYTES - chunk.stored_bytes) return false;
        total_bytes += chunk.stored_bytes;
    }
    return total_bytes <= ROOT_PUBLICATION_MAX_STORED_BYTES;
}

} // namespace

bool ChunkAuthorizationAccumulator::Add(const AuthorizedChunk& chunk)
{
    if (m_failed || m_chunk_count >= ROOT_PUBLICATION_MAX_CHUNKS || IsZero(chunk.id) ||
        chunk.stored_bytes < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
        chunk.stored_bytes > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
        chunk.stored_bytes > ROOT_PUBLICATION_MAX_STORED_BYTES - m_total_bytes) {
        m_failed = true;
        return false;
    }

    try {
        auto carry = HashLeaf(chunk);
        auto level = std::size_t{0};
        while (level < m_frontier.size() && m_frontier[level].has_value()) {
            carry = HashNode(*m_frontier[level], carry);
            m_frontier[level].reset();
            ++level;
        }
        if (level == m_frontier.size()) {
            m_failed = true;
            return false;
        }
        m_frontier[level] = carry;
        ++m_chunk_count;
        m_total_bytes += chunk.stored_bytes;
        return true;
    } catch (...) {
        m_failed = true;
        return false;
    }
}

std::optional<ChunkAuthorizationSummary> ChunkAuthorizationAccumulator::Finish() const
{
    if (m_failed || m_chunk_count == 0) return std::nullopt;
    try {
        std::optional<ChunkId> root;
        std::size_t root_level{0};
        for (std::size_t level = 0; level < m_frontier.size(); ++level) {
            if (!m_frontier[level]) continue;
            if (!root) {
                root = *m_frontier[level];
                root_level = level;
                continue;
            }
            while (root_level < level) {
                *root = HashNode(*root, *root);
                ++root_level;
            }
            *root = HashNode(*m_frontier[level], *root);
            root_level = level + 1;
        }
        if (!root) return std::nullopt;
        return ChunkAuthorizationSummary{*root, m_chunk_count, m_total_bytes};
    } catch (...) {
        return std::nullopt;
    }
}

bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const AuthorizedChunk& chunk,
    const std::uint32_t leaf_index,
    const std::uint32_t chunk_count,
    const std::span<const ChunkId> siblings)
{
    if (chunk_count == 0 || leaf_index >= chunk_count ||
        IsZero(chunk.id) || chunk.stored_bytes < ENCRYPTED_CHUNK_MIN_STORED_BYTES ||
        chunk.stored_bytes > ENCRYPTED_CHUNK_MAX_STORED_BYTES || siblings.size() > 32) return false;
    try {
        ChunkId current = HashLeaf(chunk);
        auto index = static_cast<std::size_t>(leaf_index);
        auto width = static_cast<std::size_t>(chunk_count);
        std::size_t sibling_index{0};
        while (width > 1) {
            if (sibling_index >= siblings.size()) return false;
            const auto& sibling = siblings[sibling_index++];
            if ((index & 1U) != 0) {
                current = HashNode(sibling, current);
            } else if (index + 1 >= width) {
                if (sibling != current) return false;
                current = HashNode(current, sibling);
            } else {
                current = HashNode(current, sibling);
            }
            index /= 2;
            width = (width + 1) / 2;
        }
        return sibling_index == siblings.size() && current == expected_root;
    } catch (...) {
        return false;
    }
}

std::optional<ChunkAuthorizationCommitment> BuildChunkAuthorizationCommitment(
    const std::span<const AuthorizedChunk> chunks)
{
    std::uint64_t total_bytes{0};
    if (!ValidChunkSet(chunks, total_bytes)) return std::nullopt;
    try {
        std::vector<AuthorizedChunk> ordered{chunks.begin(), chunks.end()};
        std::set<ChunkId> unique_ids;
        for (const auto& chunk : ordered) if (!unique_ids.insert(chunk.id).second) return std::nullopt;

        std::vector<std::vector<ChunkId>> levels;
        levels.emplace_back();
        levels.back().reserve(ordered.size());
        for (const auto& chunk : ordered) levels.back().push_back(HashLeaf(chunk));
        while (levels.back().size() > 1) {
            const auto& current = levels.back();
            std::vector<ChunkId> next;
            next.reserve((current.size() + 1) / 2);
            for (std::size_t i = 0; i < current.size(); i += 2) {
                const auto& right = i + 1 < current.size() ? current[i + 1] : current[i];
                next.push_back(HashNode(current[i], right));
            }
            levels.push_back(std::move(next));
        }

        ChunkAuthorizationCommitment result;
        result.root = levels.back().front();
        result.chunk_count = static_cast<std::uint32_t>(ordered.size());
        result.authorized_stored_bytes = total_bytes;
        result.proofs.reserve(ordered.size());
        for (std::size_t leaf = 0; leaf < ordered.size(); ++leaf) {
            ChunkAuthorizationProof proof;
            proof.chunk_id = ordered[leaf].id;
            proof.stored_bytes = ordered[leaf].stored_bytes;
            proof.leaf_index = static_cast<std::uint32_t>(leaf);
            proof.chunk_count = result.chunk_count;
            auto index = leaf;
            auto width = ordered.size();
            for (std::size_t level = 0; width > 1; ++level) {
                const auto sibling = index ^ 1U;
                proof.siblings.push_back(levels[level][sibling < width ? sibling : index]);
                index /= 2;
                width = (width + 1) / 2;
            }
            result.proofs.push_back(std::move(proof));
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkAuthorizationProof& proof)
{
    if (publication.chunk_count == 0 || publication.chunk_count != proof.chunk_count ||
        proof.leaf_index >= proof.chunk_count || IsZero(proof.chunk_id) ||
        proof.stored_bytes > publication.authorized_stored_bytes) return false;
    return VerifyChunkAuthorizationPath(publication.chunk_authorization_root,
        {proof.chunk_id, proof.stored_bytes}, proof.leaf_index, proof.chunk_count, proof.siblings);
}

} // namespace cybou
