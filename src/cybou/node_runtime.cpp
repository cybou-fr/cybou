// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/node_runtime.h>
#include <cybou/p2p/peer_manager.h>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>

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
    if (m_config.p2p_endpoint) m_peer_manager = std::make_unique<p2p::PeerManager>(*this);
}

CybouNodeRuntime::~CybouNodeRuntime() = default;

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
    return SubmitOperationInternal(std::move(op), std::nullopt);
}

OperationSubmitResult CybouNodeRuntime::SubmitPeerOperation(ProtocolOperation op, std::string source_peer)
{
    return SubmitOperationInternal(std::move(op), std::move(source_peer));
}

std::optional<OperationSubmitStatus> CybouNodeRuntime::KnownOperationStatus(const uint256& op_id) const
{
    if (op_id.IsNull()) return std::nullopt;
    std::lock_guard lock(m_mutex);
    if (m_authority_node && m_authority_node->HasPendingOperation(op_id)) {
        return OperationSubmitStatus::ALREADY_PENDING;
    }
    if (m_store.HasIndexedFinalizedOperation(op_id)) return OperationSubmitStatus::ALREADY_FINALIZED;
    return std::nullopt;
}

std::vector<ProtocolOperation> CybouNodeRuntime::RecentOperationsForGossip() const
{
    std::lock_guard lock(m_mutex);
    std::vector<ProtocolOperation> operations;
    operations.reserve(m_recent_gossip_operations.size());
    for (const auto& entry : m_recent_gossip_operations) operations.push_back(entry.operation);
    return operations;
}

std::vector<FinalizedHead> CybouNodeRuntime::RecentFinalizedBlocksForGossip() const
{
    std::lock_guard lock(m_mutex);
    return {m_recent_finalized_blocks.begin(), m_recent_finalized_blocks.end()};
}

void CybouNodeRuntime::RememberFinalizedBlockForGossip(const FinalizedBlock& block)
{
    const auto id = ComputeBlockId(block.block);
    if (id.IsNull()) return;
    if (!m_recent_finalized_blocks.empty() &&
        m_recent_finalized_blocks.back().height >= block.block.height) return;
    m_recent_finalized_blocks.push_back({id, block.block.height});
    if (m_recent_finalized_blocks.size() > 32) m_recent_finalized_blocks.pop_front();
}

void CybouNodeRuntime::RememberOperationForGossip(const ProtocolOperation& op, const uint256& id)
{
    if (id.IsNull() || m_recent_gossip_ids.contains(id)) return;
    const auto encoded = SerializeProtocolOperation(op);
    if (!encoded || encoded->empty() || encoded->size() > MAX_PENDING_OPERATION_BYTES) return;
    while (!m_recent_gossip_operations.empty() &&
        (m_recent_gossip_operations.size() >= MAX_PENDING_OPERATIONS ||
         encoded->size() > MAX_PENDING_OPERATION_BYTES - m_recent_gossip_bytes)) {
        m_recent_gossip_bytes -= m_recent_gossip_operations.front().bytes;
        m_recent_gossip_ids.erase(m_recent_gossip_operations.front().id);
        m_recent_gossip_operations.pop_front();
    }
    m_recent_gossip_operations.push_back({op, id, encoded->size()});
    m_recent_gossip_ids.insert(id);
    m_recent_gossip_bytes += encoded->size();
}

OperationSubmitResult CybouNodeRuntime::SubmitOperationInternal(
    ProtocolOperation op, std::optional<std::string> source_peer)
{
    const uint256 op_id = ComputeOperationId(op).value_or(uint256{});
    std::optional<std::pair<std::string, uint16_t>> endpoint;
    std::optional<std::pair<std::string, uint16_t>> p2p_endpoint;
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
            const auto status = m_authority_node->SubmitOperationWithStatus(op, std::move(source_peer));
            if (status == OperationSubmitStatus::ACCEPTED) RememberOperationForGossip(op, op_id);
            return OperationSubmitResult{.status = status, .op_id = op_id};
        }
        endpoint = m_submit_endpoint;
        p2p_endpoint = m_config.p2p_endpoint;
        net_id = m_network_id;
    }
    if (p2p_endpoint && m_peer_manager) {
        std::lock_guard p2p_lock(m_p2p_mutex);
        if (m_peer_manager->ConnectedCount() == 0 &&
            !m_peer_manager->Connect(p2p_endpoint->first, p2p_endpoint->second)) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        return m_peer_manager->SubmitOperation(p2p_endpoint->first, p2p_endpoint->second, op);
    }
    if (endpoint.has_value()) {
        return SubmitOperationRemote(endpoint->first, endpoint->second, net_id, op);
    }
    return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
}

