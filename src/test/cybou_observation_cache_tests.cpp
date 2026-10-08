// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/observation_cache.h>
#include <cybou/traffic_meter.h>
#include <boost/test/unit_test.hpp>
#include <atomic>
#include <future>
namespace {
using namespace cybou;
using Clock = ObservationCache::Clock;
ObservationReport Sample()
{
    ObservationReport r;
    r.cursor.known = true; r.cursor.height = 42; r.cursor.tip.fill(17);
    r.storage = {true, uint64_t{15} << 30, 7, uint64_t{10} << 30, 8};
    r.traffic = {true, 60000, 1, 2};
    r.cpu = {true, 4, 60000, 12, 58000, 100};
    r.memory = {true, 500};
    return r;
}
}
BOOST_AUTO_TEST_SUITE(cybou_observation_cache_tests)
BOOST_AUTO_TEST_CASE(startup_expiry_clear_and_clock_regression_are_unknown)
{
    ObservationBytes32 binding{}, nonce{}; binding.fill(31); nonce.fill(47);
    ObservationCache cache{binding}; const auto start = Clock::time_point{};
    const auto check_unknown = [&](Clock::time_point now) {
        const auto r = DecodeObservationReport(cache.Read(nonce, now));
        BOOST_CHECK(r.network_binding == binding && r.challenge == nonce);
        BOOST_CHECK_EQUAL(r.cache_age_ms, 5001U);
        BOOST_CHECK(!r.cursor.known && !r.storage.known && !r.traffic.known && !r.cpu.known && !r.memory.known);
    };
    check_unknown(start);
    auto sample = Sample(); sample.challenge.fill(99); sample.network_binding.fill(88); sample.cache_age_ms = 123;
    cache.Publish(sample, start);
    const auto fresh = DecodeObservationReport(cache.Read(nonce, start));
    BOOST_CHECK(fresh.network_binding == binding && fresh.challenge == nonce);
    BOOST_CHECK_EQUAL(fresh.cache_age_ms, 0U); BOOST_CHECK_EQUAL(fresh.cursor.height, 42U);
    const auto edge = DecodeObservationReport(cache.Read(nonce, start + std::chrono::seconds{5}));
    BOOST_CHECK_EQUAL(edge.cache_age_ms, 5000U); BOOST_CHECK(edge.storage.known);
    check_unknown(start + std::chrono::seconds{5} + std::chrono::nanoseconds{1});
    check_unknown(start - std::chrono::nanoseconds{1});
    cache.Clear(); check_unknown(start);
}
BOOST_AUTO_TEST_CASE(cpu_age_advances_and_expires_without_invalidating_other_metrics)
{
    ObservationCache cache{{}}; const auto start = Clock::time_point{};
    cache.Publish(Sample(), start);
    auto r = DecodeObservationReport(cache.Read({}, start + std::chrono::seconds{2}));
    BOOST_CHECK(r.cpu.known); BOOST_CHECK_EQUAL(r.cpu.age_ms, 60000U);
    r = DecodeObservationReport(cache.Read({}, start + std::chrono::milliseconds{2001}));
    BOOST_CHECK(!r.cpu.known); BOOST_CHECK(r.storage.known && r.cursor.known && r.memory.known);
    r = DecodeObservationReport(cache.Read({}, start + std::chrono::milliseconds{1000}));
    BOOST_CHECK_EQUAL(r.cpu.age_ms, 59000U); // Reads never mutate the cached CPU baseline.
}
BOOST_AUTO_TEST_CASE(publication_is_atomic_under_concurrent_reads)
{
    ObservationCache cache{{}}; const auto start = Clock::time_point{};
    std::atomic_bool invalid{false};
    std::jthread writer{[&] {
        for (uint64_t i = 0; i < 2000; ++i) {
            auto r = Sample(); r.cursor.height = i; r.memory.resident_mib = i;
            cache.Publish(r, start);
        }
    }};
    for (unsigned i = 0; i < 2000; ++i) {
        const auto r = DecodeObservationReport(cache.Read({}, start));
        if (r.cursor.known && r.cursor.height != r.memory.resident_mib) invalid = true;
    }
    writer.join(); BOOST_CHECK(!invalid);
}
BOOST_AUTO_TEST_CASE(blocked_source_does_not_block_reads_and_stop_interrupts_wait)
{
    std::promise<void> entered, release;
    auto gate = release.get_future().share(); auto entered_future = entered.get_future();
    std::atomic_uint calls{0};
    auto collector = std::make_unique<ObservationCollector>(ObservationBytes32{}, [&] {
        ++calls; entered.set_value(); gate.wait(); return Sample();
    });
    const bool began = entered_future.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
    // Release before assertions on failure so cleanup cannot strand the worker.
    if (!began) release.set_value();
    BOOST_REQUIRE(began);
    for (unsigned i = 0; i < 20; ++i) {
        const auto r = DecodeObservationReport(collector->Read({}));
        BOOST_CHECK_EQUAL(r.cache_age_ms, 5001U);
    }
    BOOST_CHECK_EQUAL(calls.load(), 1U);
    release.set_value();
    const auto deadline = Clock::now() + std::chrono::seconds{2};
    while (!DecodeObservationReport(collector->Read({})).memory.known && Clock::now() < deadline)
        std::this_thread::yield();
    BOOST_CHECK(DecodeObservationReport(collector->Read({})).memory.known);
    const auto stopping = Clock::now(); collector.reset();
    BOOST_CHECK(Clock::now() - stopping < std::chrono::seconds{2});
}
BOOST_AUTO_TEST_CASE(source_failure_returns_unknown_without_read_triggered_retry)
{
    std::atomic_uint calls{0};
    ObservationCollector collector{{}, [&]() -> ObservationReport { ++calls; throw std::runtime_error{"fixture failure"}; }};
    const auto deadline = Clock::now() + std::chrono::seconds{2};
    while (!calls && Clock::now() < deadline) std::this_thread::yield();
    BOOST_CHECK_EQUAL(calls.load(), 1U);
    for (unsigned i = 0; i < 20; ++i) BOOST_CHECK_EQUAL(DecodeObservationReport(collector.Read({})).cache_age_ms, 5001U);
    BOOST_CHECK_EQUAL(calls.load(), 1U);
}
BOOST_AUTO_TEST_CASE(narrow_traffic_window_matches_diagnostics_without_history)
{
    const auto start = Clock::time_point{}; TrafficMeter traffic{start};
    traffic.Record(12345, 67890, start + std::chrono::seconds{5});
    BOOST_CHECK_EQUAL(traffic.WindowSnapshot(start).window_ms, 0U);
    const auto narrow = traffic.WindowSnapshot(start + std::chrono::seconds{60});
    const auto full = traffic.Snapshot(start + std::chrono::seconds{60});
    BOOST_CHECK(narrow.history.empty()); BOOST_CHECK(!full.history.empty());
    BOOST_CHECK_EQUAL(narrow.window_ms, full.window_ms);
    BOOST_CHECK_EQUAL(narrow.window_received_bytes, full.window_received_bytes);
    BOOST_CHECK_EQUAL(narrow.window_sent_bytes, full.window_sent_bytes);
}
BOOST_AUTO_TEST_SUITE_END()
