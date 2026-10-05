// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Finalized chain, candidates, PoA and Identity operation coordination.

#include <cybou/node_runtime.h>
#include <cybou/operation_work.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/identity_crypto.h>
#include <algorithm>
#include <limits>

namespace cybou {

CybouNodeRuntime::ChainCore::ChainCore(const NodeRuntimeConfig& config)
    : db{std::make_unique<KVStore>(KVStoreOptions{
          .path = config.data_dir, .cache_bytes = config.db_cache_bytes,
          .memory_only = config.memory_only, .wipe_data = config.wipe_data})},
      store{*db, config.network_genesis},
      operation_pool{store, OperationPoolLimits{.operation_work_bits = config.operation_work_bits}}
{
}

CybouNodeRuntime::ChainCore::~ChainCore() = default;

bool CybouNodeRuntime::InitializeGenesis(const CybouState& genesis, const bool sync)
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (loaded.error == StateLoadError::NONE && bool(loaded.state)) {
        return true;
    }
    // Any non-NOT_FOUND load error is treated as authoritative here: a caller
    // must not overwrite corrupt or foreign-network data by "initializing again".
    if (loaded.error != StateLoadError::NOT_FOUND) {
        return false;
    }
    const auto result = m_chain.store.InitializeGenesis(genesis, sync);
    return result.error == GenesisInitError::NONE;
}

NodeRuntimeStatus CybouNodeRuntime::GetStatus() const
{
    std::lock_guard lock(m_chain.mutex);
    NodeRuntimeStatus status;
    status.network_binding = m_network_binding;
    status.poa_signer_active = m_chain.poa_finalizer && m_chain.poa_finalizer->SignerEnabled();

    const auto loaded = m_chain.store.GetStateSnapshot();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        status.runtime_state = NodeRuntimeState::NETWORK_MISMATCH;
    } else if (loaded.error == StateLoadError::CORRUPT) {
        status.runtime_state = NodeRuntimeState::CORRUPT;
    } else if (loaded.error == StateLoadError::NOT_FOUND) {
        status.runtime_state = NodeRuntimeState::UNINITIALIZED;
    } else if (loaded.error == StateLoadError::NONE && bool(loaded.state)) {
        status.is_initialized = true;
        status.runtime_state = NodeRuntimeState::READY;
    }

    const auto head = m_chain.store.GetFinalizedHead();
    if (head) {
        status.finalized_height = head->height;
        status.finalized_tip = head->block_id;
    }
    const auto root = m_chain.store.GetStateRoot();
    if (root) {
        status.state_root = *root;
    }
    status.poa_safety_halted = m_chain.production_status == BlockProductionStatus::SAFETY_HALT || m_chain.store.PoaSafetyHalted() ||
        (m_chain.poa_finalizer && m_chain.poa_finalizer->SafetyHalted());
    // Safety halt intentionally dominates READY once state exists: UX must
    // surface fail-closed signing state even though finalized reads still work.
    if (status.poa_safety_halted && status.is_initialized) status.runtime_state = NodeRuntimeState::SAFETY_HALTED;
    return status;
}

PoaEvidenceReadResult CybouNodeRuntime::ReadPoaSafetyEvidence() const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.store.ReadPoaSafetyEvidence();
}

std::optional<uint64_t> CybouNodeRuntime::GetFinalizedHeight() const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.store.GetFinalizedHeight();
}

std::optional<cybou::Hash256> CybouNodeRuntime::GetFinalizedTip() const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.store.GetFinalizedTip();
}

std::optional<cybou::Hash256> CybouNodeRuntime::GetStateRoot() const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.store.GetStateRoot();
}

std::optional<AccountState> CybouNodeRuntime::GetAccountState(const AccountId& account_id) const
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (!loaded || !loaded.state) return std::nullopt;
    const auto it = loaded.state->accounts.find(account_id);
    if (it == loaded.state->accounts.end()) return std::nullopt;
    return it->second;
}

