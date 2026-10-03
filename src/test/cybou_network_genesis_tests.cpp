// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_genesis.h>
#include <cybou/identity_crypto.h>
#include <cybou/official_devnet_constants.h>
#include <cybou/official_networks.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <span>

BOOST_AUTO_TEST_SUITE(cybou_network_genesis_tests)

BOOST_AUTO_TEST_CASE(test_network_key_and_signed_genesis_lifecycle)
{
    // Generate simulated offline Network Key
    std::array<unsigned char, 32> net_secret{};
    net_secret.fill(0x42);
    auto net_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT);
    BOOST_REQUIRE(net_pub.has_value());
    BOOST_CHECK(net_pub->purpose == cybou::IdentityKeyPurpose::NETWORK_ROOT);
    BOOST_CHECK_EQUAL(net_pub->ml_dsa.size(), 1952);

    // Canonical serialization of Network Public Key (NetworkID bytes)
    const auto net_id_bytes = cybou::CanonicalSerializeNetworkPublicKey(*net_pub);
    BOOST_CHECK_GT(net_id_bytes.size(), 1952 + 32);
    auto deserialized_net_pub = cybou::CanonicalDeserializeNetworkPublicKey(net_id_bytes);
    BOOST_REQUIRE(deserialized_net_pub.has_value());
    BOOST_CHECK(*deserialized_net_pub == *net_pub);

    // Generate PoA Key
    std::array<unsigned char, 32> poa_secret{};
    poa_secret.fill(0x55);
    auto poa_pub = cybou::DeriveIdentityPublicKey(poa_secret, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(poa_pub.has_value());

    // Prepare NetworkGenesis specification
    cybou::NetworkGenesis spec;
    spec.network_public_key = *net_pub;
    spec.genesis_state_root = cybou::Hash256::ONE;
    spec.poa_finalizer_public_key = *poa_pub;
    spec.protocol_parameters.account_creation_work_bits = 8;
    spec.protocol_parameters.max_account_creates_per_block = 16;
    spec.protocol_parameters.epoch_blocks = 100;
    spec.protocol_parameters.name_claim_work_bits = 8;
    spec.protocol_parameters.name_commit_min_depth = 2;
    spec.protocol_parameters.name_commit_max_lifetime = 10;
    spec.protocol_parameters.max_pending_name_commits = 32;

    // Compute digest and sign with Network Private Key
    const auto digest = cybou::ComputeNetworkGenesisDigest(spec);
    auto signature = cybou::SignIdentityMessage(
        net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    BOOST_REQUIRE(signature.has_value());
    spec.signature = *signature;

    // Verify raw signed specification
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(spec) == cybou::NetworkGenesisError::NONE);

    // Serialization / Deserialization of Signed Genesis
    const auto wire_bytes = cybou::SerializeSignedNetworkGenesis(spec);
    auto parsed_spec = cybou::DeserializeSignedNetworkGenesis(wire_bytes);
    BOOST_REQUIRE(parsed_spec.has_value());
    BOOST_CHECK(*parsed_spec == spec);

    // Verify parsed spec
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(*parsed_spec) == cybou::NetworkGenesisError::NONE);

    // VerifiedNetworkGenesis high-integrity container
    auto verified = cybou::VerifiedNetworkGenesis::Create(*parsed_spec);
    BOOST_REQUIRE(verified.has_value());
    BOOST_CHECK(verified->GetGenesisDigest() == digest);
    BOOST_CHECK(verified->GetNetworkPublicKey() == *net_pub);
    BOOST_CHECK_EQUAL(verified->GetNetworkId().size(), net_id_bytes.size());

    // Tampering test: modify state root and check failure
    auto tampered = spec;
    tampered.genesis_state_root = cybou::Hash256::ZERO;
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(tampered) != cybou::NetworkGenesisError::NONE);
    BOOST_CHECK(!cybou::VerifiedNetworkGenesis::Create(tampered).has_value());
}

