// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Реализация пула локально проверенных операций.

#include <cybou/operation_pool.h>

#include <cybou/block_executor.h>
#include <cybou/operation_work.h>
#include <cybou/protocol_limits.h>

#include <limits>
#include <map>

namespace cybou {

bool OperationPool::FitsGlobalLimits(const size_t bytes) const
{
    // Keep arithmetic fail-closed against accidental underflow when callers
    // hand us a pool already at or above its byte ceiling.
    return m_entries.size() < m_limits.max_count &&
        m_bytes <= m_limits.max_bytes &&
        bytes <= m_limits.max_bytes - m_bytes;
}

bool OperationPool::FitsPeerLimits(const std::string& peer, const size_t bytes) const
{
    const auto found = m_peer_usage.find(peer);
    const PeerUsage usage = found == m_peer_usage.end() ? PeerUsage{} : found->second;
    return usage.count < m_limits.max_peer_count &&
        usage.bytes <= m_limits.max_peer_bytes &&
        bytes <= m_limits.max_peer_bytes - usage.bytes;
}

void OperationPool::RecordPeerUsage(const std::optional<std::string>& peer, const size_t bytes)
{
    if (!peer) return;
    auto& usage = m_peer_usage[*peer];
    ++usage.count;
    usage.bytes += bytes;
}

uint32_t OperationPool::RequiredWorkBits(const ProtocolOperation& operation, const CybouState& finalized) const
{
    const uint32_t bits = RequiredOperationWorkBits(operation, finalized);
    return bits == 0 || !m_limits.operation_work_bits ? bits : *m_limits.operation_work_bits;
}

std::optional<uint64_t> OperationPool::WorkNonce(const cybou::Hash256& id) const
{
    for (const auto& entry : m_entries) if (entry.id == id) return entry.work_nonce;
    return std::nullopt;
}

PoolAdmission OperationPool::Admit(const ProtocolOperation& operation, const uint64_t work_nonce,
                                   std::optional<std::string> source_peer)
{
    const auto encoded = SerializeProtocolOperation(operation);
    const auto id = ComputeOperationId(operation);
    if (!encoded || !id || id->IsNull() || encoded->empty() ||
        encoded->size() > MAX_OPERATION_PAYLOAD_BYTES) return PoolAdmission::REJECTED;
    if (m_ids.contains(*id)) return PoolAdmission::ALREADY_PENDING;
    if (m_store.HasIndexedFinalizedOperation(*id)) return PoolAdmission::ALREADY_FINALIZED;

    {
        const auto loaded = m_store.LoadState();
        if (!loaded || !loaded.state) return PoolAdmission::REJECTED;
        if (const auto* create = std::get_if<AccountCreateOp>(&operation);
            create && loaded.state->accounts.contains(create->account_id)) {
            return PoolAdmission::REJECTED;
        }
        // Relay PoW (DEC-273): no Full Node, PoA included, holds or forwards an
        // operation whose work does not meet its author's tier on finalized state.
        if (!CheckOperationWork(m_store.GetNetworkBinding(), *id, work_nonce,
                RequiredWorkBits(operation, *loaded.state))) {
            return PoolAdmission::REJECTED;
        }
    }

    if (!FitsGlobalLimits(encoded->size())) return PoolAdmission::REJECTED;
    if (source_peer) {
        if (source_peer->empty()) return PoolAdmission::REJECTED;
        if (!FitsPeerLimits(*source_peer, encoded->size())) return PoolAdmission::REJECTED;
    }
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max()) return PoolAdmission::REJECTED;
    // Candidate execution always replays the whole ordered set plus the new
    // operation. This preserves the Full Node invariant: relay never depends
    // on trust in a peer's "already checked" claim.
    auto candidate = Snapshot();
    candidate.push_back(operation);
    if (!m_store.ComputeCandidateStateRoot(candidate, head->height + 1)) return PoolAdmission::REJECTED;
    m_entries.push_back(Entry{operation, *id, work_nonce, encoded->size(), std::move(source_peer)});
    m_ids.insert(*id);
    m_bytes += encoded->size();
    RecordPeerUsage(m_entries.back().source_peer, m_entries.back().bytes);
    return PoolAdmission::ACCEPTED;
}

