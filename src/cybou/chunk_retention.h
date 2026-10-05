// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Локальный реестр pin/cache для общего зашифрованного ChunkStore.

#ifndef CYBOU_CHUNK_RETENTION_H
#define CYBOU_CHUNK_RETENTION_H

#include <cybou/chunk_id.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

class ChunkBlobStore;
class KVStore;

/// \brief Непрозрачная локальная ссылка удержания: владелец и его ссылка.
///
/// Оба поля — 32-байтовые теги. Реестр не знает их прикладной семантики.
struct RetentionKey {
    /// \brief Opaque holder tag, обычно выводимый из Identity или другой локальной сущности.
    std::array<unsigned char, 32> holder{};
    /// \brief Opaque reference tag, различающий конкретную причину удержания у holder.
    std::array<unsigned char, 32> reference{};
};

/// \brief Строит 32-байтовый retention tag из домена и байтов полезной нагрузки.
/// \param domain Домен разделения тегов.
/// \param bytes Полезная нагрузка, из которой детерминированно выводится tag.
/// \return 32-байтовый BLAKE3-based tag.
/// \par Потокобезопасность
/// Потокобезопасна.
std::array<unsigned char, 32> RetentionTag(std::string_view domain, std::span<const unsigned char> bytes);

/// \brief Узел-локальный реестр pin/reference для общего encrypted ChunkStore.
///
/// Физический ChunkStore остаётся простой картой ChunkId -> encrypted bytes.
/// Реестр отдельно хранит причины, почему локальный blob должен жить:
/// - pins: (holder, reference) -> ChunkId, которые нельзя вытеснять;
/// - cache entries: blob, которые можно вытеснять по LRU.
/// Обязательства провайдера остаются в FinalizedChunkStore.
class ChunkRetentionRegistry final {
public:
    /// \brief Создаёт реестр; memory_only оставляет всё только в памяти.
    /// \param path Путь к локальному KV-хранилищу.
    /// \param memory_only Если \c true, реестр не пишет состояние на диск.
    /// \param wipe_data Если \c true, предыдущее состояние реестра стирается.
    /// \par Потокобезопасность
    /// Конструктор не рассчитан на конкурентный доступ к объекту до завершения создания.
    ChunkRetentionRegistry(const std::filesystem::path& path, bool memory_only, bool wipe_data);
    ~ChunkRetentionRegistry();
    ChunkRetentionRegistry(const ChunkRetentionRegistry&) = delete;
    ChunkRetentionRegistry& operator=(const ChunkRetentionRegistry&) = delete;

    /// \brief Атомарно добавляет чанки к pin-ссылке.
    /// \param key Opaque owner/reference ключ удержания.
    /// \param chunks Чанки, которые должны стать pinned.
    /// \return \c true при успешной фиксации всех связей; \c false при ошибке KVStore.
    /// \post При успехе каждый чанк из \p chunks будет считаться pinned.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool Pin(const RetentionKey& key, std::span<const ChunkId> chunks);
    /// \brief Удаляет все pin-ссылки и переводит чанки в вытесняемый cache.
    /// \param key Opaque owner/reference ключ удержания.
    /// \param now_ms Текущая монотонно интерпретируемая отметка времени в миллисекундах.
    /// \return \c true при успешном освобождении ссылки; \c false при ошибке KVStore.
    /// \post При успехе все чанки ссылки перестают быть pinned этой ссылкой и получают cache timestamp.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool Release(const RetentionKey& key, std::uint64_t now_ms);
    /// \brief Возвращает true, если хотя бы одна pin-ссылка удерживает чанк.
    /// \param chunk Проверяемый ChunkId.
    /// \return \c true, если существует хотя бы одна pin-ссылка; при
    /// внутренней ошибке функция fail-closed тоже возвращает \c true.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool IsPinned(const ChunkId& chunk) const;
    /// \brief Возвращает все чанки, закрепленные конкретной ссылкой.
    /// \param key Opaque owner/reference ключ удержания.
    /// \return Список ChunkId, связанных с \p key; пустой список допустим.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::vector<ChunkId> Pinned(const RetentionKey& key) const;
    /// \brief Обновляет отметку последнего использования cache-чанка.
    /// \param chunk Cache-чанк.
    /// \param now_ms Новая отметка последнего использования в миллисекундах.
    /// \return \c true при успешной записи; \c false при ошибке KVStore.
    /// \post При успехе чанк становится более новым кандидатом в LRU.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool NoteCacheUse(const ChunkId& chunk, std::uint64_t now_ms);

    /// \brief Сводка одной итерации вытеснения cache-чанков.
    struct CollectResult {
        /// \brief Итоговый объём cache-чанков, оставшийся после прохода.
        std::uint64_t cache_bytes{0};
        /// \brief Сколько blobs реально удалено в этом проходе.
        std::size_t removed{0};
        /// \brief Суммарный объём реально удалённых blobs.
        std::uint64_t removed_bytes{0};
    };
    /// \brief Вытесняет unpinned cache-чанки по LRU, пока cache не уложится в бюджет.
    ///
    /// remove(chunk) удаляет blob только если на нём нет provider-обязательства.
    /// \param blobs Физический ChunkStore, из которого читается размер blob.
    /// \param cache_budget_bytes Целевой верхний предел для объёма cache-части.
    /// \param now_ms Текущая отметка времени.
    /// \param grace_ms Защитный grace period, в течение которого свежие cache blobs не вытесняются.
    /// \param max_removals Максимальное число удалений за один проход.
    /// \param remove Callback фактического удаления blob из физического store.
    /// \return Сводка прохода вытеснения.
    /// \post Pinned blobs не удаляются; неизвестные или уже отсутствующие blobs лишь вычищаются из cache-индекса.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта, но callback \p remove
    /// обязан выдерживать вызов под внутренней сериализацией реестра.
    CollectResult Collect(const ChunkBlobStore& blobs, std::uint64_t cache_budget_bytes, std::uint64_t now_ms,
        std::uint64_t grace_ms, std::size_t max_removals, const std::function<bool(const ChunkId&)>& remove);

private:
    bool IsPinnedLocked(const ChunkId& chunk) const;

    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_CHUNK_RETENTION_H
