// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_NODE_RUNTIME_H
#define CYBOU_NODE_RUNTIME_H

#include <cybou/finalizer_node.h>
#include <cybou/diagnostics.h>
#include <cybou/event_record.h>
#include <cybou/protocol_limits.h>
#include <cybou/sync_result.h>
#include <cybou/network_definition.h>
#include <cybou/p2p/ingress_budget.h>
#include <cybou/state_store.h>
#include <cybou/chunk_retention.h>
#include <cybou/finalized_chunk_store.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace cybou {
namespace p2p { class PeerManager; }
class CybouKeyStore;
class IdentityOperationCoordinator;

struct NodeRuntimeConfig {
    CybouNetworkDefinition network_definition;
    std::filesystem::path data_dir;
    std::optional<std::array<unsigned char, 32>> poa_finalizer_recovery_entropy{std::nullopt};
    std::optional<std::pair<std::string, uint16_t>> p2p_endpoint{std::nullopt};
    /** This node's own CYP2 listener; used to filter self-addresses out of discovery. */
    std::optional<std::pair<std::string, uint16_t>> local_p2p_endpoint{std::nullopt};
    size_t db_cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
    bool storage_enabled{false};
    uint64_t storage_capacity_bytes{0};
    std::shared_ptr<EventWriter> event_writer;
};

enum class NodeRuntimeState : uint8_t {
    UNINITIALIZED = 0,
    READY = 1,
    NETWORK_MISMATCH = 2,
    CORRUPT = 3,
    SAFETY_HALTED = 4,
};

struct NodeRuntimeStatus {
    uint256 network_id;
    uint64_t finalized_height{0};
    uint256 finalized_tip;
    uint256 state_root;
    bool is_finalizer{false};
    bool is_initialized{false};
    bool poa_safety_halted{false};
    NodeRuntimeState runtime_state{NodeRuntimeState::UNINITIALIZED};
};

enum class FinalizedOperationLookupStatus : uint8_t {
    FOUND, NOT_FOUND, HISTORY_UNAVAILABLE,
};

struct FinalizedOperationLookupResult {
    FinalizedOperationLookupStatus status{FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE};
    uint64_t scanned_height{0};
    uint64_t height{0};
    uint32_t operation_index{0};
    uint256 block_id;
};

enum class IdentityKemPackageLookupStatus : uint8_t {
    FOUND,
    NOT_FOUND,
    ACCOUNT_NOT_FOUND,
    KEY_EPOCH_UNAVAILABLE,
    HISTORY_UNAVAILABLE,
};

struct IdentityKemPackageLookupResult {
    IdentityKemPackageLookupStatus status{IdentityKemPackageLookupStatus::HISTORY_UNAVAILABLE};
    IdentityKemPackage package{};
    std::array<unsigned char, 32> package_id{};
    uint64_t key_epoch{0};
    uint64_t finalized_height{0};
    uint64_t operation_height{0};
    uint32_t operation_index{0};
    uint256 block_id;
    uint256 state_root;
};

enum class OperationStatusKind : uint8_t {
    UNKNOWN,
    LOCAL_PENDING,
    ACCEPTED_REMOTE,
    FINALIZED,
    REJECTED_KNOWN,
    HISTORY_UNAVAILABLE,
};

struct OperationStatus {
    OperationStatusKind kind{OperationStatusKind::UNKNOWN};
    uint64_t finalized_height{0};
};

/**
 * CybouNodeRuntime provides a unified, thread-safe runtime service
 * for both headless (cybou-node) and GUI (cybou desktop).
 */
class CybouNodeRuntime {
public:
    explicit CybouNodeRuntime(NodeRuntimeConfig config);
    ~CybouNodeRuntime();

    CybouNodeRuntime(const CybouNodeRuntime&) = delete;
    CybouNodeRuntime& operator=(const CybouNodeRuntime&) = delete;

    /** Initialize store with genesis state if not already initialized. */
    bool InitializeGenesis(const CybouState& genesis, bool sync = true);

