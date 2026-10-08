// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <cybou/observation_report.h>
#include <boost/test/unit_test.hpp>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
namespace {
using namespace cybou;
ObservationReport Known()
{
    ObservationReport r;
    for (size_t i = 0; i < 32; ++i) {
        r.network_binding[i] = static_cast<unsigned char>(i);
        r.challenge[i] = static_cast<unsigned char>(i + 32);
        r.cursor.tip[i] = static_cast<unsigned char>(i + 64);
    }
    r.cache_age_ms = 5000;
    r.cursor.known = true; r.cursor.height = 0x0807060504030201ULL;
    r.storage = {true, uint64_t{15} << 30, 123, uint64_t{10} << 30, 456};
    r.traffic = {true, 60000, 789, 1011};
    r.cpu = {true, 8, 60000, 12, 1234, 5678};
    r.memory = {true, 1024};
    return r;
}
void Put(std::vector<unsigned char>& bytes, size_t offset, uint64_t value, size_t width)
{
    for (size_t i = 0; i < width; ++i) bytes[offset + i] = static_cast<unsigned char>(value >> (8 * i));
}
void RejectEncoding(const ObservationReport& r)
{
    BOOST_CHECK_THROW(EncodeObservationReport(r), std::invalid_argument);
}
}
BOOST_AUTO_TEST_SUITE(cybou_observation_report_tests)
BOOST_AUTO_TEST_CASE(layout_matches_independent_fixed_offset_vector)
{
    const auto r = Known();
    std::vector<unsigned char> expected(191);
    for (size_t i = 0; i < 64; ++i) expected[i] = static_cast<unsigned char>(i);
    Put(expected, 64, 5000, 4); expected[68] = 1;
    Put(expected, 69, 0x0807060504030201ULL, 8);
    for (size_t i = 0; i < 32; ++i) expected[77 + i] = static_cast<unsigned char>(i + 64);
    expected[109] = 1; Put(expected, 110, uint64_t{15} << 30, 8); Put(expected, 118, 123, 8);
    Put(expected, 126, uint64_t{10} << 30, 8); Put(expected, 134, 456, 8);
    expected[142] = 1; Put(expected, 143, 60000, 4); Put(expected, 147, 789, 8); Put(expected, 155, 1011, 8);
    expected[163] = 1; Put(expected, 164, 8, 4); Put(expected, 168, 60000, 4); Put(expected, 172, 12, 4);
    Put(expected, 176, 1234, 4); Put(expected, 180, 5678, 2);
    expected[182] = 1; Put(expected, 183, 1024, 8);
    BOOST_CHECK(EncodeObservationReport(r) == expected);
    BOOST_CHECK(EncodeObservationReport(DecodeObservationReport(expected)) == expected);
    ObservationRequest request{r.network_binding, r.challenge};
    const std::vector<unsigned char> request_expected(expected.begin(), expected.begin() + 64);
    BOOST_CHECK(EncodeObservationRequest(request) == request_expected);
    BOOST_CHECK(EncodeObservationRequest(DecodeObservationRequest(request_expected)) == request_expected);
}
BOOST_AUTO_TEST_CASE(rejects_every_truncation_and_trailing_bytes)
{
    auto bytes = EncodeObservationReport(Known());
    for (size_t size = 0; size < bytes.size(); ++size)
        BOOST_CHECK_THROW(DecodeObservationReport(std::span{bytes}.first(size)), std::invalid_argument);
    bytes.push_back(0);
    BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
    bytes = EncodeObservationRequest({});
    for (size_t size = 0; size < bytes.size(); ++size)
        BOOST_CHECK_THROW(DecodeObservationRequest(std::span{bytes}.first(size)), std::invalid_argument);
    bytes.push_back(0);
    BOOST_CHECK_THROW(DecodeObservationRequest(bytes), std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(rejects_non_boolean_and_every_nonzero_unknown_byte)
{
    const auto empty = EncodeObservationReport({});
    BOOST_CHECK(empty == std::vector<unsigned char>(191));
    const std::array<size_t, 5> flags{68, 109, 142, 163, 182};
    for (size_t i = 68; i < 191; ++i) {
        auto bytes = empty;
        const bool flag = std::find(flags.begin(), flags.end(), i) != flags.end();
        bytes[i] = flag ? 2 : 1;
        BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
    }
    for (const auto offset : flags) {
        auto bytes = empty; bytes[offset] = 255;
        BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
    }
    auto r = Known(); r.cursor.known = false; RejectEncoding(r);
    r = Known(); r.storage.known = false; RejectEncoding(r);
    r = Known(); r.traffic.known = false; RejectEncoding(r);
    r = Known(); r.cpu.known = false; RejectEncoding(r);
    r = Known(); r.memory.known = false; RejectEncoding(r);
}
BOOST_AUTO_TEST_CASE(stale_requires_all_unknown_and_preserves_structural_binding_only)
{
    ObservationReport r; r.cache_age_ms = 5001; r.network_binding.fill(255); r.challenge.fill(123);
    BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r)));
    for (const auto age : {uint32_t{5002}, std::numeric_limits<uint32_t>::max()}) {
        auto bytes = EncodeObservationReport(r); Put(bytes, 64, age, 4);
        BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
        r.cache_age_ms = age; RejectEncoding(r); r.cache_age_ms = 5001;
    }
    auto known = Known(); known.cache_age_ms = 5001; RejectEncoding(known);
    const auto fields = Known();
    for (unsigned block = 0; block < 5; ++block) {
        ObservationReport single;
        if (block == 0) single.cursor = fields.cursor;
        if (block == 1) single.storage = fields.storage;
        if (block == 2) single.traffic = fields.traffic;
        if (block == 3) single.cpu = fields.cpu;
        if (block == 4) single.memory = fields.memory;
        auto bytes = EncodeObservationReport(single); Put(bytes, 64, 5001, 4);
        BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
        single.cache_age_ms = 5001; RejectEncoding(single);
    }
    r.cache_age_ms = 5000; r.cursor.known = true;
    BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r))); // initialized height zero
}
BOOST_AUTO_TEST_CASE(storage_and_units_accept_over_policy_and_reject_overflow)
{
    constexpr auto max = std::numeric_limits<uint64_t>::max();
    auto r = Known(); r.storage.capacity_bytes = max;
    r.storage.provider_budget_bytes = max / 3 * 2 + max % 3 * 2 / 3;
    for (uint64_t remainder = 0; remainder < 3; ++remainder) {
        r.storage.capacity_bytes = max - remainder;
        r.storage.provider_budget_bytes = r.storage.capacity_bytes / 3 * 2 + r.storage.capacity_bytes % 3 * 2 / 3;
        BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r)));
    }
    r.storage.stored_mib = max >> 20; r.storage.obligations_mib = max >> 20;
    r.traffic.received_kib = max >> 10; r.traffic.sent_kib = max >> 10; r.memory.resident_mib = max >> 20;
    BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r)));
    r = Known(); r.storage.stored_mib = 50000; r.storage.obligations_mib = 50000;
    BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r)));
    const auto valid = EncodeObservationReport(Known());
    for (const auto& [offset, value] : std::array<std::pair<size_t, uint64_t>, 7>{{
        {110, (uint64_t{15} << 30) - 1}, {126, (uint64_t{10} << 30) + 1},
        {118, (max >> 20) + 1}, {134, (max >> 20) + 1}, {147, (max >> 10) + 1},
        {155, (max >> 10) + 1}, {183, (max >> 20) + 1}}}) {
        auto bytes = valid; Put(bytes, offset, value, 8);
        BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
    }
    r = Known(); r.storage.capacity_bytes--; RejectEncoding(r);
    r = Known(); r.storage.provider_budget_bytes++; RejectEncoding(r);
    r = Known(); r.storage.stored_mib = (max >> 20) + 1; RejectEncoding(r);
    r = Known(); r.storage.obligations_mib = (max >> 20) + 1; RejectEncoding(r);
    r = Known(); r.traffic.received_kib = (max >> 10) + 1; RejectEncoding(r);
    r = Known(); r.traffic.sent_kib = (max >> 10) + 1; RejectEncoding(r);
    r = Known(); r.memory.resident_mib = (max >> 20) + 1; RejectEncoding(r);
}
BOOST_AUTO_TEST_CASE(cpu_and_traffic_exact_ranges)
{
    auto r = Known(); r.cpu = {true, 1, 60000, 1, 0, 0};
    BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r)));
    r.cpu = {true, 65536, 120000, 120000, 60000, 10000};
    BOOST_CHECK_NO_THROW(DecodeObservationReport(EncodeObservationReport(r)));
    const auto valid = EncodeObservationReport(r);
    for (const auto& [offset, value] : std::array<std::pair<size_t, uint64_t>, 11>{{
        {143, 0}, {143, 59999}, {143, 60001}, {164, 0}, {164, 65537},
        {168, 59999}, {168, 120001}, {172, 0}, {172, 120001}, {176, 60001}, {180, 10001}}}) {
        auto bytes = valid; Put(bytes, offset, value, offset == 180 ? 2 : 4);
        BOOST_CHECK_THROW(DecodeObservationReport(bytes), std::invalid_argument);
    }
    r.cpu.mean_basis_points = 10001; RejectEncoding(r);
    r = Known(); r.traffic.window_ms = 60001; RejectEncoding(r);
}
BOOST_AUTO_TEST_SUITE_END()
