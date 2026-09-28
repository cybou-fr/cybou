// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/node_runtime.h>
#include <cybou/device_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/p2p/peer_manager.h>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <limits>
#include <type_traits>

namespace cybou {

namespace {

// Discovered routing hints are untrusted. Reject address scopes this node has
// no business dialing: unspecified, multicast, and (unless the local CYP2
// listener lives in the same scope) loopback and link-local targets. Without a
// known local listener the policy stays permissive for DEV tooling.
bool IsPrivateAddress(const boost::asio::ip::address& addr)
{
    const auto is_private_v4 = [](const uint32_t value) {
        return (value & 0xFF000000U) == 0x0A000000U ||
            (value & 0xFFF00000U) == 0xAC100000U ||
            (value & 0xFFFF0000U) == 0xC0A80000U;
    };
    if (addr.is_v4()) {
        return is_private_v4(addr.to_v4().to_uint());
    }
    const auto bytes = addr.to_v6().to_bytes();
    if (addr.to_v6().is_v4_mapped()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[12]) << 24) |
            (static_cast<uint32_t>(bytes[13]) << 16) |
            (static_cast<uint32_t>(bytes[14]) << 8) | static_cast<uint32_t>(bytes[15]);
        return is_private_v4(value);
    }
    return (bytes[0] & 0xFEU) == 0xFCU; // Unique-local IPv6 (fc00::/7).
}