    /** Current runtime status */
    NodeRuntimeStatus GetStatus() const;
    NodeDiagnosticsSnapshot GetDiagnostics() const;
    void SetServicePeerDiagnostics(std::vector<PeerDiagnostics> peers);
    std::shared_ptr<EventWriter> EventLog() const { return m_config.event_writer; }
    PoaEvidenceReadResult ReadPoaSafetyEvidence() const;

    /** Network definition and identifier */
    const CybouNetworkDefinition& GetNetworkDefinition() const { return m_config.network_definition; }
    const uint256& GetNetworkId() const { return m_network_id; }

    /** Finalized height and head */
    std::optional<uint64_t> GetFinalizedHeight() const;
    std::optional<uint256> GetFinalizedTip() const;
    std::optional<uint256> GetStateRoot() const;

    /** Account state lookup */
    std::optional<AccountState> GetAccountState(const AccountId& account_id) const;

    /** Submit an operation to pending pool (producer) or direct execution */
    OperationSubmitResult SubmitOperation(ProtocolOperation op);
    OperationSubmitResult SubmitPeerOperation(ProtocolOperation op, std::string source_peer);
    OperationStatus GetOperationStatus(const uint256& op_id) const;
    IdentityOperationCoordinator& GetIdentityOperationCoordinator(CybouKeyStore& keystore);
    std::vector<FinalizedHead> RecentFinalizedBlocksForGossip() const;

    /** Produce a block if running as the PoA finalizer */
    std::optional<FinalizedBlock> ProduceBlock(bool sync = true);

    /** Commit a finalized block */
    BlockTransitionResult CommitBlock(const FinalizedBlock& block, bool sync = true);

    /** Block lookup by height */
    std::optional<FinalizedBlock> GetBlockAtHeight(uint64_t height) const;
    FinalizedOperationLookupResult FindFinalizedOperation(const uint256& op_id) const;
    /** Resolve a RootPublication only from verified canonical finalized history. */
    std::optional<RootPublication> FindFinalizedRootPublication(const uint256& op_id) const;
    /** Resolve the finalized KEM capability for an Identity key epoch. */
    IdentityKemPackageLookupResult FindIdentityKemPackage(
        const AccountId& account_id, uint64_t key_epoch) const;

    /** Maintain discovered CYP2 sessions, fail over across peers, and sync verified blocks. */
    SyncPeerResult SyncFromConfiguredPeer(uint64_t max_blocks = 100);
    size_t ConnectedPeerCount() const;
    bool HasP2pEndpoint() const { return m_config.p2p_endpoint.has_value(); }
    bool HasStorageProvider() const { return m_finalized_chunk_store != nullptr; }
    /** Local encrypted staging/cache, available independently of provider mode. */
    ChunkBlobStore& GetChunkBlobStore() { return *m_chunk_blob_store; }
    /** Why local blobs stay: pins and evictable cache entries (never content semantics). */
    ChunkRetentionRegistry& GetChunkRetention() { return *m_chunk_retention; }
    /**
     * Evicts least-recently-used unpinned cache blobs over cache_budget_bytes.
     * Pinned and provider-admitted blobs are never removed.
     */
    ChunkRetentionRegistry::CollectResult CollectChunkGarbage(std::uint64_t cache_budget_bytes,
        std::uint64_t now_ms, std::size_t max_removals = 256);
    const ChunkBlobStore& GetChunkBlobStore() const { return *m_chunk_blob_store; }
    ChunkAdmissionResult PutFinalizedChunk(const uint256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes,
        const ChunkAuthorizationProof& proof);
    std::optional<std::vector<unsigned char>> GetFinalizedChunk(const ChunkId& chunk_id) const;
    std::optional<ChunkAuthorizationProof> GetFinalizedChunkAuthorizationProof(
        const uint256& publication_operation_id, const ChunkId& chunk_id) const;
    bool HasFinalizedChunk(const ChunkId& chunk_id) const;
    /** A connected CYP2 storage peer and the ProviderID it proved in the handshake. */
    struct StoragePeer {
        std::string address;
        uint16_t port{0};
        std::array<unsigned char, 32> provider_id{};
    };
    std::vector<StoragePeer> StoragePeerEndpoints() const;
    /** Storage calls go only to a session that proved the expected ProviderID. */
    std::optional<ChunkAdmissionResult> PutChunkToStoragePeer(const std::string& address, uint16_t port,
        const std::array<unsigned char, 32>& provider_id, const uint256& publication_operation_id,
        const ChunkId& chunk_id, std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof);
    std::optional<std::vector<unsigned char>> GetChunkFromStoragePeer(const std::string& address,
        uint16_t port, const std::array<unsigned char, 32>& provider_id, const ChunkId& chunk_id);
    /** This node's storage provider identity; nullopt unless storage is enabled. */
    std::optional<std::array<unsigned char, 32>> LocalProviderId() const;
    /** Encoded PROVIDER_PROOF for a handshake message; nullopt unless storage is enabled. */
    std::optional<std::vector<unsigned char>> SignProviderProof(std::span<const unsigned char> message) const;
    std::optional<std::vector<unsigned char>> SignFinalizerTransportProof(std::span<const unsigned char> message) const;
    std::optional<ChunkAuthorizationProof> GetChunkAuthorizationProofFromStoragePeer(
        const std::string& address, uint16_t port, const std::array<unsigned char, 32>& provider_id,
        const uint256& publication_operation_id, const ChunkId& chunk_id);

