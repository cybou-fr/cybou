// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_P2P_OBSERVATION_GROUPS_H
#define CYBOU_P2P_OBSERVATION_GROUPS_H
#include <cybou/p2p/observation_exchange.h>
#include <vector>
namespace cybou::p2p {
struct ObservationRow {
    bool local_loopback{false};
    uint32_t receipt_age_ms{0}, declared_cache_age_ms{0};
    ObservationCursor cursor;
    ObservationStorage storage;
    ObservationTraffic traffic;
    ObservationCpu cpu;
    ObservationMemory memory;
};
struct ObservationStorageTotals {
    size_t contributors{0};
    std::optional<uint64_t> capacity_bytes, stored_copy_bytes, provider_budget_bytes, obligations_bytes;
    std::optional<double> utilization_percent;
};
struct ObservationTrafficTotals {
    size_t contributors{0};
    std::optional<uint64_t> received_bytes, sent_bytes;
    std::optional<double> received_bytes_per_second, sent_bytes_per_second;
};
struct ObservationCpuTotals {
    size_t contributors{0};
    std::optional<double> mean_percent;
    uint32_t min_window_ms{0}, max_window_ms{0}, min_age_ms{0}, max_age_ms{0};
};
// Declarations from a partial reporting set, never a node/host census or network
// ceiling. No addresses, handles, challenges, stable row IDs or memory/chain sum.
struct ObservationGroupSnapshot {
    bool clock_valid{true}, slots_full{false};
    uint64_t cohort_revision{0}; // Volatile chart segmentation marker, not a schema version.
    size_t selected_remote_groups{0}, fresh_remote_groups{0}, missing_remote_groups{0}, fresh_local_groups{0};
    std::vector<ObservationRow> reports;
    ObservationStorageTotals storage;
    ObservationTrafficTotals traffic;
    ObservationCpuTotals cpu;
};
// Bounded volatile store only. Select is called in the transaction owner's local
// session order; Record only after ObservationExchange accepted that exact reply.
// All addresses must come from the socket. No I/O, logs, persistence or forwarding.
class ObservationGroups {
public:
    using Clock = ObservationExchange::Clock;
    explicit ObservationGroups(ObservationBytes32 binding) : m_binding{binding} {}
    bool Select(const ObservationSession& session, const boost::asio::ip::address& address,
        std::optional<Clock::time_point> at = std::nullopt);
    bool Record(const ObservationSession& session, const boost::asio::ip::address& address,
        const ObservationReport& accepted, std::optional<Clock::time_point> at = std::nullopt);
    void Close(uint64_t handle, std::optional<Clock::time_point> at = std::nullopt);
    void Expire(std::optional<Clock::time_point> at = std::nullopt);
    ObservationGroupSnapshot Snapshot(std::optional<Clock::time_point> at = std::nullopt);
private:
    struct Slot {
        uint64_t handle{0};
        bool local{false};
        Clock::time_point activity;
        std::optional<Clock::time_point> received;
        ObservationRow metrics;
    };
    bool Eligible(const ObservationSession& session) const;
    bool Advance(Clock::time_point now);
    const ObservationBytes32 m_binding;
    std::mutex m_mutex;
    std::optional<Clock::time_point> m_last_time;
    std::map<std::string, Slot> m_slots; // <=32, including one collapsed loopback group
    uint64_t m_cohort_revision{0};
};
}
#endif
