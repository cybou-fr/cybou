// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_NODE_RUNTIME_H
#define CYBOU_NODE_RUNTIME_H

/// \file
/// \brief Потокобезопасный Full Node runtime: finalized state, candidate execution, relay и storage.

#include <cybou/operation_submit.h>
#include <cybou/operation_pool.h>
#include <cybou/poa_finalizer.h>
#include <cybou/account_id.h>
#include <cybou/diagnostics.h>
#include <cybou/event_record.h>
#include <cybou/protocol_limits.h>
#include <cybou/sync_result.h>
#include <cybou/network_genesis.h>
#include <cybou/official_networks.h>
#include <cybou/p2p/ingress_budget.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/state_store.h>
#include <cybou/chunk_retention.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/storage_audit.h>
#include <cybou/operation_relay.h>
#include <cybou/secret32.h>
#include <cybou/identity_signer.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cybou {
class ObservationCollector;
struct ObservationReport;
struct NetworkObservationSnapshot;
class NetworkObservationHistory;
namespace p2p { class PeerAdmissionPolicy; class PeerManager; class StorageSessionPool; class ObservationExchange; class ObservationGroups; }
class StorageIoScheduler;
class CybouKeyStore;
class IdentityOperationCoordinator;
class PoaSigner;

/// \brief Стабильная TLS identity для узла, обслуживающего compiled bootstrap locator.
/// \brief Минимальная локальная CYBOU capacity production Full Node (DEC-275).
inline constexpr uint64_t MIN_STORAGE_CAPACITY_BYTES{15ULL << 30};
/// \brief Capacity, когда оператор не выбрал `V` явно.
inline constexpr uint64_t DEFAULT_STORAGE_CAPACITY_BYTES{MIN_STORAGE_CAPACITY_BYTES};

/// \brief Provider budget `floor(2V/3)`: остаток `V` — локальный резерв, не network quota.
constexpr uint64_t ProviderBudgetBytes(const uint64_t capacity_bytes)
{
    return capacity_bytes / 3 * 2 + capacity_bytes % 3 * 2 / 3;
}

struct TlsServerIdentity {
    /// \brief Путь к certificate chain PEM/DER для входящего TLS listener.
    std::filesystem::path certificate_chain_file;
    /// \brief Путь к приватному TLS-ключу listener.
    std::filesystem::path private_key_file;
};

/// \brief Явно сконфигурированный peer с optional transport pin.
struct ConfiguredPeer {
    /// \brief Адрес и TCP-порт CYBOU P2P endpoint.
    std::pair<std::string, uint16_t> endpoint;
    /// \brief Optional TLS SPKI pin для transport authentication конкретного peer.
    std::optional<std::array<unsigned char, 32>> tls_spki_sha256;
};

/// \brief Полная конфигурация одного Full Node runtime.
struct NodeRuntimeConfig {
    /// \brief Verified immutable NetworkGenesis, определяющий сеть и PoA authority.
    VerifiedNetworkGenesis network_genesis;
    /// \brief Каталог локального runtime state и provider storage.
    std::filesystem::path data_dir;
    /// \brief Optional PoA signer secret для локального finalizer; после конструктора стирается из config.
    std::optional<Secret32> poa_finalizer_recovery_entropy{std::nullopt};
    /// \brief Compiled/operator peers; pinned locators набираются первыми как обычные Full Nodes.
    std::vector<ConfiguredPeer> configured_peers;
    /// \brief Собственный CYBOU P2P listener узла; нужен для фильтрации self-addresses.
    std::optional<std::pair<std::string, uint16_t>> advertised_endpoint{std::nullopt};
    /// \brief Размер кэша KVStore, байты.
    size_t db_cache_bytes{8 << 20};
    /// \brief true включает полностью memory-only runtime без файловой персистентности.
    bool memory_only{false};
    /// \brief true удаляет прежние локальные данные runtime/storage при открытии.
    bool wipe_data{false};
    /// \brief Только для узла bootstrap locator; обычные узлы используют ephemeral TLS.
    std::optional<TlsServerIdentity> tls_server_identity;
    /// \brief Локальная CYBOU capacity `V` (DEC-275); nullopt = DEFAULT_STORAGE_CAPACITY_BYTES.
    /// \details Production требует `V >= MIN_STORAGE_CAPACITY_BYTES`; меньшие значения и zero
    /// допустимы только в memory-only tests.
    std::optional<uint64_t> storage_capacity_bytes;
    /// \brief Optional sink для событий runtime; отсутствие логирования допустимо.
    std::shared_ptr<EventWriter> event_writer;
    /// \brief Локальная policy-проверка адресов для всех публичных P2P sockets.
    std::shared_ptr<const p2p::PeerAdmissionPolicy> peer_admission_policy;
    /// \brief Только для component tests: фиксированная сложность relay-PoW (DEC-273).
    /// \details nullopt (production) = единая сложность `RequiredOperationWorkBits`.
    std::optional<uint32_t> operation_work_bits;
};

/// \brief Строит согласованную runtime-конфигурацию из verified compiled official network.
/// \param network Official network profile с verified genesis и rendezvous peers.
/// \param data_dir Каталог локального state/store для runtime.
/// \return Конфигурация, где compiled rendezvous peers перенесены в configured_peers.
NodeRuntimeConfig MakeNodeRuntimeConfig(const OfficialNetwork& network, const std::filesystem::path& data_dir);