bool IsConnectableDiscoveredAddress(
    const boost::asio::ip::address& addr,
    const std::optional<std::pair<std::string, uint16_t>>& local_p2p_endpoint)
{
    auto is_link_local = [](const boost::asio::ip::address& a) {
        if (a.is_v4()) return (a.to_v4().to_uint() & 0xFFFF0000U) == 0xA9FE0000U;
        const auto bytes = a.to_v6().to_bytes();
        return bytes[0] == 0xFEU && (bytes[1] & 0xC0U) == 0x80U;
    };
    if (addr.is_unspecified() || addr.is_multicast()) return false;
    if (IsPrivateAddress(addr) && local_p2p_endpoint) {
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(local_p2p_endpoint->first, ec);
        if (!ec && !IsPrivateAddress(local) && !local.is_loopback() && !is_link_local(local) &&
            !local.is_unspecified() && !local.is_multicast()) {
            return false;
        }
    }
    if (addr.is_loopback()) {
        if (!local_p2p_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(local_p2p_endpoint->first, ec);
        return !ec && local.is_loopback();
    }
    if (is_link_local(addr)) {
        if (!local_p2p_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(local_p2p_endpoint->first, ec);
        return !ec && is_link_local(local);
    }
    return true;
}

} // namespace

CybouNodeRuntime::CybouNodeRuntime(NodeRuntimeConfig config)
    : m_config{std::move(config)},
      m_network_id{NetworkId(m_config.network_definition)},
      m_db{std::make_unique<KVStore>(KVStoreOptions{
          .path = m_config.data_dir,
          .cache_bytes = m_config.db_cache_bytes,
          .memory_only = m_config.memory_only,
          .wipe_data = m_config.wipe_data,
      })},
      m_store{*m_db, m_config.network_definition},
      m_submit_endpoint{m_config.submit_endpoint}
{
    if (m_config.storage_enabled) {
        if (m_config.storage_capacity_bytes == 0) {
            throw std::invalid_argument("storage provider requires a positive capacity");
        }
        std::filesystem::path storage_path;
        if (!m_config.memory_only) {
            storage_path = std::filesystem::path{m_config.data_dir.string() + ".objects"};
        }
        m_storage_store = std::make_unique<StorageObjectStore>(storage_path,
            std::span<const unsigned char, 32>{m_network_id.begin(), 32},
            m_config.storage_capacity_bytes, m_config.memory_only, m_config.wipe_data);
    }
    if (m_config.validator_private_key.has_value()) {
        m_authority_node = std::make_unique<CybouAuthorityNode>(
            m_store, *m_config.validator_private_key,
            m_config.memory_only ? std::nullopt :
                std::optional<std::filesystem::path>{m_config.data_dir / "validator-signing.journal"});
    }
    if (m_config.p2p_endpoint) m_peer_manager = std::make_unique<p2p::PeerManager>(*this);
}

CybouNodeRuntime::~CybouNodeRuntime() = default;

StorageWriteResult CybouNodeRuntime::StoreEncryptedChunk(
    const StorageObjectId& object_id, const StorageEncryptedChunk& chunk)
{
    if (!m_storage_store) return {StorageWriteStatus::DISABLED};
    return m_storage_store->PutChunk(object_id, chunk);
}

StorageWriteResult CybouNodeRuntime::CommitStoredManifest(const StoragePublicManifest& manifest)
{
    if (!m_storage_store) return {StorageWriteStatus::DISABLED};
    return m_storage_store->CommitManifest(manifest);
}

bool CybouNodeRuntime::AbortStoredObject(
    const StorageObjectId& object_id, const uint32_t chunk_count)
{
    return m_storage_store && m_storage_store->AbortUncommittedObject(object_id, chunk_count);
}

uint64_t CybouNodeRuntime::GarbageCollectStorageStaging()
{
    return m_storage_store ? m_storage_store->GarbageCollectExpiredStaging() : 0;
}

std::optional<StoragePublicManifest> CybouNodeRuntime::GetStoredManifest(const StorageObjectId& object_id) const
{
    if (!m_storage_store) return std::nullopt;
    return m_storage_store->GetManifest(object_id);
}

std::optional<StorageEncryptedChunk> CybouNodeRuntime::GetStoredChunk(
    const StorageObjectId& object_id, const uint32_t index) const
{
    if (!m_storage_store) return std::nullopt;
    return m_storage_store->GetChunk(object_id, index);
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

OperationStatus CybouNodeRuntime::GetOperationStatus(const uint256& op_id) const
{
    if (op_id.IsNull()) return {};
    std::lock_guard lock(m_mutex);
    if (m_authority_node && m_authority_node->HasPendingOperation(op_id)) {
        return {.kind = OperationStatusKind::LOCAL_PENDING};
    }
    if (const auto height = m_store.GetFinalizedOperationHeight(op_id)) {
        return {.kind = OperationStatusKind::FINALIZED, .finalized_height = *height};
    }
    const auto known = m_recent_operation_status.find(op_id);
    if (known != m_recent_operation_status.end()) return known->second;
    return {};
}

DeviceOperationCoordinator& CybouNodeRuntime::GetDeviceOperationCoordinator(CybouKeyStore& keystore)
{
    std::lock_guard lock(m_mutex);
    if (!m_device_operation_coordinator) {
        m_device_operation_coordinator = std::make_unique<DeviceOperationCoordinator>(
            *this, keystore, m_config.memory_only ? std::filesystem::path{} :
                m_config.data_dir / "device-operation.cydop");
    }
    return *m_device_operation_coordinator;
}

void CybouNodeRuntime::RememberOperationStatus(const uint256& id, OperationStatus status)
{
    if (id.IsNull()) return;
    if (!m_recent_operation_status.contains(id)) m_recent_operation_status_order.push_back(id);
    m_recent_operation_status[id] = status;
    while (m_recent_operation_status_order.size() > 256) {
        m_recent_operation_status.erase(m_recent_operation_status_order.front());
        m_recent_operation_status_order.pop_front();
    }
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
            if (status == OperationSubmitStatus::ACCEPTED || status == OperationSubmitStatus::ALREADY_PENDING) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::LOCAL_PENDING});
            } else if (status == OperationSubmitStatus::ALREADY_FINALIZED) {
                const auto height = m_store.GetFinalizedOperationHeight(op_id).value_or(0);
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED, .finalized_height = height});
            } else {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            }
            return OperationSubmitResult{.status = status, .op_id = op_id};
        }
        endpoint = m_submit_endpoint;
        p2p_endpoint = m_config.p2p_endpoint;
        net_id = m_network_id;
    }
    if (p2p_endpoint && m_peer_manager) {
        std::lock_guard p2p_lock(m_p2p_mutex);
        auto connected = m_peer_manager->Peers();
        if (connected.empty() && !m_peer_manager->Connect(p2p_endpoint->first, p2p_endpoint->second)) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        connected = m_peer_manager->Peers();
        std::vector<std::pair<std::string, uint16_t>> accepting_candidates;
        accepting_candidates.reserve(connected.size());
        for (const auto& peer : connected) accepting_candidates.emplace_back(peer.address, peer.port);
        const auto submitted = m_peer_manager->SubmitOperationToAny(accepting_candidates, op);
        auto result = submitted.acknowledgment.value_or(
            OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id});
        result.delivery_uncertain = submitted.delivery_uncertain;
        {
            std::lock_guard lock(m_mutex);
            if (result.status == OperationSubmitStatus::ACCEPTED ||
                result.status == OperationSubmitStatus::ALREADY_PENDING) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::ACCEPTED_REMOTE});
            } else if (result.status == OperationSubmitStatus::ALREADY_FINALIZED) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED,
                    .finalized_height = m_store.GetFinalizedOperationHeight(op_id).value_or(0)});
            } else if (!result.delivery_uncertain) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            }
        }
        return result;
    }
    if (endpoint.has_value()) {
        auto result = SubmitOperationRemote(endpoint->first, endpoint->second, net_id, op);
        std::lock_guard lock(m_mutex);
        if (result.status == OperationSubmitStatus::ACCEPTED ||
            result.status == OperationSubmitStatus::ALREADY_PENDING) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::ACCEPTED_REMOTE});
        } else if (result.status == OperationSubmitStatus::ALREADY_FINALIZED) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED,
                .finalized_height = m_store.GetFinalizedOperationHeight(op_id).value_or(0)});
        } else if (!result.delivery_uncertain) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
        }
        return result;
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
    BftProposalResult buffered_result;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return;
        const auto head = m_store.GetFinalizedHead();
        const auto set = m_store.GetValidatorSet();
        if (!head || !set || set->validators.size() <= 1 ||
            head->height == std::numeric_limits<uint64_t>::max()) return;
        const auto height = head->height + 1;
        const auto now = std::chrono::steady_clock::now();
        const auto round_backoff = std::chrono::milliseconds{
            static_cast<int64_t>(std::min(m_consensus_round, 60U)) * 75};
        const auto effective_round_timeout = std::min(
            round_timeout + round_backoff, std::chrono::milliseconds{5000});
        if (m_consensus_height != height) {
            m_consensus_height = height;
            m_consensus_round = 0;
            m_consensus_phase = 0;
            // Resume where the engine actually is: after a CBS2 restart the
            // validator recovers its durable round/step/lock from the signing
            // journal, and the driver must not restart orchestration at
            // round 0 and slowly time its way back up.
            if (const auto progress = m_authority_node->GetConsensusProgress();
                progress && progress->height == height) {
                m_consensus_round = progress->round;
                m_consensus_phase = progress->step == BftStep::PROPOSE ? 0 :
                    (progress->step == BftStep::PREVOTE ? 1 : 2);
            }
            m_round_started = now;
            proposal = m_authority_node->StartConsensusRound(m_consensus_round);
            if (!proposal) buffered_result = ProcessBufferedConsensusProposalLocked();
        } else {
            if (now - m_round_started < effective_round_timeout ||
                m_consensus_round == std::numeric_limits<uint32_t>::max()) return;
            if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
                std::fprintf(stderr, "[cybou-debug] ROUND_TIMEOUT height=%llu round=%u phase=%u timeout_ms=%lld\n",
                    static_cast<unsigned long long>(height), m_consensus_round, m_consensus_phase,
                    static_cast<long long>(effective_round_timeout.count()));
            }
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
                if (!proposal) buffered_result = ProcessBufferedConsensusProposalLocked();
            }
        }
    }
    if (prevote) BroadcastConsensusPrevote(*prevote);
    if (precommit) BroadcastConsensusPrecommit(*precommit);
    if (buffered_result.prevote) BroadcastConsensusPrevote(*buffered_result.prevote);
    if (buffered_result.precommit) BroadcastConsensusPrecommit(*buffered_result.precommit);
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
    std::optional<BftProposalMsg> next_proposal;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return std::nullopt;
        const auto result = m_authority_node->ReceiveProposal(proposal);
        pv = result.prevote;
        pc = result.precommit;
        if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
            std::fprintf(stderr, "[cybou-debug] PROPOSAL height=%llu round=%u id=%s prevote=%s precommit=%s\n",
                static_cast<unsigned long long>(proposal.height), proposal.round,
                ComputeBlockId(proposal.block).GetHex().c_str(),
                pv ? (pv->block_id ? pv->block_id->GetHex().c_str() : "nil") : "none",
                pc ? (pc->block_id ? pc->block_id->GetHex().c_str() : "nil") : "none");
        }
        if (pv && !pc && !result.finalized) {
            pc = m_authority_node->ReceivePrevote(*pv);
        }
        if (pv) {
            if (m_consensus_height != proposal.height) {
                m_consensus_height = proposal.height;
            }
            m_consensus_phase = pc ? 2 : 1;
            m_round_started = std::chrono::steady_clock::now();
            if (pc) {
                CommitConsensusPrecommit(*pc);
            } else if (result.finalized) {
                const auto& finalized = m_authority_node->GetLatestFinalizedBlock();
                if (finalized) CommitConsensusFinalized(*finalized);
            }
        }
        // The engine may have jumped to a higher round while processing.
        next_proposal = SyncConsensusDriverWithEngine();
    }
    if (pv) BroadcastConsensusPrevote(*pv);
    if (pc) BroadcastConsensusPrecommit(*pc);
    if (next_proposal) {
        BroadcastConsensusProposal(*next_proposal);
        ReceiveConsensusProposal(*next_proposal);
    }
    return pv;
}

