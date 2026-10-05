// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#include <cybou/hex.h>
#include <boost/test/unit_test.hpp>
BOOST_AUTO_TEST_SUITE(cybou_hex_tests)
BOOST_AUTO_TEST_CASE(canonical_forward_hash_hex) {
    const std::string hex="0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20";
    const auto hash=cybou::ParseHash256UserHex(hex);
    BOOST_REQUIRE(hash);
    BOOST_CHECK_EQUAL(hash->begin()[0],1);
    BOOST_CHECK_EQUAL(hash->begin()[31],32);
    BOOST_CHECK_EQUAL(hash->GetHex(),hex);
    BOOST_CHECK_EQUAL(cybou::HexEncode(std::span{hash->data(),hash->size()}),hex);
    BOOST_CHECK(!cybou::ParseHash256UserHex("1"));
    BOOST_CHECK(!cybou::ParseHash256UserHex("0x"+hex));
    BOOST_CHECK(!cybou::ParseHash256UserHex(std::string(64,'z')));
    BOOST_CHECK(!cybou::ParseHash256UserHex(hex+"0"));
    BOOST_CHECK_THROW(cybou::Hash256(std::span<const unsigned char>{}),std::invalid_argument);
    BOOST_CHECK(cybou::Hash256{uint8_t{1}}<cybou::Hash256{uint8_t{2}});
}
BOOST_AUTO_TEST_SUITE_END()
