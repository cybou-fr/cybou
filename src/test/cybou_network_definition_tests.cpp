// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/network_definition.h>
#include <cybou/network_genesis.h>
#include <cybou/official_networks.h>
#include <test/cybou_test_helpers.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <limits>

BOOST_FIXTURE_TEST_SUITE(cybou_network_definition_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(network_definition_binds_poa_key_and_rejects_invalid_keys)
{
    const auto genesis = cybou::CreateDevGenesisState();
    const auto poa_key = cybou::TestPoaFinalizerPublicKey(0x31);
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, poa_key, cybou::TestNetworkPublicKey(0x31));
    BOOST_CHECK(cybou::ValidateNetworkDefinition(definition) == cybou::NetworkDefinitionError::NONE);
    BOOST_CHECK(cybou::ComputeGenesisBlockId(definition.genesis_state_root, poa_key) == definition.genesis_block_id);

    auto invalid = definition;
    invalid.poa_finalizer_public_key.purpose = cybou::IdentityKeyPurpose::RECOVERY_ROOT;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::INVALID_POA_FINALIZER_KEY);
    invalid = definition;
    invalid.poa_finalizer_public_key.ml_dsa.clear();
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::INVALID_POA_FINALIZER_KEY);
    invalid = definition;
    invalid.network_public_key.purpose = cybou::IdentityKeyPurpose::POA_FINALIZER;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::INVALID_NETWORK_KEY);
    invalid = definition;
    invalid.network_public_key.ml_dsa.clear();
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::INVALID_NETWORK_KEY);
    invalid = definition;
    invalid.genesis_block_id = uint256::ONE;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(invalid) == cybou::NetworkDefinitionError::GENESIS_BLOCK_ID_MISMATCH);

    auto overflowing = definition;
    overflowing.protocol_parameters.root_publication_fee_per_chunk = std::numeric_limits<uint64_t>::max();
    BOOST_CHECK(cybou::ValidateNetworkDefinition(overflowing) ==
        cybou::NetworkDefinitionError::INVALID_ROOT_PUBLICATION_FEES);
}

BOOST_AUTO_TEST_CASE(network_binding_depends_only_on_the_network_public_key)
{
    const auto genesis = cybou::CreateDevGenesisState();
    const auto key = cybou::TestNetworkPublicKey(0x41);
    const auto binding = cybou::ComputeNetworkBinding(key);
    BOOST_CHECK(!binding.IsNull());
    BOOST_CHECK(cybou::ComputeNetworkBinding(cybou::TestNetworkPublicKey(0x42)) != binding);

    // One genesis per key: genesis contents never re-derive the binding.
    auto definition = cybou::CreateDevNetworkDefinition(genesis, cybou::TestPoaFinalizerPublicKey(0x41), key);
    definition.protocol_parameters.root_publication_fee_per_chunk = 13;
    BOOST_CHECK(cybou::ComputeNetworkBinding(definition.network_public_key) == binding);

    // The compiled DEVNET definition carries its exact Network Public Key (NetworkID).
    const auto& devnet = cybou::RequireOfficialNetwork("devnet");
    BOOST_CHECK(cybou::CanonicalSerializeNetworkPublicKey(devnet.network_definition.network_public_key) ==
        std::vector<unsigned char>(devnet.genesis.GetNetworkId().begin(), devnet.genesis.GetNetworkId().end()));
    BOOST_CHECK(cybou::ValidateNetworkDefinition(devnet.network_definition) == cybou::NetworkDefinitionError::NONE);
}

BOOST_AUTO_TEST_SUITE_END()
