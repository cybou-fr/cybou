// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

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
#include <cybou/state_store.h>
#include <cybou/chunk_retention.h>
#include <cybou/finalized_chunk_store.h>
#include <cybou/operation_relay.h>
#include <cybou/secret32.h>
#include <cybou/validation_pool.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cybou {
namespace p2p { class PeerAdmissionPolicy; class PeerManager; }
class CybouKeyStore;
class IdentityOperationCoordinator;
class PoaSigner;

/// \brief Стабильная TLS identity для узла, обслуживающего compiled bootstrap locator.
struct TlsServerIdentity {
    std::filesystem::path certificate_chain_file;
    std::filesystem::path private_key_file;
};

/// \brief Явно сконфигурированный peer с optional transport pin.
struct ConfiguredPeer {
    std::pair<std::string, uint16_t> endpoint;
    std::optional<std::array<unsigned char, 32>> tls_spki_sha256;
};

/// \brief Полная конфигурация одного Full Node runtime.
struct NodeRuntimeConfig {
    VerifiedNetworkGenesis network_genesis;
    /// \brief Каталог локального runtime state и provider storage.
    std::filesystem::path data_dir;
    std::optional<Secret32> poa_finalizer_recovery_entropy{std::nullopt};
    /// \brief Compiled/operator peers; pinned locators набираются первыми как обычные Full Nodes.
    std::vector<ConfiguredPeer> configured_peers;
    /// \brief Собственный CYBOU P2P listener узла; нужен для фильтрации self-addresses.
    std::optional<std::pair<std::string, uint16_t>> advertised_endpoint{std::nullopt};
    size_t db_cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
    /// \brief Только для узла bootstrap locator; обычные узлы используют ephemeral TLS.
    std::optional<TlsServerIdentity> tls_server_identity;
    /// \brief nullopt = автоматическая local storage allocation; zero допустим только в memory-only tests.
    std::optional<uint64_t> storage_capacity_bytes;
    std::shared_ptr<EventWriter> event_writer;
    /// \brief Локальная policy-проверка адресов для всех публичных P2P sockets.
    std::shared_ptr<const p2p::PeerAdmissionPolicy> peer_admission_policy;
};

/// \brief Строит согласованную runtime-конфигурацию из verified compiled official network.
NodeRuntimeConfig MakeNodeRuntimeConfig(const OfficialNetwork& network, const std::filesystem::path& data_dir);

/// \brief Высокоуровневое состояние локального runtime и persistent state.
enum class NodeRuntimeState : uint8_t {
    UNINITIALIZED = 0,
    READY = 1,
    NETWORK_MISMATCH = 2,
    CORRUPT = 3,
    SAFETY_HALTED = 4,
};

/// \brief Последний статус попытки локального PoA block production.
enum class BlockProductionStatus : uint8_t { PRODUCED, SIGNER_UNAVAILABLE, RETRY, SAFETY_HALT };

/// \brief Снимок ключевого состояния runtime для UI, CLI и service-логики.
struct NodeRuntimeStatus {
    cybou::Hash256 network_binding;
    uint64_t finalized_height{0};
    cybou::Hash256 finalized_tip;
    cybou::Hash256 state_root;
    bool poa_signer_active{false};
    bool is_initialized{false};
    bool poa_safety_halted{false};
    NodeRuntimeState runtime_state{NodeRuntimeState::UNINITIALIZED};
};

/// \brief Итог поиска finalized операции в локальной проверенной истории.
enum class FinalizedOperationLookupStatus : uint8_t {
    FOUND, NOT_FOUND, HISTORY_UNAVAILABLE,
};

/// \brief Координаты finalized операции в canonical history.
struct FinalizedOperationLookupResult {
    FinalizedOperationLookupStatus status{FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE};
    uint64_t scanned_height{0};
    uint64_t height{0};
    uint32_t operation_index{0};
    cybou::Hash256 block_id;
};

/// \brief Итог поиска finalized KEM package для Identity и её key epoch.
enum class IdentityKemPackageLookupStatus : uint8_t {
    FOUND,
    NOT_FOUND,
    ACCOUNT_NOT_FOUND,
    KEY_EPOCH_UNAVAILABLE,
    HISTORY_UNAVAILABLE,
};