/// \brief Высокоуровневое состояние локального runtime и persistent state.
enum class NodeRuntimeState : uint8_t {
    /// \brief Локальное state store ещё не инициализировано genesis.
    UNINITIALIZED = 0,
    /// \brief Runtime открыт, network binding совпадает и finalized state доступен.
    READY = 1,
    /// \brief Локальные данные принадлежат другой official network.
    NETWORK_MISMATCH = 2,
    /// \brief State store повреждён или не проходит локальную верификацию.
    CORRUPT = 3,
    /// \brief PoA signing safety потребовала fail-closed остановку.
    SAFETY_HALTED = 4,
};

/// \brief Последний статус попытки локального PoA block production.
enum class BlockProductionStatus : uint8_t { PRODUCED, SIGNER_UNAVAILABLE, RETRY, SAFETY_HALT };
/// \var BlockProductionStatus::PRODUCED
/// \brief Финализованный блок успешно произведён и закоммичен локально.
/// \var BlockProductionStatus::SIGNER_UNAVAILABLE
/// \brief Для производства блока нет доступного PoA signer.
/// \var BlockProductionStatus::RETRY
/// \brief Попытка не дала блока, но может быть повторена без нарушения safety.
/// \var BlockProductionStatus::SAFETY_HALT
/// \brief Обнаружено safety condition; дальнейшее подписание должно быть остановлено.

/// \brief Снимок ключевого состояния runtime для UI, CLI и service-логики.
struct NodeRuntimeStatus {
    /// \brief NetworkBinding текущей official network.
    cybou::Hash256 network_binding;
    /// \brief Высота последнего finalized block.
    uint64_t finalized_height{0};
    /// \brief BlockID последнего finalized block.
    cybou::Hash256 finalized_tip;
    /// \brief State root текущего finalized state.
    cybou::Hash256 state_root;
    /// \brief true, если локальный PoA signer включён и ready.
    bool poa_signer_active{false};
    /// \brief true, если genesis/state уже присутствуют и читаются.
    bool is_initialized{false};
    /// \brief true, если PoA safety перешла в fail-closed halt.
    bool poa_safety_halted{false};
    /// \brief Высокоуровневое состояние runtime.
    NodeRuntimeState runtime_state{NodeRuntimeState::UNINITIALIZED};
};

/// \brief Итог поиска finalized операции в локальной проверенной истории.
enum class FinalizedOperationLookupStatus : uint8_t { FOUND, NOT_FOUND, HISTORY_UNAVAILABLE };
/// \var FinalizedOperationLookupStatus::FOUND
/// \brief Операция найдена и координаты заполнены.
/// \var FinalizedOperationLookupStatus::NOT_FOUND
/// \brief История доступна, но операция не найдена.
/// \var FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE
/// \brief История отсутствует или нарушает инварианты проверки.

/// \brief Координаты finalized операции в canonical history.
struct FinalizedOperationLookupResult {
    /// \brief Итог поиска.
    FinalizedOperationLookupStatus status{FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE};
    /// \brief До какой высоты история была успешно просмотрена.
    uint64_t scanned_height{0};
    /// \brief Высота найденного блока.
    uint64_t height{0};
    /// \brief Индекс операции внутри блока.
    uint32_t operation_index{0};
    /// \brief BlockID найденного finalized блока.
    cybou::Hash256 block_id;
};

/// \brief Итог поиска finalized KEM package для Identity и её key epoch.
enum class IdentityKemPackageLookupStatus : uint8_t {
    /// \brief Пакет найден и прошёл все локальные проверки истории.
    FOUND,
    /// \brief История доступна, но нужный пакет не найден.
    NOT_FOUND,
    /// \brief Указанный AccountID отсутствует в текущем finalized state.
    ACCOUNT_NOT_FOUND,
    /// \brief Для Identity нельзя определить требуемую key epoch.
    KEY_EPOCH_UNAVAILABLE,
    /// \brief История недоступна или нарушает инварианты согласованности.
    HISTORY_UNAVAILABLE,
};

/// \brief Результат разрешения finalized KEM package из проверенной истории.
struct IdentityKemPackageLookupResult {
    /// \brief Итог поиска.
    IdentityKemPackageLookupStatus status{IdentityKemPackageLookupStatus::HISTORY_UNAVAILABLE};
    /// \brief Найденный KEM package; значим только при FOUND.
    IdentityKemPackage package{};
    /// \brief Commitment найденного package.
    std::array<unsigned char, 32> package_id{};
    /// \brief Искомая key epoch.
    uint64_t key_epoch{0};
    /// \brief Последняя известная finalized высота на момент поиска.
    uint64_t finalized_height{0};
    /// \brief Высота операции, опубликовавшей пакет.
    uint64_t operation_height{0};
    /// \brief Индекс операции внутри блока.
    uint32_t operation_index{0};
    /// \brief BlockID блока, где найден пакет.
    cybou::Hash256 block_id;
    /// \brief State root текущего finalized state на момент поиска.
    cybou::Hash256 state_root;
};

