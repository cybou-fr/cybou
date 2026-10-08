// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/observation_report.h>
#include <cybou/binary_codec.h>
namespace cybou {
namespace {
void Require(bool valid)
{
    if (!valid) throw std::invalid_argument{"invalid observation payload"};
}
void Validate(const ObservationReport& r)
{
    constexpr auto max_mib = std::numeric_limits<uint64_t>::max() >> 20;
    constexpr auto max_kib = std::numeric_limits<uint64_t>::max() >> 10;
    Require(r.cache_age_ms <= 5001);
    if (r.cache_age_ms == 5001)
        Require(!r.cursor.known && !r.storage.known && !r.traffic.known && !r.cpu.known && !r.memory.known);
    if (!r.cursor.known) Require(r.cursor.height == 0 && r.cursor.tip == ObservationBytes32{});
    const auto& s = r.storage;
    if (s.known) {
        // DEC-289 minimum reporting policy; not a storage admission decision.
        Require(s.capacity_bytes >= (uint64_t{15} << 30));
        const auto budget = (s.capacity_bytes / 3) * 2 + (s.capacity_bytes % 3) * 2 / 3;
        Require(s.provider_budget_bytes == budget && s.stored_mib <= max_mib && s.obligations_mib <= max_mib);
    } else Require(s.capacity_bytes == 0 && s.stored_mib == 0 && s.provider_budget_bytes == 0 && s.obligations_mib == 0);
    const auto& t = r.traffic;
    if (t.known) Require(t.window_ms == 60000 && t.received_kib <= max_kib && t.sent_kib <= max_kib);
    else Require(t.window_ms == 0 && t.received_kib == 0 && t.sent_kib == 0);
    const auto& c = r.cpu;
    if (c.known) Require(c.processors >= 1 && c.processors <= 65536 && c.window_ms >= 60000 && c.window_ms <= 120000 &&
        c.intervals >= 1 && c.intervals <= 120000 && c.age_ms <= 60000 && c.mean_basis_points <= 10000);
    else Require(c.processors == 0 && c.window_ms == 0 && c.intervals == 0 && c.age_ms == 0 && c.mean_basis_points == 0);
    if (r.memory.known) Require(r.memory.resident_mib <= max_mib);
    else Require(r.memory.resident_mib == 0);
}
}
std::vector<unsigned char> EncodeObservationRequest(const ObservationRequest& request)
{
    BinaryWriter w{OBSERVATION_REQUEST_BYTES};
    w.Fixed(request.network_binding); w.Fixed(request.challenge);
    return w.Take();
}
ObservationRequest DecodeObservationRequest(std::span<const unsigned char> bytes)
{
    Require(bytes.size() == OBSERVATION_REQUEST_BYTES);
    BinaryReader r{bytes, OBSERVATION_REQUEST_BYTES};
    ObservationRequest result{r.Fixed<ObservationBytes32>(), r.Fixed<ObservationBytes32>()};
    r.Finish();
    return result;
}
std::vector<unsigned char> EncodeObservationReport(const ObservationReport& r)
{
    Validate(r);
    BinaryWriter w{OBSERVATION_REPORT_BYTES};
    w.Fixed(r.network_binding); w.Fixed(r.challenge); w.U32(r.cache_age_ms);
    w.U8(r.cursor.known); w.U64(r.cursor.height); w.Fixed(r.cursor.tip);
    w.U8(r.storage.known); w.U64(r.storage.capacity_bytes); w.U64(r.storage.stored_mib);
    w.U64(r.storage.provider_budget_bytes); w.U64(r.storage.obligations_mib);
    w.U8(r.traffic.known); w.U32(r.traffic.window_ms); w.U64(r.traffic.received_kib); w.U64(r.traffic.sent_kib);
    w.U8(r.cpu.known); w.U32(r.cpu.processors); w.U32(r.cpu.window_ms); w.U32(r.cpu.intervals);
    w.U32(r.cpu.age_ms); w.U16(r.cpu.mean_basis_points);
    w.U8(r.memory.known); w.U64(r.memory.resident_mib);
    return w.Take();
}
ObservationReport DecodeObservationReport(std::span<const unsigned char> bytes)
{
    Require(bytes.size() == OBSERVATION_REPORT_BYTES);
    BinaryReader r{bytes, OBSERVATION_REPORT_BYTES};
    ObservationReport out;
    out.network_binding = r.Fixed<ObservationBytes32>(); out.challenge = r.Fixed<ObservationBytes32>(); out.cache_age_ms = r.U32();
    out.cursor = {r.Flag(), r.U64(), r.Fixed<ObservationBytes32>()};
    out.storage = {r.Flag(), r.U64(), r.U64(), r.U64(), r.U64()};
    out.traffic = {r.Flag(), r.U32(), r.U64(), r.U64()};
    out.cpu = {r.Flag(), r.U32(), r.U32(), r.U32(), r.U32(), r.U16()};
    out.memory = {r.Flag(), r.U64()};
    r.Finish();
    Validate(out);
    return out;
}
}
