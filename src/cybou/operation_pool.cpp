// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/operation_pool.h>

#include <cybou/block_executor.h>
#include <cybou/protocol_limits.h>

#include <limits>
#include <map>

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

    std::map<std::string, size_t> peer_counts;
    std::map<std::string, size_t> peer_bytes;
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
        if (m_entries.size() >= m_limits.max_count || m_bytes > m_limits.max_bytes ||
            entry.bytes > m_limits.max_bytes - m_bytes) {
            dropped.push_back(entry.id);
            continue;
        }
        if (entry.source_peer) {
            if (entry.source_peer->empty()) {
                dropped.push_back(entry.id);
                continue;
            }
            const auto p_count = peer_counts[*entry.source_peer];
            const auto p_bytes = peer_bytes[*entry.source_peer];
            if (p_count >= m_limits.max_peer_count || p_bytes > m_limits.max_peer_bytes ||
                entry.bytes > m_limits.max_peer_bytes - p_bytes) {
                dropped.push_back(entry.id);
                continue;
            }
        }

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
        if (entry.source_peer) {
            peer_counts[*entry.source_peer]++;
            peer_bytes[*entry.source_peer] += entry.bytes;
        }
        m_entries.push_back(std::move(entry));
    }

    return dropped;
}

void OperationPool::Clear()
{
    m_entries.clear();
    m_ids.clear();
    m_bytes = 0;
}

} // namespace cybou