std::vector<cybou::Hash256> OperationPool::Ids() const
{
    std::vector<cybou::Hash256> ids;
    ids.reserve(m_entries.size());
    for (const auto& entry : m_entries) ids.push_back(entry.id);
    return ids;
}

std::vector<ProtocolOperation> OperationPool::Snapshot() const
{
    std::vector<ProtocolOperation> operations;
    operations.reserve(m_entries.size());
    for (const auto& entry : m_entries) operations.push_back(entry.operation);
    return operations;
}

std::vector<cybou::Hash256> OperationPool::Revalidate()
{
    auto previous = std::move(m_entries);
    Clear();
    std::vector<cybou::Hash256> dropped;
    if (previous.empty()) return dropped;

    const auto loaded = m_store.LoadState();
    const auto head = m_store.GetFinalizedHead();
    if (!loaded || !loaded.state || !head || head->height == std::numeric_limits<uint64_t>::max()) {
        for (const auto& entry : previous) dropped.push_back(entry.id);
        return dropped;
    }

    const auto& params = m_store.GetNetworkGenesis().GetProtocolParameters();
    const auto& poa_key = m_store.GetNetworkGenesis().GetPoaPublicKey();
    const auto& binding = m_store.GetNetworkBinding();
    const uint64_t target_height = head->height + 1;

    CybouState current_state = *loaded.state;
    if (params.name_commit_max_lifetime > 0) {
        std::erase_if(current_state.names.pending_commits, [&](const auto& item) {
            return target_height > item.second.commit_height &&
                target_height - item.second.commit_height > params.name_commit_max_lifetime;
        });
    }

    size_t account_creates{0};

    for (auto& entry : previous) {
        if (m_ids.contains(entry.id)) {
            dropped.push_back(entry.id);
            continue;
        }
        if (m_store.HasIndexedFinalizedOperation(entry.id)) {
            dropped.push_back(entry.id);
            continue;
        }
        if (const auto* create = std::get_if<AccountCreateOp>(&entry.operation)) {
            if (account_creates >= params.max_account_creates_per_block ||
                current_state.accounts.contains(create->account_id)) {
                dropped.push_back(entry.id);
                continue;
            }
        }
        // A tier can drop (BURN): the held work must still meet the finalized requirement.
        if (!CheckOperationWork(binding, entry.id, entry.work_nonce, RequiredWorkBits(entry.operation, *loaded.state))) {
            dropped.push_back(entry.id);
            continue;
        }
        if (!FitsGlobalLimits(entry.bytes)) {
            dropped.push_back(entry.id);
            continue;
        }
        if (entry.source_peer) {
            if (entry.source_peer->empty()) {
                dropped.push_back(entry.id);
                continue;
            }
            if (!FitsPeerLimits(*entry.source_peer, entry.bytes)) {
                dropped.push_back(entry.id);
                continue;
            }
        }

        // Revalidate sequentially against the evolving candidate state for the
        // next block height; keeping a once-valid prefix while dropping only
        // later conflicts avoids rebuilding an invalid mixed pool.
        auto exec = ExecuteBlockOperations(current_state, {entry.operation}, binding,
            target_height, params, &poa_key);
        if (!exec || !exec.state) {
            dropped.push_back(entry.id);
            continue;
        }

        if (std::holds_alternative<AccountCreateOp>(entry.operation)) {
            ++account_creates;
        }

        current_state = std::move(*exec.state);
        m_ids.insert(entry.id);
        m_bytes += entry.bytes;
        RecordPeerUsage(entry.source_peer, entry.bytes);
        m_entries.push_back(std::move(entry));
    }

    return dropped;
}

void OperationPool::Clear()
{
    m_entries.clear();
    m_ids.clear();
    m_peer_usage.clear();
    m_bytes = 0;
}

} // namespace cybou
