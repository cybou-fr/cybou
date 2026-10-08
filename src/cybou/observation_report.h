// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_OBSERVATION_REPORT_H
#define CYBOU_OBSERVATION_REPORT_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace cybou {
using ObservationBytes32 = std::array<unsigned char, 32>;
inline constexpr std::size_t OBSERVATION_REQUEST_BYTES = 64;
inline constexpr std::size_t OBSERVATION_REPORT_BYTES = 191;
struct ObservationRequest {
    ObservationBytes32 network_binding{}, challenge{};
};
struct ObservationCursor {
    bool known{false};
    uint64_t height{0};
    ObservationBytes32 tip{};
};
struct ObservationStorage {
    // provider_used_mib counts admitted provider replica lengths, not active
    // contractual lease/placement obligations. Wire position/units are unchanged.
    bool known{false};
    uint64_t capacity_bytes{0}, stored_mib{0}, provider_budget_bytes{0}, provider_used_mib{0};
};
struct ObservationTraffic {
    bool known{false};
    uint32_t window_ms{0};
    uint64_t received_kib{0}, sent_kib{0};
};
struct ObservationCpu {
    bool known{false};
    uint32_t processors{0}, window_ms{0}, intervals{0}, age_ms{0};
    uint16_t mean_basis_points{0};
};
struct ObservationMemory {
    bool known{false};
    uint64_t resident_mib{0};
};
struct ObservationReport {
    ObservationBytes32 network_binding{}, challenge{};
    uint32_t cache_age_ms{0};
    ObservationCursor cursor;
    ObservationStorage storage;
    ObservationTraffic traffic;
    ObservationCpu cpu;
    ObservationMemory memory;
};
// Strict DEC-289 payload codecs only. They do not authenticate network, challenge,
// session, measurement truth or freshness. Transport acceptance is separate.
// Malformed size, flag, unknown block or range throws std::invalid_argument.
std::vector<unsigned char> EncodeObservationRequest(const ObservationRequest& request);
ObservationRequest DecodeObservationRequest(std::span<const unsigned char> bytes);
std::vector<unsigned char> EncodeObservationReport(const ObservationReport& report);
ObservationReport DecodeObservationReport(std::span<const unsigned char> bytes);
}
#endif