/// \brief Локально наблюдаемый статус операции до или после finalization.
enum class OperationStatusKind : uint8_t {
    /// \brief Узел не знает этот OperationID и не видел итогов relay/finalization.
    UNKNOWN,
    /// \brief Операция удерживается локально на узле с активным PoA signer как собственный pending candidate.
    LOCAL_PENDING,
    /// \brief Операция принята хотя бы одним relay-маршрутом, но ещё не finalized локально.
    ACCEPTED_REMOTE,
    /// \brief Операция найдена в verified finalized history.
    FINALIZED,
    /// \brief Узел локально знает, что кандидат-операция была отвергнута.
    REJECTED_KNOWN,
    /// \brief Для ответа нужен history lookup, но история сейчас недоступна или повреждена.
    HISTORY_UNAVAILABLE,
};

/// \brief Локально наблюдаемый статус операции.
struct OperationStatus {
    OperationStatusKind kind{OperationStatusKind::UNKNOWN};
    uint64_t finalized_height{0};
};

/// \brief Унифицированный потокобезопасный runtime для headless и desktop Full Node.
/// \details Публичные методы сериализуют доступ к finalized state и P2P bookkeeping внутренними mutex.
///          Исключение — возвращаемые ссылки на низкоуровневые store/blob объекты: дальнейший доступ к ним
///          должен координироваться вызывающей стороной.
class CybouNodeRuntime {
public:
    /// \brief Создаёт runtime и открывает локальные хранилища выбранной official network.
    /// \param config Полная конфигурация runtime, включая verified network definition.
    /// \throws std::runtime_error или std::invalid_argument при невозможности безопасно открыть local storage.
    explicit CybouNodeRuntime(NodeRuntimeConfig config);
    /// \brief Очищает секрет provider storage и освобождает локальные ресурсы.
    ~CybouNodeRuntime();

    CybouNodeRuntime(const CybouNodeRuntime&) = delete;
    CybouNodeRuntime& operator=(const CybouNodeRuntime&) = delete;

    /// \brief Инициализирует store canonical genesis state, если state ещё не существует.
    /// \param genesis Canonical genesis state, чей root должен совпадать с network_genesis.
    /// \param sync true просит синхронно сбросить инициализацию в backing store.
    /// \return true, если state уже существовал либо был успешно инициализирован.
    /// \post При true runtime остаётся привязан к той же official network и готов к дальнейшей работе.
    bool InitializeGenesis(const CybouState& genesis, bool sync = true);

    /// \brief Возвращает текущий агрегированный статус runtime.
    /// \return Потокобезопасный snapshot finalized head, NetworkBinding и PoA safety.
    NodeRuntimeStatus GetStatus() const;
    /// \brief Возвращает диагностический snapshot runtime, peer set и storage usage.
    /// \return Данные для UI/CLI diagnostics; peer и state locks не удерживаются одновременно дольше нужного.
    NodeDiagnosticsSnapshot GetDiagnostics() const;
    /// Fixed cached DEC-289 payload; does not collect or touch chain/provider locks.
    std::array<unsigned char, 191> ReadObservationReport(const std::array<unsigned char, 32>& challenge) const;
    std::shared_ptr<p2p::ObservationExchange> GetObservationExchange() const { return m_observation_exchange; }
    // Trusted session-owner store; Record only exchange-accepted replies.
    std::shared_ptr<p2p::ObservationGroups> GetObservationGroups() const { return m_observation_groups; }
    std::shared_ptr<const NetworkObservationSnapshot> GetNetworkObservation() const;
    std::shared_ptr<TrafficMeter> GetTrafficMeter() const { return m_traffic; }
    /// \brief Доступ к optional writer'у событий runtime.
    /// \return Shared pointer на writer либо nullptr, если логирование отключено.
    std::shared_ptr<EventWriter> EventLog() const { return m_config.event_writer; }
    /// rief Replicas must sit at distinct network addresses, never on this machine (loopback).
    /// Off for memory-only component fixtures, whose providers all share loopback.
    bool RequiresReplicaAddressDiversity() const { return !m_config.memory_only; }
    /// \brief Возвращает сохранённые PoA safety evidence из local state store.
    /// \return Результат чтения evidence; содержимое зависит от локальной истории safety events.
    PoaEvidenceReadResult ReadPoaSafetyEvidence() const;

    /// \brief Verified immutable network definition runtime.
    /// \return VerifiedNetworkGenesis, с которым был открыт runtime.
    const VerifiedNetworkGenesis& GetNetworkGenesis() const { return m_config.network_genesis; }
    /// \brief NetworkBinding текущей official network.
    /// \return 32-байтовый NetworkBinding, неизменный для жизни runtime.
    const cybou::Hash256& GetNetworkBinding() const { return m_network_binding; }

    /// \brief Высота и tip canonical finalized chain, если они уже инициализированы.
    /// \return Finalized height или std::nullopt, если state не инициализирован.
    std::optional<uint64_t> GetFinalizedHeight() const;
    /// \return Finalized tip BlockID или std::nullopt, если state не инициализирован.
    std::optional<cybou::Hash256> GetFinalizedTip() const;
    /// \return Current finalized state root или std::nullopt, если state не инициализирован.
    std::optional<cybou::Hash256> GetStateRoot() const;

