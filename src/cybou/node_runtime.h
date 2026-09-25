// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_NODE_RUNTIME_H
#define CYBOU_NODE_RUNTIME_H

#include <cybou/authority_node.h>
#include <cybou/block_feed.h>
#include <cybou/network_definition.h>
#include <cybou/state_store.h>
#include <dbwrapper.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace cybou {
namespace p2p { class PeerManager; }

struct NodeRuntimeConfig {
    CybouNetworkDefinition network_definition;
    std::filesystem::path data_dir;
    std::optional<std::array<unsigned char, 32>> validator_private_key{std::nullopt};
    std::optional<std::pair<std::string, uint16_t>> submit_endpoint{std::nullopt};
    std::optional<std::pair<std::string, uint16_t>> p2p_endpoint{std::nullopt};
    size_t db_cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
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
    size_t validator_count{0};
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

    /** Current validator set */
    std::optional<ValidatorSet> GetValidatorSet() const;

    /** Account state lookup */
    std::optional<AccountState> GetAccountState(const AccountId& account_id) const;

    /** Submit an operation to pending pool (producer) or direct execution */
    OperationSubmitResult SubmitOperation(ProtocolOperation op);
    OperationSubmitResult SubmitPeerOperation(ProtocolOperation op, std::string source_peer);

    /** Produce a block if running in authority mode */
    std::optional<FinalizedBlock> ProduceBlock(bool sync = true);

    /** Commit a finalized block */
    BlockTransitionResult CommitBlock(const FinalizedBlock& block, bool sync = true);

    /** Block lookup by height */
    std::optional<FinalizedBlock> GetBlockAtHeight(uint64_t height) const;
    FinalizedOperationLookupResult FindFinalizedOperation(const uint256& op_id) const;

    /** Sync up to max_blocks from a remote peer block feed */
    SyncPeerResult SyncFromPeer(const std::string& host, uint16_t port, uint64_t max_blocks = 100);
    SyncPeerResult SyncFromConfiguredPeer(uint64_t max_blocks = 100);
    bool HasP2pEndpoint() const { return m_config.p2p_endpoint.has_value(); }

    /** Remote operation submit endpoint */
    void SetSubmitEndpoint(const std::string& host, uint16_t port);
    bool HasSubmitEndpoint() const;
    std::optional<std::pair<std::string, uint16_t>> GetSubmitEndpoint() const;

    /** Access underlying store */
    CybouStateStore& GetStore() { return m_store; }
    const CybouStateStore& GetStore() const { return m_store; }

private:
    OperationSubmitResult SubmitOperationInternal(ProtocolOperation op, std::optional<std::string> source_peer);
    NodeRuntimeConfig m_config;
    uint256 m_network_id;
    std::unique_ptr<CDBWrapper> m_db;
    CybouStateStore m_store;
    std::unique_ptr<CybouAuthorityNode> m_authority_node;
    std::optional<std::pair<std::string, uint16_t>> m_submit_endpoint;
    std::unique_ptr<p2p::PeerManager> m_peer_manager;
    mutable std::mutex m_p2p_mutex;
    mutable std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_NODE_RUNTIME_H