std::optional<BftPrecommitMsg> CybouNodeRuntime::ReceiveConsensusPrevote(const BftPrevoteMsg& prevote)
{
    std::optional<BftPrecommitMsg> pc;
    std::optional<BftPrevoteMsg> buffered_prevote;
    std::optional<BftProposalMsg> next_proposal;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return std::nullopt;
        pc = m_authority_node->ReceivePrevote(prevote);
        if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
            const auto progress = m_authority_node->GetConsensusProgress();
            std::fprintf(stderr, "[cybou-debug] PREVOTE height=%llu round=%u voter=%s block=%s local_round=%u locked=%d precommit=%s\n",
                static_cast<unsigned long long>(prevote.height), prevote.round,
                prevote.validator_id.GetHex().c_str(),
                prevote.block_id ? prevote.block_id->GetHex().c_str() : "nil",
                progress ? progress->round : 0, progress ? progress->locked_round : -1,
                pc ? (pc->block_id ? pc->block_id->GetHex().c_str() : "nil") : "none");
        }
        if (pc) {
            m_consensus_phase = 2;
            m_round_started = std::chrono::steady_clock::now();
            CommitConsensusPrecommit(*pc);
        }
        const auto buffered = ProcessBufferedConsensusProposalLocked();
        buffered_prevote = buffered.prevote;
        if (buffered.precommit) pc = buffered.precommit;
        next_proposal = SyncConsensusDriverWithEngine();
    }
    if (buffered_prevote) BroadcastConsensusPrevote(*buffered_prevote);
    if (pc) BroadcastConsensusPrecommit(*pc);
    if (next_proposal) {
        BroadcastConsensusProposal(*next_proposal);
        ReceiveConsensusProposal(*next_proposal);
    }
    return pc;
}

