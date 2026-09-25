// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_OPERATION_POOL_H
#define CYBOU_OPERATION_POOL_H

#include <cybou/protocol_operation.h>
#include <cybou/state_store.h>

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace cybou {

inline constexpr size_t MAX_PENDING_OPERATIONS{256};
inline constexpr size_t MAX_PENDING_OPERATION_BYTES{8U * 1024U * 1024U};
inline constexpr size_t MAX_PEER_PENDING_OPERATIONS{32};
inline constexpr size_t MAX_PEER_PENDING_BYTES{1U * 1024U * 1024U};

enum class PoolAdmission { ACCEPTED, ALREADY_PENDING, ALREADY_FINALIZED, REJECTED };

struct OperationPoolLimits {
    size_t max_count{MAX_PENDING_OPERATIONS};
    size_t max_bytes{MAX_PENDING_OPERATION_BYTES};
    size_t max_peer_count{MAX_PEER_PENDING_OPERATIONS};
    size_t max_peer_bytes{MAX_PEER_PENDING_BYTES};
};

class OperationPool
{
public:
    explicit OperationPool(CybouStateStore& store, OperationPoolLimits limits = {})
        : m_store{store}, m_limits{limits} {}

    PoolAdmission Admit(const ProtocolOperation& operation,
                        std::optional<std::string> source_peer = std::nullopt);
    std::vector<ProtocolOperation> Snapshot() const;
    void Revalidate();
    void Clear();
    size_t Size() const { return m_entries.size(); }
    size_t Bytes() const { return m_bytes; }
    bool Contains(const uint256& id) const { return m_ids.contains(id); }

private:
    struct Entry {
        ProtocolOperation operation;
        uint256 id;
        size_t bytes;
        std::optional<std::string> source_peer;
    };
    CybouStateStore& m_store;
    const OperationPoolLimits m_limits;
    std::vector<Entry> m_entries;
    std::set<uint256> m_ids;
    size_t m_bytes{0};
};

} // namespace cybou

#endif // CYBOU_OPERATION_POOL_H
