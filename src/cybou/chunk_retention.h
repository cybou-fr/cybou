// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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
    std::array<unsigned char, 32> holder{};
    std::array<unsigned char, 32> reference{};
};

/// \brief Строит 32-байтовый retention tag из домена и байтов полезной нагрузки.
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
    ChunkRetentionRegistry(const std::filesystem::path& path, bool memory_only, bool wipe_data);
    ~ChunkRetentionRegistry();
    ChunkRetentionRegistry(const ChunkRetentionRegistry&) = delete;
    ChunkRetentionRegistry& operator=(const ChunkRetentionRegistry&) = delete;

    /// \brief Атомарно добавляет чанки к pin-ссылке.
    bool Pin(const RetentionKey& key, std::span<const ChunkId> chunks);
    /// \brief Удаляет все pin-ссылки и переводит чанки в вытесняемый cache.
    bool Release(const RetentionKey& key, std::uint64_t now_ms);
    /// \brief Возвращает true, если хотя бы одна pin-ссылка удерживает чанк.
    bool IsPinned(const ChunkId& chunk) const;
    /// \brief Возвращает все чанки, закрепленные конкретной ссылкой.
    std::vector<ChunkId> Pinned(const RetentionKey& key) const;
    /// \brief Обновляет отметку последнего использования cache-чанка.
    bool NoteCacheUse(const ChunkId& chunk, std::uint64_t now_ms);

    /// \brief Сводка одной итерации вытеснения cache-чанков.
    struct CollectResult {
        std::uint64_t cache_bytes{0};
        std::size_t removed{0};
        std::uint64_t removed_bytes{0};
    };
    /// \brief Вытесняет unpinned cache-чанки по LRU, пока cache не уложится в бюджет.
    ///
    /// remove(chunk) удаляет blob только если на нём нет provider-обязательства.
    CollectResult Collect(const ChunkBlobStore& blobs, std::uint64_t cache_budget_bytes, std::uint64_t now_ms,
        std::uint64_t grace_ms, std::size_t max_removals, const std::function<bool(const ChunkId&)>& remove);

private:
    bool IsPinnedLocked(const ChunkId& chunk) const;

    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_CHUNK_RETENTION_H
