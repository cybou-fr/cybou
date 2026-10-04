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
#include <cybou/storage_audit.h>
#include <cybou/storage_economy.h>
#include <cybou/storage_lease.h>
#include <cybou/hash256.h>

#include <compare>
#include <condition_variable>
#include <functional>
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
///
/// Локальная копия в ChunkBlobStore сюда не входит и отдельно не считается.
inline constexpr std::uint8_t DEVELOPMENT_REMOTE_REPLICA_TARGET{1};
/// \brief DEV target: одна удалённая полная реплика на чанк.
inline constexpr std::uint8_t BETA_REMOTE_REPLICA_TARGET{2};
/// \brief Beta target: две независимые удалённые полные реплики на чанк.

/// \brief Достижимый storage provider: StorageId плюс последний известный endpoint.
///
/// Реплики считаются по StorageId, а не по address:port.
struct StorageEndpoint {
    /// \brief StorageId удалённого Full Node provider.
    std::array<unsigned char, 32> storage_id{};
    /// \brief Последний известный сетевой адрес этого provider endpoint.
    std::string address;
    /// \brief Последний известный TCP/UDP порт этого provider endpoint.
    std::uint16_t port{0};
    /// \brief Проверенный payout-аккаунт provider'а (DEC-280); не сохраняется в placement.
    std::optional<std::array<unsigned char, 32>> payout_account;
    auto operator<=>(const StorageEndpoint&) const = default;
};

/// \brief Сравнивает provider по StorageId, игнорируя address:port.
/// \return \c true, если оба endpoint представляют один и тот же provider key.
inline bool SameProvider(const StorageEndpoint& a, const StorageEndpoint& b) { return a.storage_id == b.storage_id; }

/// \brief Транспорт exact encrypted chunks между full nodes.
class StorageTransport {
public:
    virtual ~StorageTransport() = default;
    /// \brief Возвращает достижимых удалённых storage providers, но не текущий узел.
    /// \return Список достижимых providers без дедупликации по endpoint-путям выше уровня StorageId.
    virtual std::vector<StorageEndpoint> Providers() = 0;
    /// \brief Отправляет authorized chunk удалённому provider.
    /// \return Результат удалённого admission либо \c std::nullopt, если transport не получил осмысленного ответа.
    virtual std::optional<ChunkAdmissionResult> Put(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
        std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof) = 0;
    /// \brief Загружает exact stored bytes чанка у конкретного provider.
    /// \return Exact stored bytes либо \c std::nullopt, если transport/peer не вернул валидный ответ.
    virtual std::optional<std::vector<unsigned char>> Get(const StorageEndpoint& provider,
        const ChunkId& chunk_id) = 0;
    /// \brief Загружает authorization proof для чанка у конкретного provider.
    /// \return Proof либо \c std::nullopt, если peer его не знает или transport не получил ответ.
    virtual std::optional<ChunkAuthorizationProof> GetProof(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) = 0;
    /// \brief Отправляет random-offset audit challenge конкретному provider (DEC-276).
    /// \return Ответ provider либо \c std::nullopt, если transport не поддерживает audit или не получил ответ;
    /// тогда StorageService проверяет реплику полным GET.
    virtual std::optional<StorageAuditAnswer> Audit(const StorageEndpoint& provider,
        const StorageAuditChallenge& challenge)
    {
        (void)provider;
        (void)challenge;
        return std::nullopt;
    }
};

/// \brief Реализация StorageTransport поверх P2P peer runtime.
class RuntimeStorageTransport final : public StorageTransport {
public:
    /// \brief Создаёт transport, использующий уже подключенных peer runtime.
    /// \param runtime Локальный runtime, через который идут P2P запросы.
    explicit RuntimeStorageTransport(CybouNodeRuntime& runtime) : m_runtime{runtime} {}
    std::vector<StorageEndpoint> Providers() override;
    std::optional<ChunkAdmissionResult> Put(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
        std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof) override;
    std::optional<std::vector<unsigned char>> Get(const StorageEndpoint& provider,
        const ChunkId& chunk_id) override;
    std::optional<ChunkAuthorizationProof> GetProof(const StorageEndpoint& provider,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) override;
    std::optional<StorageAuditAnswer> Audit(const StorageEndpoint& provider,
        const StorageAuditChallenge& challenge) override;

private:
    CybouNodeRuntime& m_runtime;
};

