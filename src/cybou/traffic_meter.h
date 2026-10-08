// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_TRAFFIC_METER_H
#define CYBOU_TRAFFIC_METER_H

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <utility>
#include <cybou/observation_history.h>

namespace cybou {
struct TrafficDiagnostics {
    std::vector<ObservationPoint> history;
    uint64_t received_bytes{0}, sent_bytes{0};
    uint64_t window_received_bytes{0}, window_sent_bytes{0}, window_ms{0};
};

/// Bounded passive byte windows. No peer identifiers.
class ByteRateMeter {
public:
    using Clock = std::chrono::steady_clock;
    explicit ByteRateMeter(Clock::time_point started = Clock::now()) : m_started{started} {}
    void Record(uint64_t received, uint64_t sent, Clock::time_point now = Clock::now())
    {
        std::lock_guard lock{m_mutex};
        const auto second = Second(now);
        auto& bucket = m_buckets[second % m_buckets.size()];
        m_received += received;
        m_sent += sent;
        // A delayed writer must not replace a newer bucket after a full ring turn.
        if (bucket.valid && bucket.second > second) return;
        if (!bucket.valid || bucket.second != second) bucket = {second, 0, 0, true};
        bucket.received += received;
        bucket.sent += sent;
    }
    TrafficDiagnostics Snapshot(Clock::time_point now = Clock::now()) const
    {
        return SnapshotImpl(now, true);
    }
    // Narrow report collection does not allocate/copy chart history.
    TrafficDiagnostics WindowSnapshot(Clock::time_point now = Clock::now()) const
    {
        return SnapshotImpl(now, false);
    }
private:
    TrafficDiagnostics SnapshotImpl(Clock::time_point now, bool history) const
    {
        std::lock_guard lock{m_mutex};
        TrafficDiagnostics result;
        result.received_bytes = m_received;
        result.sent_bytes = m_sent;
        const auto second = Second(now);
        if (history) result.history = BuildObservationHistory(second, m_buckets,
            [](const auto& bucket) { return std::pair{bucket.received, bucket.sent}; });
        if (second < 60) return result;
        result.window_ms = 60000;
        for (const auto& bucket : m_buckets) {
            if (bucket.valid && bucket.second >= second - 60 && bucket.second < second) {
                result.window_received_bytes += bucket.received;
                result.window_sent_bytes += bucket.sent;
            }
        }
        return result;
    }
private:
    uint64_t Second(Clock::time_point now) const
    {
        return now < m_started ? 0 : static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(now - m_started).count());
    }
    struct Bucket { uint64_t second{0}, received{0}, sent{0}; bool valid{false}; };
    const Clock::time_point m_started;
    mutable std::mutex m_mutex;
    // Retain the completed 15-minute history plus the current partial five-second interval.
    std::array<Bucket, 905> m_buckets{};
    uint64_t m_received{0}, m_sent{0};
};
struct StorageTransferDiagnostics {
    TrafficDiagnostics put, get;
};

/// Local frame traffic plus completed encrypted chunk transfers, kept separate.
class TrafficMeter : public ByteRateMeter {
public:
    explicit TrafficMeter(Clock::time_point started = Clock::now())
        : ByteRateMeter{started}, m_put{started}, m_get{started} {}
    void RecordPut(uint64_t received, uint64_t sent, Clock::time_point now = Clock::now())
    { m_put.Record(received, sent, now); }
    void RecordGet(uint64_t received, uint64_t sent, Clock::time_point now = Clock::now())
    { m_get.Record(received, sent, now); }
    StorageTransferDiagnostics StorageSnapshot(Clock::time_point now = Clock::now()) const
    { return {m_put.WindowSnapshot(now), m_get.WindowSnapshot(now)}; }
private:
    ByteRateMeter m_put, m_get;
};
} // namespace cybou
#endif
