// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/p2p/observation_groups.h>
#include <boost/asio/ip/address_v4.hpp>
#include <algorithm>
#include <limits>
namespace cybou::p2p {
namespace {
auto GroupKey(boost::asio::ip::address address)
{
    if (address.is_v6() && address.to_v6().is_v4_mapped()) {
        const auto bytes = address.to_v6().to_bytes();
        address = boost::asio::ip::address_v4{{bytes[12], bytes[13], bytes[14], bytes[15]}};
    }
    const bool local = address.is_loopback();
    return std::pair{local ? std::string{"loopback"} : address.to_string(), local};
}
unsigned KnownFields(const ObservationRow& r)
{
    return r.cursor.known | (r.storage.known << 1) | (r.traffic.known << 2) | (r.cpu.known << 3) | (r.memory.known << 4);
}
void Add(std::optional<uint64_t>& total, uint64_t bytes)
{
    if (!total) return; // An overflow remains unknown for the entire contributor set.
    if (bytes > std::numeric_limits<uint64_t>::max() - *total) total.reset();
    else *total += bytes;
}
}
bool ObservationGroups::Eligible(const ObservationSession& s) const
{
    return s.handle && s.admitted_hello && s.network_binding == m_binding;
}
bool ObservationGroups::Advance(Clock::time_point now)
{
    if (m_last_time && now < *m_last_time) return false;
    m_last_time = now;
    for (auto it = m_slots.begin(); it != m_slots.end();) {
        auto& slot = it->second;
        if (slot.received && now - *slot.received >= std::chrono::seconds{90}) {
            slot.received.reset(); slot.metrics = {}; ++m_cohort_revision;
        }
        if (slot.received && slot.metrics.cpu.known) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - *slot.received).count();
            if (elapsed + slot.metrics.cpu.age_ms > 60000) { slot.metrics.cpu = {}; ++m_cohort_revision; }
        }
        if (now - slot.activity >= std::chrono::seconds{90}) {
            it = m_slots.erase(it); ++m_cohort_revision;
        } else ++it;
    }
    return true;
}
bool ObservationGroups::Select(const ObservationSession& session, const boost::asio::ip::address& address,
    std::optional<Clock::time_point> at)
{
    if (!Eligible(session)) return false;
    const auto [key, local] = GroupKey(address);
    std::lock_guard lock{m_mutex}; const auto now = at.value_or(Clock::now());
    if (!Advance(now)) return false;
    // One actual connection cannot occupy two address groups, even with a bad caller.
    for (const auto& [existing, slot] : m_slots)
        if (slot.handle == session.handle && existing != key) return false;
    auto it = m_slots.find(key);
    if (it == m_slots.end()) {
        if (m_slots.size() >= 32) return false;
        m_slots.emplace(key, Slot{session.handle, local, now, {}, {}}); ++m_cohort_revision;
        return true;
    }
    auto& slot = it->second;
    if (slot.handle && slot.handle != session.handle) return false;
    if (!slot.handle) { slot.handle = session.handle; slot.received.reset(); slot.metrics = {}; ++m_cohort_revision; }
    slot.activity = now;
    return true;
}
bool ObservationGroups::Record(const ObservationSession& session, const boost::asio::ip::address& address,
    const ObservationReport& accepted, std::optional<Clock::time_point> at)
{
    if (!Eligible(session) || accepted.network_binding != m_binding) return false;
    // Validate unit/range bounds outside the store mutex; do not retain binding/challenge.
    (void)EncodeObservationReport(accepted);
    const auto [key, local] = GroupKey(address);
    std::lock_guard lock{m_mutex}; const auto now = at.value_or(Clock::now());
    if (!Advance(now)) return false;
    const auto it = m_slots.find(key);
    if (it == m_slots.end() || it->second.handle != session.handle) return false;
    auto& slot = it->second;
    ObservationRow next{local, 0, accepted.cache_age_ms, accepted.cursor, accepted.storage, accepted.traffic, accepted.cpu, accepted.memory};
    if (!slot.received || KnownFields(next) != KnownFields(slot.metrics)) ++m_cohort_revision;
    slot.metrics = next; slot.received = now; slot.activity = now;
    return true;
}
void ObservationGroups::Close(uint64_t handle, std::optional<Clock::time_point> at)
{
    std::lock_guard lock{m_mutex}; const auto now = at.value_or(Clock::now());
    // Disconnection invalidates immediately even if an explicit clock regresses.
    const bool advanced = Advance(now);
    for (auto& [key, slot] : m_slots) {
        (void)key;
        if (slot.handle == handle && handle) {
            slot.handle = 0; slot.received.reset(); slot.metrics = {};
            if (advanced) slot.activity = now;
            ++m_cohort_revision;
        }
    }
}
void ObservationGroups::Expire(std::optional<Clock::time_point> at)
{
    std::lock_guard lock{m_mutex}; Advance(at.value_or(Clock::now()));
}
ObservationGroupSnapshot ObservationGroups::Snapshot(std::optional<Clock::time_point> at)
{
    std::lock_guard lock{m_mutex}; const auto now = at.value_or(Clock::now());
    ObservationGroupSnapshot out;
    out.clock_valid = Advance(now); out.cohort_revision = m_cohort_revision;
    if (!out.clock_valid) return out;
    out.slots_full = m_slots.size() == 32;
    uint32_t cpu_basis_points{0};
    for (const auto& [key, slot] : m_slots) {
        (void)key;
        if (!slot.handle) continue;
        if (!slot.local) ++out.selected_remote_groups;
        if (!slot.received) { if (!slot.local) ++out.missing_remote_groups; continue; }
        auto row = slot.metrics;
        row.receipt_age_ms = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now - *slot.received).count());
        if (row.cpu.known) row.cpu.age_ms += row.receipt_age_ms;
        out.reports.push_back(row);
        if (slot.local) { ++out.fresh_local_groups; continue; }
        ++out.fresh_remote_groups;
        if (row.storage.known) {
            auto& s = out.storage;
            if (!s.contributors++) { s.capacity_bytes = 0; s.stored_copy_bytes = 0; s.provider_budget_bytes = 0; s.obligations_bytes = 0; }
            Add(s.capacity_bytes, row.storage.capacity_bytes); Add(s.stored_copy_bytes, row.storage.stored_mib << 20);
            Add(s.provider_budget_bytes, row.storage.provider_budget_bytes); Add(s.obligations_bytes, row.storage.obligations_mib << 20);
        }
        if (row.traffic.known) {
            auto& t = out.traffic;
            if (!t.contributors++) { t.received_bytes = 0; t.sent_bytes = 0; }
            Add(t.received_bytes, row.traffic.received_kib << 10); Add(t.sent_bytes, row.traffic.sent_kib << 10);
        }
        if (row.cpu.known) {
            auto& c = out.cpu;
            if (!c.contributors++) { c.min_window_ms = c.max_window_ms = row.cpu.window_ms; c.min_age_ms = c.max_age_ms = row.cpu.age_ms; }
            c.min_window_ms = std::min(c.min_window_ms, row.cpu.window_ms); c.max_window_ms = std::max(c.max_window_ms, row.cpu.window_ms);
            c.min_age_ms = std::min(c.min_age_ms, row.cpu.age_ms); c.max_age_ms = std::max(c.max_age_ms, row.cpu.age_ms);
            cpu_basis_points += row.cpu.mean_basis_points;
        }
    }
    if (out.storage.capacity_bytes && out.storage.stored_copy_bytes)
        out.storage.utilization_percent = *out.storage.stored_copy_bytes * 100.0 / *out.storage.capacity_bytes;
    if (out.traffic.received_bytes) out.traffic.received_bytes_per_second = *out.traffic.received_bytes / 60.0;
    if (out.traffic.sent_bytes) out.traffic.sent_bytes_per_second = *out.traffic.sent_bytes / 60.0;
    if (out.cpu.contributors) out.cpu.mean_percent = cpu_basis_points / (out.cpu.contributors * 100.0);
    return out;
}
}
