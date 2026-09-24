// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/node_runtime.h>

namespace cybou {

CybouNodeRuntime::CybouNodeRuntime(NodeRuntimeConfig config)
    : m_config{std::move(config)},
      m_network_id{NetworkId(m_config.network_definition)},
      m_db{std::make_unique<CDBWrapper>(DBParams{
          .path = m_config.data_dir,
          .cache_bytes = m_config.db_cache_bytes,
          .memory_only = m_config.memory_only,
          .wipe_data = m_config.wipe_data,
          .obfuscate = false,
      })},
      m_store{*m_db, m_config.network_definition},
      m_submit_endpoint{m_config.submit_endpoint}
{
    if (m_config.validator_private_key.has_value()) {
        m_authority_node = std::make_unique<CybouAuthorityNode>(
            m_store, *m_config.validator_private_key,
            m_config.memory_only ? std::nullopt :
                std::optional<std::filesystem::path>{m_config.data_dir / "validator-signing.journal"});
    }
}

bool CybouNodeRuntime::InitializeGenesis(const CybouState& genesis, const bool sync)
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NONE && loaded.state.has_value()) {
        return true;
    }
    if (loaded.error != StateLoadError::NOT_FOUND) {
        return false;
    }
    const auto result = m_store.InitializeGenesis(genesis, sync);
    return result.error == GenesisInitError::NONE;
}

NodeRuntimeStatus CybouNodeRuntime::GetStatus() const
{
    std::lock_guard lock(m_mutex);
    NodeRuntimeStatus status;
    status.network_id = m_network_id;
    status.is_authority = (m_authority_node != nullptr);

    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        status.runtime_state = NodeRuntimeState::NETWORK_MISMATCH;
    } else if (loaded.error == StateLoadError::CORRUPT || loaded.error == StateLoadError::INVALID_NETWORK_DEFINITION) {
        status.runtime_state = NodeRuntimeState::CORRUPT;
    } else if (loaded.error == StateLoadError::NOT_FOUND) {
        status.runtime_state = NodeRuntimeState::UNINITIALIZED;
    } else if (loaded.error == StateLoadError::NONE && loaded.state.has_value()) {
        status.is_initialized = true;
        status.runtime_state = NodeRuntimeState::READY;
    }

    const auto head = m_store.GetFinalizedHead();
    if (head) {
        status.finalized_height = head->height;
        status.finalized_tip = head->block_id;
    }
    const auto root = m_store.GetStateRoot();
    if (root) {
        status.state_root = *root;
    }
    const auto val_set = m_store.GetValidatorSet();
    if (val_set) {
        status.validator_count = val_set->Size();
    }
    return status;
}

std::optional<uint64_t> CybouNodeRuntime::GetFinalizedHeight() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetFinalizedHeight();
}

std::optional<uint256> CybouNodeRuntime::GetFinalizedTip() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetFinalizedTip();
}

std::optional<uint256> CybouNodeRuntime::GetStateRoot() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetStateRoot();
}

std::optional<ValidatorSet> CybouNodeRuntime::GetValidatorSet() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetValidatorSet();
}

std::optional<AccountState> CybouNodeRuntime::GetAccountState(const AccountId& account_id) const
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (!loaded || !loaded.state) return std::nullopt;
    const auto it = loaded.state->accounts.find(account_id);
    if (it == loaded.state->accounts.end()) return std::nullopt;
    return it->second;
}

OperationSubmitResult CybouNodeRuntime::SubmitOperation(ProtocolOperation op)
{
    const uint256 op_id = ComputeOperationId(op).value_or(uint256{});
    std::optional<std::pair<std::string, uint16_t>> endpoint;
    uint256 net_id{};
    {
        std::lock_guard lock(m_mutex);
        const auto loaded = m_store.LoadState();
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            return OperationSubmitResult{.status = OperationSubmitStatus::NETWORK_MISMATCH, .op_id = op_id};
        }
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_authority_node) {
            const auto status = m_authority_node->SubmitOperationWithStatus(op);
            return OperationSubmitResult{.status = status, .op_id = op_id};
        }
        endpoint = m_submit_endpoint;
        net_id = m_network_id;
    }
    if (endpoint.has_value()) {
        return SubmitOperationRemote(endpoint->first, endpoint->second, net_id, op);
    }
    return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
}

std::optional<FinalizedBlock> CybouNodeRuntime::ProduceBlock(const bool sync)
{
    std::lock_guard lock(m_mutex);
    if (!m_authority_node) return std::nullopt;
    const auto loaded = m_store.LoadState();
    if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) return std::nullopt;
    const auto res = m_authority_node->ProduceNextBlock(sync);
    if (!res) return std::nullopt;
    return res.finalized_block;
}

BlockTransitionResult CybouNodeRuntime::CommitBlock(const FinalizedBlock& block, const bool sync)
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        return BlockTransitionResult{.error = BlockTransitionError::NETWORK_MISMATCH};
    }
    if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
        return BlockTransitionResult{.error = BlockTransitionError::STATE_NOT_INITIALIZED};
    }
    return m_store.CommitFinalizedBlock(block, std::nullopt, sync);
}

std::optional<FinalizedBlock> CybouNodeRuntime::GetBlockAtHeight(const uint64_t height) const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetBlockAtHeight(height);
}

SyncPeerResult CybouNodeRuntime::SyncFromPeer(const std::string& host, const uint16_t port, const uint64_t max_blocks)
{
    SyncPeerResult result;
    {
        std::lock_guard lock(m_mutex);
        const auto loaded = m_store.LoadState();
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            result.status = SyncPeerStatus::NETWORK_MISMATCH;
            return result;
        }
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            return result;
        }
    }
    while (result.blocks_applied < max_blocks) {
        uint64_t next_height{0};
        uint256 net_id{};
        {
            std::lock_guard lock(m_mutex);
            const auto height = m_store.GetFinalizedHeight();
            if (!height || *height == std::numeric_limits<uint64_t>::max()) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
                break;
            }
            next_height = *height + 1;
            net_id = m_network_id;
        }

        const auto fetch_res = FetchFinalizedBlock(host, port, net_id, next_height);
        if (fetch_res.status == FetchBlockStatus::NOT_FOUND) {
            if (result.blocks_applied == 0) {
                result.status = SyncPeerStatus::UP_TO_DATE;
            }
            break;
        }
        if (fetch_res.status == FetchBlockStatus::CONNECTION_FAILED) {
            result.status = SyncPeerStatus::CONNECTION_FAILED;
            break;
        }
        if (fetch_res.status == FetchBlockStatus::NETWORK_MISMATCH) {
            result.status = SyncPeerStatus::NETWORK_MISMATCH;
            break;
        }
        if (fetch_res.status == FetchBlockStatus::CORRUPT_BLOCK || !fetch_res.block.has_value()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            break;
        }

        {
            std::lock_guard lock(m_mutex);
            if (!m_store.CommitFinalizedBlock(*fetch_res.block)) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
                break;
            }
        }
        ++result.blocks_applied;
        result.status = SyncPeerStatus::BLOCKS_APPLIED;
    }
    return result;
}

void CybouNodeRuntime::SetSubmitEndpoint(const std::string& host, const uint16_t port)
{
    std::lock_guard lock(m_mutex);
    m_submit_endpoint = std::make_pair(host, port);
}

bool CybouNodeRuntime::HasSubmitEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_authority_node != nullptr || m_submit_endpoint.has_value();
}

std::optional<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetSubmitEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_submit_endpoint;
}

} // namespace cybou