    /** True for the PoA finalizer or a node with a configured CYP2 finalizer peer. */
    bool CanSubmitOperations() const;

    /** Peer discovery endpoints */
    /** Local pre-parse abuse limiter; it has no protocol or Authority effect. */
    bool AdmitIngress(const std::string& address, p2p::IngressBudget::Work work, size_t bytes = 0)
    { return m_ingress.Admit(address, work, bytes); }
    std::vector<std::pair<std::string, uint16_t>> GetPeerEndpointsForGossip() const;
    /**
     * Replace the explicit peer endpoints supplied by the operator.
     * Explicit peers always come first in gossip targets and cannot be crowded
     * out by discovered routing hints.
     */
    void SetExplicitPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);
    /** The explicit peer endpoints as last configured. */
    std::vector<std::pair<std::string, uint16_t>> GetExplicitPeerEndpoints() const;
    void AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints);

    /** Access underlying store */
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
    void RememberOperationStatus(const uint256& id, OperationStatus status);
    void RememberFinalizedBlockForGossip(const FinalizedBlock& block);
    void EmitFinalizedEvents(const FinalizedBlock& block, bool produced);
    NodeRuntimeConfig m_config;
    uint256 m_network_id;
    std::unique_ptr<KVStore> m_db;
    std::unique_ptr<ChunkBlobStore> m_chunk_blob_store;
    std::unique_ptr<FinalizedChunkStore> m_finalized_chunk_store;
    std::unique_ptr<ChunkRetentionRegistry> m_chunk_retention;
    /** Storage provider key secret (persisted beside provider data). */
    std::optional<std::array<unsigned char, 32>> m_provider_secret;
    std::optional<std::array<unsigned char, 32>> m_provider_id;
    CybouStateStore m_store;
    std::unique_ptr<CybouFinalizerNode> m_finalizer_node;
    std::map<const CybouKeyStore*, std::unique_ptr<IdentityOperationCoordinator>> m_identity_operation_coordinators;
    std::map<uint256, OperationStatus> m_recent_operation_status;
    std::deque<uint256> m_recent_operation_status_order;
    std::unique_ptr<p2p::PeerManager> m_peer_manager;
    mutable std::mutex m_p2p_mutex;
    std::map<std::pair<std::string, uint16_t>, PeerRetryState> m_peer_retry_after;
    std::chrono::steady_clock::time_point m_next_peer_ping{};
    std::chrono::steady_clock::time_point m_next_peer_discovery{};
    mutable std::mutex m_mutex;
    std::deque<FinalizedHead> m_recent_finalized_blocks;
    std::vector<PeerDiagnostics> m_service_peers;
    using Endpoint = std::pair<std::string, uint16_t>;
    std::set<Endpoint> m_explicit_peer_endpoints;
    p2p::IngressBudget m_ingress;
    std::set<Endpoint> m_discovered_peer_endpoints;
};

} // namespace cybou

#endif // CYBOU_NODE_RUNTIME_H
