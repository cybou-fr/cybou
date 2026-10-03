// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_OPERATION_RELAY_H
#define CYBOU_OPERATION_RELAY_H

/// \file
/// \brief FIFO-очередь точных байтов операций для hop-by-hop ретрансляции.

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

/// \brief Итог постановки операции в локальную очередь ретрансляции.
enum class OperationRelayEnqueueStatus : uint8_t {
    QUEUED,
    DUPLICATE,
    QUEUE_FULL,
    INVALID_OPERATION,
};

/// \brief Точные байты операции вместе с уже вычисленным OperationID.
struct RelayedOperation {
    cybou::Hash256 operation_id;
    std::vector<unsigned char> exact_bytes;
};

/// \brief Ограниченная RAM-очередь локально исполненных операций до подтверждения relay-пиром.
class OperationRelay final {
public:
    explicit OperationRelay(size_t max_operations = 256,
        size_t max_queued_bytes = 8U * 1024U * 1024U);

    /// \brief Добавляет точные canonical bytes операции; allow_seen_retry разрешает повтор от исходного отправителя.
    OperationRelayEnqueueStatus Enqueue(std::span<const unsigned char> exact_operation_bytes,
        bool allow_seen_retry = false);
    /// \brief Возвращает текущую голову FIFO без резервирования для передачи.
    std::optional<RelayedOperation> Peek() const;
    /// \brief Резервирует голову FIFO под одну активную передачу peer-to-peer.
    std::optional<RelayedOperation> Claim();
    /// \brief Снимает резерв после неуспешной/неполной отправки.
    void Release(const cybou::Hash256& operation_id);
    /// \brief Подтверждает успешную доставку головы FIFO и удаляет её из очереди.
    bool Acknowledge(const cybou::Hash256& operation_id);
    /// \brief Проверяет, удерживается ли операция в очереди ожидания.
    bool HasQueued(const cybou::Hash256& operation_id) const;
    /// \brief Забывает уже finalized операцию, если она ещё оставалась в relay-очереди.
    void ForgetFinalized(const cybou::Hash256& operation_id);
    size_t QueuedOperations() const;
    size_t QueuedBytes() const;

private:
    bool FitsQueueLimits(size_t bytes) const;
    void RememberSeen(const cybou::Hash256& operation_id);

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
