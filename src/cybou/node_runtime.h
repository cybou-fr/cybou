// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_NODE_RUNTIME_H
#define CYBOU_NODE_RUNTIME_H

#include <cybou/authority_node.h>
#include <cybou/block_feed.h>
#include <cybou/network_definition.h>
#include <cybou/state_store.h>
#include <cybou/storage_store.h>

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
    std::optional<std::array<unsigned char, 32>> validator_private_key{std::nullopt};
    std::optional<std::pair<std::string, uint16_t>> submit_endpoint{std::nullopt};
    std::optional<std::pair<std::string, uint16_t>> p2p_endpoint{std::nullopt};
    /** This node's own CYP2 listener; used to filter self-addresses out of discovery. */
    std::optional<std::pair<std::string, uint16_t>> local_p2p_endpoint{std::nullopt};
    size_t db_cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
    bool storage_enabled{false};
    uint64_t storage_capacity_bytes{0};
};

enum class NodeRuntimeState : uint8_t {
    UNINITIALIZED = 0,
    READY = 1,
    NETWORK_MISMATCH = 2,
    CORRUPT = 3,
};

struct NodeRuntimeStatus {
    uint256 network_id;
    uint64_t finalized_height{0};
    uint256 finalized_tip;
    uint256 state_root;
    bool is_authority{false};
    bool is_initialized{false};
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

    /** Produce a block if running in authority mode */
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

    /** Sync up to max_blocks from a remote peer block feed */
    SyncPeerResult SyncFromPeer(const std::string& host, uint16_t port, uint64_t max_blocks = 100);
    /** Maintain discovered CYP2 sessions, fail over across peers, and sync verified blocks. */
    SyncPeerResult SyncFromConfiguredPeer(uint64_t max_blocks = 100);
    size_t ConnectedPeerCount() const;
    bool HasP2pEndpoint() const { return m_config.p2p_endpoint.has_value(); }
    bool HasStorageProvider() const { return m_storage_store != nullptr; }
    StorageWriteResult StoreEncryptedChunk(const StorageObjectId& object_id, const StorageEncryptedChunk& chunk);
    StorageWriteResult CommitStoredManifest(const StoragePublicManifest& manifest);
    bool AbortStoredObject(const StorageObjectId& object_id, uint32_t chunk_count);
    uint64_t GarbageCollectStorageStaging();
    std::optional<StoragePublicManifest> GetStoredManifest(const StorageObjectId& object_id) const;
    std::optional<StorageEncryptedChunk> GetStoredChunk(const StorageObjectId& object_id, uint32_t index) const;

    /** Remote operation submit endpoint */
    void SetSubmitEndpoint(const std::string& host, uint16_t port);
    bool HasSubmitEndpoint() const;
    std::optional<std::pair<std::string, uint16_t>> GetSubmitEndpoint() const;

    /** Peer discovery endpoints */
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
    NodeRuntimeConfig m_config;
    uint256 m_network_id;
    std::unique_ptr<KVStore> m_db;
    std::unique_ptr<StorageObjectStore> m_storage_store;
    CybouStateStore m_store;
    std::unique_ptr<CybouAuthorityNode> m_authority_node;
    std::unique_ptr<IdentityOperationCoordinator> m_identity_operation_coordinator;
    std::map<uint256, OperationStatus> m_recent_operation_status;
    std::deque<uint256> m_recent_operation_status_order;
    std::optional<std::pair<std::string, uint16_t>> m_submit_endpoint;
    std::unique_ptr<p2p::PeerManager> m_peer_manager;
    mutable std::mutex m_p2p_mutex;
    std::map<std::pair<std::string, uint16_t>, PeerRetryState> m_peer_retry_after;
    std::chrono::steady_clock::time_point m_next_peer_ping{};
    std::chrono::steady_clock::time_point m_next_peer_discovery{};
    mutable std::mutex m_mutex;
    std::deque<FinalizedHead> m_recent_finalized_blocks;
    using Endpoint = std::pair<std::string, uint16_t>;
    std::set<Endpoint> m_explicit_peer_endpoints;
    std::set<Endpoint> m_discovered_peer_endpoints;
};

} // namespace cybou

#endif // CYBOU_NODE_RUNTIME_H