bool CybouNodeRuntime::IsTreasuryClaimant(const AccountId& account_id) const
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (!loaded || !loaded.state) return false;
    return std::any_of(loaded.state->genesis_allocations.begin(), loaded.state->genesis_allocations.end(),
        [&](const auto& entry) {
            return entry.second.label == CENTRAL_AUTHORITY_NAME && entry.second.claimed_by == account_id;
        });
}

std::optional<uint64_t> CybouNodeRuntime::PrepareOperationWork(const ProtocolOperation& op)
{
    // The originating node does the relay proof-of-work (DEC-273) outside the
    // runtime lock: it is meant to take noticeable time on the author's machine.
    // Without finalized state the author counts as T0, the hardest tier.
    const auto op_id = ComputeOperationId(op);
    if (!op_id) return std::nullopt;
    uint32_t bits{m_chain.operation_pool.RequiredWorkBits(op, CybouState{})};
    std::optional<uint64_t> nonce;
    {
        std::lock_guard lock(m_chain.mutex);
        if (const auto solved = m_chain.solved_work.find(*op_id); solved != m_chain.solved_work.end()) nonce = solved->second;
        const auto loaded = m_chain.store.GetStateSnapshot();
        if (loaded && loaded.state) bits = m_chain.operation_pool.RequiredWorkBits(op, *loaded.state);
    }
    if (!nonce || !CheckOperationWork(m_network_binding, *op_id, *nonce, bits)) {
        nonce = SolveOperationWork(m_network_binding, *op_id, bits);
        if (!nonce) return std::nullopt;
        std::lock_guard lock(m_chain.mutex);
        if (m_chain.solved_work.size() >= 1024) m_chain.solved_work.erase(m_chain.solved_work.begin());
        m_chain.solved_work[*op_id] = *nonce;
    }
    return nonce;
}

bool CybouNodeRuntime::IsPublicationActive(const cybou::Hash256& publication_id) const
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    return loaded && loaded.state && loaded.state->publications.contains(publication_id);
}

std::optional<StorageLeaseRecord> CybouNodeRuntime::GetStorageLease(const cybou::Hash256& publication_id) const
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (!loaded || !loaded.state) return std::nullopt;
    const auto lease = loaded.state->leases.find(publication_id);
    if (lease == loaded.state->leases.end()) return std::nullopt;
    return lease->second;
}

std::optional<StorageSettlementCursor> CybouNodeRuntime::GetStorageSettlementCursor() const
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (!loaded || !loaded.state) return std::nullopt;
    return loaded.state->settlement;
}

bool CybouNodeRuntime::IsStorageLeaseActive(const cybou::Hash256& publication_id) const
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (!loaded || !loaded.state || !loaded.state->publications.contains(publication_id)) return false;
    const auto lease = loaded.state->leases.find(publication_id);
    const auto period = loaded.state->settlement.next_period;
    return lease != loaded.state->leases.end() && lease->second.first_period <= period &&
        period < lease->second.end_period;
}

OperationSubmitResult CybouNodeRuntime::SubmitOperation(ProtocolOperation op)
{
    const auto nonce = PrepareOperationWork(op);
    if (!nonce) return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED,
        .op_id = ComputeOperationId(op).value_or(cybou::Hash256{})};
    return SubmitOperationInternal(std::move(op), *nonce, std::nullopt);
}

