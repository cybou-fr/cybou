// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/canonical_cbor.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <limits>
#include <stdexcept>
#include <vector>

namespace {

template <typename Exception, typename Function>
void CheckThrows(Function&& function)
{
    BOOST_CHECK_THROW(function(), Exception);
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_canonical_cbor_tests)

BOOST_AUTO_TEST_CASE(cbor_integers_use_shortest_preferred_encoding)
{
    BOOST_CHECK(cybou::EncodeCanonicalCbor(cybou::CborValue::Unsigned(23)) == std::vector<std::uint8_t>{0x17});
    BOOST_CHECK(cybou::EncodeCanonicalCbor(cybou::CborValue::Unsigned(24)) == (std::vector<std::uint8_t>{0x18, 0x18}));
    BOOST_CHECK(cybou::EncodeCanonicalCbor(cybou::CborValue::Unsigned(256)) == (std::vector<std::uint8_t>{0x19, 0x01, 0x00}));
    BOOST_CHECK(cybou::DecodeCanonicalCbor(cybou::EncodeCanonicalCbor(cybou::CborValue::Negative(std::numeric_limits<std::int64_t>::min()))) ==
                cybou::CborValue::Negative(std::numeric_limits<std::int64_t>::min()));
}

BOOST_AUTO_TEST_CASE(cbor_maps_use_rfc8949_core_deterministic_key_order)
{
    const auto map = cybou::CborValue::MapValue({
        {cybou::CborValue::Text(""), cybou::CborValue::Unsigned(1)},
        {cybou::CborValue::Unsigned(24), cybou::CborValue::Unsigned(2)},
    });
    const std::vector<std::uint8_t> expected{0xa2, 0x18, 0x18, 0x02, 0x60, 0x01};
    BOOST_CHECK(cybou::EncodeCanonicalCbor(map) == expected);
    BOOST_CHECK(cybou::DecodeCanonicalCbor(expected) == map);
    BOOST_CHECK(cybou::EncodeCanonicalCbor(cybou::DecodeCanonicalCbor(expected)) == expected);
}

BOOST_AUTO_TEST_CASE(cbor_round_trips_supported_nested_values)
{
    const auto original = cybou::CborValue::ArrayValue({
        cybou::CborValue::Null(),
        cybou::CborValue::Boolean(true),
        cybou::CborValue::Negative(-42),
        cybou::CborValue::Bytes({0x00, 0x80, 0xff}),
        cybou::CborValue::Text("CYBOU \xe2\x9c\x93"),
        cybou::CborValue::MapValue({{cybou::CborValue::Unsigned(1), cybou::CborValue::Boolean(false)}}),
    });
    BOOST_CHECK(cybou::DecodeCanonicalCbor(cybou::EncodeCanonicalCbor(original)).value == original.value);
}

BOOST_AUTO_TEST_CASE(cbor_decoder_rejects_noncanonical_and_unsupported_forms)
{
    CheckThrows<std::invalid_argument>([] { (void)cybou::DecodeCanonicalCbor(std::vector<std::uint8_t>{0x18, 0x17}); }); // overlong integer
    CheckThrows<std::invalid_argument>([] { (void)cybou::DecodeCanonicalCbor(std::vector<std::uint8_t>{0x9f, 0xff}); }); // indefinite array
    CheckThrows<std::invalid_argument>([] { (void)cybou::DecodeCanonicalCbor(std::vector<std::uint8_t>{0xc0, 0x00}); }); // tag
    CheckThrows<std::invalid_argument>([] { (void)cybou::DecodeCanonicalCbor(std::vector<std::uint8_t>{0xf9, 0x3c, 0x00}); }); // float
    CheckThrows<std::invalid_argument>([] { (void)cybou::DecodeCanonicalCbor(std::vector<std::uint8_t>{0x82, 0x00}); }); // truncated array
    CheckThrows<std::invalid_argument>([] { (void)cybou::DecodeCanonicalCbor(std::vector<std::uint8_t>{0x00, 0x00}); }); // trailing item
    CheckThrows<std::invalid_argument>([] { (void)cybou::EncodeCanonicalCbor(cybou::CborValue::Text("\xc0\xaf")); }); // invalid UTF-8

    const std::vector<std::uint8_t> out_of_order_map{0xa2, 0x60, 0x01, 0x18, 0x18, 0x02};
    CheckThrows<std::invalid_argument>([&] { (void)cybou::DecodeCanonicalCbor(out_of_order_map); });
    const std::vector<std::uint8_t> duplicate_map{0xa2, 0x01, 0x00, 0x01, 0x01};
    CheckThrows<std::invalid_argument>([&] { (void)cybou::DecodeCanonicalCbor(duplicate_map); });
}

BOOST_AUTO_TEST_CASE(cbor_encoder_rejects_duplicate_map_keys)
{
    const auto duplicate = cybou::CborValue::MapValue({
        {cybou::CborValue::Unsigned(1), cybou::CborValue::Text("first")},
        {cybou::CborValue::Unsigned(1), cybou::CborValue::Text("second")},
    });
    CheckThrows<std::invalid_argument>([&] { (void)cybou::EncodeCanonicalCbor(duplicate); });
}

BOOST_AUTO_TEST_CASE(cbor_profile_limits_input_and_nesting)
{
    const std::vector<std::uint8_t> too_large(cybou::cbor_profile::MAX_ENCODED_BYTES + 1, 0);
    CheckThrows<std::length_error>([&] { (void)cybou::DecodeCanonicalCbor(too_large); });

    cybou::CborValue nested = cybou::CborValue::Null();
    for (std::size_t i = 0; i <= cybou::cbor_profile::MAX_NESTING_DEPTH; ++i) nested = cybou::CborValue::ArrayValue({std::move(nested)});
    CheckThrows<std::length_error>([&] { (void)cybou::EncodeCanonicalCbor(nested); });
}

BOOST_AUTO_TEST_SUITE_END()
