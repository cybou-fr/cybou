// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Физическое content-addressed хранилище exact encrypted bytes.

#ifndef CYBOU_CHUNK_BLOB_STORE_H
#define CYBOU_CHUNK_BLOB_STORE_H

#include <cybou/chunk_id.h>

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Результат сохранения blob в локальный ChunkStore.
enum class ChunkBlobPutStatus {
    STORED,
    ALREADY_STORED,
    INVALID,
    CONFLICT,
    STORAGE_ERROR,
};

/// \brief Общее физическое хранилище exact encrypted bytes.
///
/// Одни и те же байты используются и локальным staging/cache, и provider-retention.
class ChunkBlobStore final {
public:
    /// \brief Создаёт ChunkStore на диске или только в памяти.
    ChunkBlobStore(std::filesystem::path root, bool memory_only = false, bool wipe_data = false);
    ChunkBlobStore(const ChunkBlobStore&) = delete;
    ChunkBlobStore& operator=(const ChunkBlobStore&) = delete;

    /// \brief Сохраняет blob по ChunkId после полной проверки содержимого.
    ChunkBlobPutStatus Put(const ChunkId& id, std::span<const unsigned char> stored_bytes);
    /// \brief Возвращает blob только если файл/запись проходит полную BLAKE3-проверку.
    std::optional<std::vector<unsigned char>> Get(const ChunkId& id) const;
    /// \brief Дешёвая проверка наличия и размера без чтения содержимого.
    ///
    /// BLAKE3 не пересчитывается: это делают Get(), provider GET и owner-аудиты.
    std::optional<std::uint64_t> StoredSize(const ChunkId& id) const;
    /// \brief Возвращает true, если blob выглядит локально присутствующим.
    bool Has(const ChunkId& id) const;
    /// \brief Удаляет blob после снятия всех локальных и provider-обязательств.
    bool Remove(const ChunkId& id);
    /// \brief Возвращает оценку физически занятых байтов в store.
    std::uint64_t UsedBytes() const;
    /// \brief Возвращает true, если store работает только в памяти.
    bool MemoryOnly() const { return m_memory_only; }

private:
    const std::filesystem::path m_root;
    const bool m_memory_only;
    mutable std::mutex m_mutex;
    std::map<ChunkId, std::vector<unsigned char>> m_memory_blobs;
    std::uint64_t m_used_bytes{0};
};

} // namespace cybou

#endif // CYBOU_CHUNK_BLOB_STORE_H