std::optional<FinalizedBlock> CybouNodeRuntime::ProduceBlock(const bool sync)
{
    std::optional<BftProposalMsg> prop;
    std::optional<BftPrevoteMsg> pv;
    std::optional<BftPrecommitMsg> pc;
    std::optional<FinalizedBlock> finalized;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return std::nullopt;
        const auto loaded = m_store.LoadState();
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) return std::nullopt;
        const auto set = m_store.GetValidatorSet();
        if (!set) return std::nullopt;

        if (set->validators.size() == 1) {
            const auto res = m_authority_node->ProduceNextBlock(sync);
            if (!res) return std::nullopt;
            if (res.finalized_block) RememberFinalizedBlockForGossip(*res.finalized_block);
            return res.finalized_block;
        }

        finalized = m_authority_node->GetLatestFinalizedBlock();
        if (finalized) RememberFinalizedBlockForGossip(*finalized);
    }
    return finalized;
}

void CybouNodeRuntime::TickConsensus(const std::chrono::milliseconds round_timeout)
{
    if (round_timeout.count() <= 0) return;
    std::optional<BftProposalMsg> proposal;
    std::optional<BftPrevoteMsg> prevote;
    std::optional<BftPrecommitMsg> precommit;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return;
        const auto head = m_store.GetFinalizedHead();
        const auto set = m_store.GetValidatorSet();
        if (!head || !set || set->validators.size() <= 1 ||
            head->height == std::numeric_limits<uint64_t>::max()) return;
        const auto height = head->height + 1;
        const auto now = std::chrono::steady_clock::now();
        if (m_consensus_height != height) {
            m_consensus_height = height;
            m_consensus_round = 0;
            m_consensus_phase = 0;
            m_round_started = now;
            proposal = m_authority_node->StartConsensusRound(0);
        } else {
            if (now - m_round_started < round_timeout ||
                m_consensus_round == std::numeric_limits<uint32_t>::max()) return;
            m_round_started = now;
            if (m_consensus_phase == 0) {
                m_consensus_phase = 1;
                prevote = m_authority_node->OnProposalTimeout();
                if (prevote) {
                    precommit = m_authority_node->ReceivePrevote(*prevote);
                    if (precommit) CommitConsensusPrecommit(*precommit);
                }
            } else if (m_consensus_phase == 1) {
                m_consensus_phase = 2;
                precommit = m_authority_node->OnPrevoteTimeout();
                if (precommit) CommitConsensusPrecommit(*precommit);
            } else {
                ++m_consensus_round;
                m_consensus_phase = 0;
                proposal = m_authority_node->StartConsensusRound(m_consensus_round);
            }
        }
    }
    if (prevote) BroadcastConsensusPrevote(*prevote);
    if (precommit) BroadcastConsensusPrecommit(*precommit);
    if (proposal) {
        BroadcastConsensusProposal(*proposal);
        ReceiveConsensusProposal(*proposal);
    }
}

std::optional<BftProposalMsg> CybouNodeRuntime::ProposeConsensusBlock(const uint32_t round)
{
    std::optional<BftProposalMsg> prop;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return std::nullopt;
        const auto loaded = m_store.LoadState();
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) return std::nullopt;
        prop = m_authority_node->StartConsensusRound(round);
    }
    if (prop) BroadcastConsensusProposal(*prop);
    return prop;
}

std::optional<BftPrevoteMsg> CybouNodeRuntime::ReceiveConsensusProposal(const BftProposalMsg& proposal)
{
    std::optional<BftPrevoteMsg> pv;
    std::optional<BftPrecommitMsg> pc;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return std::nullopt;
        pv = m_authority_node->ReceiveProposal(proposal);
        if (pv) {
            if (m_consensus_height != proposal.height || m_consensus_round != proposal.round) {
                m_consensus_height = proposal.height;
                m_consensus_round = proposal.round;
            }
            m_consensus_phase = 1;
            m_round_started = std::chrono::steady_clock::now();
            pc = m_authority_node->ReceivePrevote(*pv);
            if (pc) {
                m_consensus_phase = 2;
                m_round_started = std::chrono::steady_clock::now();
                CommitConsensusPrecommit(*pc);
            }
        }
    }
    if (pv) BroadcastConsensusPrevote(*pv);
    if (pc) BroadcastConsensusPrecommit(*pc);
    return pv;
}