/// \brief Результат разрешения finalized KEM package из проверенной истории.
struct IdentityKemPackageLookupResult {
    IdentityKemPackageLookupStatus status{IdentityKemPackageLookupStatus::HISTORY_UNAVAILABLE};
    IdentityKemPackage package{};
    std::array<unsigned char, 32> package_id{};
    uint64_t key_epoch{0};
    uint64_t finalized_height{0};
    uint64_t operation_height{0};
    uint32_t operation_index{0};
    cybou::Hash256 block_id;
    cybou::Hash256 state_root;
};

/// \brief Локально наблюдаемый статус операции до или после finalization.
enum class OperationStatusKind : uint8_t {
    UNKNOWN,
    LOCAL_PENDING,
    ACCEPTED_REMOTE,
    FINALIZED,
    REJECTED_KNOWN,
    HISTORY_UNAVAILABLE,
};

/// \brief Локально наблюдаемый статус операции и число удерживаемых Validation-attestations.
struct OperationStatus {
    OperationStatusKind kind{OperationStatusKind::UNKNOWN};
    uint64_t finalized_height{0};
    /// \brief Число verified eligible attestations для локально валидного, но ещё не finalized кандидата.
    uint32_t validation_signatures{0};

    /// \brief true, если кандидат локально валиден и имеет хотя бы одну eligible attestation; state не меняется.
    bool IsValidated() const { return kind != OperationStatusKind::FINALIZED && validation_signatures > 0; }
};

/// \brief Итог приёма peer Validation-attestation для уже исполненного локального кандидата.
enum class ValidationAcceptStatus : uint8_t {
    ADDED,
    DUPLICATE,
    /// \brief Узел не держит локально валидный candidate с данным OperationID.
    NOT_CANDIDATE,
    STALE_BASE,
    INVALID,
    FULL,
};

/// \brief Унифицированный потокобезопасный runtime для headless и desktop Full Node.
class CybouNodeRuntime {
public:
    explicit CybouNodeRuntime(NodeRuntimeConfig config);
    ~CybouNodeRuntime();

    CybouNodeRuntime(const CybouNodeRuntime&) = delete;
    CybouNodeRuntime& operator=(const CybouNodeRuntime&) = delete;

    /// \brief Инициализирует store canonical genesis state, если state ещё не существует.
    bool InitializeGenesis(const CybouState& genesis, bool sync = true);

    /// \brief Возвращает текущий агрегированный статус runtime.
    NodeRuntimeStatus GetStatus() const;
    /// \brief Возвращает диагностический snapshot runtime, peer set и storage usage.
    NodeDiagnosticsSnapshot GetDiagnostics() const;
    /// \brief Доступ к optional writer'у событий runtime.
    std::shared_ptr<EventWriter> EventLog() const { return m_config.event_writer; }
    /// \brief Возвращает сохранённые PoA safety evidence из local state store.
    PoaEvidenceReadResult ReadPoaSafetyEvidence() const;

    /// \brief Verified immutable network definition runtime.
    const VerifiedNetworkGenesis& GetNetworkGenesis() const { return m_config.network_genesis; }
    /// \brief NetworkBinding текущей official network.
    const cybou::Hash256& GetNetworkBinding() const { return m_network_binding; }

    /// \brief Высота и tip canonical finalized chain, если они уже инициализированы.
    std::optional<uint64_t> GetFinalizedHeight() const;
    std::optional<cybou::Hash256> GetFinalizedTip() const;
    std::optional<cybou::Hash256> GetStateRoot() const;

    /// \brief Читает finalized AccountState из локального canonical state.
    std::optional<AccountState> GetAccountState(const AccountId& account_id) const;

