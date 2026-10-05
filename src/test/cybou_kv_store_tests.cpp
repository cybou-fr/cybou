// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/state_store.h>

#include <boost/test/unit_test.hpp>

#include <array>
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

    const std::vector<unsigned char> compact_253(253, 0xaa);
    const auto encoded_253 = cybou::detail::SerializeLocalRecord(compact_253);
    BOOST_CHECK_EQUAL(encoded_253[0], 253);
    BOOST_CHECK_EQUAL(encoded_253[1], 253);
    BOOST_CHECK_EQUAL(encoded_253[2], 0);

    const std::vector<unsigned char> compact_65536(65536, 0xbb);
    const auto encoded_65536 = cybou::detail::SerializeLocalRecord(compact_65536);
    BOOST_CHECK_EQUAL(encoded_65536[0], 254);
    BOOST_CHECK_EQUAL(encoded_65536[1], 0);
    BOOST_CHECK_EQUAL(encoded_65536[2], 0);
    BOOST_CHECK_EQUAL(encoded_65536[3], 1);
    BOOST_CHECK_EQUAL(encoded_65536[4], 0);

    std::string decoded;
    BOOST_CHECK(cybou::detail::DeserializeLocalRecord(std::span{encoded_string}, decoded));
    BOOST_CHECK_EQUAL(decoded, "key");
    const std::array<unsigned char, 3> noncanonical_string_size{253, 1, 0};
    BOOST_CHECK(!cybou::detail::DeserializeLocalRecord(std::span{noncanonical_string_size}, decoded));

    cybou::FinalizedHead head{};
    for (size_t i{0}; i < cybou::Hash256::size(); ++i) head.block_id.begin()[i] = static_cast<unsigned char>(i);
    head.height = 0x0102030405060708;
    const auto encoded_head = cybou::detail::SerializeLocalRecord(head);
    BOOST_REQUIRE_EQUAL(encoded_head.size(), 40);
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_head.begin(), encoded_head.begin() + 32,
        head.block_id.begin(), head.block_id.end());
    const std::array<unsigned char, 8> expected_height{8, 7, 6, 5, 4, 3, 2, 1};
    BOOST_CHECK_EQUAL_COLLECTIONS(encoded_head.begin() + 32, encoded_head.end(),
        expected_height.begin(), expected_height.end());

    cybou::FinalizedHead decoded_head{};
    BOOST_CHECK(cybou::detail::DeserializeLocalRecord(std::span{encoded_head}, decoded_head));
    BOOST_CHECK(decoded_head.block_id == head.block_id);
    BOOST_CHECK_EQUAL(decoded_head.height, head.height);
}

BOOST_AUTO_TEST_SUITE_END()
