// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_PROCESS_CPU_H
#define CYBOU_PROCESS_CPU_H
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
namespace cybou {
struct ProcessCpuReading { uint64_t cpu_ns; uint32_t processors; };
std::optional<ProcessCpuReading> ReadProcessCpu();
struct ProcessCpuDiagnostics {
    std::optional<double> interval_percent, mean_percent;
    uint32_t processors{0};
    uint64_t interval_ms{0}, mean_window_ms{0}, mean_intervals{0}, mean_age_ms{0};
};
/// Cumulative OS process time / elapsed wall time / OS online logical processors.
/// Completed averaging windows last at least 60 seconds; no gap interpolation.
class ProcessCpuMeter {
public:
    using Clock = std::chrono::steady_clock;
    ProcessCpuDiagnostics Observe(std::optional<ProcessCpuReading> reading, Clock::time_point now);
    ProcessCpuDiagnostics Sample();
private:
    ProcessCpuDiagnostics ObserveLocked(std::optional<ProcessCpuReading> reading, Clock::time_point now);
    std::mutex m_mutex;
    std::optional<ProcessCpuReading> m_previous, m_window_start;
    Clock::time_point m_previous_time{}, m_window_time{}, m_mean_time{};
    uint64_t m_intervals{0};
    ProcessCpuDiagnostics m_mean;
};
}
#endif
