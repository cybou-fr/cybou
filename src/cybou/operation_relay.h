// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_OPERATION_RELAY_H
#define CYBOU_OPERATION_RELAY_H

#include <cybou/hash256.h>

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
    cybou::Hash256 operation_id;
    std::vector<unsigned char> exact_bytes;
};

/** Bounded, volatile queue of locally executed operations for hop-by-hop P2P relay. */
class OperationRelay final {
public:
    explicit OperationRelay(size_t max_operations = 256,
        size_t max_queued_bytes = 8U * 1024U * 1024U);

    /** allow_seen_retry lets a submitting origin retry bytes already forwarded from this queue. */
    OperationRelayEnqueueStatus Enqueue(std::span<const unsigned char> exact_operation_bytes,
        bool allow_seen_retry = false);
    /** View the FIFO head until a connected full node accepts the operation. */
    std::optional<RelayedOperation> Peek() const;
    /** Reserve the FIFO head for one in-flight peer transfer. */
    std::optional<RelayedOperation> Claim();
    /** Release a failed/incomplete transfer so another peer can retry it. */
    void Release(const cybou::Hash256& operation_id);
    bool Acknowledge(const cybou::Hash256& operation_id);
    bool HasQueued(const cybou::Hash256& operation_id) const;
    void ForgetFinalized(const cybou::Hash256& operation_id);
    size_t QueuedOperations() const;
    size_t QueuedBytes() const;

private:
    const size_t m_max_operations;
    const size_t m_max_queued_bytes;
    const size_t m_seen_limit;
    mutable std::mutex m_mutex;
    std::deque<RelayedOperation> m_queue;
    std::optional<cybou::Hash256> m_claimed_id;
    std::set<cybou::Hash256> m_queued_ids;
    std::deque<cybou::Hash256> m_seen_order;
    std::set<cybou::Hash256> m_seen_ids;
    size_t m_queued_bytes{0};
};

} // namespace cybou

#endif // CYBOU_OPERATION_RELAY_H
