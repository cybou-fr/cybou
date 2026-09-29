// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/hex.h>

#include <boost/test/unit_test.hpp>

#include <string>

BOOST_AUTO_TEST_SUITE(cybou_hex_tests)

BOOST_AUTO_TEST_CASE(parse_user_uint256_hex)
{
    const auto plain = cybou::ParseUint256UserHex("1");
    const auto prefixed = cybou::ParseUint256UserHex("0x01");
    BOOST_REQUIRE(plain);
    BOOST_REQUIRE(prefixed);
    BOOST_CHECK(*plain == *prefixed);
    BOOST_CHECK(plain->begin()[0] == 1);
    BOOST_CHECK(!cybou::ParseUint256UserHex(std::string(65, '1')));
    BOOST_CHECK(!cybou::ParseUint256UserHex("not-hex"));
}

BOOST_AUTO_TEST_SUITE_END()
