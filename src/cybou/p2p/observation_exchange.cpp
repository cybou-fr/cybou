// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/p2p/observation_exchange.h>
#include <boost/asio/ip/address_v4.hpp>
#include <openssl/rand.h>
namespace cybou::p2p {
namespace {
std::string AddressKey(const boost::asio::ip::address& address)
{
    if (address.is_v6() && address.to_v6().is_v4_mapped()) {
        const auto bytes = address.to_v6().to_bytes();
        return boost::asio::ip::address_v4{{bytes[12], bytes[13], bytes[14], bytes[15]}}.to_string();
    }
    return address.to_string();
}
}
bool ObservationExchange::Eligible(const ObservationSession& s) const
{
    return s.handle && s.admitted_hello && s.network_binding == m_binding;
}
bool ObservationExchange::Advance(Clock::time_point now)
{
    // A backwards clock never resets cooldowns or grants a new burst.
    if (m_last_time && now < *m_last_time) return false;
    m_last_time = now;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (now - it->second.started >= std::chrono::seconds{5}) it = m_pending.erase(it);
        else ++it;
    }
    for (auto it = m_addresses.begin(); it != m_addresses.end();) {
        if (now - it->second.activity >= std::chrono::seconds{90}) it = m_addresses.erase(it);
        else ++it;
    }
    for (auto* queue : {&m_sent, &m_served})
        while (!queue->empty() && now - queue->front() >= std::chrono::seconds{30}) queue->pop_front();
    return true;
}
bool ObservationExchange::Reserve(const std::string& address, bool collector, Clock::time_point now)
{
    auto& global = collector ? m_sent : m_served;
    if (global.size() >= (collector ? 16U : 32U)) return false;
    auto it = m_addresses.find(address);
    if (it == m_addresses.end()) {
        if (m_addresses.size() >= 128) return false;
        it = m_addresses.emplace(address, AddressBudget{{}, {}, now}).first;
    }
    auto& last = collector ? it->second.sent : it->second.served;
    if (last && now - *last < std::chrono::seconds{30}) return false;
    last = now; it->second.activity = now; global.push_back(now);
    return true;
}
std::optional<ObservationRequest> ObservationExchange::Begin(const ObservationSession& session,
    const boost::asio::ip::address& address, std::optional<Clock::time_point> at)
{
    if (!Eligible(session)) return std::nullopt;
    ObservationRequest request{m_binding, {}};
    // No cache/budget mutex is held while calling the OS-backed CSPRNG.
    if (RAND_bytes(request.challenge.data(), static_cast<int>(request.challenge.size())) != 1) return std::nullopt;
    const auto key = AddressKey(address);
    std::lock_guard lock{m_mutex};
    const auto now = at.value_or(Clock::now());
    if (!Advance(now) || m_pending.contains(session.handle) || m_pending.size() >= 4 || !Reserve(key, true, now)) return std::nullopt;
    m_pending.emplace(session.handle, Pending{request.challenge, now});
    return request;
}
bool ObservationExchange::AdmitResponse(const ObservationSession& session, const boost::asio::ip::address& address,
    const ObservationRequest& request, std::optional<Clock::time_point> at)
{
    if (!Eligible(session) || request.network_binding != m_binding) return false;
    const auto key = AddressKey(address);
    std::lock_guard lock{m_mutex};
    const auto now = at.value_or(Clock::now());
    return Advance(now) && Reserve(key, false, now);
}
std::optional<ObservationReport> ObservationExchange::Accept(const ObservationSession& session,
    std::span<const unsigned char> payload, std::optional<Clock::time_point> at)
{
    const auto report = DecodeObservationReport(payload);
    if (!Eligible(session) || report.network_binding != m_binding) return std::nullopt;
    std::lock_guard lock{m_mutex};
    const auto now = at.value_or(Clock::now());
    if (!Advance(now)) return std::nullopt;
    const auto it = m_pending.find(session.handle);
    if (it == m_pending.end() || report.challenge != it->second.challenge) return std::nullopt;
    m_pending.erase(it);
    return report;
}
void ObservationExchange::Close(uint64_t session)
{
    std::lock_guard lock{m_mutex};
    m_pending.erase(session); // Per-IP cooldown survives port/session churn.
}
void ObservationExchange::Expire(std::optional<Clock::time_point> at)
{
    std::lock_guard lock{m_mutex};
    Advance(at.value_or(Clock::now()));
}
}
