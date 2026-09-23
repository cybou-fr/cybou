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
      m_store{*m_db, m_config.network_definition}
{
    if (m_config.validator_private_key.has_value()) {
        m_authority_node = std::make_unique<CybouAuthorityNode>(
            m_store, *m_config.validator_private_key);
    }
}

bool CybouNodeRuntime::InitializeGenesis(const CybouState& genesis, const bool sync)
{
    std::lock_guard lock(m_mutex);
    if (m_store.GetFinalizedHead().has_value()) {
        return true;
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

    const auto head = m_store.GetFinalizedHead();
    if (head) {
        status.is_initialized = true;
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

std::optional<ValidatorSetV1> CybouNodeRuntime::GetValidatorSet() const
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

bool CybouNodeRuntime::SubmitOperation(ProtocolOperationV1 op)
{
    std::lock_guard lock(m_mutex);
    if (m_authority_node) {
        return m_authority_node->SubmitOperation(op);
    }
    return false;
}

std::optional<FinalizedBlockV1> CybouNodeRuntime::ProduceBlock(const bool sync)
{
    std::lock_guard lock(m_mutex);
    if (!m_authority_node) return std::nullopt;
    const auto res = m_authority_node->ProduceNextBlock(sync);
    if (!res) return std::nullopt;
    return res.finalized_block;
}

BlockTransitionResult CybouNodeRuntime::CommitBlock(const FinalizedBlockV1& block, const bool sync)
{
    std::lock_guard lock(m_mutex);
    return m_store.CommitFinalizedBlock(block, std::nullopt, sync);
}

std::optional<FinalizedBlockV1> CybouNodeRuntime::GetBlockAtHeight(const uint64_t height) const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetBlockAtHeight(height);
}

uint64_t CybouNodeRuntime::SyncFromPeer(const std::string& host, const uint16_t port, const uint64_t max_blocks)
{
    std::lock_guard lock(m_mutex);
    uint64_t synced{0};
    while (synced < max_blocks && SyncNextFinalizedBlock(m_store, host, port)) {
        ++synced;
    }
    return synced;
}

} // namespace cybou
