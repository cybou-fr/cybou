// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_genesis.h>
#include <cybou/identity_crypto.h>
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
    spec.version = cybou::CYBOU_NETWORK_GENESIS_VERSION;
    spec.network_public_key = *net_pub;
    spec.genesis_state_root = uint256::ONE;
    spec.poa_finalizer_public_key = *poa_pub;
    spec.protocol_parameters.account_creation_work_bits = 8;
    spec.protocol_parameters.max_account_creates_per_block = 16;
    spec.protocol_parameters.epoch_blocks = 100;
    spec.protocol_parameters.name_claim_work_bits = 8;
    spec.protocol_parameters.name_commit_min_depth = 2;
    spec.protocol_parameters.name_commit_max_lifetime = 10;
    spec.protocol_parameters.max_pending_name_commits = 32;

    // Add initial Authority baseline for bootstrap recovery key
    cybou::IdentityKeyId bootstrap_recovery_id{};
    bootstrap_recovery_id.fill(0x01);
    spec.initial_authority.push_back({bootstrap_recovery_id, 1000001});

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
    BOOST_CHECK_EQUAL(verified->GetInitialAuthority().size(), 1);
    BOOST_CHECK_EQUAL(verified->GetInitialAuthority()[0].initial_authority, 1000001);
    BOOST_CHECK_EQUAL(verified->GetNetworkId().size(), net_id_bytes.size());

    // Tampering test: modify state root and check failure
    auto tampered = spec;
    tampered.genesis_state_root = uint256::ZERO;
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(tampered) != cybou::NetworkGenesisError::NONE);
    BOOST_CHECK(!cybou::VerifiedNetworkGenesis::Create(tampered).has_value());
}

BOOST_AUTO_TEST_CASE(test_initial_authority_hardening_and_validation)
{
    std::array<unsigned char, 32> net_secret{};
    net_secret.fill(0x11);
    auto net_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT);
    auto poa_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(net_pub && poa_pub);

    cybou::NetworkGenesis spec;
    spec.version = cybou::CYBOU_NETWORK_GENESIS_VERSION;
    spec.network_public_key = *net_pub;
    spec.genesis_state_root = uint256::ONE;
    spec.poa_finalizer_public_key = *poa_pub;
    spec.protocol_parameters = cybou::DevProtocolParameters();

    cybou::IdentityKeyId id1{}, id2{};
    id1[0] = 0x10;
    id2[0] = 0x20;

    // Zero ID rejected
    cybou::IdentityKeyId zero_id{};
    spec.initial_authority = {{zero_id, 1000001}};
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(spec) == cybou::NetworkGenesisError::INVALID_INITIAL_AUTHORITY);

    // Zero authority value rejected
    spec.initial_authority = {{id1, 0}};
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(spec) == cybou::NetworkGenesisError::INVALID_INITIAL_AUTHORITY);

    // Unsorted keys rejected
    spec.initial_authority = {{id2, 1000001}, {id1, 1000002}};
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(spec) == cybou::NetworkGenesisError::INVALID_INITIAL_AUTHORITY);

    // Duplicate keys rejected
    spec.initial_authority = {{id1, 1000001}, {id1, 1000002}};
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(spec) == cybou::NetworkGenesisError::INVALID_INITIAL_AUTHORITY);

    // Strictly canonical order accepted
    spec.initial_authority = {{id1, 1000001}, {id2, 1000002}};
    const auto digest = cybou::ComputeNetworkGenesisDigest(spec);
    auto sig = cybou::SignIdentityMessage(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    BOOST_REQUIRE(sig.has_value());
    spec.signature = *sig;
    BOOST_CHECK(cybou::VerifySignedNetworkGenesis(spec) == cybou::NetworkGenesisError::NONE);
}