bool CybouNodeRuntime::ReceiveConsensusPrecommit(const BftPrecommitMsg& precommit)
{
    bool committed{false};
    BftProposalResult buffered;
    std::optional<BftProposalMsg> next_proposal;
    {
        std::lock_guard lock(m_mutex);
        if (!m_authority_node) return false;
        committed = CommitConsensusPrecommit(precommit);
        if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
            const auto progress = m_authority_node->GetConsensusProgress();
            std::fprintf(stderr, "[cybou-debug] PRECOMMIT height=%llu round=%u voter=%s block=%s committed=%d local_round=%u locked=%d\n",
                static_cast<unsigned long long>(precommit.height), precommit.round,
                precommit.validator_id.GetHex().c_str(),
                precommit.block_id ? precommit.block_id->GetHex().c_str() : "nil", committed,
                progress ? progress->round : 0, progress ? progress->locked_round : -1);
        }
        buffered = ProcessBufferedConsensusProposalLocked();
        committed = committed || buffered.finalized;
        next_proposal = SyncConsensusDriverWithEngine();
    }
    if (buffered.prevote) BroadcastConsensusPrevote(*buffered.prevote);
    if (buffered.precommit) BroadcastConsensusPrecommit(*buffered.precommit);
    if (next_proposal) {
        BroadcastConsensusProposal(*next_proposal);
        ReceiveConsensusProposal(*next_proposal);
    }
    return committed;
}

