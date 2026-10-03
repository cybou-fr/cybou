// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_OPERATION_POOL_H
#define CYBOU_OPERATION_POOL_H

/// \file
/// \brief Волатильный пул локально проверенных операций до PoA-финализации.

#include <cybou/protocol_operation.h>
#include <cybou/state_store.h>

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cybou {

inline constexpr size_t MAX_PENDING_OPERATIONS{256};
inline constexpr size_t MAX_PENDING_OPERATION_BYTES{8U * 1024U * 1024U};
inline constexpr size_t MAX_PEER_PENDING_OPERATIONS{32};
inline constexpr size_t MAX_PEER_PENDING_BYTES{1U * 1024U * 1024U};

/// \brief Результат допуска операции в локальный кандидатный пул.
enum class PoolAdmission { ACCEPTED, ALREADY_PENDING, ALREADY_FINALIZED, REJECTED };

/// \brief Лимиты RAM-пула и квоты на один источник ретрансляции.
struct OperationPoolLimits {
    size_t max_count{MAX_PENDING_OPERATIONS};
    size_t max_bytes{MAX_PENDING_OPERATION_BYTES};
    size_t max_peer_count{MAX_PEER_PENDING_OPERATIONS};
    size_t max_peer_bytes{MAX_PEER_PENDING_BYTES};
};

/// \brief Хранит только локально исполненные кандидаты поверх последнего finalized head.
class OperationPool
{
public:
    explicit OperationPool(CybouStateStore& store, OperationPoolLimits limits = {})
        : m_store{store}, m_limits{limits} {}

    /// \brief Исполняет и при успехе добавляет операцию в RAM-пул кандидатов.
    PoolAdmission Admit(const ProtocolOperation& operation,
                        std::optional<std::string> source_peer = std::nullopt);
    /// \brief Возвращает копию текущего порядка кандидатов для сборки блока.
    std::vector<ProtocolOperation> Snapshot() const;
    /// \brief Переисполняет все кандидаты на новом finalized head и возвращает удалённые OperationID.
    std::vector<cybou::Hash256> Revalidate();
    /// \brief Полностью очищает RAM-пул и статистику по источникам.
    void Clear();
    size_t Size() const { return m_entries.size(); }
    size_t Bytes() const { return m_bytes; }
    bool Contains(const cybou::Hash256& id) const { return m_ids.contains(id); }
    /// \brief Возвращает OperationID кандидатов в текущем порядке пула.
    std::vector<cybou::Hash256> Ids() const;

private:
    struct Entry {
        ProtocolOperation operation;
        cybou::Hash256 id;
        size_t bytes;
        std::optional<std::string> source_peer;
    };
    struct PeerUsage {
        size_t count{0};
        size_t bytes{0};
    };

    bool FitsGlobalLimits(size_t bytes) const;
    bool FitsPeerLimits(const std::string& peer, size_t bytes) const;
    void RecordPeerUsage(const std::optional<std::string>& peer, size_t bytes);

    CybouStateStore& m_store;
    const OperationPoolLimits m_limits;
    std::vector<Entry> m_entries;
    std::set<cybou::Hash256> m_ids;
    std::map<std::string, PeerUsage> m_peer_usage;
    size_t m_bytes{0};
};

} // namespace cybou

#endif // CYBOU_OPERATION_POOL_H