    /// \brief Локально исполняет и подаёт операцию в candidate pool и/или relay.
    OperationSubmitResult SubmitOperation(ProtocolOperation op);
    /// \brief Только для PoA signer: подписывает AUTH GRANT/BURN, валидный лишь для следующего блока.
    OperationSubmitResult SubmitPoaAuthAdjustment(PoaAuthAction action, const AccountId& target, uint64_t amount);
    /// \brief Возвращает локально известный статус операции.
    OperationStatus GetOperationStatus(const cybou::Hash256& op_id) const;
    /// \brief Возвращает coordinator, привязанный к конкретному keystore и его nonce journal.
    IdentityOperationCoordinator& GetIdentityOperationCoordinator(CybouKeyStore& keystore);
    /// \brief Просит все зарегистрированные coordinators повторить просроченные relay-попытки.
    void RetryPendingIdentityOperations();
    /// \brief Возвращает недавние finalized heads для gossip.
    std::vector<FinalizedHead> RecentFinalizedBlocksForGossip() const;

    /// \brief Производит следующий finalized block, если локальный узел сейчас выполняет роль PoA finalizer.
    std::optional<FinalizedBlock> ProduceBlock(bool sync = true);
    /// \brief Возвращает статус последней попытки block production.
    BlockProductionStatus LastBlockProductionStatus() const;
    /// \brief Включает локальный PoA signer, если он соответствует genesis-authorized key.
    bool EnablePoaSigner(std::shared_ptr<PoaSigner> signer);
    /// \brief Отключает локальное PoA signing без очистки candidate pool и journal.
    void DisablePoaSigner();
    /// \brief true, если локальный PoA signer включён и готов к подписи.
    bool IsPoaSignerActive() const;

    /// \brief Ставит в relay только операцию, уже независимо исполненную этим узлом на finalized state.
    OperationRelayEnqueueStatus EnqueueRelayedOperation(std::span<const unsigned char> exact_bytes,
        bool allow_seen_retry = false, std::optional<std::string> source_peer = std::nullopt);
    /// \brief Настраивает локальную Identity для attestation кандидатов; nullptr очищает signer.
    void SetValidationSigner(ValidationSignerRef signer);
    /// \brief Проверяет право локального signer'а подписывать Validation в текущем finalized state.
    bool IsLocalValidationEligible() const;
    /// \brief Проверяет peer-attestation только против локального кандидата и собственного finalized state.
    ValidationAcceptStatus AcceptValidationAttestation(const ValidationAttestation& attestation);
    /// \brief Возвращает удерживаемые attestations для одной локальной candidate operation.
    std::vector<ValidationAttestation> GetValidationAttestations(const cybou::Hash256& operation_id) const;
    /// \brief Возвращает следующую attestation, ещё неизвестную вызывающей стороне.
    std::optional<ValidationAttestation> NextValidationAttestation(
        const std::function<bool(const ValidationPool::Key&)>& skip) const;
    /// \brief Число локально исполненных, но ещё не finalized кандидатов.
    size_t CandidateOperationCount() const;
    /// \brief Проверяет наличие локально удерживаемого candidate operation.
    bool HasCandidateOperation(const cybou::Hash256& operation_id) const;
    /// \brief Резервирует голову relay-очереди для одной активной передачи.
    std::optional<RelayedOperation> ClaimRelayedOperation();
    /// \brief Освобождает ранее зарезервированную relay-операцию после неуспешной отправки.
    void ReleaseRelayedOperation(const cybou::Hash256& operation_id);
    /// \brief Подтверждает успешную отправку зарезервированной relay-операции.
    bool AcknowledgeRelayedOperation(const cybou::Hash256& operation_id);
    /// \brief Проверяет, удерживается ли операция в relay-очереди.
    bool HasRelayedOperation(const cybou::Hash256& operation_id) const;

    /// \brief Коммитит внешний verified finalized block в локальную canonical chain.
    BlockTransitionResult CommitBlock(const FinalizedBlock& block, bool sync = true);

    /// \brief Читает finalized block по высоте из локальной canonical history.
    std::optional<FinalizedBlock> GetBlockAtHeight(uint64_t height) const;
    FinalizedOperationLookupResult FindFinalizedOperation(const cybou::Hash256& op_id) const;
    /// \brief Разрешает RootPublication только из проверенной canonical finalized history.
    std::optional<RootPublication> FindFinalizedRootPublication(const cybou::Hash256& op_id) const;
    /// \brief Находит finalized KEM package для указанной Identity и key epoch.
    IdentityKemPackageLookupResult FindIdentityKemPackage(
        const AccountId& account_id, uint64_t key_epoch) const;