    /// \brief Читает finalized AccountState из локального canonical state.
    /// \param account_id Искомый AccountID.
    /// \return AccountState или std::nullopt, если state не инициализирован либо аккаунт отсутствует.
    std::optional<AccountState> GetAccountState(const AccountId& account_id) const;
    /// \brief true, если аккаунт заявил genesis-аллокацию Central Treasury (`cybou`).
    /// \details Claimant Treasury не получает onboarding bonus: он его источник (DEC-277).
    bool IsTreasuryClaimant(const AccountId& account_id) const;
    /// \brief true, пока финализированная RootPublication числится в регистре (не отозвана).
    bool IsPublicationActive(const cybou::Hash256& publication_id) const;
    /// \brief true, если финализированная аренда покрывает текущий несettled период (DEC-279).
    bool IsStorageLeaseActive(const cybou::Hash256& publication_id) const;
    /// \brief Finalized аренда публикации или std::nullopt.
    std::optional<StorageLeaseRecord> GetStorageLease(const cybou::Hash256& publication_id) const;
    /// \brief Finalized курсор StorageSettlement (следующий период и его UTC-начало).
    std::optional<StorageSettlementCursor> GetStorageSettlementCursor() const;
    /// \brief Chunks this node stores for others, per publication, with the finalized owner and name.
    struct StorageHolding {
        cybou::Hash256 publication_id;
        std::optional<AccountId> owner;
        std::optional<std::string> owner_name;
        std::uint64_t chunks{0};
        std::uint64_t bytes{0};
    };
    std::vector<StorageHolding> StorageHoldings() const;

    /// \brief Локально исполняет и подаёт операцию в candidate pool и/или relay.
    /// \param op Candidate operation; exact signed bytes будут восстановлены canonical serialization.
    /// \return Итог локального исполнения и/или relay.
    /// \post При ACCEPTED/ALREADY_PENDING/RELAY_QUEUED операция остаётся известной runtime до finalization или вытеснения.
    OperationSubmitResult SubmitOperation(ProtocolOperation op);
    /// \brief Решает relay-PoW операции (DEC-273, DEC-284); кэширует результат.
    /// \return Nonce или std::nullopt, если операция не сериализуется.
    std::optional<uint64_t> PrepareOperationWork(const ProtocolOperation& op);
    /// \brief Только для PoA signer: подписывает и ставит в pool StorageSettlement следующего периода.
    /// \param period_start_utc UTC-начало периода; после первого settlement обязано совпасть с курсором.
    /// \param entries Выплаты providers, строго упорядоченные по (publication, payout account).
    /// \return Итог локальной подготовки; невалидный против finalized state settlement отвергается pool.
    OperationSubmitResult SubmitStorageSettlement(uint64_t period_start_utc, std::vector<StorageSettlementEntry> entries);
    /// \brief Возвращает локально известный статус операции.
    /// \param op_id Искомый OperationID.
    /// \return Snapshot локального знания о candidate/finalized состоянии этой операции.
    OperationStatus GetOperationStatus(const cybou::Hash256& op_id) const;
    /// \brief Возвращает coordinator, привязанный к конкретному keystore и его nonce journal.
    /// \param keystore Конкретный keystore локальной Identity.
    /// \return Coordinator, уникальный для данного keystore.
    /// \post Для одного keystore всегда возвращается один и тот же coordinator.
    IdentityOperationCoordinator& GetIdentityOperationCoordinator(CybouKeyStore& keystore);
    /// \brief Просит все зарегистрированные coordinators повторить просроченные relay-попытки.
    /// \post Каждый известный coordinator получает шанс запланировать новый relay без удержания runtime lock во время callback.
    void RetryPendingIdentityOperations();
    /// \brief Производит следующий finalized block, если локальный узел сейчас выполняет роль PoA finalizer.
    /// \param sync true просит синхронно коммитить новый блок в store.
    /// \return Finalized block или std::nullopt, если сейчас нельзя безопасно произвести блок.
    /// \post При успехе локальный finalized head продвигается, candidate pool переисполняется, relay для finalized операций очищается.
    std::optional<FinalizedBlock> ProduceBlock(bool sync = true);
    /// \brief Возвращает статус последней попытки block production.
    /// \return Snapshot последнего итога ProduceBlock().
    BlockProductionStatus LastBlockProductionStatus() const;
    /// \brief Включает локальный PoA signer, если он соответствует genesis-authorized key.
    /// \param signer Реализация локального PoA signer.
    /// \return true только если signer принят и готов к использованию.
    /// \post При false signer не активирован; safety halt не снимается.
    bool EnablePoaSigner(std::shared_ptr<PoaSigner> signer);
    /// \brief Отключает локальное PoA signing без очистки candidate pool и journal.
    /// \post Новый блок не будет подписан, пока signer снова не включат.
    void DisablePoaSigner();
    /// \brief true, если локальный PoA signer включён и готов к подписи.
    /// \return true при активном signer.
    bool IsPoaSignerActive() const;

