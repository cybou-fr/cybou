// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/finalization_meter.h>
namespace cybou {
FinalizationMeter::FinalizationMeter(Clock::time_point started) : m_started{started} {}
uint64_t FinalizationMeter::Second(Clock::time_point now) const
{
    return now < m_started ? 0 : static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now - m_started).count());
}
void FinalizationMeter::Reset(Clock::time_point now)
{
    m_started = now;
    m_buckets = {};
    m_observed = m_produced = m_history = 0;
}
void FinalizationMeter::Record(uint64_t operations, BlockObservation source, Clock::time_point now)
{
    const bool history = source == BlockObservation::HISTORY;
    const bool produced = source == BlockObservation::LOCAL_PRODUCTION;
    if (history) m_history += operations;
    else m_observed += operations;
    if (produced) m_produced += operations;
    const auto second = Second(now);
    auto& bucket = m_buckets[second % m_buckets.size()];
    if (bucket.valid && bucket.second > second) return;
    if (!bucket.valid || bucket.second != second) bucket = {second, 0, 0, 0, true};
    if (history) bucket.history += operations;
    else bucket.observed += operations;
    if (produced) bucket.produced += operations;
}
FinalizationDiagnostics FinalizationMeter::Snapshot(Clock::time_point now) const
{
    FinalizationDiagnostics result;
    result.observed_total = m_observed;
    result.local_produced_total = m_produced;
    result.history_total = m_history;
    const auto second = Second(now);
    for (auto& window : result.windows) {
        const auto seconds = window.window_ms / 1000;
        if (second < seconds) continue;
        window.complete = true;
        for (const auto& bucket : m_buckets) {
            if (!bucket.valid || bucket.second < second - seconds || bucket.second >= second) continue;
            window.observed_operations += bucket.observed;
            window.local_produced_operations += bucket.produced;
            window.history_operations += bucket.history;
        }
    }
    return result;
}
} // namespace cybou
