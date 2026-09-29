// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/network_definition.h>
#include <cybou/validator.h>
#include <test/cybou_test_helpers.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <limits>

BOOST_FIXTURE_TEST_SUITE(cybou_network_definition_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(network_definition_binds_poa_key_and_rejects_invalid_keys)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 1;
    const auto validator = cybou::GenerateValidatorKeyPair(seed);
    BOOST_REQUIRE(validator);
    const auto genesis = cybou::CreateDevGenesisState(validator->public_key);
    const auto poa_key = cybou::TestPoaFinalizerPublicKey(0x31);
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, poa_key);
    BOOST_CHECK(cybou::ValidateNetworkDefinition(definition) == cybou::NetworkDefinitionError::NONE);
    BOOST_CHECK(cybou::ComputeGenesisBlockId(definition.genesis_state_root, poa_key) == definition.genesis_block_id);
    BOOST_CHECK(cybou::NetworkId(cybou::CreateDevNetworkDefinition(genesis, cybou::TestPoaFinalizerPublicKey(0x32))) !=
        cybou::NetworkId(definition));

    auto invalid = definition;
    invalid.poa_finalizer_public_key.purpose = cybou::IdentityKeyPurpose::RECOVERY_ROOT;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::INVALID_POA_FINALIZER_KEY);
    invalid = definition;
    invalid.poa_finalizer_public_key.ml_dsa.clear();
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::INVALID_POA_FINALIZER_KEY);
    invalid = definition;
    invalid.genesis_block_id = uint256::ONE;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::GENESIS_BLOCK_ID_MISMATCH);
}

BOOST_AUTO_TEST_CASE(network_definition_v5_commits_poa_key_and_root_publication_fee_parameters)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 7;
    const auto key = cybou::GenerateValidatorKeyPair(seed);
    BOOST_REQUIRE(key);
    const auto genesis = cybou::CreateDevGenesisState(key->public_key);
    auto definition = cybou::CreateDevNetworkDefinition(genesis, cybou::TestPoaFinalizerPublicKey());
    const auto default_network_id = cybou::NetworkId(definition);
    BOOST_CHECK_EQUAL(definition.protocol_version, 5);
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
    legacy[0] = 4;
    BOOST_CHECK(!cybou::DeserializeNetworkDefinition(legacy));

    auto overflowing = definition;
    overflowing.protocol_parameters.root_publication_fee_per_chunk = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK(cybou::ValidateNetworkDefinition(overflowing) ==
        cybou::NetworkDefinitionError::INVALID_ROOT_PUBLICATION_FEES);
}

BOOST_AUTO_TEST_SUITE_END()
