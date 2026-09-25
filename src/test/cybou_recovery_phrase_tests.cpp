// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/recovery_phrase.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_recovery_phrase_tests)

BOOST_AUTO_TEST_CASE(bip39_256_bit_zero_vector_and_checksum)
{
    cybou::RecoveryEntropy entropy{};
    auto words = cybou::EncodeRecoveryWords(entropy);
    for (size_t i{0}; i < 23; ++i) BOOST_CHECK_EQUAL(words[i], "abandon");
    BOOST_CHECK_EQUAL(words[23], "art");
    BOOST_CHECK(cybou::DecodeRecoveryWords(words) == entropy);
    words[23] = "abandon";
    BOOST_CHECK(!cybou::DecodeRecoveryWords(words));
    words[23] = "ART";
    BOOST_CHECK(!cybou::DecodeRecoveryWords(words));
}

BOOST_AUTO_TEST_CASE(random_phrase_round_trip)
{
    const auto entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(entropy);
    const auto words = cybou::EncodeRecoveryWords(*entropy);
    BOOST_CHECK(cybou::DecodeRecoveryWords(words) == entropy);
}

BOOST_AUTO_TEST_SUITE_END()
