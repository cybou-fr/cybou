// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/p2p/observation_groups.h>
#include <boost/asio/ip/address.hpp>
#include <boost/test/unit_test.hpp>
#include <atomic>
#include <limits>
#include <thread>
namespace {
using namespace cybou;
using namespace cybou::p2p;
using Clock = ObservationGroups::Clock;
auto Ip(unsigned n) { return boost::asio::ip::make_address("192.168.1." + std::to_string(n)); }
ObservationSession Session(uint64_t handle) { return {handle, {}, true}; }
ObservationReport Report(uint64_t capacity_gib = 15, uint64_t stored_mib = 1024)
{
    ObservationReport r;
    const auto capacity = capacity_gib << 30;
    r.storage = {true, capacity, stored_mib, capacity / 3 * 2 + capacity % 3 * 2 / 3, 512};
    r.traffic = {true, 60000, 60, 120};
    r.cpu = {true, 4, 60000, 12, 0, 2500}; r.memory = {true, 100};
    r.cursor.known = true; r.cursor.height = 123;
    return r;
}
void Put(ObservationGroups& groups, uint64_t handle, unsigned ip, const ObservationReport& r, Clock::time_point now)
{
    BOOST_REQUIRE(groups.Select(Session(handle), Ip(ip), now));
    BOOST_REQUIRE(groups.Record(Session(handle), Ip(ip), r, now));
}
}
BOOST_AUTO_TEST_SUITE(cybou_observation_groups_tests)
BOOST_AUTO_TEST_CASE(one_selected_session_per_address_and_replacement_discards_report)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    auto report = Report(); report.challenge.fill(99);
    Put(groups, 1, 1, report, start);
    const auto before = groups.Snapshot(start);
    BOOST_CHECK_EQUAL(before.fresh_remote_groups, 1U);
    const auto mapped = boost::asio::ip::make_address("::ffff:192.168.1.1");
    BOOST_CHECK(!groups.Select(Session(2), mapped, start));
    BOOST_CHECK(!groups.Record(Session(2), mapped, Report(30), start));
    BOOST_CHECK(!groups.Select(Session(1), Ip(2), start)); // one session cannot alias addresses
    BOOST_CHECK_EQUAL(groups.Snapshot(start).cohort_revision, before.cohort_revision);
    groups.Close(1, start);
    BOOST_CHECK_EQUAL(groups.Snapshot(start).fresh_remote_groups, 0U);
    BOOST_REQUIRE(groups.Select(Session(2), mapped, start));
    const auto replaced = groups.Snapshot(start);
    BOOST_CHECK_EQUAL(replaced.missing_remote_groups, 1U); BOOST_CHECK(replaced.reports.empty());
    BOOST_CHECK(replaced.cohort_revision != before.cohort_revision);
    BOOST_CHECK(!groups.Record(Session(1), Ip(1), report, start));
    BOOST_REQUIRE(groups.Record(Session(2), mapped, Report(30), start));
    BOOST_CHECK_EQUAL(*groups.Snapshot(start).storage.capacity_bytes, uint64_t{30} << 30);
}
BOOST_AUTO_TEST_CASE(loopback_addresses_collapse_and_do_not_enter_remote_totals)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    const auto local = boost::asio::ip::make_address("127.0.0.2");
    BOOST_REQUIRE(groups.Select(Session(1), local, start));
    BOOST_REQUIRE(groups.Record(Session(1), local, Report(100), start));
    BOOST_CHECK(!groups.Select(Session(2), boost::asio::ip::make_address("::1"), start));
    BOOST_CHECK(!groups.Select(Session(3), boost::asio::ip::make_address("::ffff:127.0.0.1"), start));
    auto snapshot = groups.Snapshot(start);
    BOOST_CHECK_EQUAL(snapshot.fresh_local_groups, 1U); BOOST_CHECK_EQUAL(snapshot.fresh_remote_groups, 0U);
    BOOST_CHECK(!snapshot.storage.capacity_bytes && !snapshot.cpu.mean_percent && !snapshot.traffic.received_bytes);
    BOOST_REQUIRE_EQUAL(snapshot.reports.size(), 1U); BOOST_CHECK(snapshot.reports.front().local_loopback);
    Put(groups, 4, 1, Report(), start);
    snapshot = groups.Snapshot(start);
    BOOST_CHECK_EQUAL(snapshot.storage.contributors, 1U); BOOST_CHECK_EQUAL(*snapshot.storage.capacity_bytes, uint64_t{15} << 30);
}
BOOST_AUTO_TEST_CASE(receipt_expiry_is_not_extended_by_reads_or_session_selection)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    Put(groups, 1, 1, Report(), start);
    BOOST_REQUIRE(groups.Select(Session(1), Ip(1), start + std::chrono::seconds{89}));
    BOOST_CHECK_EQUAL(groups.Snapshot(start + std::chrono::seconds{89}).fresh_remote_groups, 1U);
    auto expired = groups.Snapshot(start + std::chrono::seconds{90});
    BOOST_CHECK_EQUAL(expired.fresh_remote_groups, 0U); BOOST_CHECK_EQUAL(expired.missing_remote_groups, 1U);
    BOOST_CHECK(expired.reports.empty() && !expired.storage.capacity_bytes);
    expired = groups.Snapshot(start + std::chrono::seconds{179});
    BOOST_CHECK_EQUAL(expired.selected_remote_groups, 0U);
    ObservationGroups edge{{}}; Put(edge, 1, 1, Report(), start);
    BOOST_CHECK_EQUAL(edge.Snapshot(start + std::chrono::seconds{90} - std::chrono::nanoseconds{1}).fresh_remote_groups, 1U);
    BOOST_CHECK_EQUAL(edge.Snapshot(start + std::chrono::seconds{90}).fresh_remote_groups, 0U);
}
BOOST_AUTO_TEST_CASE(storage_weights_traffic_directions_and_cpu_subsets_are_explicit)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    auto first = Report(15, 15 * 1024); first.cpu.mean_basis_points = 2000;
    auto second = Report(45, 0); second.cpu = {true, 64, 120000, 24, 1000, 8000};
    second.traffic.received_kib = 120; second.traffic.sent_kib = 180;
    Put(groups, 1, 1, first, start); Put(groups, 2, 2, second, start);
    ObservationReport memory_only; memory_only.memory = {true, 1000};
    Put(groups, 3, 3, memory_only, start);
    const auto snapshot = groups.Snapshot(start);
    BOOST_CHECK_EQUAL(snapshot.fresh_remote_groups, 3U);
    BOOST_CHECK_EQUAL(snapshot.storage.contributors, 2U); BOOST_CHECK_CLOSE(*snapshot.storage.utilization_percent, 25.0, 0.001);
    BOOST_CHECK_EQUAL(snapshot.traffic.contributors, 2U);
    BOOST_CHECK_EQUAL(*snapshot.traffic.received_bytes, 180U << 10); BOOST_CHECK_EQUAL(*snapshot.traffic.sent_bytes, 300U << 10);
    BOOST_CHECK_CLOSE(*snapshot.traffic.received_bytes_per_second, 3072.0, 0.001);
    BOOST_CHECK_CLOSE(*snapshot.traffic.sent_bytes_per_second, 5120.0, 0.001);
    BOOST_CHECK_EQUAL(snapshot.cpu.contributors, 2U); BOOST_CHECK_CLOSE(*snapshot.cpu.mean_percent, 50.0, 0.001);
    BOOST_CHECK_EQUAL(snapshot.cpu.min_window_ms, 60000U); BOOST_CHECK_EQUAL(snapshot.cpu.max_window_ms, 120000U);
    BOOST_CHECK_EQUAL(snapshot.cpu.min_age_ms, 0U); BOOST_CHECK_EQUAL(snapshot.cpu.max_age_ms, 1000U);
    BOOST_CHECK_EQUAL(snapshot.reports.size(), 3U); // memory/cursor stay per-report, no sum
}
BOOST_AUTO_TEST_CASE(overflow_is_unknown_and_legitimate_over_policy_zero_or_missing_are_distinct)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    auto huge = Report(); const auto max = std::numeric_limits<uint64_t>::max();
    huge.storage.capacity_bytes = max; huge.storage.provider_budget_bytes = max / 3 * 2 + max % 3 * 2 / 3;
    huge.storage.stored_mib = max >> 20; huge.storage.obligations_mib = max >> 20;
    huge.traffic.received_kib = max >> 10; huge.traffic.sent_kib = 0;
    Put(groups, 1, 1, huge, start); Put(groups, 2, 2, huge, start);
    auto snapshot = groups.Snapshot(start);
    BOOST_CHECK_EQUAL(snapshot.storage.contributors, 2U);
    BOOST_CHECK(!snapshot.storage.capacity_bytes && !snapshot.storage.stored_copy_bytes && !snapshot.storage.provider_budget_bytes &&
        !snapshot.storage.obligations_bytes && !snapshot.storage.utilization_percent);
    BOOST_CHECK(!snapshot.traffic.received_bytes && !snapshot.traffic.received_bytes_per_second);
    BOOST_REQUIRE(snapshot.traffic.sent_bytes); BOOST_CHECK_EQUAL(*snapshot.traffic.sent_bytes, 0U);
    Put(groups, 3, 3, Report(), start); // Overflow must remain sticky across later contributors.
    BOOST_CHECK(!groups.Snapshot(start).storage.capacity_bytes);
    ObservationGroups over{{}}; Put(over, 1, 1, Report(15, 30 * 1024), start);
    BOOST_CHECK_CLOSE(*over.Snapshot(start).storage.utilization_percent, 200.0, 0.001);
    ObservationGroups zero{{}}; Put(zero, 1, 1, Report(15, 0), start);
    BOOST_CHECK_EQUAL(*zero.Snapshot(start).storage.stored_copy_bytes, 0U);
    ObservationGroups empty{{}}; BOOST_CHECK(!empty.Snapshot(start).storage.stored_copy_bytes);
}
BOOST_AUTO_TEST_CASE(cpu_age_and_known_contributors_start_new_cohort_segments)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    auto report = Report(); report.cpu.age_ms = 59000;
    Put(groups, 1, 1, report, start);
    const auto initial = groups.Snapshot(start).cohort_revision;
    BOOST_CHECK_EQUAL(*groups.Snapshot(start + std::chrono::seconds{1}).cpu.mean_percent, 25.0);
    auto snapshot = groups.Snapshot(start + std::chrono::milliseconds{1001});
    BOOST_CHECK(!snapshot.cpu.mean_percent); BOOST_CHECK_EQUAL(snapshot.cpu.contributors, 0U);
    BOOST_CHECK(snapshot.storage.capacity_bytes); BOOST_CHECK(snapshot.cohort_revision != initial);
    BOOST_REQUIRE(groups.Record(Session(1), Ip(1), Report(), start + std::chrono::seconds{2}));
    const auto renewed = groups.Snapshot(start + std::chrono::seconds{2}).cohort_revision;
    BOOST_REQUIRE(groups.Record(Session(1), Ip(1), Report(30), start + std::chrono::seconds{3}));
    BOOST_CHECK_EQUAL(groups.Snapshot(start + std::chrono::seconds{3}).cohort_revision, renewed);
    report = Report(); report.traffic = {};
    BOOST_REQUIRE(groups.Record(Session(1), Ip(1), report, start + std::chrono::seconds{4}));
    BOOST_CHECK(groups.Snapshot(start + std::chrono::seconds{4}).cohort_revision != renewed);
}
BOOST_AUTO_TEST_CASE(selection_bounds_clock_and_network_fail_closed)
{
    ObservationGroups groups{{}}; const auto start = Clock::time_point{};
    auto bad = Session(1); bad.admitted_hello = false; BOOST_CHECK(!groups.Select(bad, Ip(1), start));
    bad = Session(1); bad.network_binding[0] = 1; BOOST_CHECK(!groups.Select(bad, Ip(1), start));
    for (unsigned i = 1; i <= 32; ++i) BOOST_REQUIRE(groups.Select(Session(i), Ip(i), start));
    BOOST_CHECK(groups.Snapshot(start).slots_full); BOOST_CHECK(!groups.Select(Session(33), Ip(33), start));
    BOOST_REQUIRE(groups.Record(Session(1), Ip(1), Report(), start));
    auto bad_report = Report(); bad_report.network_binding[0] = 1;
    BOOST_CHECK(!groups.Record(Session(1), Ip(1), bad_report, start));
    bad_report = Report(); bad_report.storage.provider_budget_bytes++;
    BOOST_CHECK_THROW(groups.Record(Session(1), Ip(1), bad_report, start), std::invalid_argument);
    BOOST_CHECK(!groups.Snapshot(start - std::chrono::seconds{1}).clock_valid);
    groups.Close(1, start - std::chrono::seconds{1}); // close invalidates despite backwards test clock
    BOOST_CHECK_EQUAL(groups.Snapshot(start).fresh_remote_groups, 0U);
    groups.Expire(start + std::chrono::seconds{90}); BOOST_REQUIRE(groups.Select(Session(33), Ip(33), start + std::chrono::seconds{90}));
}
BOOST_AUTO_TEST_CASE(concurrent_default_clock_selection_stays_bounded)
{
    ObservationGroups groups{{}}; std::atomic_uint selected{0}; std::vector<std::jthread> workers;
    for (unsigned i = 1; i <= 64; ++i) workers.emplace_back([&, i] {
        if (groups.Select(Session(i), Ip(i))) ++selected;
    });
    workers.clear(); BOOST_CHECK_EQUAL(selected.load(), 32U);
    const auto snapshot = groups.Snapshot(); BOOST_CHECK_EQUAL(snapshot.selected_remote_groups, 32U);
    BOOST_CHECK_EQUAL(snapshot.missing_remote_groups, 32U); BOOST_CHECK(snapshot.slots_full && snapshot.reports.empty());
}
BOOST_AUTO_TEST_SUITE_END()