std::optional<BftPrecommitMsg> CybouNodeRuntime::ReceiveConsensusPrevote(const BftPrevoteMsg& prevote)
{
    std::optional<BftPrecommitMsg> pc;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return std::nullopt;
        pc = m_authority_node->ReceivePrevote(prevote);
        if (pc) {
            m_consensus_phase = 2;
            m_round_started = std::chrono::steady_clock::now();
            CommitConsensusPrecommit(*pc);
        }
    }
    if (pc) BroadcastConsensusPrecommit(*pc);
    return pc;
}

bool CybouNodeRuntime::ReceiveConsensusPrecommit(const BftPrecommitMsg& precommit)
{
    std::lock_guard lock(m_mutex);
    if (!m_authority_node) return false;
    return CommitConsensusPrecommit(precommit);
}

bool CybouNodeRuntime::CommitConsensusPrecommit(const BftPrecommitMsg& precommit)
{
    // Caller holds m_mutex. Commit and gossip bookkeeping share this boundary.
    const auto finalized = m_authority_node->ReceivePrecommit(precommit);
    if (!finalized) return false;
    const auto set = m_store.GetValidatorSet();
    if (!set || !m_store.CommitFinalizedBlock(*finalized, *set, true)) return false;
    m_authority_node->RevalidatePending();
    RememberFinalizedBlockForGossip(*finalized);
    return true;
}

void CybouNodeRuntime::BroadcastConsensusProposal(const BftProposalMsg& proposal)
{
    std::lock_guard lock(m_p2p_mutex);
    if (m_replay_height != proposal.height || m_replay_round != proposal.round) {
        m_replay_prevote.reset();
        m_replay_precommit.reset();
    }
    m_replay_height = proposal.height;
    m_replay_round = proposal.round;
    m_replay_proposal = proposal;
    // Keep at most one potentially large block in the outbound queue.
    std::erase_if(m_consensus_outbox, [](const ConsensusMessage& message) {
        return std::holds_alternative<BftProposalMsg>(message);
    });
    if (m_consensus_outbox.size() < 256) m_consensus_outbox.emplace_back(proposal);
}

void CybouNodeRuntime::BroadcastConsensusPrevote(const BftPrevoteMsg& prevote)
{
    std::lock_guard lock(m_p2p_mutex);
    if (m_replay_height != prevote.height || m_replay_round != prevote.round) {
        m_replay_proposal.reset();
        m_replay_precommit.reset();
    }
    m_replay_height = prevote.height;
    m_replay_round = prevote.round;
    m_replay_prevote = prevote;
    if (m_consensus_outbox.size() < 256) m_consensus_outbox.emplace_back(prevote);
}

void CybouNodeRuntime::BroadcastConsensusPrecommit(const BftPrecommitMsg& precommit)
{
    std::lock_guard lock(m_p2p_mutex);
    if (m_replay_height != precommit.height || m_replay_round != precommit.round) {
        m_replay_proposal.reset();
        m_replay_prevote.reset();
    }
    m_replay_height = precommit.height;
    m_replay_round = precommit.round;
    m_replay_precommit = precommit;
    if (m_consensus_outbox.size() < 256) m_consensus_outbox.emplace_back(precommit);
}

void CybouNodeRuntime::ReplayConsensusToPeer(p2p::PeerManager& peers,
    const std::string& address, uint16_t port)
{
    uint64_t height;
    uint32_t round;
    {
        std::lock_guard lock(m_mutex);
        height = m_consensus_height;
        round = m_consensus_round;
    }
    std::optional<BftProposalMsg> proposal;
    std::optional<BftPrevoteMsg> prevote;
    std::optional<BftPrecommitMsg> precommit;
    {
        std::lock_guard lock(m_p2p_mutex);
        if (height != m_replay_height || round != m_replay_round) return;
        proposal = m_replay_proposal;
        prevote = m_replay_prevote;
        precommit = m_replay_precommit;
    }
    peers.SendConsensusTo(address, port, proposal, prevote, precommit);
}

