// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/observation_cache.h>
#include <cybou/binary_codec.h>
namespace cybou {
void ObservationCache::Publish(ObservationReport report, Clock::time_point sampled)
{
    report.network_binding = m_binding;
    report.challenge = {};
    report.cache_age_ms = 0;
    const auto bytes = EncodeObservationReport(report);
    std::lock_guard lock{m_mutex};
    std::copy(bytes.begin(), bytes.end(), m_payload.begin());
    m_sampled = sampled;
}
void ObservationCache::Clear()
{
    std::lock_guard lock{m_mutex};
    m_sampled.reset(); m_payload = {};
}
ObservationCache::Payload ObservationCache::Read(const ObservationBytes32& challenge, Clock::time_point now) const
{
    Payload bytes{};
    std::optional<Clock::time_point> sampled;
    {
        std::lock_guard lock{m_mutex};
        bytes = m_payload; sampled = m_sampled;
    }
    const bool fresh = sampled && now >= *sampled && now - *sampled <= std::chrono::seconds{5};
    const uint32_t age = fresh ? static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - *sampled).count()) : 5001;
    if (!fresh) std::fill(bytes.begin() + 68, bytes.end(), 0);
    const auto put32 = [&](size_t offset, uint32_t value) {
        for (size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    std::copy(m_binding.begin(), m_binding.end(), bytes.begin());
    std::copy(challenge.begin(), challenge.end(), bytes.begin() + 32);
    put32(64, age);
    // CPU age is relative to receipt of this reply, not frozen at cache refresh.
    if (bytes[163]) {
        const auto cpu_age = ReadLittleEndian<uint32_t>(std::span{bytes}.subspan(176, 4));
        if (cpu_age + age > 60000) std::fill(bytes.begin() + 163, bytes.begin() + 182, 0);
        else put32(176, cpu_age + age);
    }
    return bytes;
}
ObservationCollector::ObservationCollector(ObservationBytes32 binding, Source source)
    : m_cache{binding}, m_worker{[this, source = std::move(source)](std::stop_token stop) {
        while (!stop.stop_requested()) {
            const auto started = ObservationCache::Clock::now();
            try { m_cache.Publish(source(), started); }
            catch (...) { m_cache.Clear(); } // Local collection failure is Unknown; no raw diagnostics log.
            std::unique_lock lock{m_wait_mutex};
            const auto next = started + std::chrono::seconds{5};
            // Skip missed refreshes; a slow source must not trigger a catch-up burst.
            m_wait.wait_until(lock, stop, next > ObservationCache::Clock::now() ? next :
                ObservationCache::Clock::now() + std::chrono::seconds{5}, [] { return false; });
        }
    }} {}
ObservationCollector::~ObservationCollector()
{
    m_worker.request_stop(); m_wait.notify_all();
    m_worker.join();
}
}