BftProposalResult CybouNodeRuntime::ProcessBufferedConsensusProposalLocked()
{
    if (!m_authority_node) return {};
    const auto proposal = m_authority_node->TakeBufferedProposalForCurrentRound();
    if (!proposal) return {};
    auto result = m_authority_node->ReceiveProposal(*proposal);
    if (result.prevote && !result.precommit && !result.finalized) {
        result.precommit = m_authority_node->ReceivePrevote(*result.prevote);
    }
    if (result.prevote) {
        m_consensus_height = proposal->height;
        m_consensus_phase = result.precommit ? 2 : 1;
        m_round_started = std::chrono::steady_clock::now();
    }
    if (result.precommit) {
        CommitConsensusPrecommit(*result.precommit);
    } else if (result.finalized) {
        const auto& finalized = m_authority_node->GetLatestFinalizedBlock();
        if (finalized) CommitConsensusFinalized(*finalized);
    }
    return result;
}

std::optional<BftProposalMsg> CybouNodeRuntime::SyncConsensusDriverWithEngine()
{
    // Caller holds m_mutex. The engine may legitimately run ahead of the
    // orchestration driver: it jumps rounds on verified higher-round votes.
    // Follow it so the timeout machine drives the round the engine is in.
    const auto progress = m_authority_node->GetConsensusProgress();
    if (!progress || progress->height != m_consensus_height) return std::nullopt;
    const auto phase = progress->step == BftStep::PROPOSE ? 0 :
        (progress->step == BftStep::PREVOTE ? 1 : 2);
    if (progress->round == m_consensus_round && phase == m_consensus_phase) return std::nullopt;
    if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
        std::fprintf(stderr, "[cybou-debug] ENGINE_PROGRESS height=%llu round=%u phase=%u -> round=%u phase=%u locked=%d\n",
            static_cast<unsigned long long>(progress->height), m_consensus_round, m_consensus_phase,
            progress->round, phase, progress->locked_round);
    }
    m_consensus_round = progress->round;
    m_consensus_phase = phase;
    m_round_started = std::chrono::steady_clock::now();
    // A quorum of future-round votes can move the engine directly into a new
    // PROPOSE step. If this validator is that round's leader, start the
    // proposal now; waiting for the proposal timeout would make it prevote nil.
    if (phase == 0) return m_authority_node->StartConsensusRound(progress->round);
    return std::nullopt;
}

bool CybouNodeRuntime::CommitConsensusPrecommit(const BftPrecommitMsg& precommit)
{
    // Caller holds m_mutex. Commit and gossip bookkeeping share this boundary.
    const auto finalized = m_authority_node->ReceivePrecommit(precommit);
    if (!finalized) return false;
    return CommitConsensusFinalized(*finalized);
}

bool CybouNodeRuntime::CommitConsensusFinalized(const FinalizedBlock& finalized)
{
    // Caller holds m_mutex. Commit and gossip bookkeeping share this boundary.
    const auto set = m_store.GetValidatorSet();
    if (!set || !m_store.CommitFinalizedBlock(finalized, *set, true)) {
        if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
            std::fprintf(stderr, "[cybou-debug] FINALIZED BUT COMMIT FAILED height=%llu store_height=%llu\n",
                static_cast<unsigned long long>(finalized.block.height),
                static_cast<unsigned long long>(m_store.GetFinalizedHead().value_or(FinalizedHead{}).height));
        }
        return false;
    }
    m_authority_node->RevalidatePending();
    RememberFinalizedBlockForGossip(finalized);
    if (std::getenv("CYBOU_CONSENSUS_DEBUG")) {
        std::fprintf(stderr, "[cybou-debug] FINALIZED height=%llu\n",
            static_cast<unsigned long long>(finalized.block.height));
    }
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

