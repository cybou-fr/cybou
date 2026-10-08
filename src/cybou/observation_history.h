// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_OBSERVATION_HISTORY_H
#define CYBOU_OBSERVATION_HISTORY_H
#include <algorithm>
#include <cstdint>
#include <vector>
namespace cybou {
/// Completed five-second intervals, measured relative to this collector's start/reset.
/// Primary/secondary are bytes (traffic) or operations (finalization), never rates.
struct ObservationPoint { uint64_t end_elapsed_ms{0}, primary{0}, secondary{0}; };
inline constexpr uint64_t OBSERVATION_INTERVAL_MS{5000};
inline constexpr uint64_t MAX_OBSERVATION_POINTS{180};
template <typename Buckets, typename Values>
std::vector<ObservationPoint> BuildObservationHistory(uint64_t second, const Buckets& buckets, Values values)
{
    const auto count = std::min(second / 5, MAX_OBSERVATION_POINTS);
    const auto end = second / 5 * 5;
    const auto first = end - count * 5;
    std::vector<ObservationPoint> result(static_cast<std::size_t>(count));
    for (uint64_t i = 0; i < count; ++i) result[i].end_elapsed_ms = (first + (i + 1) * 5) * 1000;
    for (const auto& bucket : buckets) {
        if (!bucket.valid || bucket.second < first || bucket.second >= end) continue;
        auto& point = result[(bucket.second - first) / 5];
        const auto value = values(bucket);
        point.primary += value.first;
        point.secondary += value.second;
    }
    return result;
}
} // namespace cybou
#endif
