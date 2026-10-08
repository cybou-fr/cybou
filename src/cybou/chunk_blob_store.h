// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Физическое content-addressed хранилище exact encrypted bytes.

#ifndef CYBOU_CHUNK_BLOB_STORE_H
#define CYBOU_CHUNK_BLOB_STORE_H

#include <cybou/chunk_id.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

/// \brief Результат сохранения blob в локальный ChunkStore.
enum class ChunkBlobPutStatus {
    /// \brief Новый корректный blob сохранён.
    STORED,
    /// \brief Такой же корректный blob уже присутствовал локально.
    ALREADY_STORED,
    /// \brief Входные bytes не соответствуют ChunkId или лимитам формата.
    INVALID,
    /// \brief По этому ChunkId уже лежат другие bytes.
    CONFLICT,
    /// \brief Локальная файловая система/память не позволила завершить запись надёжно.
    STORAGE_ERROR,
    /// \brief Новый blob превысил бы локальную CYBOU capacity `V` (DEC-275).
    CAPACITY_EXCEEDED,
};

/// \brief Общее физическое хранилище exact encrypted bytes.
///
/// Одни и те же байты используются и локальным staging/cache, и provider-retention.
class ChunkBlobStore final {
public:
    /// \brief Создаёт ChunkStore на диске или только в памяти.
    /// \param root Корневой каталог physical store; игнорируется в режиме memory_only.
    /// \param memory_only Если \c true, все blobs живут только в памяти процесса.
    /// \param wipe_data Если \c true, перед открытием безопасно удаляется существующий store.
    /// \pre При `memory_only == false` путь \p root должен быть непустым.
    /// \post При успехе дисковый store просканирован и `UsedBytes()` согласован с ним.
    /// \throw std::invalid_argument При некорректной конфигурации.
    /// \throw std::runtime_error При небезопасном wipe или повреждённой файловой структуре.
    /// \par Потокобезопасность
    /// Конструктор не рассчитан на конкурентный доступ к объекту до завершения создания.
    /// \param capacity_bytes Локальная CYBOU capacity `V`: предел всех physical blobs.
    ChunkBlobStore(std::filesystem::path root, bool memory_only = false, bool wipe_data = false,
        std::uint64_t capacity_bytes = std::numeric_limits<std::uint64_t>::max());
    ChunkBlobStore(const ChunkBlobStore&) = delete;
    ChunkBlobStore& operator=(const ChunkBlobStore&) = delete;

    /// \brief Сохраняет blob по ChunkId после полной проверки содержимого.
    /// \param id Ожидаемый ChunkId exact stored bytes.
    /// \param stored_bytes Exact stored bytes чанка.
    /// \return Статус сохранения; \c INVALID при несоответствии BLAKE3/лимитам,
    /// \c CONFLICT при уже существующих других bytes под тем же ChunkId.
    /// \post При \c STORED или \c ALREADY_STORED последующий Get(\p id) вернёт
    /// корректные bytes либо store уже сообщил о локальной storage error.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    ChunkBlobPutStatus Put(const ChunkId& id, std::span<const unsigned char> stored_bytes);
    /// \brief Возвращает blob только если файл/запись проходит полную BLAKE3-проверку.
    /// \param id ChunkId запрашиваемого blob.
    /// \return Exact stored bytes либо \c std::nullopt, если blob отсутствует,
    /// повреждён, не проходит проверку формата или не совпадает по ChunkId.
    /// \post Возвращаемые bytes независимы от внутреннего хранения объекта.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<std::vector<unsigned char>> Get(const ChunkId& id) const;
    /// \brief Дешёвая проверка наличия и размера без чтения содержимого.
    ///
    /// BLAKE3 не пересчитывается: это делают Get(), provider GET и owner-аудиты.
    /// \param id ChunkId проверяемого blob.
    /// \return Локальный размер blob либо \c std::nullopt, если blob отсутствует
    /// или размер выходит за допустимые границы stored-формата.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<std::uint64_t> StoredSize(const ChunkId& id) const;
    /// \brief Возвращает true, если blob выглядит локально присутствующим.
    /// \param id ChunkId проверяемого blob.
    /// \return \c true, если локально присутствует regular file/запись допустимого размера.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool Has(const ChunkId& id) const;
    /// \brief Удаляет blob после снятия всех локальных и provider-обязательств.
    /// \param id ChunkId удаляемого blob.
    /// \return \c true, если локальный blob удалён или уже отсутствует.
    /// \pre Вызывающая сторона уже проверила, что blob не удерживается retention
    /// и не admit-нут в FinalizedChunkStore.
    /// \post При успехе `Has(id) == false`.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool Remove(const ChunkId& id);
    /// \brief Возвращает оценку физически занятых байтов в store.
    /// \return Суммарный объём локально учтённых blobs.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::uint64_t UsedBytes() const;
    /// \brief Возвращает true, если store работает только в памяти.
    /// \return \c true для memory-only режима.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool MemoryOnly() const { return m_memory_only; }
    /// \brief Локальная CYBOU capacity `V`, ограничивающая все blobs.
    std::uint64_t CapacityBytes() const { return m_capacity_bytes; }
    /// OS space available to this process on the blob filesystem; no directory scan.
    /// Memory-only stores and OS errors have no disk measurement.
    std::optional<std::uint64_t> AvailableDiskBytes() const;

private:
    const std::filesystem::path m_root;
    const bool m_memory_only;
    const std::uint64_t m_capacity_bytes;
    mutable std::mutex m_mutex;
    std::map<ChunkId, std::vector<unsigned char>> m_memory_blobs;
    std::uint64_t m_used_bytes{0};
};

} // namespace cybou

#endif // CYBOU_CHUNK_BLOB_STORE_H
