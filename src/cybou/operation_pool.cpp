// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/operation_pool.h>

#include <cybou/block_feed.h>

#include <limits>

namespace cybou {

PoolAdmission OperationPool::Admit(const ProtocolOperation& operation,
                                   std::optional<std::string> source_peer)
{
    const auto encoded = SerializeProtocolOperation(operation);
    const auto id = ComputeOperationId(operation);
    if (!encoded || !id || id->IsNull() || encoded->empty() ||
        encoded->size() > MAX_OPERATION_PAYLOAD_BYTES) return PoolAdmission::REJECTED;
    if (m_ids.contains(*id)) return PoolAdmission::ALREADY_PENDING;
    if (m_store.HasIndexedFinalizedOperation(*id)) return PoolAdmission::ALREADY_FINALIZED;

    if (const auto* create = std::get_if<AccountCreateOp>(&operation)) {
        const auto loaded = m_store.LoadState();
        if (!loaded || !loaded.state) return PoolAdmission::REJECTED;
        if (loaded.state->accounts.contains(create->account_id)) {
            // A pre-index AccountCreate can still be retried exactly. Never
            // treat a different operation for the same AccountID as finalized.
            const auto head = m_store.GetFinalizedHead();
            if (!head) return PoolAdmission::REJECTED;
            for (uint64_t height = head->height; height > 0; --height) {
                const auto finalized = m_store.GetBlockAtHeight(height);
                if (!finalized) return PoolAdmission::REJECTED;
                for (const auto& prior : finalized->block.operations) {
                    if (const auto* previous = std::get_if<AccountCreateOp>(&prior);
                        previous && previous->account_id == create->account_id) {
                        return prior == operation ? PoolAdmission::ALREADY_FINALIZED
                                                  : PoolAdmission::REJECTED;
                    }
                }
            }
            return PoolAdmission::REJECTED;
        }
    }

    if (m_entries.size() >= m_limits.max_count || m_bytes > m_limits.max_bytes ||
        encoded->size() > m_limits.max_bytes - m_bytes) return PoolAdmission::REJECTED;
    if (source_peer) {
        if (source_peer->empty()) return PoolAdmission::REJECTED;
        size_t peer_count{0};
        size_t peer_bytes{0};
        for (const auto& entry : m_entries) {
            if (entry.source_peer == source_peer) {
                ++peer_count;
                peer_bytes += entry.bytes;
            }
        }
        if (peer_count >= m_limits.max_peer_count || peer_bytes > m_limits.max_peer_bytes ||
            encoded->size() > m_limits.max_peer_bytes - peer_bytes) return PoolAdmission::REJECTED;
    }
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max()) return PoolAdmission::REJECTED;
    auto candidate = Snapshot();
    candidate.push_back(operation);
    if (!m_store.ComputeCandidateStateRoot(candidate, head->height + 1)) return PoolAdmission::REJECTED;
    m_entries.push_back(Entry{operation, *id, encoded->size(), std::move(source_peer)});
    m_ids.insert(*id);
    m_bytes += encoded->size();
    return PoolAdmission::ACCEPTED;
}

std::vector<ProtocolOperation> OperationPool::Snapshot() const
{
    std::vector<ProtocolOperation> operations;
    operations.reserve(m_entries.size());
    for (const auto& entry : m_entries) operations.push_back(entry.operation);
    return operations;
}

void OperationPool::Revalidate()
{
    auto previous = std::move(m_entries);
    Clear();
    for (const auto& entry : previous) Admit(entry.operation, entry.source_peer);
}

void OperationPool::Clear()
{
    m_entries.clear();
    m_ids.clear();
    m_bytes = 0;
}

} // namespace cybou