/// \brief Один полный GET из `STORAGE_FULL_GET_ONE_IN` проверок реплики; остальные — random-offset audit.
inline constexpr std::uint32_t STORAGE_FULL_GET_ONE_IN{8};
/// \brief Предел числа providers в локальной rolling evidence одного StorageService.
inline constexpr std::size_t MAX_TRACKED_STORAGE_PROVIDERS{1024};

/// \brief Локальная off-chain evidence по одному provider (DEC-276); не consensus state.
struct StorageProviderEvidence {
    /// \brief Проверенные подписанные receipts после admission.
    std::uint64_t receipts{0};
    /// \brief Успешные проверки реплик (audit или полный GET).
    std::uint64_t successes{0};
    /// \brief Неуспешные проверки: отказ, неверный ответ или недоступность.
    std::uint64_t failures{0};
    /// \brief Успешные полные GET с пересчётом ChunkID.
    std::uint64_t full_verifications{0};
    /// \brief Unix-время (ms) последнего успеха, последней неудачи и последнего полного GET.
    std::int64_t last_success_ms{0}, last_failure_ms{0}, last_full_verification_ms{0};
    /// \brief Shadow accounting (M4): billing-unit-seconds, подтверждённые двумя успешными проверками подряд.
    std::uint64_t verified_unit_seconds{0};
    /// \brief Shadow-вознаграждение за `verified_unit_seconds`; CYBOU не перемещаются.
    StorageRentAccumulator shadow_reward;
};

/// \brief Максимальный интервал между проверками, засчитываемый как непрерывное хранение.
inline constexpr std::int64_t STORAGE_MAX_CREDITED_GAP_MS{24LL * 60 * 60 * 1000};
/// \brief Предел числа реплик с отслеживаемым временем последней успешной проверки.
inline constexpr std::size_t MAX_TRACKED_REPLICA_CHECKS{1U << 16};

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
    /// \brief Сводное логическое состояние placement.
    DurabilityState state{DurabilityState::SECURING};
    /// \brief Полное число чанков finalized публикации.
    std::uint32_t chunk_count{0};
    /// \brief Сколько чанков уже достигли удалённого target.
    std::uint32_t chunks_at_target{0};
    /// Минимальное число healthy remote replicas среди всех чанков.
    std::uint32_t min_replicas{0};
    /// \brief Последняя диагностическая ошибка/объяснение, почему target ещё не достигнут.
    std::string error;
    /// 0..100 по доле чанков, достигших target.
    /// \param target Текущее целевое число удалённых реплик.
    /// \return 0..100, а для PROTECTED всегда 100.
    int ProgressPercent(std::uint8_t target) const;
};

