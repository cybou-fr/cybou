// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Потоковая сборка и чтение деревьев encrypted ROOT/INDEX/DATA чанков.

#ifndef CYBOU_ENCRYPTED_CHUNK_TREE_H
#define CYBOU_ENCRYPTED_CHUNK_TREE_H

#include <cybou/protocol_limits.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/chunk_authorization.h>

#include <functional>
#include <limits>
#include <optional>
#include <span>

namespace cybou {

/// \brief Верхние границы структуры encrypted tree.
inline constexpr std::size_t ENCRYPTED_TREE_MAX_CHILDREN{128};
inline constexpr std::size_t ENCRYPTED_TREE_MAX_DEPTH{32};
inline constexpr std::size_t ENCRYPTED_TREE_DATA_MIN_BYTES{160 * 1024};
inline constexpr std::size_t ENCRYPTED_TREE_DATA_MAX_BYTES{320 * 1024};
inline constexpr std::size_t ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES{240 * 1024};

/// \brief Итоговая сводка построенного encrypted tree.
struct EncryptedTreeSummary {
    ChunkId root_chunk_id{};
    ChunkId chunk_authorization_root{};
    ContentKey content_key{};
    std::uint64_t plaintext_bytes{0};
    std::uint64_t chunk_count{0};
};

/// \brief Источник plaintext-байтов: nullopt при ошибке, 0 на EOF, иначе число прочитанных байтов.
using EncryptedTreeSource = std::function<std::optional<std::size_t>(std::span<unsigned char> output)>;
/// \brief Ставит чанк в staging атомарно вместе с leaf_index; дубликаты ChunkId отвергаются.
using EncryptedTreeStage = std::function<bool(std::uint32_t leaf_index, const EncryptedChunk& chunk)>;
/// \brief Возвращает exact stored bytes по ChunkId.
using EncryptedChunkLookup = std::function<std::optional<std::vector<unsigned char>>(const ChunkId&)>;
/// \brief Принимает поток восстановленного plaintext.
using EncryptedTreeSink = std::function<bool(std::span<const unsigned char> plaintext)>;
/// \brief Принимает opaque private metadata из ROOT после расшифровки.
using EncryptedTreeRootMetadataSink = std::function<bool(std::span<const unsigned char> metadata)>;
/// \brief Принимает каждый новый ChunkId ровно один раз; отказ прерывает обход.
using EncryptedTreeVisit = std::function<bool(const ChunkId& chunk_id)>;

/// \brief Потоково строит локальный encrypted tree без буфера всего файла.
std::optional<EncryptedTreeSummary> BuildEncryptedChunkTree(
    std::span<const unsigned char, 32> network_binding,
    const EncryptedTreeSource& source,
    const EncryptedTreeStage& stage,
    std::span<const unsigned char> private_root_metadata = {});

/// \brief Загружает, проверяет, расшифровывает и потоково выдаёт ROOT/INDEX/DATA tree.
///
/// sink может успеть получить частичный вывод до более поздней ошибки.
std::optional<std::uint64_t> FetchEncryptedChunkTree(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup,
    const EncryptedTreeRootMetadataSink& root_metadata_sink,
    const EncryptedTreeVisit& visit,
    const EncryptedTreeSink& sink,
    std::uint64_t max_output_bytes);

/// \brief Перечисляет ChunkId дерева без загрузки DATA-чанков.
bool EnumerateEncryptedTreeChunks(std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key, const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup, const EncryptedTreeVisit& visit);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_TREE_H