void CybouNodeRuntime::SchedulePeerRetry(
    const std::pair<std::string, uint16_t>& endpoint, const PeerFailureClass failure)
{
    auto& retry = m_peer_retry_after[endpoint];
    const auto now = std::chrono::steady_clock::now();
    const auto backoff = [](uint32_t failures, const uint32_t base_seconds, const uint32_t max_seconds) {
        uint32_t delay = base_seconds;
        while (failures > 1 && delay < max_seconds) {
            delay = std::min(max_seconds, delay * 2);
            --failures;
        }
        return std::chrono::seconds{delay};
    };

    switch (failure) {
    case PeerFailureClass::TEMPORARY:
        retry.temporary_failures = std::min<uint32_t>(retry.temporary_failures + 1, 16);
        retry.retry_after = now + backoff(retry.temporary_failures, 5, 300);
        break;
    case PeerFailureClass::PROTOCOL:
        retry.protocol_failures = std::min<uint32_t>(retry.protocol_failures + 1, 16);
        retry.retry_after = now + backoff(retry.protocol_failures, 1800, 86400);
        break;
    case PeerFailureClass::WRONG_NETWORK:
        retry.retry_after = now + std::chrono::hours{24};
        break;
    }
}

SyncPeerResult CybouNodeRuntime::SyncFromConfiguredPeer(const uint64_t max_blocks)
{
    if (!m_config.p2p_endpoint || !m_peer_manager) return {};
    std::lock_guard p2p_lock(m_p2p_mutex);

    const auto local_status = GetStatus();
    if (local_status.runtime_state == NodeRuntimeState::NETWORK_MISMATCH) {
        return SyncPeerResult{.status = SyncPeerStatus::NETWORK_MISMATCH};
    }
    if (local_status.runtime_state == NodeRuntimeState::CORRUPT || !local_status.is_initialized) {
        return SyncPeerResult{.status = SyncPeerStatus::PROTOCOL_ERROR};
    }

    auto explicit_endpoints = GetExplicitPeerEndpoints();
    if (std::find(explicit_endpoints.begin(), explicit_endpoints.end(), *m_config.p2p_endpoint) ==
        explicit_endpoints.end()) {
        explicit_endpoints.push_back(*m_config.p2p_endpoint);
    }
    m_peer_manager->SetExplicitEndpoints(explicit_endpoints);
    const auto maintenance_now = std::chrono::steady_clock::now();
    const bool have_connected_peers = m_peer_manager->ConnectedCount() != 0;
    if (have_connected_peers && maintenance_now >= m_next_peer_discovery) {
        m_next_peer_discovery = maintenance_now + std::chrono::seconds{60};
        m_peer_manager->DiscoverPeers(1);
    } else if (have_connected_peers && maintenance_now >= m_next_peer_ping) {
        m_next_peer_ping = maintenance_now + std::chrono::seconds{15};
        m_peer_manager->PingSome(1);
    }

    const auto targets = GetPeerEndpointsForGossip();
    const auto connected_before_dial = m_peer_manager->Peers();
    if (connected_before_dial.size() < p2p::MAX_OUTBOUND_PEERS) {
        const auto now = std::chrono::steady_clock::now();
        const auto candidate = std::find_if(targets.begin(), targets.end(), [&](const auto& endpoint) {
            const bool connected = std::any_of(connected_before_dial.begin(), connected_before_dial.end(),
                [&](const p2p::PeerInfo& peer) { return peer.address == endpoint.first && peer.port == endpoint.second; });
            const auto retry = m_peer_retry_after.find(endpoint);
            return !connected && (retry == m_peer_retry_after.end() || now >= retry->second.retry_after);
        });
        if (candidate != targets.end()) {
            if (m_peer_manager->Connect(candidate->first, candidate->second)) {
                m_peer_retry_after.erase(*candidate);
            } else {
                switch (m_peer_manager->LastConnectStatus()) {
                case p2p::PeerConnectStatus::UNAVAILABLE:
                    SchedulePeerRetry(*candidate, PeerFailureClass::TEMPORARY);
                    break;
                case p2p::PeerConnectStatus::WRONG_NETWORK:
                    SchedulePeerRetry(*candidate, PeerFailureClass::WRONG_NETWORK);
                    break;
                case p2p::PeerConnectStatus::HANDSHAKE_FAILED:
                    SchedulePeerRetry(*candidate, PeerFailureClass::PROTOCOL);
                    break;
                default:
                    break;
                }
            }
        }
    }

    auto peers = m_peer_manager->Peers();
    if (peers.empty()) {
        return SyncPeerResult{.status = SyncPeerStatus::CONNECTION_FAILED};
    }
    std::sort(peers.begin(), peers.end(), [](const p2p::PeerInfo& left, const p2p::PeerInfo& right) {
        if (left.hello.finalized_height != right.hello.finalized_height) {
            return left.hello.finalized_height > right.hello.finalized_height;
        }
        if (left.address != right.address) return left.address < right.address;
        return left.port < right.port;
    });

    SyncPeerResult result{.status = SyncPeerStatus::CONNECTION_FAILED};
    bool any_peer_up_to_date{false};
    for (const auto& peer : peers) {
        const auto attempt = m_peer_manager->SyncFromPeer(peer.address, peer.port, max_blocks);
        if (attempt.status == SyncPeerStatus::BLOCKS_APPLIED) {
            result = attempt;
            break;
        }
        // HELLO height is only a snapshot from connection time. A peer that
        // reports UP_TO_DATE may have stopped advancing while another
        // connected peer has newer blocks, so keep checking the whole set.
        // Peer protocol failures are isolated to that session by PeerManager;
        // they must not become a fatal network-service result.
        if (attempt.status == SyncPeerStatus::UP_TO_DATE) {
            any_peer_up_to_date = true;
            m_peer_retry_after.erase({peer.address, peer.port});
        } else if (attempt.status == SyncPeerStatus::CONNECTION_FAILED) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::TEMPORARY);
        } else if (attempt.status == SyncPeerStatus::NETWORK_MISMATCH) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::WRONG_NETWORK);
        } else if (attempt.status == SyncPeerStatus::PROTOCOL_ERROR) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::PROTOCOL);
        }
    }
    if (any_peer_up_to_date) result.status = SyncPeerStatus::UP_TO_DATE;
    m_peer_manager->FanoutRecentBlocks();
    return result;
}

