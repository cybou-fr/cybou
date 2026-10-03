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
    /// \brief ChunkId exact stored bytes, который авторизуется RootPublication.
    ChunkId id{};

    friend bool operator==(const AuthorizedChunk&, const AuthorizedChunk&) = default;
};

/// \brief Минимальное доказательство включения чанка в корень авторизации публикации.
struct ChunkAuthorizationProof {
    /// \brief Нулевой индекс leaf в детерминированном порядке публикации.
    std::uint32_t leaf_index{0};
    /// \brief Соседние Merkle-хэши от leaf к root включительно по уровням дерева.
    std::vector<ChunkId> siblings;
};

/// \brief Полное дерево авторизации для построения inclusion proof за O(log N).
class ChunkAuthorizationTree {
public:
    ChunkId root{};
    std::uint32_t chunk_count{0};
    /// \brief Возвращает корень Merkle-коммитмента.
    /// \return Ссылка на корень авторизации.
    /// \pre Объект должен быть построен BuildChunkAuthorizationTree().
    /// \par Потокобезопасность
    /// Безопасно для конкурентного чтения готового неизменяемого объекта.
    const ChunkId& Root() const { return root; }
    /// \brief Возвращает число листьев, зафиксированных в корне.
    /// \return Число leaf-чанков, закоммиченных в \ref Root().
    /// \par Потокобезопасность
    /// Безопасно для конкурентного чтения готового неизменяемого объекта.
    std::uint32_t ChunkCount() const { return chunk_count; }
    /// \brief Строит inclusion proof для leaf_index.
    /// \param leaf_index Индекс leaf в детерминированном порядке публикации.
    /// \return Минимальное inclusion proof для этого leaf.
    /// \pre `leaf_index < ChunkCount()`.
    /// \post Возвращаемое proof проходит VerifyChunkAuthorizationPath() для \ref Root().
    /// \throw std::out_of_range Если индекс выходит за границы дерева.
    /// \par Потокобезопасность
    /// Безопасно для конкурентного чтения готового неизменяемого объекта.
    ChunkAuthorizationProof Proof(std::uint32_t leaf_index) const;
private:
    std::vector<std::vector<ChunkId>> m_levels;
    friend std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(std::span<const AuthorizedChunk>);
};

/// \brief Сводка потокового накопления Merkle-коммитмента публикации.
struct ChunkAuthorizationSummary {
    /// \brief Корень авторизации в том же домене, что и RootPublication::chunk_authorization_root.
    ChunkId root{};
    /// \brief Число leaf-чанков, учтённых в \ref root.
    std::uint32_t chunk_count{0};
};

/// \brief Хэширует лист дерева авторизации публикации.
/// \param chunk_id ChunkId авторизуемого stored-чанка.
/// \return Домен-разделённый Merkle leaf hash.
/// \pre \p chunk_id не должен быть нулевым идентификатором в корректном протокольном использовании.
/// \par Потокобезопасность
/// Потокобезопасна.
ChunkId ChunkAuthorizationLeafHash(const ChunkId& chunk_id);
/// \brief Хэширует внутренний узел дерева авторизации публикации.
/// \param left Левый дочерний Merkle hash.
/// \param right Правый дочерний Merkle hash.
/// \return Домен-разделённый Merkle node hash.
/// \par Потокобезопасность
/// Потокобезопасна.
ChunkId ChunkAuthorizationNodeHash(const ChunkId& left, const ChunkId& right);

/// \brief Потоковый Merkle-накопитель с памятью O(log N).
///
/// Вызывающая сторона отдельно отвечает за запрет дубликатов ChunkId.
class ChunkAuthorizationAccumulator final {
public:
    /// \brief Добавляет очередной уникальный чанк в потоковый коммитмент.
    /// \param chunk Очередной leaf в точном порядке публикации.
    /// \return \c true при успешном включении leaf; \c false при нулевом
    /// ChunkId, переполнении лимитов или внутренней ошибке. После первого
    /// отказа накопитель переходит в failed-состояние.
    /// \pre Вызывающая сторона должна исключить дубликаты ChunkId.
    /// \post При успехе счётчик leaf увеличивается на один.
    /// \par Потокобезопасность
    /// Не потокобезопасен; требует внешней синхронизации для записи.
    bool Add(const AuthorizedChunk& chunk);
    /// \brief Завершает накопление и возвращает корень вместе с числом листьев.
    /// \return Сводка коммитмента либо \c std::nullopt, если накопитель уже
    /// failed, ещё пуст, либо завершение обнаружило внутреннюю ошибку.
    /// \post Состояние накопителя не изменяется.
    /// \par Потокобезопасность
    /// Конкурентное чтение без записи безопасно только при внешней синхронизации.
    std::optional<ChunkAuthorizationSummary> Finish() const;

private:
    std::array<std::optional<ChunkId>, 32> m_frontier{};
    std::uint32_t m_chunk_count{0};
    bool m_failed{false};
};

std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(
    std::span<const AuthorizedChunk> chunks);
/// \brief Строит полное Merkle-дерево по точному порядку leaf-чанков.
/// \param chunks Упорядоченный список уникальных authorized leaf-чанков.
/// \return Полное дерево либо \c std::nullopt при пустом наборе, дубликатах,
/// нулевых ChunkId или нарушении лимитов публикации.
/// \pre Вызывающая сторона задаёт канонический leaf-order публикации.
/// \post При успехе `result->ChunkCount() == chunks.size()`.
/// \par Потокобезопасность
/// Потокобезопасна.

/// \brief Проверяет inclusion path для ожидаемого корня авторизации.
/// \param expected_root Ожидаемый Merkle root.
/// \param chunk_id ChunkId leaf-чанка.
/// \param leaf_index Индекс leaf в публикации.
/// \param chunk_count Полное число leaf-чанков в публикации.
/// \param siblings Последовательность sibling hash по уровням дерева.
/// \return \c true только если путь корректен и заканчивается в \p expected_root.
/// \par Потокобезопасность
/// Потокобезопасна.
bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const ChunkId& chunk_id,
    std::uint32_t leaf_index,
    std::uint32_t chunk_count,
    std::span<const ChunkId> siblings);

/// \brief Проверяет proof включения чанка относительно finalized RootPublication.
/// \param publication Finalized RootPublication с каноническими root и chunk_count.
/// \param chunk_id Проверяемый ChunkId.
/// \param proof Inclusion proof, который якорится в \p publication.
/// \return \c true, если \p proof корректно доказывает включение \p chunk_id
/// в \p publication.chunk_authorization_root.
/// \par Потокобезопасность
/// Потокобезопасна.
bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkId& chunk_id,
    const ChunkAuthorizationProof& proof);

} // namespace cybou

#endif // CYBOU_CHUNK_AUTHORIZATION_H
