// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/network_definition.h>
#include <cybou/validator.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>

BOOST_FIXTURE_TEST_SUITE(cybou_network_definition_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(four_validator_genesis_is_canonical_and_rejects_duplicates)
{
    std::array<cybou::IdentityHybridPublicKey, 4> keys;
    for (size_t i = 0; i < keys.size(); ++i) {
        std::array<unsigned char, 32> seed{};
        seed[0] = static_cast<unsigned char>(i + 1);
        const auto pair = cybou::GenerateValidatorKeyPair(seed);
        BOOST_REQUIRE(pair);
        keys[i] = pair->public_key;
    }
    const auto genesis = cybou::CreateDevGenesisState(keys);
    BOOST_REQUIRE(genesis);
    BOOST_CHECK_EQUAL(genesis->validator_set.Size(), 4U);
    BOOST_CHECK(cybou::ValidateValidatorSet(genesis->validator_set) == cybou::ValidatorSetValidationError::NONE);
    const auto definition = cybou::CreateDevNetworkDefinition(*genesis);
    BOOST_CHECK(cybou::ValidateNetworkDefinition(definition) == cybou::NetworkDefinitionError::NONE);
    std::reverse(keys.begin(), keys.end());
    const auto reordered = cybou::CreateDevGenesisState(keys);
    BOOST_REQUIRE(reordered);
    BOOST_CHECK(cybou::NetworkId(cybou::CreateDevNetworkDefinition(*reordered)) == cybou::NetworkId(definition));
    keys[0] = keys[1];
    BOOST_CHECK(!cybou::CreateDevGenesisState(keys));
}

BOOST_AUTO_TEST_SUITE_END()