void CybouNodeRuntime::DrainConsensusMessages(p2p::PeerManager& peers)
{
    std::deque<ConsensusMessage> pending;
    {
        std::lock_guard lock(m_p2p_mutex);
        pending.swap(m_consensus_outbox);
    }
    for (const auto& message : pending) {
        std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, BftProposalMsg>) peers.BroadcastProposal(value);
            else if constexpr (std::is_same_v<T, BftPrevoteMsg>) peers.BroadcastPrevote(value);
            else peers.BroadcastPrecommit(value);
        }, message);
    }
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
    const auto result = m_store.CommitFinalizedBlock(block, std::nullopt, sync);
    if (result) {
        RememberFinalizedBlockForGossip(block);
        if (m_authority_node) m_authority_node->RevalidatePending();
    }
    return result;
}

std::optional<FinalizedBlock> CybouNodeRuntime::GetBlockAtHeight(const uint64_t height) const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetBlockAtHeight(height);
}

FinalizedOperationLookupResult CybouNodeRuntime::FindFinalizedOperation(const uint256& op_id) const
{
    FinalizedOperationLookupResult result;
    const auto status = GetStatus();
    if (!status.is_initialized || op_id.IsNull()) return result;
    result.status = FinalizedOperationLookupStatus::NOT_FOUND;
    uint256 previous_id = m_config.network_definition.genesis_block_id;
    for (uint64_t height = 1; height <= status.finalized_height; ++height) {
        const auto finalized = GetBlockAtHeight(height);
        if (!finalized) {
            result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
            return result;
        }
        const auto block_id = ComputeBlockId(finalized->block);
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_id != status.network_id ||
            finalized->certificate.height != height ||
            finalized->certificate.block_id != block_id) {
            result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
            return result;
        }
        for (size_t index = 0; index < finalized->block.operations.size(); ++index) {
            const auto candidate = ComputeOperationId(finalized->block.operations[index]);
            if (!candidate) {
                result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
                return result;
            }
            if (*candidate == op_id) {
                result.status = FinalizedOperationLookupStatus::FOUND;
                result.scanned_height = height;
                result.height = height;
                result.operation_index = static_cast<uint32_t>(index);
                result.block_id = block_id;
                return result;
            }
        }
        previous_id = block_id;
        result.scanned_height = height;
        if (height == status.finalized_height) break;
    }
    return result;
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
            RememberFinalizedBlockForGossip(*fetch_res.block);
            if (m_authority_node) m_authority_node->RevalidatePending();
        }
        ++result.blocks_applied;
        result.status = SyncPeerStatus::BLOCKS_APPLIED;
    }
    return result;
}

SyncPeerResult CybouNodeRuntime::SyncFromConfiguredPeer(const uint64_t max_blocks)
{
    if (!m_config.p2p_endpoint || !m_peer_manager) return {};
    std::lock_guard p2p_lock(m_p2p_mutex);
    const auto& [host, port] = *m_config.p2p_endpoint;
    if (m_peer_manager->ConnectedCount() == 0 && !m_peer_manager->Connect(host, port)) {
        return SyncPeerResult{.status = m_peer_manager->LastConnectStatus() == p2p::PeerConnectStatus::UNAVAILABLE
            ? SyncPeerStatus::CONNECTION_FAILED : SyncPeerStatus::PROTOCOL_ERROR};
    }
    return m_peer_manager->SyncFromPeer(host, port, max_blocks);
}

void CybouNodeRuntime::SetSubmitEndpoint(const std::string& host, const uint16_t port)
{
    std::lock_guard lock(m_mutex);
    m_submit_endpoint = std::make_pair(host, port);
}

bool CybouNodeRuntime::HasSubmitEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_authority_node != nullptr || m_submit_endpoint.has_value() || m_config.p2p_endpoint.has_value();
}

std::optional<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetSubmitEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_submit_endpoint;
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetPeerEndpointsForGossip() const
{
    std::lock_guard lock(m_mutex);
    std::vector<std::pair<std::string, uint16_t>> result;
    if (m_config.p2p_endpoint.has_value()) {
        result.push_back(*m_config.p2p_endpoint);
    }
    for (const auto& ep : m_known_peer_endpoints) {
        if (result.size() >= 32) break;
        if (!m_config.p2p_endpoint || ep != *m_config.p2p_endpoint) {
            result.push_back(ep);
        }
    }
    return result;
}

void CybouNodeRuntime::AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_mutex);
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (m_config.p2p_endpoint && host == m_config.p2p_endpoint->first && port == m_config.p2p_endpoint->second) {
            continue;
        }
        if (m_known_peer_endpoints.size() >= 256) break;
        m_known_peer_endpoints.emplace(addr.to_string(), port);
    }
}

} // namespace cybou