    /// \brief Ставит в relay только операцию, уже независимо исполненную этим узлом на finalized state.
    /// \param exact_bytes Exact canonical bytes signed operation.
    /// \param allow_seen_retry true разрешает обход seen-cache для повторной отправки источнику.
    /// \param source_peer Optional идентификатор peer-источника для per-peer accounting candidate pool.
    /// \return Статус постановки в relay-очередь.
    /// \pre exact_bytes должны кодировать ту же операцию, которую этот Full Node готов исполнить сам.
    OperationRelayEnqueueStatus EnqueueRelayedOperation(std::span<const unsigned char> exact_bytes,
        uint64_t work_nonce, bool allow_seen_retry = false, std::optional<std::string> source_peer = std::nullopt);
    /// \brief Настраивает разблокированную Identity узла для payout binding; nullptr очищает signer.
    void SetIdentitySigner(IdentitySignerRef signer);
    /// \brief Число локально исполненных, но ещё не finalized кандидатов.
    /// \return Размер candidate pool.
    size_t CandidateOperationCount() const;
    /// \brief OperationID кандидатов в порядке пула (для операторской консоли).
    std::vector<cybou::Hash256> CandidateOperationIds() const;
    /// \brief Проверяет наличие локально удерживаемого candidate operation.
    /// \param operation_id Искомый OperationID.
    bool HasCandidateOperation(const cybou::Hash256& operation_id) const;
    /// \brief Резервирует голову relay-очереди для одной активной передачи.
    /// \return Exact bytes текущей головы relay-очереди или std::nullopt.
    std::optional<RelayedOperation> ClaimRelayedOperation();
    /// Next locally valid relay candidate not already served to this session.
    std::optional<RelayedOperation> NextRelayedOperation(
        const std::function<bool(const cybou::Hash256&)>& skip) const;
    /// \brief Освобождает ранее зарезервированную relay-операцию после неуспешной отправки.
    /// \param operation_id OperationID, ранее возвращённый ClaimRelayedOperation().
    void ReleaseRelayedOperation(const cybou::Hash256& operation_id);
    /// \brief Подтверждает успешную отправку зарезервированной relay-операции.
    /// \param operation_id OperationID, ранее возвращённый ClaimRelayedOperation().
    /// \return true только если подтверждена текущая зарезервированная голова очереди.
    bool AcknowledgeRelayedOperation(const cybou::Hash256& operation_id);
    /// \brief Проверяет, удерживается ли операция в relay-очереди.
    /// \param operation_id Искомый OperationID.
    bool HasRelayedOperation(const cybou::Hash256& operation_id) const;

    /// \brief Коммитит внешний verified finalized block в локальную canonical chain.
    /// \param block Финализованный блок с PoA certificate.
    /// \param sync true просит синхронно сбросить commit в backing store.
    /// \return Итог verified commit.
    /// \post При успехе candidate pool приведён к новому finalized head.
    BlockTransitionResult CommitBlock(const FinalizedBlock& block, bool sync = true,
        BlockObservation observation = BlockObservation::HISTORY);

    /// \brief Читает finalized block по высоте из локальной canonical history.
    /// \param height Целевая высота finalized chain.
    /// \return Блок или std::nullopt, если история не содержит его локально.
    std::optional<FinalizedBlock> GetBlockAtHeight(uint64_t height) const;
    std::optional<FinalizedPublicationScan> ScanFinalizedPublications(uint64_t after, uint64_t through,
        uint64_t max_blocks) const;
    /// \brief Находит координаты finalized операции в локальной проверенной истории.
    /// \param op_id Искомый OperationID.
    /// \return Координаты, отсутствие либо признак недоступной/несогласованной истории.
    FinalizedOperationLookupResult FindFinalizedOperation(const cybou::Hash256& op_id) const;
    /// \brief Разрешает RootPublication только из проверенной canonical finalized history.
    /// \param op_id OperationID публикации.
    /// \return RootPublication или std::nullopt, если операция не найдена/не является публикацией.
    std::optional<RootPublication> FindFinalizedRootPublication(const cybou::Hash256& op_id) const;
    /// \brief Находит finalized KEM package для указанной Identity и key epoch.
    /// \param account_id Identity, чей package ищется.
    /// \param key_epoch Требуемая key epoch.
    /// \return Результат поиска с координатами либо с причиной отсутствия.
    IdentityKemPackageLookupResult FindIdentityKemPackage(
        const AccountId& account_id, uint64_t key_epoch) const;

