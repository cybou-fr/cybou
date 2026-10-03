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
    STORED,
    ALREADY_STORED,
    NOT_FINALIZED,
    INVALID,
    NOT_AUTHORIZED,
    CONFLICT,
    CAPACITY_EXCEEDED,
    STORAGE_ERROR,
};

/// \brief Результат admission-попытки для одного чанка.
struct ChunkAdmissionResult {
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
    FinalizedChunkStore(ChunkBlobStore& blobs, const std::filesystem::path& path,
        std::span<const unsigned char, 32> network_binding, std::uint64_t capacity_bytes,
        bool wipe_data = false);
    ~FinalizedChunkStore();
    FinalizedChunkStore(const FinalizedChunkStore&) = delete;
    FinalizedChunkStore& operator=(const FinalizedChunkStore&) = delete;

    /// \brief Принимает authorized chunk только для finalized публикации и корректного proof.
    ChunkAdmissionResult PutChunk(const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof, const FinalizedPublicationLookup& lookup);
    /// \brief Возвращает сохранённый proof только пока сам admitted blob реально присутствует.
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
        const FinalizedPublicationLookup& lookup) const;
    /// \brief Возвращает локально сохранённый provider blob после проверки метаданных размера.
    std::optional<std::vector<unsigned char>> GetChunk(const ChunkId& chunk_id) const;
    /// \brief Возвращает true, если admitted chunk присутствует и размер совпадает с метаданными.
    bool HasChunk(const ChunkId& chunk_id) const;
    /// \brief Возвращает размер admitted blob только по provider-метаданным.
    std::optional<uint64_t> StoredSize(const ChunkId& chunk_id) const;
    /// \brief Удаляет локальный cache-blob только если provider его не admit-ил.
    bool RemoveUnlessAdmitted(const ChunkId& chunk_id);
    /// \brief Возвращает число байтов, занятых admitted provider-репликами.
    std::uint64_t UsedBytes() const;
    /// \brief Возвращает локальную квоту provider-store.
    std::uint64_t CapacityBytes() const { return m_capacity_bytes; }

private:
    std::optional<std::uint64_t> ReadCounter(const std::string& key) const;

    ChunkBlobStore& m_blobs;
    const std::string m_namespace;
    const std::uint64_t m_capacity_bytes;
    const std::filesystem::path m_path;
    mutable std::mutex m_mutex;
    std::unique_ptr<KVStore> m_db;
};

} // namespace cybou

#endif // CYBOU_FINALIZED_CHUNK_STORE_H