OperationSubmitResult CybouNodeRuntime::SubmitStorageSettlement(const uint64_t period_start_utc,
    std::vector<StorageSettlementEntry> entries)
{
    std::lock_guard lock(m_chain.mutex);
    if (!m_chain.poa_finalizer || !m_chain.poa_finalizer->SignerEnabled() || m_chain.store.PoaSafetyHalted()) {
        return {.status = OperationSubmitStatus::POA_SIGNER_UNAVAILABLE};
    }
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (loaded.error != StateLoadError::NONE || !loaded.state || m_chain.poa_finalizer->SafetyHalted()) return {};
    StorageSettlement settlement{.period = loaded.state->settlement.next_period,
        .period_start_utc = period_start_utc, .entries = std::move(entries)};
    if (!m_chain.poa_finalizer->SignStorageSettlement(settlement)) return {};
    const ProtocolOperation operation{std::move(settlement)};
    OperationSubmitStatus status{OperationSubmitStatus::REJECTED};
    // Signed by the genesis PoA key: its own protection, no relay PoW.
    switch (m_chain.operation_pool.Admit(operation, 0)) {
    case PoolAdmission::ACCEPTED: status = OperationSubmitStatus::ACCEPTED; break;
    case PoolAdmission::ALREADY_PENDING: status = OperationSubmitStatus::ALREADY_PENDING; break;
    case PoolAdmission::ALREADY_FINALIZED: status = OperationSubmitStatus::ALREADY_FINALIZED; break;
    case PoolAdmission::REJECTED: break;
    }
    const OperationSubmitResult result{.status = status, .op_id = ComputeOperationId(operation).value_or(cybou::Hash256{})};
    if (result.status == OperationSubmitStatus::ACCEPTED) {
        RememberOperationStatus(result.op_id, {.kind = OperationStatusKind::LOCAL_PENDING});
    }
    return result;
}

OperationRelayEnqueueStatus CybouNodeRuntime::EnqueueRelayedOperation(
    const std::span<const unsigned char> exact_bytes, const uint64_t work_nonce, const bool allow_seen_retry,
    std::optional<std::string> source_peer)
{
    const auto operation = DeserializeProtocolOperation(exact_bytes);
    if (!operation) return OperationRelayEnqueueStatus::INVALID_OPERATION;
    const auto canonical_bytes = SerializeProtocolOperation(*operation);
    if (!canonical_bytes || !std::ranges::equal(*canonical_bytes, exact_bytes)) {
        return OperationRelayEnqueueStatus::INVALID_OPERATION;
    }
    {
        std::lock_guard lock{m_chain.mutex};
        const auto loaded = m_chain.store.GetStateSnapshot();
        if (loaded.error != StateLoadError::NONE || !loaded.state ||
            !VerifyProtocolOperationRelayProofs(*operation, m_network_binding, loaded.state->identities)) {
            return OperationRelayEnqueueStatus::INVALID_OPERATION;
        }
        // Full candidate execution, the same path PoA uses. A node never
        // forwards an operation it could not execute itself.
        switch (m_chain.operation_pool.Admit(*operation, work_nonce, std::move(source_peer))) {
        case PoolAdmission::ACCEPTED:
        case PoolAdmission::ALREADY_PENDING:
            break;
        case PoolAdmission::ALREADY_FINALIZED:
            return OperationRelayEnqueueStatus::DUPLICATE;
        case PoolAdmission::REJECTED:
            return OperationRelayEnqueueStatus::INVALID_OPERATION;
        }
    }
    return m_chain.operation_relay.Enqueue(exact_bytes, work_nonce, allow_seen_retry);
}

size_t CybouNodeRuntime::CandidateOperationCount() const
{
    std::lock_guard lock{m_chain.mutex};
    return m_chain.operation_pool.Size();
}

std::vector<cybou::Hash256> CybouNodeRuntime::CandidateOperationIds() const
{
    std::lock_guard lock{m_chain.mutex};
    return m_chain.operation_pool.Ids();
}

bool CybouNodeRuntime::HasCandidateOperation(const cybou::Hash256& operation_id) const
{
    std::lock_guard lock{m_chain.mutex};
    return m_chain.operation_pool.Contains(operation_id);
}

void CybouNodeRuntime::RevalidateCandidates()
{
    for (const auto& dropped : m_chain.operation_pool.Revalidate()) m_chain.operation_relay.ForgetFinalized(dropped);
}

void CybouNodeRuntime::SetIdentitySigner(IdentitySignerRef signer)
{
    std::lock_guard lock{m_chain.mutex};
    m_chain.identity_signer = std::move(signer);
}

