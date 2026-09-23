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

struct NodeRuntimeConfig {
    CybouNetworkDefinitionV1 network_definition;
    std::filesystem::path data_dir;
    std::optional<std::array<unsigned char, 32>> validator_private_key{std::nullopt};
    size_t db_cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
};

struct NodeRuntimeStatus {
    uint256 network_id;
    uint64_t finalized_height{0};
    uint256 finalized_tip;
    uint256 state_root;
    size_t validator_count{0};
    bool is_authority{false};
    bool is_initialized{false};
};

/**
 * CybouNodeRuntime provides a unified, thread-safe runtime service
 * for both headless (cybou-node) and GUI (cybou desktop).
 */
class CybouNodeRuntime {
public:
    explicit CybouNodeRuntime(NodeRuntimeConfig config);
    ~CybouNodeRuntime() = default;

    CybouNodeRuntime(const CybouNodeRuntime&) = delete;
    CybouNodeRuntime& operator=(const CybouNodeRuntime&) = delete;

    /** Initialize store with genesis state if not already initialized. */
    bool InitializeGenesis(const CybouState& genesis, bool sync = true);

    /** Current runtime status */
    NodeRuntimeStatus GetStatus() const;

    /** Network definition and identifier */
    const CybouNetworkDefinitionV1& GetNetworkDefinition() const { return m_config.network_definition; }
    const uint256& GetNetworkId() const { return m_network_id; }

    /** Finalized height and head */
    std::optional<uint64_t> GetFinalizedHeight() const;
    std::optional<uint256> GetFinalizedTip() const;
    std::optional<uint256> GetStateRoot() const;

    /** Current validator set */
    std::optional<ValidatorSetV1> GetValidatorSet() const;

    /** Account state lookup */
    std::optional<AccountState> GetAccountState(const AccountId& account_id) const;

    /** Submit an operation to pending pool (producer) or direct execution */
    bool SubmitOperation(ProtocolOperationV1 op);

    /** Produce a block if running in authority mode */
    std::optional<FinalizedBlockV1> ProduceBlock(bool sync = true);

    /** Sync up to max_blocks from a remote peer block feed */
    uint64_t SyncFromPeer(const std::string& host, uint16_t port, uint64_t max_blocks = 100);

    /** Access underlying store */
    CybouStateStore& GetStore() { return m_store; }
    const CybouStateStore& GetStore() const { return m_store; }

private:
    NodeRuntimeConfig m_config;
    uint256 m_network_id;
    std::unique_ptr<CDBWrapper> m_db;
    CybouStateStore m_store;
    std::unique_ptr<CybouAuthorityNode> m_authority_node;
    mutable std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_NODE_RUNTIME_H
