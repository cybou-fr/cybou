// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_OPERATION_RELAY_H
#define CYBOU_OPERATION_RELAY_H

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

enum class OperationRelayEnqueueStatus : uint8_t {
    QUEUED,
    DUPLICATE,
    QUEUE_FULL,
    INVALID_OPERATION,
};

struct RelayedOperation {
    uint256 operation_id;
    std::vector<unsigned char> exact_bytes;
};

/**
 * Bounded, volatile operation queue for hop-by-hop P2P relay.
 * Finalizer route handles are local process tokens and are never serialized.
 */
class OperationRelay final {
public:
    using FinalizerSession = uint64_t;

    explicit OperationRelay(size_t max_operations = 256,
        size_t max_queued_bytes = 8U * 1024U * 1024U);

    /** Caller must invoke this only after verifying the genesis PoA proof for a live session. */
    std::optional<FinalizerSession> AttachAuthenticatedFinalizer();
    /** Disconnect removes the live route; queued operations remain volatile and can use another hop. */
    void DetachFinalizer(FinalizerSession session);
    bool HasAuthenticatedFinalizer() const;

    /** allow_seen_retry lets a submitting origin retry bytes already forwarded from this queue. */
    OperationRelayEnqueueStatus Enqueue(std::span<const unsigned char> exact_operation_bytes,
        bool allow_seen_retry = false);
    /** View the FIFO head until a connected full node accepts the operation. */
    std::optional<RelayedOperation> Peek() const;
    bool Acknowledge(const uint256& operation_id);
    bool HasQueued(const uint256& operation_id) const;
    void ForgetFinalized(const uint256& operation_id);
    size_t QueuedOperations() const;
    size_t QueuedBytes() const;

private:
    const size_t m_max_operations;
    const size_t m_max_queued_bytes;
    const size_t m_seen_limit;
    mutable std::mutex m_mutex;
    FinalizerSession m_next_session{1};
    std::optional<FinalizerSession> m_finalizer_session;
    std::deque<RelayedOperation> m_queue;
    std::set<uint256> m_queued_ids;
    std::deque<uint256> m_seen_order;
    std::set<uint256> m_seen_ids;
    size_t m_queued_bytes{0};
};

} // namespace cybou

#endif // CYBOU_OPERATION_RELAY_H
