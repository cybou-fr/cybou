// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Реализация Merkle-авторизации чанков публикации.

#include <cybou/chunk_authorization.h>

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <set>
#include <string_view>

namespace cybou {
namespace {

/// Домен для leaf-хэша: отделяет авторизацию чанков от любых иных BLAKE3-коммитментов.
constexpr std::string_view LEAF_DOMAIN{"CYBOU/CHUNK-AUTH/LEAF"};
/// Домен для внутреннего узла: root нельзя подменить leaf-хэшем из другой структуры.
constexpr std::string_view NODE_DOMAIN{"CYBOU/CHUNK-AUTH/NODE"};

/// Представляет ASCII domain как span байтов без копирования.
std::span<const unsigned char> Bytes(const std::string_view text)
{
    return {reinterpret_cast<const unsigned char*>(text.data()), text.size()};
}

/// Нулевой ChunkId никогда не считается корректным leaf публикации.
bool IsZero(const ChunkId& id)
{
    return std::all_of(id.begin(), id.end(), [](const auto byte) { return byte == 0; });
}

/// Leaf коммитится именно по ChunkId stored-чанка, а не по plaintext или порядковому номеру.
ChunkId HashLeaf(const AuthorizedChunk& chunk)
{
    const std::array parts{Bytes(LEAF_DOMAIN), std::span<const unsigned char>{chunk.id}};
    return ComputeBlake3Digest(parts);
}

/// Внутренний узел хэширует упорядоченную пару дочерних хэшей с отдельным доменом.
ChunkId HashNode(const ChunkId& left, const ChunkId& right)
{
    const std::array parts{
        Bytes(NODE_DOMAIN),
        std::span<const unsigned char>{left},
        std::span<const unsigned char>{right},
    };
    return ComputeBlake3Digest(parts);
}

/// Базовая валидация без проверки дубликатов: порядок leaves задаётся вызывающей стороной.
bool ValidChunkSet(const std::span<const AuthorizedChunk> chunks)
{
    if (chunks.empty() || chunks.size() > MAX_PUBLICATION_CHUNKS) return false;
    for (const auto& chunk : chunks) {
        if (IsZero(chunk.id)) return false;
    }
    return true;
}

} // namespace

ChunkId ChunkAuthorizationLeafHash(const ChunkId& chunk_id)
{
    return HashLeaf(AuthorizedChunk{chunk_id});
}

ChunkId ChunkAuthorizationNodeHash(const ChunkId& left, const ChunkId& right)
{
    return HashNode(left, right);
}

bool ChunkAuthorizationAccumulator::Add(const AuthorizedChunk& chunk)
{
    if (m_failed || m_chunk_count >= MAX_PUBLICATION_CHUNKS || IsZero(chunk.id)) {
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
        return ChunkAuthorizationSummary{*root, m_chunk_count};
    } catch (...) {
        return std::nullopt;
    }
}

bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const ChunkId& chunk_id,
    const std::uint32_t leaf_index,
    const std::uint32_t chunk_count,
    const std::span<const ChunkId> siblings)
{
    if (chunk_count == 0 || leaf_index >= chunk_count ||
        IsZero(chunk_id) || siblings.size() > 32) return false;
    try {
        ChunkId current = HashLeaf(AuthorizedChunk{chunk_id});
        auto index = static_cast<std::size_t>(leaf_index);
        auto width = static_cast<std::size_t>(chunk_count);
        std::size_t sibling_index{0};
        while (width > 1) {
            if (sibling_index >= siblings.size()) return false;
            const auto& sibling = siblings[sibling_index++];
            if ((index & 1U) != 0) {
                current = HashNode(sibling, current);
            } else if (index + 1 >= width) {
                // Нечётный хвост дублируется сам в себя, поэтому proof обязан вернуть тот же hash.
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

std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(
    const std::span<const AuthorizedChunk> chunks)
{
    if (!ValidChunkSet(chunks)) return std::nullopt;
    try {
        std::set<ChunkId> unique_ids;
        for (const auto& chunk : chunks) {
            if (!unique_ids.insert(chunk.id).second) return std::nullopt;
        }

        // Полное дерево нужно только там, где затем запрашиваются независимые Merkle proofs.
        std::vector<std::vector<ChunkId>> levels;
        levels.emplace_back();
        levels.back().reserve(chunks.size());
        for (const auto& chunk : chunks) levels.back().push_back(HashLeaf(chunk));
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

        ChunkAuthorizationTree result;
        result.root = levels.back().front();
        result.chunk_count = static_cast<std::uint32_t>(chunks.size());
        result.m_levels = std::move(levels);
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

ChunkAuthorizationProof ChunkAuthorizationTree::Proof(const std::uint32_t leaf_index) const
{
    if (leaf_index >= chunk_count) throw std::out_of_range{"chunk authorization leaf index"};
    ChunkAuthorizationProof proof{.leaf_index = leaf_index};
    proof.siblings.reserve(m_levels.size());
    auto index = static_cast<std::size_t>(leaf_index);
    auto width = static_cast<std::size_t>(chunk_count);
    for (std::size_t level = 0; width > 1; ++level) {
        const auto sibling = index ^ 1U;
        proof.siblings.push_back(m_levels[level][sibling < width ? sibling : index]);
        index /= 2;
        width = (width + 1) / 2;
    }
    return proof;
}

bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkId& chunk_id,
    const ChunkAuthorizationProof& proof)
{
    if (publication.chunk_count == 0 || proof.leaf_index >= publication.chunk_count || IsZero(chunk_id)) return false;
    return VerifyChunkAuthorizationPath(publication.chunk_authorization_root,
        chunk_id, proof.leaf_index, publication.chunk_count, proof.siblings);
}

} // namespace cybou
