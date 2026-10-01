// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BOOTSTRAP_OPERATION_RELAY_H
#define CYBOU_BOOTSTRAP_OPERATION_RELAY_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace cybou {

enum class BootstrapRelayEnqueueStatus : uint8_t {
    QUEUED,
    DUPLICATE,
    FINALIZER_UNAVAILABLE,
    QUEUE_FULL,
    INVALID_OPERATION,
};

struct BootstrapRelayOperation {
    uint256 operation_id;
    std::vector<unsigned char> exact_bytes;
};

/**
 * Bounded, volatile operation relay for a single authenticated finalizer session.
 * Session handles are local process tokens and are never serialized or persisted.
 */
class BootstrapOperationRelay final {
public:
    using FinalizerSession = uint64_t;

    explicit BootstrapOperationRelay(size_t max_operations = 256,
        size_t max_queued_bytes = 8U * 1024U * 1024U);

    /** Caller must invoke this only after verifying the genesis PoA proof for a live session. */
    std::optional<FinalizerSession> AttachAuthenticatedFinalizer();
    /** Disconnect removes the live route and drops all unacknowledged volatile operations. */
    void DetachFinalizer(FinalizerSession session);
    bool HasAuthenticatedFinalizer() const;

    BootstrapRelayEnqueueStatus Enqueue(std::span<const unsigned char> exact_operation_bytes);
    /** View the FIFO head until the authenticated finalizer acknowledges that operation. */
    std::optional<BootstrapRelayOperation> Peek(FinalizerSession session) const;
    bool Acknowledge(FinalizerSession session, const uint256& operation_id);
    size_t QueuedOperations() const;
    size_t QueuedBytes() const;

private:
    const size_t m_max_operations;
    const size_t m_max_queued_bytes;
    mutable std::mutex m_mutex;
    FinalizerSession m_next_session{1};
    std::optional<FinalizerSession> m_finalizer_session;
    std::deque<BootstrapRelayOperation> m_queue;
    std::set<uint256> m_queued_ids;
    size_t m_queued_bytes{0};
};

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_OPERATION_RELAY_H
