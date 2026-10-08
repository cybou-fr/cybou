// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Full Node facade construction and aggregate diagnostics.

#include <cybou/node_runtime.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/p2p/storage_session_pool.h>
#include <cybou/storage_io_scheduler.h>
#include <cybou/process_memory.h>
#include <cybou/observation_cache.h>
#include <cybou/p2p/observation_exchange.h>
#include <cybou/p2p/observation_groups.h>
#include <cybou/network_observation_history.h>
#include <stdexcept>
#include <algorithm>

namespace cybou {

NodeRuntimeConfig MakeNodeRuntimeConfig(const OfficialNetwork& network, const std::filesystem::path& data_dir)
{
    NodeRuntimeConfig config{.network_genesis = network.genesis, .data_dir = data_dir};
    config.configured_peers.reserve(network.rendezvous_locators.size());
    for (const auto& locator : network.rendezvous_locators)
        config.configured_peers.push_back({{std::string{locator.host}, locator.port}, locator.tls_spki_sha256});
    return config;
}


CybouNodeRuntime::CybouNodeRuntime(NodeRuntimeConfig config)
    : m_config{std::move(config)},
      m_network_binding{ComputeNetworkBinding(m_config.network_genesis.GetNetworkPublicKey())},
      m_chain{m_config},
      m_network{m_config}
{
    // `V` — явная local policy оператора, не consensus state; минимум не ослабляется для production.
    if (!m_config.storage_capacity_bytes) m_config.storage_capacity_bytes = DEFAULT_STORAGE_CAPACITY_BYTES;
    if (*m_config.storage_capacity_bytes < MIN_STORAGE_CAPACITY_BYTES && !m_config.memory_only)
        throw std::invalid_argument("storage capacity must be at least 15 GiB; smaller values are memory-only tests");
    m_provider.Initialize(m_config, m_network_binding);
    // Recover a crash between durable block commit and its local purge event.
    const auto restored = m_chain.store.LoadState();
    if (restored.error == StateLoadError::NONE && restored.state) {
        m_provider.finalized_chunk_store->PurgeRevokedPublications(
            [&](const cybou::Hash256& id) { return FindFinalizedRootPublication(id); });
    }
    if (m_config.poa_finalizer_recovery_entropy.has_value()) {
        m_chain.poa_finalizer = std::make_unique<PoaFinalizer>(
            m_chain.store.GetDatabase(), m_chain.store.GetNetworkBinding(), m_chain.store.GetNetworkGenesis().GetGenesisAnchor(),
            m_config.poa_finalizer_recovery_entropy->Get(), m_chain.store.GetNetworkGenesis().GetPoaPublicKey());
        m_config.poa_finalizer_recovery_entropy.reset();
    }
    // Keep the local full node usable before it has learned or connected to a
    // peer. Configured endpoints and discovered hints can be added later.
    m_network.peer_manager = std::make_unique<p2p::PeerManager>(*this);
    m_network.storage_sessions = std::make_unique<p2p::StorageSessionPool>(*this);
    m_network.storage_io = std::make_unique<StorageIoScheduler>();
    ObservationBytes32 binding{};
    std::copy(m_network_binding.begin(), m_network_binding.end(), binding.begin());
    m_observation_exchange = std::make_shared<p2p::ObservationExchange>(binding);
    m_observation_groups = std::make_shared<p2p::ObservationGroups>(binding);
    m_network_observation_history = std::make_unique<NetworkObservationHistory>();
    m_observation_collector = std::make_unique<ObservationCollector>(binding, [this] { return CollectObservationReport(); });
}

CybouNodeRuntime::~CybouNodeRuntime() = default;

NodeDiagnosticsSnapshot CybouNodeRuntime::GetDiagnostics() const
{
    const auto status = GetStatus();
    NodeDiagnosticsSnapshot snapshot;
    snapshot.network_observation = GetNetworkObservation();
    snapshot.process_resident_bytes = ReadProcessResidentBytes();
    snapshot.process_cpu = m_cpu_observations.Sample();
    snapshot.traffic = m_traffic->Snapshot();
    snapshot.storage_transfers = m_traffic->StorageSnapshot();
    snapshot.observed_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    snapshot.uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - m_observation_started).count();
    snapshot.network_binding = status.network_binding.GetHex();
    snapshot.node_type = "Full Node";
    snapshot.poa_signer_active = status.poa_signer_active;
    snapshot.height = status.finalized_height;
    snapshot.tip = status.finalized_tip.GetHex();
    snapshot.state_root = status.state_root.GetHex();
    snapshot.initialized = status.is_initialized;
    snapshot.safety_halted = status.poa_safety_halted;
    // Never hold state and peer locks together (peer I/O can call state methods).
    {
        std::lock_guard lock{m_chain.mutex};
        snapshot.pending_operations = m_chain.operation_pool.Size();
        snapshot.finalization = m_chain.finalization_observations.Snapshot();
        snapshot.pending_operation_bytes = m_chain.operation_pool.Bytes();
        snapshot.operations.reserve(m_chain.recent_operation_status_order.size());
        for (const auto& id : m_chain.recent_operation_status_order) {
            const auto& op = m_chain.recent_operation_status.at(id);
            snapshot.operations.push_back({id.GetHex(), static_cast<std::uint32_t>(op.kind), op.finalized_height});
        }
    }
    {
        std::lock_guard lock{m_network.mutex};
        if (m_network.peer_manager) {
            const auto peers = m_network.peer_manager->Peers();
            snapshot.peers.reserve(peers.size());
            for (const auto& peer : peers) {
                std::string provider;
                if (peer.storage_id) {
                    static constexpr char HEX[] = "0123456789abcdef";
                    provider.reserve(peer.storage_id->size() * 2);
                    for (auto b : *peer.storage_id) {
                        provider += HEX[b >> 4];
                        provider += HEX[b & 15];
                    }
                }
                snapshot.peers.push_back({peer.address + ":" + std::to_string(peer.port),
                    peer.hello.finalized_height, provider});
            }
        }
    }
    if (m_provider.finalized_chunk_store) {
        snapshot.storage_used = m_provider.finalized_chunk_store->UsedBytes();
        snapshot.storage_capacity = m_provider.finalized_chunk_store->CapacityBytes();
    }
    if (m_provider.chunk_blob_store) {
        snapshot.local_storage_used = m_provider.chunk_blob_store->UsedBytes();
        snapshot.storage_disk_available = m_provider.chunk_blob_store->AvailableDiskBytes();
        snapshot.local_storage_capacity = m_config.storage_capacity_bytes.value_or(0);
    }
    return snapshot;
}