std::optional<RelayedOperation> CybouNodeRuntime::ClaimRelayedOperation()
{
    return m_chain.operation_relay.Claim();
}

std::optional<RelayedOperation> CybouNodeRuntime::NextRelayedOperation(
    const std::function<bool(const cybou::Hash256&)>& skip) const
{
    return m_chain.operation_relay.Peek(skip);
}

void CybouNodeRuntime::ReleaseRelayedOperation(const cybou::Hash256& operation_id)
{
    m_chain.operation_relay.Release(operation_id);
}

bool CybouNodeRuntime::AcknowledgeRelayedOperation(const cybou::Hash256& operation_id)
{
    return m_chain.operation_relay.Acknowledge(operation_id);
}

bool CybouNodeRuntime::HasRelayedOperation(const cybou::Hash256& operation_id) const
{
    return m_chain.operation_relay.HasQueued(operation_id);
}

OperationStatus CybouNodeRuntime::GetOperationStatus(const cybou::Hash256& op_id) const
{
    if (op_id.IsNull()) return {};
    std::lock_guard lock(m_chain.mutex);
    if (const auto height = m_chain.store.GetFinalizedOperationHeight(op_id)) {
        return {.kind = OperationStatusKind::FINALIZED, .finalized_height = *height};
    }
    // Only a PoA node's own pool is local pending; an ordinary node holding a
    // candidate must keep relaying it until the finalizer acknowledges it.
    if (m_chain.poa_finalizer && m_chain.poa_finalizer->SignerEnabled() && m_chain.operation_pool.Contains(op_id)) {
        return {.kind = OperationStatusKind::LOCAL_PENDING};
    }
    const auto known = m_chain.recent_operation_status.find(op_id);
    return known != m_chain.recent_operation_status.end() ? known->second : OperationStatus{};
}

IdentityOperationCoordinator& CybouNodeRuntime::GetIdentityOperationCoordinator(CybouKeyStore& keystore)
{
    std::lock_guard lock(m_chain.mutex);
    // One coordinator (and nonce journal) per key store: a coordinator signs
    // with the key store it was created for, so it must never be shared.
    if (const auto it = m_chain.identity_operation_coordinators.find(&keystore); it != m_chain.identity_operation_coordinators.end()) {
        return *it->second;
    }
    const auto index = m_chain.identity_operation_coordinators.size();
    const auto journal = index == 0 ? m_config.data_dir / "identity-operation.cyiop"
        : m_config.data_dir / ("identity-operation-" + std::to_string(index) + ".cyiop");
    auto& coordinator = m_chain.identity_operation_coordinators[&keystore];
    coordinator = std::make_unique<IdentityOperationCoordinator>(*this, keystore, journal);
    return *coordinator;
}

void CybouNodeRuntime::RetryPendingIdentityOperations()
{
    std::vector<IdentityOperationCoordinator*> coordinators;
    {
        std::lock_guard lock(m_chain.mutex);
        coordinators.reserve(m_chain.identity_operation_coordinators.size());
        for (const auto& [keystore, coordinator] : m_chain.identity_operation_coordinators) {
            (void)keystore;
            coordinators.push_back(coordinator.get());
        }
    }
    for (auto* coordinator : coordinators) coordinator->RetryRelayIfDue();
}

void CybouNodeRuntime::RememberOperationStatus(const cybou::Hash256& id, OperationStatus status)
{
    if (id.IsNull()) return;
    if (!m_chain.recent_operation_status.contains(id)) m_chain.recent_operation_status_order.push_back(id);
    const auto previous = m_chain.recent_operation_status.find(id);
    const bool changed = previous == m_chain.recent_operation_status.end() || previous->second.kind != status.kind ||
        previous->second.finalized_height != status.finalized_height;
    m_chain.recent_operation_status[id] = status;
    if (m_config.event_writer && changed) {
        auto event = status.kind == OperationStatusKind::FINALIZED ? NodeEvent::operation_finalized
            : status.kind == OperationStatusKind::REJECTED_KNOWN ? NodeEvent::operation_rejected
            : status.kind == OperationStatusKind::UNKNOWN ? NodeEvent::operation_uncertain
            : NodeEvent::operation_accepted;
        m_config.event_writer->Write(event, {{"operation_id",id.GetHex()},{"height",status.finalized_height}});
    }
    while (m_chain.recent_operation_status_order.size() > 256) {
        m_chain.recent_operation_status.erase(m_chain.recent_operation_status_order.front());
        m_chain.recent_operation_status_order.pop_front();
    }
}