BOOST_AUTO_TEST_CASE(test_cyg1_signed_genesis_bundle_lifecycle)
{
    std::array<unsigned char, 32> net_secret{};
    net_secret.fill(0x77);
    auto net_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT);
    auto poa_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(net_pub && poa_pub);

    auto state = cybou::CreateDevGenesisState();
    cybou::IdentityKeyId auth_id{};
    auth_id[0] = 0x42;
    state.genesis_allocations.emplace(auth_id, cybou::GenesisAllocation{.balance = 100, .label = "bootstrap"});
    const auto state_root = cybou::CybouStateHash(state);
    BOOST_REQUIRE(state_root.has_value());

    cybou::NetworkGenesis spec;
    spec.version = cybou::CYBOU_NETWORK_GENESIS_VERSION;
    spec.network_public_key = *net_pub;
    spec.genesis_state_root = *state_root;
    spec.poa_finalizer_public_key = *poa_pub;
    spec.protocol_parameters = cybou::DevProtocolParameters();
    spec.initial_authority.push_back({auth_id, 1000001});

    const auto digest = cybou::ComputeNetworkGenesisDigest(spec);
    auto sig = cybou::SignIdentityMessage(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    BOOST_REQUIRE(sig.has_value());
    spec.signature = *sig;

    // Serialization to CYG1 bundle
    const auto bundle_bytes = cybou::SerializeNetworkGenesisBundle(spec, state);
    BOOST_REQUIRE(bundle_bytes.has_value());
    BOOST_CHECK_EQUAL(bundle_bytes->at(0), 'C');
    BOOST_CHECK_EQUAL(bundle_bytes->at(1), 'Y');
    BOOST_CHECK_EQUAL(bundle_bytes->at(2), 'G');
    BOOST_CHECK_EQUAL(bundle_bytes->at(3), '1');

    // Verification of CYG1 bundle
    auto verified_bundle = cybou::VerifyNetworkGenesisBundle(*bundle_bytes);
    BOOST_REQUIRE(verified_bundle.has_value());
    BOOST_CHECK(verified_bundle->genesis_digest == digest);
    BOOST_CHECK(verified_bundle->network_definition.genesis_state_root == *state_root);
    BOOST_CHECK(verified_bundle->network_definition.poa_finalizer_public_key == *poa_pub);

    // Transparent deserialization via DeserializeCybouNetworkFile
    auto net_file = cybou::DeserializeCybouNetworkFile(*bundle_bytes);
    BOOST_REQUIRE(net_file.has_value());
    BOOST_CHECK(net_file->definition.genesis_state_root == *state_root);

    // Initial authority not in state allocations fails bundle serialization & verification
    auto bad_state = state;
    bad_state.genesis_allocations.erase(auth_id);
    const auto bad_state_root = cybou::CybouStateHash(bad_state);
    BOOST_REQUIRE(bad_state_root.has_value());
    auto bad_spec = spec;
    bad_spec.genesis_state_root = *bad_state_root;
    const auto bad_digest = cybou::ComputeNetworkGenesisDigest(bad_spec);
    auto bad_sig = cybou::SignIdentityMessage(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{bad_digest.begin(), bad_digest.size()});
    BOOST_REQUIRE(bad_sig.has_value());
    bad_spec.signature = *bad_sig;

    BOOST_CHECK(!cybou::SerializeNetworkGenesisBundle(bad_spec, bad_state).has_value());
}

BOOST_AUTO_TEST_CASE(test_official_network_profiles_constitution)
{
    // DEVNET profile checks
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(cybou::OFFICIAL_DEVNET_PROFILE.kind), 0);
    BOOST_CHECK_EQUAL(cybou::OFFICIAL_DEVNET_PROFILE.name, "DEVNET");
    BOOST_REQUIRE_EQUAL(cybou::OFFICIAL_DEVNET_PROFILE.bootstrap_locators.size(), 1);
    BOOST_CHECK_EQUAL(cybou::OFFICIAL_DEVNET_PROFILE.bootstrap_locators[0].host, "51.255.46.58");
    BOOST_CHECK_EQUAL(cybou::OFFICIAL_DEVNET_PROFILE.bootstrap_locators[0].port, 29461);

    // SPKI pin check
    BOOST_CHECK_EQUAL(cybou::OFFICIAL_DEVNET_PROFILE.bootstrap_locators[0].tls_spki_sha256[0], 0xd8);
    BOOST_CHECK_EQUAL(cybou::OFFICIAL_DEVNET_PROFILE.bootstrap_locators[0].tls_spki_sha256[31], 0xdb);

    // MAINNET profile checks
    BOOST_CHECK_EQUAL(static_cast<uint8_t>(cybou::OFFICIAL_MAINNET_PROFILE.kind), 1);
    BOOST_CHECK_EQUAL(cybou::OFFICIAL_MAINNET_PROFILE.name, "MAINNET");
}

BOOST_AUTO_TEST_SUITE_END()
