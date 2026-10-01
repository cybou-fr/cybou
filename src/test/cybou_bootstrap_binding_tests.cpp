// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_binding.h>
#include <test/cybou_test_helpers.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_bootstrap_binding_tests)

namespace {

std::optional<std::vector<unsigned char>> TestNetworkFile(const cybou::IdentityHybridPublicKey& key)
{
    const auto genesis = cybou::CreateDevGenesisState();
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, key);
    return cybou::SerializeCybouNetworkFile({definition, genesis});
}

} // namespace

BOOST_AUTO_TEST_CASE(network_file_serialization_roundtrips_exact_consensus_bytes)
{
    const auto key = cybou::TestPoaFinalizerPublicKey(0x41);
    const auto network_file = TestNetworkFile(key);
    BOOST_REQUIRE(network_file);
    const auto decoded = cybou::DeserializeCybouNetworkFile(*network_file);
    BOOST_REQUIRE(decoded);
    const auto encoded_again = cybou::SerializeCybouNetworkFile(*decoded);
    BOOST_REQUIRE(encoded_again);
    BOOST_CHECK(*encoded_again == *network_file);

    auto altered = *decoded;
    altered.genesis.onboarding_pool++;
    BOOST_CHECK(!cybou::SerializeCybouNetworkFile(altered));
}

BOOST_AUTO_TEST_CASE(signed_binding_authenticates_generation_name_and_exact_network_file)
{
    cybou::RecoveryEntropy entropy{};
    entropy[0] = 0x42;
    const auto key = cybou::DeriveIdentityPublicKey(entropy, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(key);
    const auto network_file = TestNetworkFile(*key);
    BOOST_REQUIRE(network_file);

    const auto binding = cybou::CreateBootstrapNetworkBinding(1, "CYBOU DEV", *network_file, entropy);
    BOOST_REQUIRE(binding);
    BOOST_CHECK(cybou::VerifyBootstrapNetworkBinding(*binding));
    const auto encoded = cybou::EncodeBootstrapNetworkBinding(*binding);
    BOOST_REQUIRE(encoded);
    const auto decoded = cybou::DecodeBootstrapNetworkBinding(*encoded);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(decoded->generation == 1);
    BOOST_CHECK(decoded->display_name == "CYBOU DEV");
    BOOST_CHECK(decoded->network_id == binding->network_id);
    BOOST_CHECK(decoded->network_file == *network_file);

    auto changed_name = *binding;
    changed_name.display_name = "OTHER";
    BOOST_CHECK(!cybou::VerifyBootstrapNetworkBinding(changed_name));
    auto changed_generation = *binding;
    changed_generation.generation++;
    BOOST_CHECK(!cybou::VerifyBootstrapNetworkBinding(changed_generation));
    auto changed_hash = *binding;
    changed_hash.network_file_sha256[0] ^= 1;
    BOOST_CHECK(!cybou::VerifyBootstrapNetworkBinding(changed_hash));
    auto changed_file = *binding;
    changed_file.network_file.back() ^= 1;
    BOOST_CHECK(!cybou::VerifyBootstrapNetworkBinding(changed_file));

    auto truncated = *encoded;
    truncated.pop_back();
    BOOST_CHECK(!cybou::DecodeBootstrapNetworkBinding(truncated));
}

BOOST_AUTO_TEST_CASE(binding_creation_rejects_wrong_signer_and_malformed_metadata)
{
    cybou::RecoveryEntropy expected_entropy{};
    expected_entropy[0] = 0x43;
    const auto expected_key = cybou::DeriveIdentityPublicKey(expected_entropy, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(expected_key);
    const auto network_file = TestNetworkFile(*expected_key);
    BOOST_REQUIRE(network_file);

    cybou::RecoveryEntropy wrong_entropy{};
    wrong_entropy[0] = 0x44;
    BOOST_CHECK(!cybou::CreateBootstrapNetworkBinding(1, "CYBOU DEV", *network_file, wrong_entropy));
    BOOST_CHECK(!cybou::CreateBootstrapNetworkBinding(0, "CYBOU DEV", *network_file, expected_entropy));
    BOOST_CHECK(!cybou::CreateBootstrapNetworkBinding(1, "", *network_file, expected_entropy));
    BOOST_CHECK(!cybou::CreateBootstrapNetworkBinding(1, std::string(129, 'x'), *network_file, expected_entropy));

    auto invalid_file = *network_file;
    invalid_file[0] ^= 1;
    BOOST_CHECK(!cybou::CreateBootstrapNetworkBinding(1, "CYBOU DEV", invalid_file, expected_entropy));
}

BOOST_AUTO_TEST_SUITE_END()
