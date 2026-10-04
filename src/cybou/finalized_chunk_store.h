// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Provider-facing admission store для finalized RootPublication чанков.

#ifndef CYBOU_FINALIZED_CHUNK_STORE_H
#define CYBOU_FINALIZED_CHUNK_STORE_H

#include <cybou/chunk_blob_store.h>
#include <cybou/chunk_authorization.h>
#include <cybou/kv_store.h>

#include <filesystem>
#include <functional>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <cybou/hash256.h>

namespace cybou {

/// \brief Итог попытки принять authorized chunk в provider store.
enum class ChunkAdmissionStatus {
    /// \brief Новый authorized chunk принят и учтён в provider-метаданных.
    STORED,
    /// \brief Такой же authorized chunk уже был принят ранее.
    ALREADY_STORED,
    /// \brief RootPublication ещё отсутствует в finalized history узла.
    NOT_FINALIZED,
    /// \brief Входные bytes, ChunkId или callback-параметры некорректны.
    INVALID,
    /// \brief ChunkId не доказывается Merkle proof относительно finalized RootPublication.
    NOT_AUTHORIZED,
    /// \brief Уже записаны несовместимые метаданные или bytes под тем же ключом.
    CONFLICT,
    /// \brief Локальная квота/диск fail-closed не позволяют принять ещё один чанк.
    CAPACITY_EXCEEDED,
    /// \brief Локальная ошибка хранения или метаданных не дала безопасно завершить admission.
    STORAGE_ERROR,
};

/// \brief Результат admission-попытки для одного чанка.
struct ChunkAdmissionResult {
    /// \brief Итоговый статус попытки admission.
    ChunkAdmissionStatus status{ChunkAdmissionStatus::INVALID};
    explicit operator bool() const
    {
        return status == ChunkAdmissionStatus::STORED || status == ChunkAdmissionStatus::ALREADY_STORED;
    }
};

/// \brief Разрешает только те RootPublication, которые уже есть в finalized history узла.
using FinalizedPublicationLookup = std::function<std::optional<RootPublication>(const cybou::Hash256& operation_id)>;

/// \brief Неизменяемый provider store для finalized RootPublication чанков.
class FinalizedChunkStore final {
public:
    /// \brief Создаёт admission-store, привязанный к network binding и общему blob-store.
    /// \param blobs Общий physical ChunkBlobStore exact encrypted bytes.
    /// \param path Каталог локальных provider-метаданных.
    /// \param network_binding 32-байтовый NetworkBinding, к которому жёстко привязаны метаданные.
    /// \param capacity_bytes Локальная квота provider-реплик в байтах.
    /// \param wipe_data Если \c true, предыдущее состояние provider-метаданных стирается.
    /// \post При успехе все сохранённые admitted записи согласованы по сети и размерам blobs.
    /// \throw std::invalid_argument При некорректной конфигурации или несовпадении сети.
    /// \throw std::runtime_error При повреждении сохранённых provider-метаданных.
    /// \par Потокобезопасность
    /// Конструктор не рассчитан на конкурентный доступ к объекту до завершения создания.
    FinalizedChunkStore(ChunkBlobStore& blobs, const std::filesystem::path& path,
        std::span<const unsigned char, 32> network_binding, std::uint64_t capacity_bytes,
        bool wipe_data = false);
    ~FinalizedChunkStore();
    FinalizedChunkStore(const FinalizedChunkStore&) = delete;
    FinalizedChunkStore& operator=(const FinalizedChunkStore&) = delete;

    /// \brief Принимает authorized chunk только для finalized публикации и корректного proof.
    /// \param publication_operation_id OperationID finalized RootPublication.
    /// \param chunk_id ChunkId exact stored bytes.
    /// \param stored_bytes Exact stored bytes чанка.
    /// \param proof Merkle proof этого чанка относительно публикации.
    /// \param lookup Callback поиска finalized RootPublication.
    /// \return Статус admission; успехом считаются только \c STORED и \c ALREADY_STORED.
    /// \pre \p lookup должен возвращать только публикации из finalized history.
    /// \post Успех означает, что локальный узел готов доказать authorizing proof для этого чанка.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    ChunkAdmissionResult PutChunk(const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof, const FinalizedPublicationLookup& lookup);
    /// \brief Возвращает сохранённый proof только пока сам admitted blob реально присутствует.
    /// \param publication_operation_id OperationID finalized RootPublication.
    /// \param chunk_id ChunkId admitted чанка.
    /// \param lookup Callback поиска finalized RootPublication.
    /// \return Proof либо \c std::nullopt, если публикация не finalized, blob
    /// отсутствует/повреждён или proof не проходит повторную проверку.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
        const FinalizedPublicationLookup& lookup) const;
    /// \brief Возвращает локально сохранённый provider blob после проверки метаданных размера.
    /// \param chunk_id ChunkId admitted чанка.
    /// \return Exact stored bytes либо \c std::nullopt, если admitted metadata
    /// отсутствуют или физический blob не согласован по размеру.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<std::vector<unsigned char>> GetChunk(const ChunkId& chunk_id) const;
    /// \brief Возвращает true, если admitted chunk присутствует и размер совпадает с метаданными.
    /// \param chunk_id ChunkId admitted чанка.
    /// \return \c true только если есть provider-метаданные и physical blob того же размера.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool HasChunk(const ChunkId& chunk_id) const;
    /// \brief Возвращает размер admitted blob только по provider-метаданным.
    /// \param chunk_id ChunkId admitted чанка.
    /// \return Размер admitted blob либо \c std::nullopt, если метаданных нет
    /// или они выходят за допустимые пределы stored-формата.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<uint64_t> StoredSize(const ChunkId& chunk_id) const;
    /// \brief Удаляет локальный cache-blob только если provider его не admit-ил.
    /// \param chunk_id ChunkId удаляемого blob.
    /// \return \c true только если provider-обязательства отсутствовали и blob удалён из общего store.
    /// \post Admit-нутые provider blobs никогда не удаляются этим методом.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool RemoveUnlessAdmitted(const ChunkId& chunk_id);
    /// \brief Удаляет привязки отозванной публикации и chunk-и, которые больше никто не авторизует.
    /// \param publication_operation_id OperationID финализированно отозванной RootPublication.
    /// \return Число завершённых purge-записей, включая уже отсутствующие blobs.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::size_t PurgePublication(const cybou::Hash256& publication_operation_id);
    /// Retry durable pending purges; quota is released only after deletion succeeds.
    std::size_t RetryPendingPurges(std::size_t max_chunks = 128);
    /// Reconcile persisted associations against authoritative finalized publications on startup.
    void PurgeRevokedPublications(const FinalizedPublicationLookup& lookup);
    /// \brief Возвращает число байтов, занятых admitted provider-репликами.
    /// \return Учтённый объём admitted provider-реплик; при повреждении счётчика
    /// возвращается большое fail-closed значение.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::uint64_t UsedBytes() const;
    /// \brief Возвращает локальную квоту provider-store.
    /// \return Локальная квота admitted provider-реплик.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::uint64_t CapacityBytes() const { return m_capacity_bytes; }

private:
    std::size_t RetryPendingPurgesLocked(std::size_t max_chunks);
    std::optional<std::uint64_t> ReadCounter(const std::string& key) const;

    ChunkBlobStore& m_blobs;
    const std::string m_namespace;
    const std::uint64_t m_capacity_bytes;
    const std::filesystem::path m_path;
    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
    std::optional<ChunkId> m_purge_cursor;
};

} // namespace cybou

#endif // CYBOU_FINALIZED_CHUNK_STORE_H
