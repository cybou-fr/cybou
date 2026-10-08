// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_P2P_OBSERVATION_EXCHANGE_H
#define CYBOU_P2P_OBSERVATION_EXCHANGE_H
#include <cybou/observation_report.h>
#include <boost/asio/ip/address.hpp>
#include <chrono>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <string>
namespace cybou::p2p {
// Trusted local connection context, never a wire identity. A nonzero handle
// names one live TLS session and must not be recycled; Close must run on teardown.
struct ObservationSession {
    uint64_t handle{0};
    ObservationBytes32 network_binding{};
    bool admitted_hello{false};
};
// Standalone DEC-289 scheduling/acceptance guard. Callers must supply the actual
// numeric socket address and schedule I/O through the existing transaction owner.
// This class neither reads/writes sockets nor retains accepted reports or logs.
class ObservationExchange {
public:
    using Clock = std::chrono::steady_clock;
    explicit ObservationExchange(ObservationBytes32 binding) : m_binding{binding} {}
    std::optional<ObservationRequest> Begin(const ObservationSession& session,
        const boost::asio::ip::address& address, std::optional<Clock::time_point> at = std::nullopt);
    bool AdmitResponse(const ObservationSession& session, const boost::asio::ip::address& address,
        const ObservationRequest& request, std::optional<Clock::time_point> at = std::nullopt);
    // Malformed payload throws (codec rules); mismatches/replay/expiry return empty
    // without consuming another session's pending challenge.
    std::optional<ObservationReport> Accept(const ObservationSession& session,
        std::span<const unsigned char> payload, std::optional<Clock::time_point> at = std::nullopt);
    void Close(uint64_t session);
    void Expire(std::optional<Clock::time_point> at = std::nullopt);
private:
    struct AddressBudget {
        std::optional<Clock::time_point> sent, served;
        Clock::time_point activity;
    };
    struct Pending { ObservationBytes32 challenge; Clock::time_point started; };
    bool Advance(Clock::time_point now);
    bool Eligible(const ObservationSession& session) const;
    bool Reserve(const std::string& address, bool collector, Clock::time_point now);
    const ObservationBytes32 m_binding;
    std::mutex m_mutex;
    std::optional<Clock::time_point> m_last_time;
    std::map<std::string, AddressBudget> m_addresses; // <=128, shared across both directions
    std::deque<Clock::time_point> m_sent, m_served; // rolling 30-second bounds: 16/32
    std::map<uint64_t, Pending> m_pending; // <=4, one per live session
};
}
#endif