/// \brief Транспорт контента и удалённая durability для одного unlocked Identity.
///
/// Размещаются только finalized RootPublication. Порядок leaves обязан точно
/// воспроизводить chunk-authorization root публикации.
class StorageService final {
public:
    /// \brief Создаёт сервис durability для Identity и его Application DB.
    /// \param runtime Локальный Full Node runtime.
    /// \param transport Транспорт хранения/получения exact encrypted chunks.
    /// \param application_db Application DB для placement metadata.
    /// \param remote_replica_target Целевое число удалённых реплик; будет зажато в допустимый диапазон.
    /// \par Потокобезопасность
    /// После построения объект сериализует собственные публичные операции внутренним mutex.
    StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
        PrivateApplicationStore& application_db,
        std::uint8_t remote_replica_target = DEVELOPMENT_REMOTE_REPLICA_TARGET);

    /// \brief Размещает каждый чанк finalized публикации до remote target.
    /// \param publication_operation_id OperationID finalized RootPublication.
    /// \param leaves Точный leaf-order публикации.
    /// \return Актуальная сводка durability; при отсутствии финализации
    /// возвращается SECURING без placement.
    /// \pre \p leaves должен точно соответствовать finalized publication.
    /// \post При успехе placement сохранён и может быть возобновлён Audit()/Resume().
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationDurability Secure(const cybou::Hash256& publication_operation_id, std::span<const ChunkId> leaves);
    /// \brief Восстанавливает точный порядок leaves по provider-held proof для candidate chunks.
    /// \param publication_operation_id OperationID finalized RootPublication.
    /// \param candidate_chunks Набор возможных ChunkId для этой публикации.
    /// \return Сводка durability либо NEEDS_ATTENTION/SECURING, если rebuild не завершён.
    /// \pre \p candidate_chunks может быть неполным, но не должен содержать предположений о порядке.
    /// \post При успехе сохраняется канонический rebuilt placement.
    /// Проверенные промежуточные результаты сохраняются отдельно в Application DB;
    /// незавершённое восстановление никогда не считается защищённым placement.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationDurability Rebuild(const cybou::Hash256& publication_operation_id,
        std::span<const ChunkId> candidate_chunks);
    /// \brief Проверяет, что кандидат действительно является leaf finalized публикации.
    /// \param publication_operation_id OperationID finalized RootPublication.
    /// \param chunk_id Проверяемый ChunkId.
    /// \return Proof от любого достижимого provider либо \c std::nullopt, если
    /// chunk не подтверждается finalized publication.
    /// \par Потокобезопасность
    /// Безопасен для конкурентных вызовов одного объекта при потокобезопасном transport/runtime.
    std::optional<ChunkAuthorizationProof> GetAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id);
    /// \brief Возобновляет placement уже известной публикации.
    /// \param publication_operation_id OperationID публикации с уже сохранённым placement.
    /// \return Актуальная сводка durability.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationDurability Resume(const cybou::Hash256& publication_operation_id);
    /// \brief Переаудирует все записанные реплики и при необходимости ремонтирует до target.
    /// \param publication_operation_id OperationID публикации.
    /// \return Актуальная сводка durability после audit/repair.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    PublicationDurability Audit(const cybou::Hash256& publication_operation_id);
    /// \brief Выполняет round-robin аудит следующего placement и при необходимости repair.
    /// \param max_chunks Максимум чанков, проверяемых за один вызов.
    /// \return Пара `(operation_id, durability)` либо \c std::nullopt, если placements нет.
    /// \post Может удалить деградировавшие replicas и немедленно запустить repair.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<std::pair<cybou::Hash256, PublicationDurability>> AuditNextPlacement(std::size_t max_chunks);
    /// \brief Добавляет уже существующий placement в поддерживаемый индекс.
    /// \param publication_operation_id OperationID публикации.
    /// \return \c true, если placement известен и присутствует в индексе audit.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    bool Track(const cybou::Hash256& publication_operation_id);
    /// \brief Возвращает уже известную сводку durability без запуска нового placement.
    /// \param publication_operation_id OperationID публикации.
    /// \return Сводка durability либо \c std::nullopt, если placement неизвестен.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<PublicationDurability> GetDurability(const cybou::Hash256& publication_operation_id);
    /// \brief Только для чтения: точный набор leaves и известных реплик placement.
    struct PlacementView {
        /// \brief Канонический leaf-order публикации.
        std::vector<ChunkId> leaves;
        /// \brief Известные удалённые replicas по каждому leaf; локальная копия здесь не учитывается.
        std::vector<std::vector<StorageEndpoint>> replicas;
    };
    /// \brief Возвращает placement view для диагностики и smoke tests.
    /// \param publication_operation_id OperationID публикации.
    /// \return Снимок placement либо \c std::nullopt, если placement неизвестен.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<PlacementView> DescribePlacement(const cybou::Hash256& publication_operation_id);

    /// \brief Возвращает локальный blob или первую BLAKE3-корректную удалённую копию.
    /// \param chunk_id ChunkId искомого blob.
    /// \return Exact stored bytes либо \c std::nullopt, если ни локально, ни у
    /// достижимых providers не найден валидный ciphertext.
    /// \post При удалённом успехе blob может быть локально закеширован.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::optional<std::vector<unsigned char>> Fetch(const ChunkId& chunk_id);

    /// \brief Возвращает целевое число удалённых реплик.
    /// \return Текущий remote replica target.
    /// \par Потокобезопасность
    /// Потокобезопасен для конкурентных вызовов одного объекта.
    std::uint8_t RemoteReplicaTarget() const { return m_target; }
    /// \brief Снимок rolling evidence по providers, с которыми работал этот сервис.
    /// \details Evidence живёт в памяти процесса и ограничена `MAX_TRACKED_STORAGE_PROVIDERS`.
    std::map<std::array<unsigned char, 32>, StorageProviderEvidence> ProviderEvidence();
    /// \brief Shadow-оценка суточного rent всех известных placements этой Identity (DEC-279).
    /// \return CYBOU в сутки при `chunk_count × target` billing units; std::nullopt при переполнении.
    std::optional<std::uint64_t> EstimatedDailyRent();
    /// \brief Выплаты settlement-периода \p period по placements этой Identity (DEC-280, DEC-282).
    /// \details Для каждой finalized аренды, активной в \p period, period cap делится по слотам
    ///          `units × replicas`: payout-аккаунт получает долю слотов (чанков), реплику которых он
    ///          держит и которая успешно проверена не раньше \p verified_since_ms; целочисленный
    ///          остаток раздаётся по одному CYBOU, начиная со смещения \p period.
    ///          Payout-аккаунт берётся из живой проверенной binding; плательщик не получает ничего,
    ///          не больше `replicas` аккаунтов на аренду. Записи упорядочены для StorageSettlement.
    std::vector<StorageSettlementEntry> SettlementEntries(std::uint64_t period, std::int64_t verified_since_ms);

