// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/process_cpu.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <time.h>
#include <unistd.h>
#endif
namespace cybou {
std::optional<ProcessCpuReading> ReadProcessCpu()
{
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return std::nullopt;
    const auto value = [](FILETIME time) { return (uint64_t{time.dwHighDateTime} << 32) | time.dwLowDateTime; };
    const auto processors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (!processors) return std::nullopt;
    return ProcessCpuReading{(value(kernel) + value(user)) * 100, processors};
#elif defined(__linux__)
    timespec time{};
    const auto processors = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time) != 0 || processors <= 0) return std::nullopt;
    return ProcessCpuReading{static_cast<uint64_t>(time.tv_sec) * 1000000000ULL + static_cast<uint64_t>(time.tv_nsec), static_cast<uint32_t>(processors)};
#else
    return std::nullopt;
#endif
}
ProcessCpuDiagnostics ProcessCpuMeter::Sample()
{
    std::lock_guard lock{m_mutex};
    const auto reading = ReadProcessCpu();
    return ObserveLocked(reading, Clock::now());
}
ProcessCpuDiagnostics ProcessCpuMeter::Observe(std::optional<ProcessCpuReading> reading, Clock::time_point now)
{
    std::lock_guard lock{m_mutex};
    return ObserveLocked(reading, now);
}
ProcessCpuDiagnostics ProcessCpuMeter::ObserveLocked(std::optional<ProcessCpuReading> reading, Clock::time_point now)
{
    if (!reading || !reading->processors) {
        m_previous.reset(); m_window_start.reset(); m_mean = {}; m_intervals = 0;
        return {};
    }
    if (!m_previous || now < m_previous_time || reading->cpu_ns < m_previous->cpu_ns ||
        reading->processors != m_previous->processors) {
        m_previous = m_window_start = reading;
        m_previous_time = m_window_time = now;
        m_intervals = 0; m_mean = {};
        return {.processors = reading->processors};
    }
    auto result = m_mean;
    result.processors = reading->processors;
    if (result.mean_percent) result.mean_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_mean_time).count();
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_previous_time).count();
    // Displayed intervals use milliseconds; retain the baseline for sub-ms reads.
    if (elapsed < 1000000) return result;
    result.interval_percent = (reading->cpu_ns - m_previous->cpu_ns) * 100.0 / elapsed / reading->processors;
    result.interval_ms = elapsed / 1000000;
    ++m_intervals;
    const auto window = std::chrono::duration_cast<std::chrono::nanoseconds>(now - m_window_time).count();
    if (window >= 60000000000LL) {
        result.mean_percent = (reading->cpu_ns - m_window_start->cpu_ns) * 100.0 / window / reading->processors;
        result.mean_window_ms = window / 1000000;
        result.mean_intervals = m_intervals;
        result.mean_age_ms = 0;
        m_mean = result; m_mean.interval_percent.reset(); m_mean.interval_ms = 0;
        m_mean_time = now;
        m_window_start = reading; m_window_time = now; m_intervals = 0;
    }
    m_previous = reading; m_previous_time = now;
    return result;
}
}
