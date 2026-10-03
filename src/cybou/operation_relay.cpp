// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Реализация volatile-очереди точных байтов для P2P-ретрансляции операций.

#include <cybou/operation_relay.h>

#include <cybou/protocol_limits.h>
#include <cybou/protocol_operation.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cybou {

bool OperationRelay::FitsQueueLimits(const size_t bytes) const
{
    return m_queue.size() < m_max_operations &&
        bytes <= m_max_queued_bytes - std::min(m_queued_bytes, m_max_queued_bytes);
}

void OperationRelay::RememberSeen(const cybou::Hash256& operation_id)
{
    if (!m_seen_ids.insert(operation_id).second) return;
    m_seen_order.push_back(operation_id);
    while (m_seen_order.size() > m_seen_limit) {
        const auto expired = m_seen_order.front();
        m_seen_order.pop_front();
        if (!m_queued_ids.contains(expired)) m_seen_ids.erase(expired);
    }
}

OperationRelay::OperationRelay(const size_t max_operations,
    const size_t max_queued_bytes)
    : m_max_operations{max_operations}, m_max_queued_bytes{max_queued_bytes},
      m_seen_limit{std::max<size_t>(max_operations * 16, 1024)}
{
    if (max_operations == 0 || max_queued_bytes < MAX_OPERATION_PAYLOAD_BYTES) {
        throw std::invalid_argument{"invalid operation relay bounds"};
    }
}

OperationRelayEnqueueStatus OperationRelay::Enqueue(
    const std::span<const unsigned char> exact_operation_bytes, const bool allow_seen_retry)
{
    if (exact_operation_bytes.empty() || exact_operation_bytes.size() > MAX_OPERATION_PAYLOAD_BYTES) {
        return OperationRelayEnqueueStatus::INVALID_OPERATION;
    }
    const auto operation = DeserializeProtocolOperation(exact_operation_bytes);
    if (!operation) return OperationRelayEnqueueStatus::INVALID_OPERATION;
    const auto operation_id = ComputeOperationId(*operation);
    if (!operation_id || operation_id->IsNull()) return OperationRelayEnqueueStatus::INVALID_OPERATION;

    std::lock_guard lock{m_mutex};
    if (m_queued_ids.contains(*operation_id) || (m_seen_ids.contains(*operation_id) && !allow_seen_retry)) {
        return OperationRelayEnqueueStatus::DUPLICATE;
    }
    if (!FitsQueueLimits(exact_operation_bytes.size())) {
        return OperationRelayEnqueueStatus::QUEUE_FULL;
    }
    m_queue.push_back({*operation_id, std::vector<unsigned char>{exact_operation_bytes.begin(), exact_operation_bytes.end()}});
    m_queued_ids.insert(*operation_id);
    RememberSeen(*operation_id);
    m_queued_bytes += exact_operation_bytes.size();
    return OperationRelayEnqueueStatus::QUEUED;
}

std::optional<RelayedOperation> OperationRelay::Peek() const
{
    std::lock_guard lock{m_mutex};
    if (m_queue.empty()) return std::nullopt;
    return m_queue.front();
}

std::optional<RelayedOperation> OperationRelay::Claim()
{
    std::lock_guard lock{m_mutex};
    if (m_queue.empty() || m_claimed_id) return std::nullopt;
    m_claimed_id = m_queue.front().operation_id;
    return m_queue.front();
}

void OperationRelay::Release(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    if (m_claimed_id && *m_claimed_id == operation_id) m_claimed_id.reset();
}

bool OperationRelay::Acknowledge(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    if (!m_claimed_id || *m_claimed_id != operation_id || m_queue.empty() ||
        m_queue.front().operation_id != operation_id) return false;
    m_queued_bytes -= m_queue.front().exact_bytes.size();
    m_queued_ids.erase(operation_id);
    m_queue.pop_front();
    m_claimed_id.reset();
    return true;
}

bool OperationRelay::HasQueued(const cybou::Hash256& operation_id) const
{
    std::lock_guard lock{m_mutex};
    return m_queued_ids.contains(operation_id);
}

void OperationRelay::ForgetFinalized(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto item = std::find_if(m_queue.begin(), m_queue.end(), [&](const RelayedOperation& queued) {
        return queued.operation_id == operation_id;
    });
    if (item == m_queue.end()) return;
    m_queued_bytes -= item->exact_bytes.size();
    m_queued_ids.erase(operation_id);
    m_queue.erase(item);
    if (m_claimed_id && *m_claimed_id == operation_id) m_claimed_id.reset();
}

size_t OperationRelay::QueuedOperations() const
{
    std::lock_guard lock{m_mutex};
    return m_queue.size();
}

size_t OperationRelay::QueuedBytes() const
{
    std::lock_guard lock{m_mutex};
    return m_queued_bytes;
}

} // namespace cybou