OperationSubmitResult CybouNodeRuntime::SubmitOperationInternal(
    ProtocolOperation op, const uint64_t work_nonce, std::optional<std::string> source_peer)
{
    const cybou::Hash256 op_id = ComputeOperationId(op).value_or(cybou::Hash256{});
    std::vector<std::pair<std::string, uint16_t>> configured_endpoints;
    {
        std::lock_guard lock(m_chain.mutex);
        const auto loaded = m_chain.store.GetStateSnapshot();
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            return OperationSubmitResult{.status = OperationSubmitStatus::NETWORK_MISMATCH, .op_id = op_id};
        }
        if (loaded.error != StateLoadError::NONE || !bool(loaded.state)) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_chain.store.PoaSafetyHalted() || (m_chain.poa_finalizer && m_chain.poa_finalizer->SafetyHalted())) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        // Every full node executes the candidate itself before anything else.
        const auto admission = m_chain.operation_pool.Admit(op, work_nonce, std::move(source_peer));
        if (admission == PoolAdmission::ACCEPTED) NotifyBlockProductionLocked();
        if (admission == PoolAdmission::ALREADY_FINALIZED) {
            const auto height = m_chain.store.GetFinalizedOperationHeight(op_id).value_or(0);
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED, .finalized_height = height});
            return OperationSubmitResult{.status = OperationSubmitStatus::ALREADY_FINALIZED, .op_id = op_id};
        }
        if (admission == PoolAdmission::REJECTED) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_chain.poa_finalizer && m_chain.poa_finalizer->SignerEnabled()) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::LOCAL_PENDING});
            return OperationSubmitResult{.status = admission == PoolAdmission::ACCEPTED ?
                OperationSubmitStatus::ACCEPTED : OperationSubmitStatus::ALREADY_PENDING, .op_id = op_id};
        }
        // The author's own candidate also enters its relay queue, like any relayed one: it is
        // then pushed to every connected peer and served to pollers, so a single dead-end peer
        // (a lagging node, another client) acknowledging it first cannot strand it.
        if (const auto own_bytes = SerializeProtocolOperation(op)) {
            (void)m_chain.operation_relay.Enqueue(*own_bytes, work_nonce, true);
        }
    }
    configured_endpoints = GetConfiguredPeerEndpoints();
    if (m_network.peer_manager) {
        std::lock_guard p2p_lock(m_network.mutex);
        // Ordinary mesh relay: any connected relay peer executes the operation
        // itself and forwards it. There is no preferred finalizer route.
        for (const auto& endpoint : configured_endpoints)
            m_network.peer_manager->Connect(endpoint.first, endpoint.second);
        std::vector<std::pair<std::string, uint16_t>> accepting_candidates;
        const auto peers = m_network.peer_manager->Peers();
        accepting_candidates.reserve(peers.size());
        for (const auto& peer : peers) {
            {
                const auto endpoint = std::make_pair(peer.address, peer.port);
                if (std::find(accepting_candidates.begin(), accepting_candidates.end(), endpoint) ==
                    accepting_candidates.end()) accepting_candidates.push_back(endpoint);
            }
        }
        const auto submitted = m_network.peer_manager->SubmitOperationToAny(accepting_candidates, op, work_nonce);
        // No acknowledgment from anyone is not a rejection: nothing proved the
        // operation invalid, so the exact bytes are retained for retry.
        auto result = submitted.acknowledgment.value_or(
            OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id});
        result.delivery_uncertain = submitted.delivery_uncertain || !submitted.acknowledgment ||
            (result.status != OperationSubmitStatus::ACCEPTED && result.status != OperationSubmitStatus::ALREADY_PENDING &&
                result.status != OperationSubmitStatus::RELAY_QUEUED);
        {
            std::lock_guard lock(m_chain.mutex);
            if (result.status == OperationSubmitStatus::ACCEPTED ||
                result.status == OperationSubmitStatus::ALREADY_PENDING ||
                result.status == OperationSubmitStatus::RELAY_QUEUED) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::ACCEPTED_REMOTE});
            } else if (const auto finalized = m_chain.store.GetFinalizedOperationHeight(op_id)) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED,
                    .finalized_height = *finalized});
                result.status = OperationSubmitStatus::ALREADY_FINALIZED;
                result.delivery_uncertain = false;
            } else if (result.delivery_uncertain) {
                if (result.status == OperationSubmitStatus::ALREADY_FINALIZED) result.status = OperationSubmitStatus::REJECTED;
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::UNKNOWN});
            } else {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            }
        }
        return result;
    }
    return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
}