std::optional<FinalizedBlock> CybouNodeRuntime::ProduceBlock(const bool sync)
{
    std::lock_guard lock(m_chain.mutex);
    if (m_chain.production_status == BlockProductionStatus::SAFETY_HALT) return std::nullopt;
    m_chain.production_status = BlockProductionStatus::RETRY;
    if (!m_chain.poa_finalizer || !m_chain.poa_finalizer->SignerEnabled()) {
        m_chain.production_status = BlockProductionStatus::SIGNER_UNAVAILABLE;
        return std::nullopt;
    }
    if (m_chain.store.PoaSafetyHalted() || m_chain.poa_finalizer->SafetyHalted()) {
        m_chain.production_status = BlockProductionStatus::SAFETY_HALT;
        return std::nullopt;
    }
    try {
        const auto loaded = m_chain.store.GetStateSnapshot();
        if (loaded.error != StateLoadError::NONE || !loaded.state) {
            m_chain.production_status = BlockProductionStatus::SAFETY_HALT;
            return std::nullopt;
        }
        const auto head = m_chain.store.GetFinalizedHead();
        if (!head || head->height == std::numeric_limits<uint64_t>::max()) return std::nullopt;
        if (m_chain.poa_finalizer->CheckCanonicalTip(head->height, head->block_id) != PoaJournalStatus::NONE) {
            m_chain.production_status = BlockProductionStatus::SAFETY_HALT;
            return std::nullopt;
        }
        if (m_chain.production_candidate && m_chain.production_candidate->height <= head->height) {
            // A concurrent verified commit may have completed the same intent.
            m_chain.production_candidate.reset();
            m_chain.production_finalized.reset();
        }
        if (!m_chain.production_candidate) {
            const auto operations = m_chain.operation_pool.Snapshot();
            const auto root = m_chain.store.ComputeCandidateStateRoot(operations, head->height + 1);
            if (!root) { RevalidateCandidates(); return std::nullopt; }
            // Snapshot() preserves the node's locally validated candidate order;
            // PoA finalization never imports an external mempool ordering.
            CybouBlock candidate{.parent_block_id = head->block_id,
                .height = head->height + 1, .operations = operations, .resulting_state_root = *root};
            const auto encoded = SerializeBlock(candidate);
            if (!encoded || encoded->size() > MAX_FINALIZER_SERIALIZED_BLOCK_BYTES -
                    POA_FINALITY_CERTIFICATE_SIZE - 8) return std::nullopt;
            m_chain.production_candidate = std::move(candidate);
        }
        if (!m_chain.production_finalized) {
            const auto signing = m_chain.poa_finalizer->SignFinality(head->height, head->block_id, *m_chain.production_candidate);
            if (!signing.certificate) {
                if (signing.status == PoaSigningStatus::JOURNAL_REJECTED)
                    m_chain.production_status = BlockProductionStatus::SAFETY_HALT;
                return std::nullopt;
            }
            m_chain.production_finalized = FinalizedBlock{.block = *m_chain.production_candidate, .certificate = *signing.certificate};
        }
        const auto bytes = SerializeFinalizedBlock(*m_chain.production_finalized);
        if (!bytes || bytes->size() > MAX_FINALIZER_SERIALIZED_BLOCK_BYTES) {
            m_chain.production_status = BlockProductionStatus::SAFETY_HALT;
            return std::nullopt;
        }
        if (!m_chain.store.CommitFinalizedBlock(*m_chain.production_finalized, sync)) return std::nullopt;
        auto finalized = std::move(*m_chain.production_finalized);
        m_chain.production_candidate.reset();
        m_chain.production_finalized.reset();
        m_chain.production_status = BlockProductionStatus::PRODUCED;
        EmitFinalizedEvents(finalized, true);
        RevalidateCandidates();
        return finalized;
    } catch (...) {
        // Journal failures are never retried as ordinary transport/storage failures.
        if (m_chain.poa_finalizer->SafetyHalted()) m_chain.production_status = BlockProductionStatus::SAFETY_HALT;
        return std::nullopt;
    }
}

