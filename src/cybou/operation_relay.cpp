// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/operation_relay.h>

#include <cybou/protocol_limits.h>
#include <cybou/protocol_operation.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace cybou {

OperationRelay::OperationRelay(const size_t max_operations,
    const size_t max_queued_bytes)
    : m_max_operations{max_operations}, m_max_queued_bytes{max_queued_bytes}
{
    if (max_operations == 0 || max_queued_bytes < MAX_OPERATION_PAYLOAD_BYTES) {
        throw std::invalid_argument{"invalid operation relay bounds"};
    }
}

std::optional<OperationRelay::FinalizerSession>
OperationRelay::AttachAuthenticatedFinalizer()
{
    std::lock_guard lock{m_mutex};
    if (m_finalizer_session || m_next_session == 0) return std::nullopt;
    const auto session = m_next_session++;
    m_finalizer_session = session;
    return session;
}

void OperationRelay::DetachFinalizer(const FinalizerSession session)
{
    std::lock_guard lock{m_mutex};
    if (!m_finalizer_session || *m_finalizer_session != session) return;
    m_finalizer_session.reset();
    m_queue.clear();
    m_queued_ids.clear();
    m_queued_bytes = 0;
}

bool OperationRelay::HasAuthenticatedFinalizer() const
{
    std::lock_guard lock{m_mutex};
    return m_finalizer_session.has_value();
}

OperationRelayEnqueueStatus OperationRelay::Enqueue(
    const std::span<const unsigned char> exact_operation_bytes)
{
    if (exact_operation_bytes.empty() || exact_operation_bytes.size() > MAX_OPERATION_PAYLOAD_BYTES) {
        return OperationRelayEnqueueStatus::INVALID_OPERATION;
    }
    const auto operation = DeserializeProtocolOperation(exact_operation_bytes);
    if (!operation) return OperationRelayEnqueueStatus::INVALID_OPERATION;
    const auto operation_id = ComputeOperationId(*operation);
    if (!operation_id || operation_id->IsNull()) return OperationRelayEnqueueStatus::INVALID_OPERATION;

    std::lock_guard lock{m_mutex};
    if (!m_finalizer_session) return OperationRelayEnqueueStatus::FINALIZER_UNAVAILABLE;
    if (m_queued_ids.contains(*operation_id)) return OperationRelayEnqueueStatus::DUPLICATE;
    if (m_queue.size() >= m_max_operations || exact_operation_bytes.size() >
        m_max_queued_bytes - std::min(m_queued_bytes, m_max_queued_bytes)) {
        return OperationRelayEnqueueStatus::QUEUE_FULL;
    }
    m_queue.push_back({*operation_id, {exact_operation_bytes.begin(), exact_operation_bytes.end()}});
    m_queued_ids.insert(*operation_id);
    m_queued_bytes += exact_operation_bytes.size();
    return OperationRelayEnqueueStatus::QUEUED;
}

std::optional<RelayedOperation> OperationRelay::Peek(const FinalizerSession session) const
{
    std::lock_guard lock{m_mutex};
    if (!m_finalizer_session || *m_finalizer_session != session || m_queue.empty()) return std::nullopt;
    return m_queue.front();
}

bool OperationRelay::Acknowledge(const FinalizerSession session, const uint256& operation_id)
{
    std::lock_guard lock{m_mutex};
    if (!m_finalizer_session || *m_finalizer_session != session || m_queue.empty() ||
        m_queue.front().operation_id != operation_id) return false;
    m_queued_bytes -= m_queue.front().exact_bytes.size();
    m_queued_ids.erase(operation_id);
    m_queue.pop_front();
    return true;
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
