// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_OBSERVATION_CACHE_H
#define CYBOU_OBSERVATION_CACHE_H
#include <cybou/observation_report.h>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
namespace cybou {
class ObservationCache {
public:
    using Clock = std::chrono::steady_clock;
    using Payload = std::array<unsigned char, OBSERVATION_REPORT_BYTES>;
    explicit ObservationCache(ObservationBytes32 binding) : m_binding{binding} {}
    // Collection/validation happens outside the cache lock. Sample time is the
    // beginning of collection, so a slow collector cannot renew stale values.
    void Publish(ObservationReport report, Clock::time_point sampled);
    void Clear();
    Payload Read(const ObservationBytes32& challenge, Clock::time_point now = Clock::now()) const;
private:
    const ObservationBytes32 m_binding;
    mutable std::mutex m_mutex;
    Payload m_payload{};
    std::optional<Clock::time_point> m_sampled;
};
// One runtime-owned worker; never schedules socket I/O or holds the cache lock
// while collecting. Destruction stops the wait and joins before source teardown.
class ObservationCollector {
public:
    using Source = std::function<ObservationReport()>;
    ObservationCollector(ObservationBytes32 binding, Source source);
    ~ObservationCollector();
    ObservationCache::Payload Read(const ObservationBytes32& challenge) const { return m_cache.Read(challenge); }
private:
    ObservationCache m_cache;
    std::mutex m_wait_mutex;
    std::condition_variable_any m_wait;
    std::jthread m_worker;
};
}
#endif
