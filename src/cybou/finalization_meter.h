// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_FINALIZATION_METER_H
#define CYBOU_FINALIZATION_METER_H
#include <array>
#include <chrono>
#include <cstdint>
namespace cybou {
/// Local observation provenance only; never a wire field or consensus role.
enum class BlockObservation { HISTORY, ANNOUNCEMENT, LOCAL_PRODUCTION };
struct FinalizationWindow {
    uint64_t window_ms{0}, observed_operations{0}, local_produced_operations{0}, history_operations{0};
    bool complete{false};
};
struct FinalizationDiagnostics {
    std::array<FinalizationWindow, 3> windows{{{60000}, {300000}, {900000}}};
    uint64_t observed_total{0}, local_produced_total{0}, history_total{0};
};
/// Fixed local commit history; all access serialized by the runtime chain mutex.
class FinalizationMeter {
public:
    using Clock = std::chrono::steady_clock;
    explicit FinalizationMeter(Clock::time_point started = Clock::now());
    void Record(uint64_t operations, BlockObservation source, Clock::time_point now = Clock::now());
    void Reset(Clock::time_point now = Clock::now());
    FinalizationDiagnostics Snapshot(Clock::time_point now = Clock::now()) const;
private:
    uint64_t Second(Clock::time_point now) const;
    struct Bucket { uint64_t second{0}, observed{0}, produced{0}, history{0}; bool valid{false}; };
    Clock::time_point m_started;
    std::array<Bucket, 901> m_buckets{};
    uint64_t m_observed{0}, m_produced{0}, m_history{0};
};
} // namespace cybou
#endif
