// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/p2p/observation_exchange.h>
#include <boost/asio/ip/address.hpp>
#include <boost/test/unit_test.hpp>
#include <atomic>
#include <thread>
namespace {
using namespace cybou;
using namespace cybou::p2p;
using Clock = ObservationExchange::Clock;
auto Ip(unsigned suffix) { return boost::asio::ip::make_address("192.168.1." + std::to_string(suffix)); }
ObservationSession Session(uint64_t handle) { return {handle, {}, true}; }
auto Reply(const ObservationRequest& request)
{
    ObservationReport r; r.network_binding = request.network_binding; r.challenge = request.challenge;
    r.memory = {true, 100};
    return EncodeObservationReport(r);
}
}
BOOST_AUTO_TEST_SUITE(cybou_observation_exchange_tests)
BOOST_AUTO_TEST_CASE(admitted_session_network_and_fresh_challenge_are_required)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}};
    auto s = Session(1); s.admitted_hello = false;
    BOOST_CHECK(!guard.Begin(s, Ip(1), start));
    s = Session(0); BOOST_CHECK(!guard.Begin(s, Ip(1), start));
    s = Session(1); s.network_binding[0] = 1; BOOST_CHECK(!guard.Begin(s, Ip(1), start));
    s = Session(1); const auto request = guard.Begin(s, Ip(1), start); BOOST_REQUIRE(request);
    BOOST_CHECK(!guard.Begin(s, Ip(2), start)); // same connection, different claimed address
    const auto valid = Reply(*request);
    BOOST_CHECK(!guard.Accept(Session(2), valid, start));
    auto wrong = DecodeObservationReport(valid); wrong.challenge[0] ^= 1;
    BOOST_CHECK(!guard.Accept(s, EncodeObservationReport(wrong), start));
    wrong = DecodeObservationReport(valid); wrong.network_binding[0] = 1;
    BOOST_CHECK(!guard.Accept(s, EncodeObservationReport(wrong), start));
    BOOST_CHECK_THROW(guard.Accept(s, std::span{valid}.first(190), start), std::invalid_argument);
    BOOST_CHECK(guard.Accept(s, valid, start));
    BOOST_CHECK(!guard.Accept(s, valid, start)); // duplicate consumes no new sample
    const auto next = guard.Begin(s, Ip(1), start + std::chrono::seconds{30}); BOOST_REQUIRE(next);
    BOOST_CHECK(next->challenge != request->challenge);
    BOOST_CHECK(!guard.Accept(s, valid, start + std::chrono::seconds{30}));
    BOOST_CHECK(guard.Accept(s, Reply(*next), start + std::chrono::seconds{30}));
}
BOOST_AUTO_TEST_CASE(deadline_close_and_pending_limit_do_not_release_ip_cooldown)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}};
    for (unsigned i = 1; i <= 4; ++i) BOOST_REQUIRE(guard.Begin(Session(i), Ip(i), start));
    BOOST_CHECK(!guard.Begin(Session(5), Ip(5), start));
    guard.Close(1);
    BOOST_CHECK(!guard.Begin(Session(10), Ip(1), start));
    const auto request = guard.Begin(Session(5), Ip(5), start); BOOST_REQUIRE(request);
    BOOST_CHECK(!guard.Accept(Session(5), Reply(*request), start + std::chrono::seconds{5}));
    BOOST_CHECK(!guard.Begin(Session(11), Ip(5), start + std::chrono::seconds{5}));
    BOOST_REQUIRE(guard.Begin(Session(6), Ip(6), start + std::chrono::seconds{5}));
    BOOST_REQUIRE(guard.Begin(Session(11), Ip(5), start + std::chrono::seconds{30}));
    ObservationExchange edge{{}};
    const auto early = edge.Begin(Session(1), Ip(1), start); BOOST_REQUIRE(early);
    BOOST_CHECK(edge.Accept(Session(1), Reply(*early), start + std::chrono::seconds{5} - std::chrono::nanoseconds{1}));
}
BOOST_AUTO_TEST_CASE(mapped_address_and_session_churn_share_directional_cooldowns)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}};
    const auto v4 = Ip(1); const auto mapped = boost::asio::ip::make_address("::ffff:192.168.1.1");
    auto request = guard.Begin(Session(1), v4, start); BOOST_REQUIRE(request); guard.Close(1);
    BOOST_CHECK(guard.AdmitResponse(Session(2), mapped, *request, start));
    BOOST_CHECK(!guard.Begin(Session(2), mapped, start + std::chrono::seconds{29}));
    BOOST_CHECK(!guard.AdmitResponse(Session(3), v4, *request, start + std::chrono::seconds{29}));
    request->network_binding[0] = 1;
    BOOST_CHECK(!guard.AdmitResponse(Session(4), Ip(2), *request, start + std::chrono::seconds{29}));
    BOOST_REQUIRE(guard.Begin(Session(4), mapped, start + std::chrono::seconds{30}));
    request->network_binding[0] = 0;
    BOOST_CHECK(guard.AdmitResponse(Session(5), v4, *request, start + std::chrono::seconds{30}));
}
BOOST_AUTO_TEST_CASE(global_limits_are_rolling_windows_and_directionally_independent)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}}; ObservationRequest request;
    for (unsigned i = 1; i <= 16; ++i) {
        BOOST_REQUIRE(guard.Begin(Session(i), Ip(i), start + std::chrono::seconds{10})); guard.Close(i);
    }
    BOOST_CHECK(!guard.Begin(Session(17), Ip(17), start + std::chrono::seconds{30}));
    for (unsigned i = 1; i <= 32; ++i) BOOST_CHECK(guard.AdmitResponse(Session(i), Ip(i), request, start + std::chrono::seconds{30}));
    BOOST_CHECK(!guard.AdmitResponse(Session(33), Ip(33), request, start + std::chrono::seconds{40}));
    BOOST_REQUIRE(guard.Begin(Session(17), Ip(17), start + std::chrono::seconds{40}));
    BOOST_CHECK(guard.AdmitResponse(Session(33), Ip(33), request, start + std::chrono::seconds{60}));
}
BOOST_AUTO_TEST_CASE(address_table_is_bounded_without_evicting_unexpired_cooldowns)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}}; ObservationRequest request;
    unsigned ip = 1;
    for (unsigned window = 0; window < 3; ++window) {
        const auto now = start + std::chrono::seconds{30 * window};
        for (unsigned i = 0; i < 16; ++i, ++ip) { BOOST_REQUIRE(guard.Begin(Session(ip), Ip(ip), now)); guard.Close(ip); }
        const unsigned replies = window == 2 ? 16 : 32;
        for (unsigned i = 0; i < replies; ++i, ++ip) BOOST_CHECK(guard.AdmitResponse(Session(ip), Ip(ip), request, now));
    }
    BOOST_CHECK_EQUAL(ip, 129U);
    BOOST_CHECK(!guard.AdmitResponse(Session(129), Ip(129), request, start + std::chrono::seconds{61}));
    BOOST_CHECK(!guard.AdmitResponse(Session(128), Ip(128), request, start + std::chrono::seconds{61}));
    guard.Expire(start + std::chrono::seconds{90});
    BOOST_CHECK(guard.AdmitResponse(Session(129), Ip(129), request, start + std::chrono::seconds{90}));
}
BOOST_AUTO_TEST_CASE(backwards_clock_is_fail_closed_without_refunding_limits)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}};
    auto request = guard.Begin(Session(1), Ip(1), start + std::chrono::seconds{10}); BOOST_REQUIRE(request);
    BOOST_CHECK(!guard.Begin(Session(2), Ip(2), start));
    BOOST_CHECK(!guard.Accept(Session(1), Reply(*request), start));
    BOOST_CHECK(!guard.AdmitResponse(Session(2), Ip(2), *request, start));
    BOOST_CHECK(guard.Accept(Session(1), Reply(*request), start + std::chrono::seconds{11}));
    BOOST_CHECK(!guard.Begin(Session(3), Ip(1), start + std::chrono::seconds{11}));
}
BOOST_AUTO_TEST_CASE(concurrent_calls_obey_shared_limits)
{
    const auto start = Clock::time_point{}; ObservationExchange guard{{}};
    std::atomic_uint admitted{0}; std::vector<std::jthread> workers;
    for (unsigned i = 1; i <= 16; ++i) workers.emplace_back([&, i] {
        if (guard.Begin(Session(i), Ip(i), start)) ++admitted;
    });
    workers.clear(); BOOST_CHECK_EQUAL(admitted.load(), 4U);
    guard.Expire(start + std::chrono::seconds{5});
    admitted = 0;
    for (unsigned i = 1; i <= 16; ++i) workers.emplace_back([&, i] {
        if (guard.AdmitResponse(Session(i), Ip(1), ObservationRequest{}, start + std::chrono::seconds{5})) ++admitted;
    });
    workers.clear(); BOOST_CHECK_EQUAL(admitted.load(), 1U);
}
BOOST_AUTO_TEST_SUITE_END()
