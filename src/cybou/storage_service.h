// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// \brief Удалённая durability публикаций и транспорт exact encrypted chunks.

#ifndef CYBOU_STORAGE_SERVICE_H
#define CYBOU_STORAGE_SERVICE_H

#include <cybou/chunk_authorization.h>
#include <cybou/chunk_id.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/private_application_store.h>
#include <cybou/hash256.h>

#include <compare>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace cybou {

class CybouNodeRuntime;

/// \brief Целевое число удалённых полных реплик для DEV и Beta.
inline constexpr std::uint8_t DEVELOPMENT_REMOTE_REPLICA_TARGET{1};
inline constexpr std::uint8_t BETA_REMOTE_REPLICA_TARGET{2};

/// \brief Достижимый storage provider: StorageId плюс последний известный endpoint.
///
/// Реплики считаются по StorageId, а не по address:port.
struct StorageEndpoint {
    std::array<unsigned char, 32> storage_id{};
    std::string address;
    std::uint16_t port{0};
    auto operator<=>(const StorageEndpoint&) const = default;
};

inline bool SameProvider(const StorageEndpoint& a, const StorageEndpoint& b) { return a.storage_id == b.storage_id; }

/// \brief Транспорт exact encrypted chunks между full nodes.
class StorageTransport {
public:
    virtual ~StorageTransport() = default;
    /// \brief Возвращает достижимых удалённых storage providers, но не текущий узел.
    virtual std::vector<StorageEndpoint> Providers() = 0;
    /// \brief Отправляет authorized chunk удалённому provider.
    virtual std::optional<ChunkAdmissionResult> Put(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
        std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof) = 0;
    /// \brief Загружает exact stored bytes чанка у конкретного provider.
    virtual std::optional<std::vector<unsigned char>> Get(const StorageEndpoint& provider,
        const ChunkId& chunk_id) = 0;
    /// \brief Загружает authorization proof для чанка у конкретного provider.
    virtual std::optional<ChunkAuthorizationProof> GetProof(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) = 0;
};

/// \brief Реализация StorageTransport поверх P2P peer runtime.
class RuntimeStorageTransport final : public StorageTransport {
public:
    /// \brief Создаёт transport, использующий уже подключенных peer runtime.
    explicit RuntimeStorageTransport(CybouNodeRuntime& runtime) : m_runtime{runtime} {}
    std::vector<StorageEndpoint> Providers() override;
    std::optional<ChunkAdmissionResult> Put(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
        std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof) override;
    std::optional<std::vector<unsigned char>> Get(const StorageEndpoint& provider,
        const ChunkId& chunk_id) override;
    std::optional<ChunkAuthorizationProof> GetProof(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) override;

private:
    CybouNodeRuntime& m_runtime;
};

/// \brief Логическое состояние durability публикации.
enum class DurabilityState : std::uint8_t {
    /// Finalized, но для некоторых чанков ещё меньше healthy-реплик, чем target.
    SECURING,
    /// У каждого чанка есть как минимум target healthy remote replicas.
    PROTECTED,
    /// Placement невозможен: publication или набор чанков некорректен.
    NEEDS_ATTENTION,
};

/// \brief Сводка по удалённой durability конкретной finalized публикации.
struct PublicationDurability {
    DurabilityState state{DurabilityState::SECURING};
    std::uint32_t chunk_count{0};
    std::uint32_t chunks_at_target{0};
    /// Минимальное число healthy remote replicas среди всех чанков.
    std::uint32_t min_replicas{0};
    std::string error;
    /// 0..100 по доле чанков, достигших target.
    int ProgressPercent(std::uint8_t target) const;
};

/// \brief Транспорт контента и удалённая durability для одного unlocked Identity.
///
/// Размещаются только finalized RootPublication. Порядок leaves обязан точно
/// воспроизводить chunk-authorization root публикации.
class StorageService final {
public:
    /// \brief Создаёт сервис durability для Identity и его Application DB.
    StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
        PrivateApplicationStore& application_db,
        std::uint8_t remote_replica_target = DEVELOPMENT_REMOTE_REPLICA_TARGET);

    /// \brief Размещает каждый чанк finalized публикации до remote target.
    PublicationDurability Secure(const cybou::Hash256& publication_operation_id, std::span<const ChunkId> leaves);
    /// \brief Восстанавливает точный порядок leaves по provider-held proof для candidate chunks.
    PublicationDurability Rebuild(const cybou::Hash256& publication_operation_id,
        std::span<const ChunkId> candidate_chunks);
    /// \brief Проверяет, что кандидат действительно является leaf finalized публикации.
    std::optional<ChunkAuthorizationProof> GetAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id);
    /// \brief Возобновляет placement уже известной публикации.
    PublicationDurability Resume(const cybou::Hash256& publication_operation_id);
    /// \brief Переаудирует все записанные реплики и при необходимости ремонтирует до target.
    PublicationDurability Audit(const cybou::Hash256& publication_operation_id);
    /// \brief Выполняет round-robin аудит следующего placement и при необходимости repair.
    std::optional<std::pair<cybou::Hash256, PublicationDurability>> AuditNextPlacement(std::size_t max_chunks);
    /// \brief Добавляет уже существующий placement в поддерживаемый индекс.
    bool Track(const cybou::Hash256& publication_operation_id);
    /// \brief Возвращает уже известную сводку durability без запуска нового placement.
    std::optional<PublicationDurability> GetDurability(const cybou::Hash256& publication_operation_id);
    /// \brief Только для чтения: точный набор leaves и известных реплик placement.
    struct PlacementView {
        std::vector<ChunkId> leaves;
        std::vector<std::vector<StorageEndpoint>> replicas;
    };
    /// \brief Возвращает placement view для диагностики и smoke tests.
    std::optional<PlacementView> DescribePlacement(const cybou::Hash256& publication_operation_id);

    /// \brief Возвращает локальный blob или первую BLAKE3-корректную удалённую копию.
    std::optional<std::vector<unsigned char>> Fetch(const ChunkId& chunk_id);

    /// \brief Возвращает целевое число удалённых реплик.
    std::uint8_t RemoteReplicaTarget() const { return m_target; }

private:
    struct Placement;
    std::optional<Placement> Load(const cybou::Hash256& operation_id) const;
    bool Save(const Placement& placement);
    PublicationDurability Place(std::unique_lock<std::mutex>& lock, Placement& placement);
    PublicationDurability Summarize(const Placement& placement) const;
    std::optional<std::vector<unsigned char>> FetchInternal(const ChunkId& chunk_id,
        std::span<const StorageEndpoint> preferred);

    CybouNodeRuntime& m_runtime;
    StorageTransport& m_transport;
    PrivateApplicationStore& m_application_db;
    const std::uint8_t m_target;
    std::mutex m_mutex;
    std::set<cybou::Hash256> m_active_placements;
    std::condition_variable m_placement_cv;
    /// Следующий чанк для аудита по каждой публикации; перезапуск с нуля безопасен.
    std::map<cybou::Hash256, std::size_t> m_audit_cursor;
    std::size_t m_audit_placement_cursor{0};
    std::vector<cybou::Hash256> PlacementIndex() const;
};

} // namespace cybou

#endif // CYBOU_STORAGE_SERVICE_H