BOOST_AUTO_TEST_CASE(signed_genesis_rejects_noncanonical_encoding)
{
    std::array<unsigned char, 32> net_secret{};
    net_secret.fill(0x77);
    auto net_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT);
    auto poa_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(net_pub && poa_pub);
    const auto state_root = cybou::CybouStateHash(cybou::CreateDevGenesisState());
    BOOST_REQUIRE(state_root);
    cybou::NetworkGenesis spec;
    spec.network_public_key = *net_pub;
    spec.genesis_state_root = *state_root;
    spec.poa_finalizer_public_key = *poa_pub;
    spec.protocol_parameters = cybou::DevProtocolParameters();
    const auto digest = cybou::ComputeNetworkGenesisDigest(spec);
    auto sig = cybou::SignIdentityMessage(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    BOOST_REQUIRE(sig);
    spec.signature = *sig;
    const auto bytes = cybou::SerializeSignedNetworkGenesis(spec);
    BOOST_CHECK(cybou::DeserializeSignedNetworkGenesis(bytes) == spec);
    auto truncated = bytes;
    truncated.pop_back();
    BOOST_CHECK(!cybou::DeserializeSignedNetworkGenesis(truncated));
    auto noncanonical = bytes;
    noncanonical[cybou::SerializeNetworkGenesisPayload(spec).size() - 1] = 2;
    BOOST_CHECK(!cybou::DeserializeSignedNetworkGenesis(noncanonical));
}

BOOST_AUTO_TEST_CASE(compiled_devnet_is_the_only_official_startup_source)
{
    // The compiled signed genesis verifies under the compiled Network Public Key
    // and its compiled initial state matches the signed state root.
    const auto& devnet = cybou::RequireOfficialNetwork("devnet");
    const auto network_binding = devnet.genesis.GetNetworkId();
    BOOST_CHECK(std::equal(network_binding.begin(), network_binding.end(),
        cybou::devnet_constants::NETWORK_ID_BYTES.begin(), cybou::devnet_constants::NETWORK_ID_BYTES.end()));
    BOOST_CHECK(devnet.kind == cybou::NetworkKind::DEVNET);
    BOOST_CHECK(cybou::CybouStateHash(devnet.genesis_state) == devnet.genesis.GetGenesisStateRoot());
    BOOST_CHECK(devnet.genesis.GetPoaPublicKey() == devnet.genesis.GetPoaPublicKey());
    BOOST_CHECK(&cybou::RequireOfficialNetwork("DEVNET") == &devnet);

    BOOST_CHECK_THROW(cybou::RequireOfficialNetwork("mainnet"), std::runtime_error);
    BOOST_CHECK_THROW(cybou::RequireOfficialNetwork("network.bin"), std::runtime_error);
    BOOST_CHECK_THROW(cybou::RequireOfficialNetwork(""), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(official_bootstrap_locator_is_an_ordinary_rendezvous_peer)
{
    const auto& devnet = cybou::RequireOfficialNetwork(cybou::NetworkKind::DEVNET);
    BOOST_CHECK_EQUAL(devnet.name, "DEVNET");
    BOOST_REQUIRE_EQUAL(devnet.rendezvous_locators.size(), 1U);
    BOOST_CHECK_EQUAL(devnet.rendezvous_locators[0].host, "51.255.46.58");
    BOOST_CHECK_EQUAL(devnet.rendezvous_locators[0].port, 29461);
    BOOST_CHECK_EQUAL(devnet.rendezvous_locators[0].tls_spki_sha256[0], 0xd8);
    BOOST_CHECK_EQUAL(devnet.rendezvous_locators[0].tls_spki_sha256[31], 0xdb);
    BOOST_CHECK_THROW(cybou::RequireOfficialNetwork(cybou::NetworkKind::MAINNET), std::runtime_error);
}


BOOST_AUTO_TEST_SUITE_END()