void CybouNodeRuntime::EmitFinalizedEvents(const FinalizedBlock& block, bool produced)
{
    EventFields fields{{"height",block.block.height},{"block_id",ComputeBlockId(block.block).GetHex()},
        {"state_root",block.block.resulting_state_root.GetHex()}};
    if (m_config.event_writer) {
        m_config.event_writer->Write(produced ? NodeEvent::block_produced : NodeEvent::block_verified, fields);
        m_config.event_writer->Write(NodeEvent::block_finalized, fields);
    }
    for (const auto& op : block.block.operations) if (const auto id = ComputeOperationId(op)) {
        // Finalization is the only canonical answer. Once known, clear any
        // lingering relay work and collapse local status to FINALIZED.
        m_chain.operation_relay.ForgetFinalized(*id);
        if (const auto* revoke = std::get_if<AuthorizedRevokePublication>(&op); revoke && m_provider.finalized_chunk_store) {
            // Finalized revocation: the author deleted the object; drop its replicas now.
            (void)m_provider.finalized_chunk_store->PurgePublication(revoke->revoke.publication_id);
        }
        RememberOperationStatus(*id, {.kind = OperationStatusKind::FINALIZED, .finalized_height = block.block.height});
          if (m_config.event_writer) std::visit([&](const auto& value) {
              EventFields operation{{"operation_id",id->GetHex()},{"height",block.block.height}};
              if constexpr (requires { value.authorization.account_id; value.authorization.nonce; }) {
                  operation.emplace("account_id",value.authorization.account_id.Value().GetHex());
                  operation.emplace("nonce",value.authorization.nonce);
              } else if constexpr (requires { value.account_id; value.nonce; }) {
                  operation.emplace("account_id",value.account_id.Value().GetHex());
                  operation.emplace("nonce",value.nonce);
              }
              m_config.event_writer->Write(NodeEvent::operation_finalized,operation);
          },op);
    }
}

std::optional<StoragePayoutBinding> CybouNodeRuntime::LocalStoragePayoutBinding() const
{
    IdentitySignerRef signer;
    {
        std::lock_guard lock{m_chain.mutex};
        signer = m_chain.identity_signer;
    }
    const auto account = signer ? signer->Account() : std::nullopt;
    const auto storage_id = LocalStorageId();
    if (!account || !storage_id) return std::nullopt;
    const auto digest = StoragePayoutBindingDigest(m_network_binding, *storage_id, *account);
    auto authorization = signer->SignAuthorization(digest);
    auto storage_proof = SignStorageProof(digest);
    if (!authorization || !storage_proof) return std::nullopt;
    return StoragePayoutBinding{.payout_account = *account, .authorization = std::move(*authorization),
        .storage_proof = std::move(*storage_proof)};
}

} // namespace cybou