BlockProductionStatus CybouNodeRuntime::LastBlockProductionStatus() const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.production_status;
}

bool CybouNodeRuntime::EnablePoaSigner(std::shared_ptr<PoaSigner> signer)
{
    if (!signer) return false;
    bool enabled{false};
    {
        std::lock_guard lock(m_chain.mutex);
        if (!m_chain.poa_finalizer) m_chain.poa_finalizer = std::make_unique<PoaFinalizer>(m_chain.store.GetDatabase(), m_chain.store.GetNetworkBinding(),
            m_chain.store.GetNetworkGenesis().GetGenesisAnchor(), m_chain.store.GetNetworkGenesis().GetPoaPublicKey());
        if (m_chain.production_status == BlockProductionStatus::SAFETY_HALT || m_chain.store.PoaSafetyHalted()) return false;
        enabled = m_chain.poa_finalizer->EnableSigner(std::move(signer));
    }
    return enabled;
}

void CybouNodeRuntime::DisablePoaSigner()
{
    {
        std::lock_guard lock(m_chain.mutex);
        if (m_chain.poa_finalizer) m_chain.poa_finalizer->DisableSigner();
    }

}

bool CybouNodeRuntime::IsPoaSignerActive() const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.poa_finalizer && m_chain.poa_finalizer->SignerEnabled();
}

BlockTransitionResult CybouNodeRuntime::CommitBlock(const FinalizedBlock& block, const bool sync)
{
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        return BlockTransitionResult{.error = BlockTransitionError::NETWORK_MISMATCH};
    }
    if (loaded.error != StateLoadError::NONE || !bool(loaded.state)) {
        return BlockTransitionResult{.error = BlockTransitionError::STATE_NOT_INITIALIZED};
    }
    const auto result = m_chain.store.CommitFinalizedBlock(block, sync);
    if (result) {
        EmitFinalizedEvents(block, false);
        RevalidateCandidates();
    }
    return result;
}

std::optional<FinalizedBlock> CybouNodeRuntime::GetBlockAtHeight(const uint64_t height) const
{
    std::lock_guard lock(m_chain.mutex);
    return m_chain.store.GetBlockAtHeight(height);
}