private:
    struct Placement;
    std::optional<Placement> Load(const cybou::Hash256& operation_id, bool rebuilding = false) const;
    bool Save(const Placement& placement, bool rebuilding = false);
    PublicationDurability Place(std::unique_lock<std::mutex>& lock, Placement& placement);
    PublicationDurability Summarize(const Placement& placement) const;
    std::optional<std::vector<unsigned char>> FetchInternal(const ChunkId& chunk_id,
        std::span<const StorageEndpoint> preferred);
    /// Проверяет одну реплику audit challenge или полным GET; вызывается без m_mutex.
    bool CheckReplica(const StorageEndpoint& provider, const ChunkId& chunk_id,
        const std::optional<std::vector<unsigned char>>& local_bytes, bool force_full);
    void RecordEvidence(const std::array<unsigned char, 32>& storage_id,
        const std::function<void(StorageProviderEvidence&)>& update);
    /// Засчитывает непрерывное хранение реплики с прошлой успешной проверки (shadow, M4).
    void CreditReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id,
        std::uint64_t stored_bytes, std::int64_t now_ms);
    void ForgetReplica(const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id);
    void LoadEvidence();
    /// Требует m_evidence_mutex.
    void SaveEvidenceIndex();
    bool SaveReceipt(const cybou::Hash256& operation_id, const ChunkId& chunk_id,
        const StorageEndpoint& provider, std::span<const unsigned char> receipt);
    void EraseReceipt(const cybou::Hash256& operation_id, const ChunkId& chunk_id, const StorageEndpoint& provider);

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
    std::mutex m_evidence_mutex;
    std::map<std::array<unsigned char, 32>, StorageProviderEvidence> m_evidence;
    /// Время последней успешной проверки реплики; теряется при рестарте (консервативный недосчёт).
    std::map<std::pair<ChunkId, std::array<unsigned char, 32>>, std::int64_t> m_replica_verified_ms;
    std::vector<cybou::Hash256> PlacementIndex() const;
};

} // namespace cybou

#endif // CYBOU_STORAGE_SERVICE_H
