// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_P2P_INGRESS_BUDGET_H
#define CYBOU_P2P_INGRESS_BUDGET_H
#include <chrono>
#include <array>
#include <memory>
#include <map>
#include <mutex>
#include <string>
namespace cybou::p2p {
/** Local, bounded CPU admission policy; never changes global Identity Authority. */
class IngressBudget {
    struct Transfers {
        std::mutex mutex;
        std::map<std::string, size_t> active;
        size_t total{0};
    };
    struct TransferLease {
        std::shared_ptr<Transfers> state;
        std::string address;
        bool acquired{false};
        ~TransferLease() {
            if (!acquired) return;
            std::lock_guard lock(state->mutex);
            auto it = state->active.find(address);
            if (--it->second == 0) state->active.erase(it);
            --state->total;
        }
    };
public:
    enum class Work { CONNECTION, OPERATION, STORAGE_PUT, STORAGE_GET, STORAGE_PROOF };

    /** At most two simultaneous storage requests per IP, eight per node. */
    std::shared_ptr<void> AcquireStorageTransfer(const std::string& address) {
        auto lease = std::make_shared<TransferLease>();
        lease->state = m_transfers;
        lease->address = address;
        std::lock_guard lock(m_transfers->mutex);
        if (m_transfers->total >= 8) return {};
        auto& active = m_transfers->active[address];
        if (active >= 2) return {};
        ++active;
        ++m_transfers->total;
        lease->acquired = true;
        return lease;
    }

    bool Admit(const std::string& address, Work work, size_t bytes = 0,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now())
    {
        std::lock_guard lock(m_mutex);

        // Periodically purge expired peers at most once per second, or when capacity is reached.
        if (now - m_last_cleanup >= std::chrono::seconds{1} || m_peers.size() >= 4096) {
            m_last_cleanup = now;
            for (auto it = m_peers.begin(); it != m_peers.end();) {
                if (now - it->second.start >= std::chrono::seconds{60}) {
                    it = m_peers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        auto it = m_peers.find(address);
        if (it == m_peers.end()) {
            if (m_peers.size() >= 4096) return false;
            it = m_peers.emplace(address, Window{now}).first;
        } else if (now - it->second.start >= std::chrono::seconds{60}) {
            // Expired window for this specific peer: reset in O(1).
            it->second = Window{now};
        }

        auto& w = it->second;
        // Sliding refill with no idle credit beyond one small burst.
        const auto second = std::chrono::duration_cast<std::chrono::seconds>(now - w.start).count();
        if (second != w.second) {
            w.second = second;
            w.operations_second = 0;
            w.bytes_second = 0;
            w.connections_second = 0;
            w.storage_second.fill(0);
            w.storage_bytes_second = 0;
        }
        if (work == Work::CONNECTION) {
            if (w.connections_second >= 4 || w.connections >= 60) return false;
            ++w.connections_second;
            ++w.connections;
            return true;
        }
        if (work != Work::OPERATION) {
            const auto kind = static_cast<size_t>(work) - static_cast<size_t>(Work::STORAGE_PUT);
            if (kind >= w.storage.size()) return false;
            const bool proof = work == Work::STORAGE_PROOF;
            if (w.storage_second[kind] >= (proof ? 8U : 64U) ||
                w.storage[kind] >= (proof ? 120U : 2048U) ||
                bytes > (32ULL << 20) - w.storage_bytes_second ||
                bytes > (512ULL << 20) - w.storage_bytes) return false;
            ++w.storage_second[kind];
            ++w.storage[kind];
            w.storage_bytes_second += bytes;
            w.storage_bytes += bytes;
            return true;
        }
        if (w.operations_second >= 8 || w.operations >= 120 || bytes > (1U << 20) - w.bytes_second) return false;
        ++w.operations_second;
        ++w.operations;
        w.bytes_second += bytes;
        return true;
    }

private:
    struct Window {
        std::chrono::steady_clock::time_point start;
        int64_t second{0};
        size_t operations{0};
        size_t operations_second{0};
        size_t connections{0};
        size_t connections_second{0};
        size_t bytes_second{0};
        std::array<size_t, 3> storage{}, storage_second{};
        size_t storage_bytes{0}, storage_bytes_second{0};
    };

    std::mutex m_mutex;
    std::shared_ptr<Transfers> m_transfers{std::make_shared<Transfers>()};
    std::chrono::steady_clock::time_point m_last_cleanup{};
    std::map<std::string, Window> m_peers;
};
}
#endif