    /// \brief Поддерживает P2P sessions, failover между peer routes и verified block sync.
    /// \param max_blocks Максимум finalized blocks, которые допустимо применить за один проход.
    /// \return Итог verified sync-pass и число применённых блоков.
    /// \post Может обновить connected peer set, retry backoff и gossip/relay activity.
    SyncPeerResult SyncFromConfiguredPeer(uint64_t max_blocks = 100);
    /// \brief Число currently connected peers.
    /// \return Число активных peer sessions.
    size_t ConnectedPeerCount() const;
    /// \brief Локальный encrypted staging/cache, присутствующий на каждом Full Node.
    /// \warning Возвращаемый объект не получает автоматической синхронизации сверх гарантий этого метода.
    ChunkBlobStore& GetChunkBlobStore() { return *m_provider.chunk_blob_store; }
    /// \brief Реестр pin/cache причин удержания локальных blob'ов без прикладной семантики.
    /// \warning Вызывающая сторона должна сама соблюдать lock discipline при прямой работе с реестром.
    ChunkRetentionRegistry& GetChunkRetention() { return *m_provider.chunk_retention; }
    /// \brief Эвиктит LRU unpinned cache blobs сверх cache_budget_bytes; pinned и admitted blobs не трогает.
    /// \param cache_budget_bytes Желаемый потолок для кэшированных неприбитых blob'ов.
    /// \param now_ms Текущее время в миллисекундах для age/grace расчётов.
    /// \param max_removals Верхняя граница числа удалений за один проход.
    /// \return Статистика сбора мусора.
    ChunkRetentionRegistry::CollectResult CollectChunkGarbage(std::uint64_t cache_budget_bytes,
        std::uint64_t now_ms, std::size_t max_removals = 256);
    /// \brief Константный доступ к локальному ChunkBlobStore.
    const ChunkBlobStore& GetChunkBlobStore() const { return *m_provider.chunk_blob_store; }
    /// \brief Пытается принять уже финализованный чанк в локальное storage.
    /// \param publication_operation_id Finalized OperationID RootPublication.
    /// \param chunk_id Идентификатор сохраняемого чанка.
    /// \param stored_bytes Exact stored encrypted bytes чанка.
    /// \param proof Finalized authorization proof от RootPublication.
    /// \return Итог storage admission.
    ChunkAdmissionResult PutFinalizedChunk(const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    /// \brief Возвращает finalized encrypted chunk из локального storage.
    /// \param chunk_id Идентификатор чанка.
    /// \return Exact stored bytes либо std::nullopt, если чанк отсутствует.
    std::optional<std::vector<unsigned char>> GetFinalizedChunk(const ChunkId& chunk_id) const;
    /// \brief Возвращает finalized authorization proof для сохранённого chunk.
    /// \param publication_operation_id Finalized OperationID RootPublication.
    /// \param chunk_id Идентификатор чанка.
    /// \return Merkle proof либо std::nullopt, если локальное storage его не знает.
    std::optional<ChunkAuthorizationProof> GetFinalizedChunkAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) const;
    /// \brief Проверяет наличие finalized chunk в локальном storage.
    /// \param chunk_id Идентификатор чанка.
    bool HasFinalizedChunk(const ChunkId& chunk_id) const;
    /// \brief Отвечает на storage audit challenge по admitted chunk (DEC-276).
    /// \return Ответ provider; `held == false`, если admitted chunk отсутствует или повреждён.
    StorageAuditAnswer AnswerStorageAudit(const StorageAuditChallenge& challenge) const;
    /// \brief Отправляет audit challenge только session с ожидаемым доказанным StorageId.
    /// \return Ответ provider либо std::nullopt, если session недоступна или ответ некорректен.
    std::optional<StorageAuditAnswer> AuditChunkAtStorageEndpoint(const std::string& address, uint16_t port,
        const std::array<unsigned char, 32>& storage_id, const StorageAuditChallenge& challenge);
    /// \brief Connected storage peer и доказанный им по запросу StorageId.
    struct StorageEndpoint {
        /// \brief Числовой адрес peer session.
        std::string address;
        /// \brief TCP-порт peer session.
        uint16_t port{0};
        /// \brief StorageId, доказанный этим peer по challenge-response.
        std::array<unsigned char, 32> storage_id{};
        /// \brief Payout-аккаунт, если binding проверен против finalized Identity registry (DEC-280).
        std::optional<AccountId> payout_account;
    };
    /// \brief Возвращает storage-capable peer sessions с уже доказанным StorageId.
    /// \return Список доступных storage endpoints.
    std::vector<StorageEndpoint> StorageEndpoints() const;
    /// \brief Отправляет chunk только peer-session, доказавшей ожидаемый StorageId.
    /// \param address Адрес peer session.
    /// \param port Порт peer session.
    /// \param storage_id Ожидаемый доказанный StorageId этой session.
    /// \param publication_operation_id Finalized OperationID RootPublication.
    /// \param chunk_id Идентификатор чанка.
    /// \param stored_bytes Exact stored encrypted bytes чанка.
    /// \param proof Finalized authorization proof.
    /// \return Итог приёма чанка удалённым Full Node либо std::nullopt, если сессия недоступна.
    std::optional<ChunkAdmissionResult> PutChunkToStorageEndpoint(const std::string& address, uint16_t port,
        const std::array<unsigned char, 32>& storage_id, const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof);
    /// \brief Загружает finalized чанк только из session с ожидаемым StorageId.
    /// \return Exact stored bytes либо std::nullopt, если session недоступна/чанк не получен.
    std::optional<std::vector<unsigned char>> GetChunkFromStorageEndpoint(const std::string& address,
        uint16_t port, const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id);
    /// \brief StorageId этого узла; доступен на каждом Full Node.
    /// \return Доказуемый локальный StorageId либо std::nullopt при неинициализированном storage secret.
    std::optional<std::array<unsigned char, 32>> LocalStorageId() const;
    /// \brief Кодирует STORAGE_PROOF для challenge-response внутри storage relationship.
    /// \param message Домен-специфичное сообщение, привязанное к handshake/challenge.
    /// \return Proof bytes либо std::nullopt, если локальный storage signer недоступен.
    std::optional<std::vector<unsigned char>> SignStorageProof(std::span<const unsigned char> message) const;
    /// \brief Payout binding этого узла: StorageId -> AccountID локальной Identity (DEC-282).
    /// \return Binding, подписанный STORAGE- и Authorization-ключом, либо std::nullopt без Identity.
    std::optional<StoragePayoutBinding> LocalStoragePayoutBinding() const;
    /// \brief Запрашивает у peer authorization proof для already finalized chunk.
    /// \return Authorization proof либо std::nullopt, если session недоступна/peer отказал.
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProofFromStorageEndpoint(
        const std::string& address, uint16_t port, const std::array<unsigned char, 32>& storage_id,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id);

