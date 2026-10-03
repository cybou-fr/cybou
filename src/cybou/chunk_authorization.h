// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Merkle-коммитменты и inclusion proof для авторизации чанков публикации.

#ifndef CYBOU_CHUNK_AUTHORIZATION_H
#define CYBOU_CHUNK_AUTHORIZATION_H

#include <cybou/root_publication.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Лист авторизации, содержащий идентификатор сохраненного чанка.
struct AuthorizedChunk {
    ChunkId id{};

    friend bool operator==(const AuthorizedChunk&, const AuthorizedChunk&) = default;
};

/// \brief Минимальное доказательство включения чанка в корень авторизации публикации.
struct ChunkAuthorizationProof {
    std::uint32_t leaf_index{0};
    std::vector<ChunkId> siblings;
};

/// \brief Полное дерево авторизации для построения inclusion proof за O(log N).
class ChunkAuthorizationTree {
public:
    ChunkId root{};
    std::uint32_t chunk_count{0};
    /// \brief Возвращает корень Merkle-коммитмента.
    const ChunkId& Root() const { return root; }
    /// \brief Возвращает число листьев, зафиксированных в корне.
    std::uint32_t ChunkCount() const { return chunk_count; }
    /// \brief Строит inclusion proof для leaf_index.
    ChunkAuthorizationProof Proof(std::uint32_t leaf_index) const;
private:
    std::vector<std::vector<ChunkId>> m_levels;
    friend std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(std::span<const AuthorizedChunk>);
};

/// \brief Сводка потокового накопления Merkle-коммитмента публикации.
struct ChunkAuthorizationSummary {
    ChunkId root{};
    std::uint32_t chunk_count{0};
};

/// \brief Хэширует лист дерева авторизации публикации.
ChunkId ChunkAuthorizationLeafHash(const ChunkId& chunk_id);
/// \brief Хэширует внутренний узел дерева авторизации публикации.
ChunkId ChunkAuthorizationNodeHash(const ChunkId& left, const ChunkId& right);

/// \brief Потоковый Merkle-накопитель с памятью O(log N).
///
/// Вызывающая сторона отдельно отвечает за запрет дубликатов ChunkId.
class ChunkAuthorizationAccumulator final {
public:
    /// \brief Добавляет очередной уникальный чанк в потоковый коммитмент.
    bool Add(const AuthorizedChunk& chunk);
    /// \brief Завершает накопление и возвращает корень вместе с числом листьев.
    std::optional<ChunkAuthorizationSummary> Finish() const;

private:
    std::array<std::optional<ChunkId>, 32> m_frontier{};
    std::uint32_t m_chunk_count{0};
    bool m_failed{false};
};

std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(
    std::span<const AuthorizedChunk> chunks);

/// \brief Проверяет inclusion path для ожидаемого корня авторизации.
bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const ChunkId& chunk_id,
    std::uint32_t leaf_index,
    std::uint32_t chunk_count,
    std::span<const ChunkId> siblings);

/// \brief Проверяет proof включения чанка относительно finalized RootPublication.
bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkId& chunk_id,
    const ChunkAuthorizationProof& proof);

} // namespace cybou

#endif // CYBOU_CHUNK_AUTHORIZATION_H
