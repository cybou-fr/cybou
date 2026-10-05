// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

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
///
/// Значения ограничивают fan-out, глубину и размер plaintext так, чтобы одно
/// дерево оставалось совместимым с лимитами RootPublication и одного чанка.
inline constexpr std::size_t ENCRYPTED_TREE_MAX_CHILDREN{128};
/// \brief Максимум child-ссылок в одном ROOT/INDEX plaintext.
inline constexpr std::size_t ENCRYPTED_TREE_MAX_DEPTH{32};
/// \brief Максимальная глубина обхода дерева, включая ROOT.
inline constexpr std::size_t ENCRYPTED_TREE_DATA_MIN_BYTES{160 * 1024};
/// \brief Нижняя граница случайной цели для DATA plaintext, уменьшающая утечки размера.
inline constexpr std::size_t ENCRYPTED_TREE_DATA_MAX_BYTES{320 * 1024};
/// \brief Верхняя граница случайной цели для DATA plaintext, уменьшающая число мелких чанков.
inline constexpr std::size_t ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES{240 * 1024};
/// \brief Лимит opaque private metadata в ROOT: оставляет запас под child refs в одном plaintext-чанке.

/// \brief Итоговая сводка построенного encrypted tree.
struct EncryptedTreeSummary {
    /// \brief ChunkId корневого ROOT-чанка дерева.
    ChunkId root_chunk_id{};
    /// \brief Корень Merkle-авторизации по точному порядку staged leaf-чанков.
    ChunkId chunk_authorization_root{};
    /// \brief ContentKey, необходимый для чтения всего дерева.
    ContentKey content_key{};
    /// \brief Суммарный объём plaintext, считанный из source.
    std::uint64_t plaintext_bytes{0};
    /// \brief Общее число staged чанков дерева, включая ROOT/INDEX/DATA.
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
/// \param network_binding 32-байтовый NetworkBinding для всех чанков дерева.
/// \param source Callback чтения plaintext-потока; \c std::nullopt означает ошибку, 0 — EOF.
/// \param stage Callback фиксации каждого чанка в точном leaf-order дерева.
/// \param private_root_metadata Opaque private metadata, записываемые только в ROOT.
/// \return Сводка дерева либо \c std::nullopt при ошибке чтения, шифрования,
/// staging, дубликатах ChunkId или нарушении лимитов.
/// \pre \p source и \p stage должны быть заданы.
/// \pre Размер \p private_root_metadata не должен превышать \ref ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES.
/// \post При успехе staged chunks уже переданы через \p stage в точном порядке,
/// пригодном для вычисления chunk_authorization_root.
/// \par Потокобезопасность
/// Не потокобезопасна относительно внешних callback'ов; они отвечают за свою синхронизацию.
std::optional<EncryptedTreeSummary> BuildEncryptedChunkTree(
    std::span<const unsigned char, 32> network_binding,
    const EncryptedTreeSource& source,
    const EncryptedTreeStage& stage,
    std::span<const unsigned char> private_root_metadata = {});

/// \brief Загружает, проверяет, расшифровывает и потоково выдаёт ROOT/INDEX/DATA tree.
///
/// sink может успеть получить частичный вывод до более поздней ошибки.
/// \param network_binding 32-байтовый NetworkBinding этого дерева.
/// \param content_key ContentKey дерева.
/// \param root_chunk_id ChunkId корневого ROOT-чанка.
/// \param lookup Callback загрузки exact stored bytes по ChunkId.
/// \param root_metadata_sink Callback приёма ROOT private metadata.
/// \param visit Callback посещения каждого нового ChunkId; \c false прерывает обход.
/// \param sink Callback приёма восстановленного plaintext DATA-чанков.
/// \param max_output_bytes Верхняя граница суммарного plaintext, выдаваемого в \p sink.
/// \return Число реально выданных plaintext-байтов либо \c std::nullopt при
/// ошибке lookup/decrypt/metadata/visit/sink или нарушении лимитов/циклов.
/// \pre Все callback'и должны быть заданы.
/// \post При успехе все возвращённые байты проверены по ChunkId и AEAD.
/// \par Потокобезопасность
/// Не потокобезопасна относительно внешних callback'ов; они отвечают за свою синхронизацию.
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
/// \param network_binding 32-байтовый NetworkBinding этого дерева.
/// \param content_key ContentKey дерева.
/// \param root_chunk_id ChunkId корневого ROOT-чанка.
/// \param lookup Callback загрузки exact stored bytes служебных чанков.
/// \param visit Callback посещения каждого нового ChunkId.
/// \return \c true, если структура дерева корректна и все ChunkId перечислены
/// без циклов; \c false при любой ошибке lookup/decrypt/visit/формата.
/// \pre \p lookup и \p visit должны быть заданы.
/// \post DATA-чанки не выгружаются в plaintext наружу.
/// \par Потокобезопасность
/// Не потокобезопасна относительно внешних callback'ов; они отвечают за свою синхронизацию.
bool EnumerateEncryptedTreeChunks(std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key, const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup, const EncryptedTreeVisit& visit);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_TREE_H