    /// \brief Локальный pre-parse abuse limiter; не имеет protocol или Authority effect.
    /// \return Известный размер финализованного чанка либо std::nullopt.
    std::optional<uint64_t> FinalizedChunkSize(const ChunkId& id) const
    { return m_provider.finalized_chunk_store->StoredSize(id); }
    /// \brief Резервирует один слот активной storage transfer для address.
    /// \return RAII-handle или nullptr, если лимит по адресу исчерпан.
    std::shared_ptr<void> AcquireStorageTransfer(const std::string& address)
    { return m_network.ingress.AcquireStorageTransfer(address); }
    /// \brief Применяет локальный ingress budget до выделения памяти и чтения payload.
    /// \param address Адрес удалённой стороны.
    /// \param work Категория работы ingress limiter.
    /// \param bytes Заявленный объём байт, подлежащий резервированию.
    /// \return true, если локальная policy допускает такую работу сейчас.
    bool AdmitIngress(const std::string& address, p2p::IngressBudget::Work work, size_t bytes = 0)
    {
        // Local-network peers are the operator's own machines (DEC-285): sharing one per-IP bucket,
        // a dozen local nodes starved each other of connections and operations.
        return p2p::IsLocalNetworkAddress(address) || m_network.ingress.Admit(address, work, bytes);
    }
    /// \brief Проверяет peer address через локальную admission policy.
    /// \param numeric_address Числовой IP-адрес пира.
    /// \return true только если policy готова и разрешает адрес.
    bool AdmitPeerAddress(const std::string& numeric_address) const;
    /// \brief Возвращает набор peer endpoints, пригодных для gossip/discovery.
    /// \return Объединение configured и discovered endpoints без self-loop listener endpoint.
    std::vector<std::pair<std::string, uint16_t>> GetPeerEndpointsForGossip() const;
    /// \brief Заменяет explicit operator endpoints, сохраняя release-pinned bootstrap entries.
    /// \param endpoints Новый набор explicit endpoints.
    /// \post Собственный advertised endpoint удалён; release-pinned bootstrap locators сохраняются в configured_peers.
    void SetConfiguredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /// \brief Возвращает explicit peer endpoints без собственного listener endpoint.
    /// \return Copy configured endpoints, которые не совпадают с advertised endpoint.
    std::vector<std::pair<std::string, uint16_t>> GetConfiguredPeerEndpoints() const;
    /// \brief Добавляет проверенные discovered endpoints в локальный bounded cache.
    /// \param endpoints Уже отфильтрованные discovered endpoints.
    /// \post Локальный discovered cache остаётся bounded и не содержит self-endpoint.
    void AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /// \brief Порт локального listener'а для HELLO; 0, если узел не принимает входящие сессии.
    uint16_t ListenPort() const { return m_network.listen_port.load(); }
    void SetListenPort(uint16_t port) { m_network.listen_port.store(port); }
    /// \brief Запоминает входящего пира, объявившего listen-порт, для обратной проверки.
    /// \details Адрес становится известным (и передаётся другим) только после успешного
    ///          исходящего подключения и рукопожатия в той же сети.
    void NoteListeningPeer(const std::string& address, uint16_t port);
    /// \brief Возвращает compiled SPKI pin bootstrap locator'а либо nullopt для любого иного peer.
    /// \param address Адрес проверяемого peer.
    /// \param port Порт проверяемого peer.
    /// \return Pinned SHA-256 SPKI только для compiled bootstrap locator.
    std::optional<std::array<unsigned char, 32>> PinnedSpki(const std::string& address, uint16_t port) const;
    /// \brief Возвращает optional стабильную TLS identity listener'а.
    /// \return TLS identity bootstrap listener'а либо std::nullopt для обычного узла.
    const std::optional<TlsServerIdentity>& GetTlsServerIdentity() const { return m_config.tls_server_identity; }

    /// \brief Доступ к underlying local state store.
    /// \warning Низкоуровневый store не становится автоматически потокобезопасным после возврата ссылки.
    CybouStateStore& GetStore() { return m_chain.store; }
    /// \brief Константный доступ к underlying local state store.
    const CybouStateStore& GetStore() const { return m_chain.store; }

    // Local production wakeup revision; never persisted or sent to peers.
    uint64_t BlockProductionRevision() const;
    void WaitForBlockProductionChange(uint64_t revision, const std::atomic_bool& stop,
        std::chrono::steady_clock::time_point deadline);
    void WakeBlockProduction();

