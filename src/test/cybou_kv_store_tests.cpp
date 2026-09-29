// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/kv_store.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_kv_store_tests)

BOOST_AUTO_TEST_CASE(local_record_encoding_preserves_existing_database_bytes)
{
    const auto encoded_string = cybou::detail::SerializeLocalRecord(std::string{"key"});
    const std::array<unsigned char, 4> expected_string{3, 'k', 'e', 'y'};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_string.begin(), encoded_string.end(),
        expected_string.begin(), expected_string.end());

    const auto encoded_integer = cybou::detail::SerializeLocalRecord(uint64_t{0x0102030405060708});
    const std::array<unsigned char, 8> expected_integer{8, 7, 6, 5, 4, 3, 2, 1};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_integer.begin(), encoded_integer.end(),
        expected_integer.begin(), expected_integer.end());

    const std::vector<unsigned char> value{0xaa, 0xbb};
    const auto encoded_vector = cybou::detail::SerializeLocalRecord(value);
    const std::array<unsigned char, 3> expected_vector{2, 0xaa, 0xbb};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_vector.begin(), encoded_vector.end(),
        expected_vector.begin(), expected_vector.end());

    std::string decoded;
    const auto bytes = std::as_bytes(std::span{encoded_string});
    cybou::detail::LocalRecordReader reader{bytes};
    reader >> decoded;
    BOOST_CHECK_EQUAL(decoded, "key");
    BOOST_CHECK(reader.empty());
}

BOOST_AUTO_TEST_SUITE_END()
