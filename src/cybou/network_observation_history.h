// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_NETWORK_OBSERVATION_HISTORY_H
#define CYBOU_NETWORK_OBSERVATION_HISTORY_H
#include <cybou/p2p/observation_groups.h>
#include <cybou/observation_history.h>
#include <deque>
namespace cybou {
// Gauge samples, not five-second transfer/CPU averages. No reporter identifiers.
struct NetworkObservationPoint {
    uint64_t end_elapsed_ms{0}, cohort_revision{0};
    bool clock_valid{true};
    size_t fresh_groups{0}, selected_groups{0}, missing_groups{0};
    p2p::ObservationStorageTotals storage;
    p2p::ObservationTrafficTotals traffic;
    p2p::ObservationCpuTotals cpu;
    uint32_t max_receipt_age_ms{0};
};
// One runtime-owned bounded ring, fed by the existing background collector.
// Reads expire old points but never sample/backfill or perform network I/O.
class NetworkObservationHistory {
public:
    using Clock = std::chrono::steady_clock;
    explicit NetworkObservationHistory(Clock::time_point start = Clock::now()) : m_start{start} {}
    void Observe(const p2p::ObservationGroupSnapshot& group, std::optional<Clock::time_point> time = std::nullopt)
    {
        std::lock_guard lock{m_mutex};
        const auto at = time.value_or(Clock::now());
        if (!Advance(at)) return;
        if (m_sampled && at - *m_sampled < std::chrono::milliseconds{OBSERVATION_INTERVAL_MS}) return;
        NetworkObservationPoint point;
        point.end_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(at - m_start).count();
        point.cohort_revision = group.cohort_revision; point.clock_valid = group.clock_valid;
        if (group.clock_valid) {
            point.fresh_groups = group.fresh_remote_groups; point.selected_groups = group.selected_remote_groups;
            point.missing_groups = group.missing_remote_groups;
            point.storage = group.storage; point.traffic = group.traffic; point.cpu = group.cpu;
            for (const auto& row : group.reports) if (!row.local_loopback)
                point.max_receipt_age_ms = std::max(point.max_receipt_age_ms, row.receipt_age_ms);
        }
        m_points.push_back(point); m_sampled = at;
        if (m_points.size() > MAX_OBSERVATION_POINTS) m_points.pop_front();
    }
    std::vector<NetworkObservationPoint> Snapshot(std::optional<Clock::time_point> time = std::nullopt)
    {
        std::lock_guard lock{m_mutex};
        const auto at = time.value_or(Clock::now());
        if (!Advance(at)) return {};
        return {m_points.begin(), m_points.end()};
    }
private:
    bool Advance(Clock::time_point at)
    {
        if (at < m_start || (m_last && at < *m_last)) {
            m_points.clear(); m_sampled.reset(); m_start = at; m_last = at; return false;
        }
        m_last = at;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(at - m_start).count();
        while (!m_points.empty() && static_cast<uint64_t>(elapsed) - m_points.front().end_elapsed_ms >= 900000) m_points.pop_front();
        return true;
    }
    std::mutex m_mutex;
    Clock::time_point m_start;
    std::optional<Clock::time_point> m_sampled, m_last;
    std::deque<NetworkObservationPoint> m_points;
};
}
#endif