    StorageIoScheduler& StorageIo();

private:
    enum class PeerFailureClass : uint8_t { TEMPORARY, PROTOCOL, WRONG_NETWORK };
    /// \var PeerFailureClass::TEMPORARY
    /// \brief Временная недоступность маршрута без доказательства протокольной несовместимости.
    /// \var PeerFailureClass::PROTOCOL
    /// \brief Нарушение протокола/handshake; повторять такой маршрут нужно значительно реже.
    /// \var PeerFailureClass::WRONG_NETWORK
    /// \brief Узел принадлежит другой сети и должен быть исключён на долгий backoff.
    struct PeerRetryState {
        /// \brief Момент, после которого разрешена следующая попытка dial.
        std::chrono::steady_clock::time_point retry_after{};
        /// \brief Счётчик временных ошибок для экспоненциального backoff.
        uint32_t temporary_failures{0};
        /// \brief Счётчик протокольных ошибок для более длинного backoff.
        uint32_t protocol_failures{0};
    };
    OperationSubmitResult SubmitOperationInternal(ProtocolOperation op, uint64_t work_nonce,
        std::optional<std::string> source_peer);
    void SchedulePeerRetry(const std::pair<std::string, uint16_t>& endpoint, PeerFailureClass failure);
    /// rief Proves the StorageId of one known, unconnected endpoint in a short session.
    void ProbeOneStorageEndpoint();
    /// rief Starts one probe on the prober thread unless one is already running.
    void StartStorageProbe();
    void RememberOperationStatus(const cybou::Hash256& id, OperationStatus status);
    void EmitFinalizedEvents(const FinalizedBlock& block, bool produced);
    /// \brief Переисполняет кандидаты на новом head и прекращает relay для ставших невалидными.
    void RevalidateCandidates();
    void NotifyBlockProductionLocked();
    // Internal ownership domains; these are parts of one uniform Full Node.
    // Network I/O may call chain methods: never nest chain -> network locks.
    struct ChainCore {
        explicit ChainCore(const NodeRuntimeConfig& config);
        ~ChainCore();
        std::unique_ptr<KVStore> db;
        CybouStateStore store;
        OperationPool operation_pool;
        FinalizationMeter finalization_observations;
        std::map<cybou::Hash256, uint64_t> solved_work;
        std::deque<cybou::Hash256> solved_work_order;
        std::unique_ptr<PoaFinalizer> poa_finalizer;
        // Preserve the exact journaled candidate across signing/commit retries.
        BlockProductionStatus production_status{BlockProductionStatus::SIGNER_UNAVAILABLE};
        std::optional<CybouBlock> production_candidate;
        std::optional<FinalizedBlock> production_finalized;
        IdentitySignerRef identity_signer;
        OperationRelay operation_relay;
        std::map<const CybouKeyStore*, std::unique_ptr<IdentityOperationCoordinator>> identity_operation_coordinators;
        std::map<cybou::Hash256, OperationStatus> recent_operation_status;
        std::deque<cybou::Hash256> recent_operation_status_order;
        mutable std::mutex mutex;
        std::condition_variable production_cv;
        uint64_t production_revision{0};
    };
    struct ProviderCore {
        ~ProviderCore();
        void Initialize(const NodeRuntimeConfig& config, const cybou::Hash256& network_binding);
        std::unique_ptr<ChunkBlobStore> chunk_blob_store;
        std::unique_ptr<FinalizedChunkStore> finalized_chunk_store;
        std::unique_ptr<ChunkRetentionRegistry> chunk_retention;
        std::optional<std::array<unsigned char, 32>> storage_secret;
        std::optional<std::array<unsigned char, 32>> storage_id;
    };
    using Endpoint = std::pair<std::string, uint16_t>;
    struct NetworkCore {
        explicit NetworkCore(const NodeRuntimeConfig& config);
        ~NetworkCore();
        std::unique_ptr<p2p::PeerManager> peer_manager;
        std::unique_ptr<p2p::StorageSessionPool> storage_sessions;
        std::unique_ptr<StorageIoScheduler> storage_io;
        mutable std::mutex mutex;
        std::map<Endpoint, PeerRetryState> peer_retry_after;
        /** The previous sync pass applied a full batch: this node is still catching up. */
        bool catching_up{false};
        /** Storage providers proven in short sessions, outside the outbound mesh slots: block
            sync keeps few sessions, storage must still see every known provider. */
        struct ProbedStorage {
            std::array<unsigned char, 32> storage_id{};
            std::optional<StoragePayoutBinding> payout_binding;
            std::chrono::steady_clock::time_point proven_at{};
        };
        mutable std::mutex storage_probe_mutex;
        std::map<Endpoint, ProbedStorage> probed_storage;
        std::map<Endpoint, std::chrono::steady_clock::time_point> next_storage_probe;
        /** Probes run on their own thread: a dial to an unreachable endpoint waits for its
            timeout and must never hold back block sync or operation relay. */
        std::atomic_bool storage_probe_running{false};
        std::thread storage_prober;
        std::chrono::steady_clock::time_point next_peer_ping{};
        std::chrono::steady_clock::time_point next_peer_discovery{};
        // Peer callbacks can consult routes while session I/O owns mutex.
        // Routing access never acquires the session or chain mutex.
        mutable std::mutex routing_mutex;
        std::vector<ConfiguredPeer> configured_peers;
        std::optional<Endpoint> advertised_endpoint;
        p2p::IngressBudget ingress;
        std::set<Endpoint> discovered_peer_endpoints;
        /// Inbound peers that announced a listen port, not yet verified by connecting back.
        std::set<Endpoint> listener_candidates;
        std::atomic<uint16_t> listen_port{0};
    };
    NodeRuntimeConfig m_config;
    ObservationReport CollectObservationReport() const;
    std::shared_ptr<TrafficMeter> m_traffic{std::make_shared<TrafficMeter>()};
    const std::chrono::steady_clock::time_point m_observation_started{std::chrono::steady_clock::now()};
    mutable ProcessCpuMeter m_cpu_observations;
    cybou::Hash256 m_network_binding;
    std::shared_ptr<p2p::ObservationExchange> m_observation_exchange;
    std::shared_ptr<p2p::ObservationGroups> m_observation_groups;
    std::unique_ptr<NetworkObservationHistory> m_network_observation_history;
    // Reverse destruction order closes peers before provider/chain storage.
    ChainCore m_chain;
    ProviderCore m_provider;
    NetworkCore m_network;
    // Destroy/join collector before any captured runtime data is torn down.
    std::unique_ptr<ObservationCollector> m_observation_collector;
};

} // namespace cybou

#endif // CYBOU_NODE_RUNTIME_H