size_t CybouNodeRuntime::ConnectedPeerCount() const
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    return m_peer_manager ? m_peer_manager->ConnectedCount() : 0;
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
    constexpr size_t MAX_GOSSIP_TARGETS{32};
    std::vector<std::pair<std::string, uint16_t>> result;
    const auto& configured = m_config.p2p_endpoint;
    if (configured.has_value()) {
        result.push_back(*configured);
    }
    // Explicit operator-approved validator peers come first: a flood of
    // malicious discovered hints must never eclipse the validator topology.
    for (const auto& ep : m_explicit_peer_endpoints) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if (!configured || ep != *configured) {
            result.push_back(ep);
        }
    }
    for (const auto& ep : m_discovered_peer_endpoints) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if ((!configured || ep != *configured) &&
            std::find(result.begin(), result.end(), ep) == result.end()) {
            result.push_back(ep);
        }
    }
    return result;
}

void CybouNodeRuntime::SetExplicitPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_mutex);
    m_explicit_peer_endpoints.clear();
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (m_config.p2p_endpoint && addr.to_string() == m_config.p2p_endpoint->first &&
            port == m_config.p2p_endpoint->second) {
            continue;
        }
        if (m_config.local_p2p_endpoint && addr.to_string() == m_config.local_p2p_endpoint->first &&
            port == m_config.local_p2p_endpoint->second) {
            continue;
        }
        m_explicit_peer_endpoints.emplace(addr.to_string(), port);
    }
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetExplicitPeerEndpoints() const
{
    std::lock_guard lock(m_mutex);
    return {m_explicit_peer_endpoints.begin(), m_explicit_peer_endpoints.end()};
}

void CybouNodeRuntime::AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_mutex);
    constexpr size_t MAX_DISCOVERED_PEER_ENDPOINTS{256};
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (!IsConnectableDiscoveredAddress(addr, m_config.local_p2p_endpoint)) continue;
        const auto canonical = std::make_pair(addr.to_string(), port);
        if (m_config.p2p_endpoint && canonical == *m_config.p2p_endpoint) continue;
        if (m_config.local_p2p_endpoint && canonical == *m_config.local_p2p_endpoint) continue;
        if (m_explicit_peer_endpoints.count(canonical) > 0) continue;
        if (m_discovered_peer_endpoints.size() >= MAX_DISCOVERED_PEER_ENDPOINTS) break;
        m_discovered_peer_endpoints.emplace(canonical);
    }
}

} // namespace cybou