    /// \brief Поддерживает P2P sessions, failover между peer routes и verified block sync.
    SyncPeerResult SyncFromConfiguredPeer(uint64_t max_blocks = 100);
    /// \brief Число currently connected peers.
    size_t ConnectedPeerCount() const;
    /// \brief Локальный encrypted staging/cache, присутствующий на каждом Full Node.
    ChunkBlobStore& GetChunkBlobStore() { return *m_chunk_blob_store; }
    /// \brief Реестр pin/cache причин удержания локальных blob'ов без прикладной семантики.
    ChunkRetentionRegistry& GetChunkRetention() { return *m_chunk_retention; }
    /// \brief Эвиктит LRU unpinned cache blobs сверх cache_budget_bytes; pinned и admitted blobs не трогает.
    ChunkRetentionRegistry::CollectResult CollectChunkGarbage(std::uint64_t cache_budget_bytes,
        std::uint64_t now_ms, std::size_t max_removals = 256);
    const ChunkBlobStore& GetChunkBlobStore() const { return *m_chunk_blob_store; }
    ChunkAdmissionResult PutFinalizedChunk(const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    /// \brief Возвращает finalized encrypted chunk из локального storage.
    std::optional<std::vector<unsigned char>> GetFinalizedChunk(const ChunkId& chunk_id) const;
    /// \brief Возвращает finalized authorization proof для сохранённого chunk.
    std::optional<ChunkAuthorizationProof> GetFinalizedChunkAuthorizationProof(
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) const;
    /// \brief Проверяет наличие finalized chunk в локальном storage.
    bool HasFinalizedChunk(const ChunkId& chunk_id) const;
    /// \brief Connected storage peer и доказанный им по запросу StorageId.
    struct StorageEndpoint {
        std::string address;
        uint16_t port{0};
        std::array<unsigned char, 32> storage_id{};
    };
    std::vector<StorageEndpoint> StorageEndpoints() const;
    /// \brief Отправляет chunk только peer-session, доказавшей ожидаемый StorageId.
    std::optional<ChunkAdmissionResult> PutChunkToStorageEndpoint(const std::string& address, uint16_t port,
        const std::array<unsigned char, 32>& storage_id, const cybou::Hash256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof);
    std::optional<std::vector<unsigned char>> GetChunkFromStorageEndpoint(const std::string& address,
        uint16_t port, const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id);
    /// \brief StorageId этого узла; доступен на каждом Full Node.
    std::optional<std::array<unsigned char, 32>> LocalStorageId() const;
    /// \brief Кодирует STORAGE_PROOF для challenge-response внутри storage relationship.
    std::optional<std::vector<unsigned char>> SignStorageProof(std::span<const unsigned char> message) const;
    /// \brief Запрашивает у peer authorization proof для already finalized chunk.
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProofFromStorageEndpoint(
        const std::string& address, uint16_t port, const std::array<unsigned char, 32>& storage_id,
        const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id);

    /// \brief Локальный pre-parse abuse limiter; не имеет protocol или Authority effect.
    std::optional<uint64_t> FinalizedChunkSize(const ChunkId& id) const
    { return m_finalized_chunk_store->StoredSize(id); }
    std::shared_ptr<void> AcquireStorageTransfer(const std::string& address)
    { return m_ingress.AcquireStorageTransfer(address); }
    bool AdmitIngress(const std::string& address, p2p::IngressBudget::Work work, size_t bytes = 0)
    { return m_ingress.Admit(address, work, bytes); }
    /// \brief Проверяет peer address через локальную admission policy.
    bool AdmitPeerAddress(const std::string& numeric_address) const;
    /// \brief Возвращает набор peer endpoints, пригодных для gossip/discovery.
    std::vector<std::pair<std::string, uint16_t>> GetPeerEndpointsForGossip() const;
    /// \brief Заменяет explicit operator endpoints, сохраняя release-pinned bootstrap entries.
    void SetConfiguredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /// \brief Возвращает explicit peer endpoints без собственного listener endpoint.
    std::vector<std::pair<std::string, uint16_t>> GetConfiguredPeerEndpoints() const;
    /// \brief Добавляет проверенные discovered endpoints в локальный bounded cache.
    void AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /// \brief Возвращает compiled SPKI pin bootstrap locator'а либо nullopt для любого иного peer.
    std::optional<std::array<unsigned char, 32>> PinnedSpki(const std::string& address, uint16_t port) const;
    /// \brief Возвращает optional стабильную TLS identity listener'а.
    const std::optional<TlsServerIdentity>& GetTlsServerIdentity() const { return m_config.tls_server_identity; }

    /// \brief Доступ к underlying local state store.
    CybouStateStore& GetStore() { return m_store; }
    const CybouStateStore& GetStore() const { return m_store; }

private:
    enum class PeerFailureClass : uint8_t { TEMPORARY, PROTOCOL, WRONG_NETWORK };
    struct PeerRetryState {
        std::chrono::steady_clock::time_point retry_after{};
        uint32_t temporary_failures{0};
        uint32_t protocol_failures{0};
    };
    OperationSubmitResult SubmitOperationInternal(ProtocolOperation op, std::optional<std::string> source_peer);
    void SchedulePeerRetry(const std::pair<std::string, uint16_t>& endpoint, PeerFailureClass failure);
    void RememberOperationStatus(const cybou::Hash256& id, OperationStatus status);
    void RememberFinalizedBlockForGossip(const FinalizedBlock& block);
    void EmitFinalizedEvents(const FinalizedBlock& block, bool produced);
    /// \brief Переисполняет кандидаты на новом head и прекращает relay для ставших невалидными.
    void RevalidateCandidates();
    /// \brief Подписывает одну локально принятую candidate operation на текущем finalized base, если signer eligible.
    void AttestCandidate(const cybou::Hash256& operation_id);
    NodeRuntimeConfig m_config;
    cybou::Hash256 m_network_binding;
    std::unique_ptr<KVStore> m_db;
    std::unique_ptr<ChunkBlobStore> m_chunk_blob_store;
    std::unique_ptr<FinalizedChunkStore> m_finalized_chunk_store;
    std::unique_ptr<ChunkRetentionRegistry> m_chunk_retention;
    /// \brief Секрет storage provider key, сохраняемый рядом с provider data.
    std::optional<std::array<unsigned char, 32>> m_storage_secret;
    std::optional<std::array<unsigned char, 32>> m_storage_id;
    CybouStateStore m_store;
    /// \brief Собственный volatile candidate pool Full Node; PoA-узел собирает блоки только из него.
    OperationPool m_operation_pool{m_store};
    std::unique_ptr<PoaFinalizer> m_poa_finalizer;
    // Preserve the exact journaled candidate across signing/commit retries.
    BlockProductionStatus m_production_status{BlockProductionStatus::SIGNER_UNAVAILABLE};
    std::optional<CybouBlock> m_production_candidate;
    std::optional<FinalizedBlock> m_production_finalized;
    ValidationPool m_validation_pool;
    ValidationSignerRef m_validation_signer;
    OperationRelay m_operation_relay;
    std::map<const CybouKeyStore*, std::unique_ptr<IdentityOperationCoordinator>> m_identity_operation_coordinators;
    std::map<cybou::Hash256, OperationStatus> m_recent_operation_status;
    std::deque<cybou::Hash256> m_recent_operation_status_order;
    std::unique_ptr<p2p::PeerManager> m_peer_manager;
    mutable std::mutex m_p2p_mutex;
    std::map<std::pair<std::string, uint16_t>, PeerRetryState> m_peer_retry_after;
    std::chrono::steady_clock::time_point m_next_peer_ping{};
    std::chrono::steady_clock::time_point m_next_peer_discovery{};
    mutable std::mutex m_mutex;
    std::deque<FinalizedHead> m_recent_finalized_blocks;
    using Endpoint = std::pair<std::string, uint16_t>;
    p2p::IngressBudget m_ingress;
    std::set<Endpoint> m_discovered_peer_endpoints;
};

} // namespace cybou

#endif // CYBOU_NODE_RUNTIME_H