FinalizedOperationLookupResult CybouNodeRuntime::FindFinalizedOperation(const cybou::Hash256& op_id) const
{
    FinalizedOperationLookupResult result;
    const auto status = GetStatus();
    if (!status.is_initialized || op_id.IsNull()) return result;
    result.status = FinalizedOperationLookupStatus::NOT_FOUND;
    cybou::Hash256 previous_id = m_config.network_genesis.GetGenesisAnchor();
    for (uint64_t height = 1; height <= status.finalized_height; ++height) {
        const auto finalized = GetBlockAtHeight(height);
        if (!finalized) {
            result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
            return result;
        }
        const auto block_id = ComputeBlockId(finalized->block);
        // Lookup is intentionally strict: any gap or certificate mismatch means
        // the local history cannot be used as verified evidence for this answer.
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_binding != status.network_binding ||
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

std::optional<RootPublication> CybouNodeRuntime::FindFinalizedRootPublication(const cybou::Hash256& op_id) const
{
    const auto location = FindFinalizedOperation(op_id);
    if (location.status != FinalizedOperationLookupStatus::FOUND) return std::nullopt;
    const auto finalized = GetBlockAtHeight(location.height);
    if (!finalized || location.operation_index >= finalized->block.operations.size() ||
        ComputeBlockId(finalized->block) != location.block_id) return std::nullopt;
    const auto& operation = finalized->block.operations[location.operation_index];
    const auto operation_id = ComputeOperationId(operation);
    if (!operation_id || *operation_id != op_id) return std::nullopt;
    const auto* publication = std::get_if<AuthorizedRootPublication>(&operation);
    if (!publication) return std::nullopt;
    // A revoked publication authorizes nothing any more (DEC-271): it left the register.
    std::lock_guard lock(m_chain.mutex);
    const auto loaded = m_chain.store.GetStateSnapshot();
    if (!loaded || !loaded.state || !loaded.state->publications.contains(op_id)) return std::nullopt;
    return publication->publication;
}

IdentityKemPackageLookupResult CybouNodeRuntime::FindIdentityKemPackage(
    const AccountId& account_id, const uint64_t key_epoch) const
{
    std::lock_guard lock(m_chain.mutex);
    IdentityKemPackageLookupResult result;
    const auto loaded = m_chain.store.GetStateSnapshot();
    const auto finalized_height = m_chain.store.GetFinalizedHeight();
    const auto state_root = m_chain.store.GetStateRoot();
    if (!loaded || !loaded.state || !finalized_height || !state_root || account_id.IsNull()) return result;
    const auto* identity = loaded.state->identities.Find(account_id);
    if (!identity) {
        result.status = IdentityKemPackageLookupStatus::ACCOUNT_NOT_FOUND;
        return result;
    }
    result.finalized_height = *finalized_height;
    result.state_root = *state_root;
    if (key_epoch > identity->key_epoch) {
        result.status = IdentityKemPackageLookupStatus::KEY_EPOCH_UNAVAILABLE;
        return result;
    }

    bool found{false};
    const auto account_bytes = account_id.Value();
    cybou::Hash256 previous_id = m_config.network_genesis.GetGenesisAnchor();
    for (uint64_t height = 1; height <= *finalized_height; ++height) {
        const auto finalized = m_chain.store.GetBlockAtHeight(height);
        if (!finalized) return result;
        const auto block_id = ComputeBlockId(finalized->block);
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_binding != m_network_binding ||
            finalized->certificate.height != height ||
            finalized->certificate.block_id != block_id) return result;
        for (size_t index = 0; index < finalized->block.operations.size(); ++index) {
            const auto& operation = finalized->block.operations[index];
            if (!ComputeOperationId(operation)) return result;
            const IdentityKemPackage* package{nullptr};
            AccountId published_account;
            uint64_t published_epoch{0};
            if (const auto* create = std::get_if<AccountCreateOp>(&operation)) {
                published_account = create->account_id;
                package = &create->kem_package;
            } else if (const auto* rotate = std::get_if<IdentityRotate>(&operation)) {
                published_account = rotate->account_id;
                published_epoch = rotate->key_epoch;
                package = &rotate->new_kem_package;
            }
            if (!package || published_account != account_id || published_epoch != key_epoch) continue;
            if (found) return result;
            const auto commitment = ComputeIdentityKemPackageCommitment(
                std::span<const unsigned char, 32>{m_network_binding.begin(), 32},
                std::span<const unsigned char, 32>{account_bytes.begin(), 32}, key_epoch, *package);
            if (!commitment) return result;
            if (key_epoch == identity->key_epoch && *commitment != identity->kem_package_id) {
                result.status = IdentityKemPackageLookupStatus::HISTORY_UNAVAILABLE;
                return result;
            }
            found = true;
            result.package = *package;
            result.package_id = *commitment;
            result.key_epoch = key_epoch;
            result.operation_height = height;
            result.operation_index = static_cast<uint32_t>(index);
            result.block_id = block_id;
        }
        previous_id = block_id;
        if (height == *finalized_height) break;
    }
    result.status = found ? IdentityKemPackageLookupStatus::FOUND : IdentityKemPackageLookupStatus::NOT_FOUND;
    return result;
}

} // namespace cybou
