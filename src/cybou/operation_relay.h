// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_OPERATION_RELAY_H
#define CYBOU_OPERATION_RELAY_H

/// \file
/// \brief FIFO-очередь точных байтов операций для hop-by-hop ретрансляции.

#include <cybou/hash256.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace cybou {

/// \brief Итог постановки операции в локальную очередь ретрансляции.
enum class OperationRelayEnqueueStatus : uint8_t {
    /// \brief Exact bytes операции поставлены в FIFO для последующей hop-by-hop отправки.
    QUEUED,
    /// \brief Такой OperationID уже удерживается в очереди или недавно был замечен и подавлен seen-cache.
    DUPLICATE,
    /// \brief Операция валидна, но очередь исчерпала лимиты RAM.
    QUEUE_FULL,
    /// \brief Переданные bytes не образуют canonical signed operation.
    INVALID_OPERATION,
};

/// \brief Точные байты операции вместе с уже вычисленным OperationID.
struct RelayedOperation {
    /// \brief Canonical OperationID exact bytes.
    cybou::Hash256 operation_id;
    /// \brief Exact signed bytes, которые нужно повторно передавать без пересериализации.
    std::vector<unsigned char> exact_bytes;
    /// rief Relay-PoW nonce, путешествующий вместе с exact bytes до финализации (DEC-273).
    uint64_t work_nonce{0};
};

/// \brief Ограниченная RAM-очередь локально исполненных операций до подтверждения relay-пиром.
/// \details Методы потокобезопасны: очередь сама сериализует доступ внутренним mutex.
class OperationRelay final {
public:
    /// \brief Создаёт ограниченную FIFO-очередь ретрансляции.
    /// \param max_operations Максимум операций в очереди.
    /// \param max_queued_bytes Максимум exact bytes, удерживаемых в очереди.
    /// \throws std::invalid_argument Если лимиты нулевые или меньше одной максимальной операции.
    explicit OperationRelay(size_t max_operations = 256,
        size_t max_queued_bytes = 8U * 1024U * 1024U);

    /// \brief Добавляет точные canonical bytes операции; allow_seen_retry разрешает повтор от исходного отправителя.
    /// \param exact_operation_bytes Exact canonical bytes signed operation.
    /// \param work_nonce Relay-PoW nonce, уже проверенный пулом.
    /// \param allow_seen_retry true разрешает обойти seen-cache для повторной попытки исходного отправителя.
    /// \return QUEUED, DUPLICATE, QUEUE_FULL или INVALID_OPERATION.
    /// \post При QUEUED exact bytes сохраняются без модификации до Acknowledge() или ForgetFinalized().
    OperationRelayEnqueueStatus Enqueue(std::span<const unsigned char> exact_operation_bytes,
        uint64_t work_nonce, bool allow_seen_retry = false);
    /// \brief Возвращает текущую голову FIFO без резервирования для передачи.
    /// \return Копия головы очереди или std::nullopt, если очередь пуста.
    std::optional<RelayedOperation> Peek(
        const std::function<bool(const cybou::Hash256&)>& skip = {}) const;
    /// \brief Резервирует голову FIFO под одну активную передачу peer-to-peer.
    /// \return Копия головы очереди или std::nullopt, если очередь пуста либо уже есть активный claim.
    /// \post До Release()/Acknowledge() новый Claim() не выдаст другую операцию.
    std::optional<RelayedOperation> Claim();
    /// \brief Снимает резерв после неуспешной/неполной отправки.
    /// \param operation_id OperationID, ранее полученный через Claim().
    /// \post При совпадении с активным claim очередь снова доступна для Claim().
    void Release(const cybou::Hash256& operation_id);
    /// \brief Подтверждает успешную доставку головы FIFO и удаляет её из очереди.
    /// \param operation_id OperationID, ранее полученный через Claim().
    /// \return true только если подтверждена именно текущая зарезервированная голова FIFO.
    bool Acknowledge(const cybou::Hash256& operation_id);
    /// \brief Проверяет, удерживается ли операция в очереди ожидания.
    /// \param operation_id Искомый OperationID.
    bool HasQueued(const cybou::Hash256& operation_id) const;
    /// \brief Забывает уже finalized операцию, если она ещё оставалась в relay-очереди.
    /// \param operation_id OperationID, который больше не нужно ретранслировать.
    void ForgetFinalized(const cybou::Hash256& operation_id);
    /// \brief Число операций, всё ещё удерживаемых в FIFO.
    size_t QueuedOperations() const;
    /// \brief Сумма exact bytes, удерживаемых в FIFO, байты.
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
