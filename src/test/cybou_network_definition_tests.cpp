// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/network_definition.h>
#include <cybou/validator.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <limits>

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

BOOST_AUTO_TEST_CASE(network_definition_v4_commits_root_publication_fee_parameters)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 7;
    const auto key = cybou::GenerateValidatorKeyPair(seed);
    BOOST_REQUIRE(key);
    const auto genesis = cybou::CreateDevGenesisState(key->public_key);
    auto definition = cybou::CreateDevNetworkDefinition(genesis);
    const auto default_network_id = cybou::NetworkId(definition);
    BOOST_CHECK_EQUAL(definition.protocol_version, 4);
    BOOST_CHECK(cybou::ValidateNetworkDefinition(definition) == cybou::NetworkDefinitionError::NONE);

    definition.protocol_parameters.root_publication_fee_per_started_kib = 9;
    definition.protocol_parameters.root_publication_fee_per_chunk = 13;
    BOOST_CHECK(cybou::NetworkId(definition) != default_network_id);
    const auto encoded = cybou::SerializeNetworkDefinition(definition);
    const auto decoded = cybou::DeserializeNetworkDefinition(encoded);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(cybou::SerializeNetworkDefinition(*decoded) == encoded);
    BOOST_CHECK(decoded->protocol_parameters.root_publication_fee_per_started_kib == 9);
    BOOST_CHECK(decoded->protocol_parameters.root_publication_fee_per_chunk == 13);

    auto legacy = encoded;
    legacy[0] = 3;
    BOOST_CHECK(!cybou::DeserializeNetworkDefinition(legacy));

    auto overflowing = definition;
    overflowing.protocol_parameters.root_publication_fee_per_chunk = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK(cybou::ValidateNetworkDefinition(overflowing) ==
        cybou::NetworkDefinitionError::INVALID_ROOT_PUBLICATION_FEES);
}

BOOST_AUTO_TEST_SUITE_END()
