// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/account_creation.h>
#include <cybou/identity_service.h>
#include <cybou/node_runtime.h>
#include <cybou/signing.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <chrono>
#include <future>

BOOST_FIXTURE_TEST_SUITE(cybou_identity_service_tests, BasicTestingSetup)

namespace {

cybou::CybouState CreateTestGenesis(const uint256& val_pub)
{
    return cybou::CybouState{
        .onboarding_pool = 1'000'000,
        .security_reward_pool = 0,
        .pending_fee_pool = 0,
        .accounts = {},
        .validator_set = {
            .version = cybou::VALIDATOR_SET_VERSION,
            .validators = {{
                .validator_id = val_pub,
                .consensus_public_key = val_pub,
                .weight = 1,
            }},
        },
    };
}

cybou::CybouNetworkDefinition CreateTestNetworkDefinition(const cybou::CybouState& genesis)
{
    auto params = cybou::DevProtocolParameters();
    params.account_creation_work_bits = 0; // fast PoW for tests
    return cybou::CybouNetworkDefinition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = cybou::CybouStateHash(genesis),
        .genesis_state_root = cybou::CybouStateHash(genesis),
        .protocol_parameters = params,
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(genesis.validator_set),
        .operator_authority = std::nullopt,
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(identity_service_creates_and_activates_identity)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(1);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);

    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-identity-test-1",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };

    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

    cybou::CybouIdentityService identity_service{runtime};
    BOOST_CHECK_EQUAL(static_cast<int>(identity_service.GetPhase()), static_cast<int>(cybou::IdentityCreationPhase::IDLE));
    BOOST_CHECK(!identity_service.GetAccountId().has_value());

    std::vector<cybou::IdentityCreationPhase> visited_phases;
    auto on_phase = [&](cybou::IdentityCreationPhase phase, const std::string&) {
        visited_phases.push_back(phase);
    };

    const auto result = identity_service.CreateIdentitySync(on_phase);
    BOOST_REQUIRE(result.success);
    BOOST_CHECK_EQUAL(static_cast<int>(result.final_phase), static_cast<int>(cybou::IdentityCreationPhase::ACTIVE));
    BOOST_CHECK_EQUAL(result.creation_height, 1);
    BOOST_CHECK_EQUAL(result.system_balance, definition.protocol_parameters.onboarding_bonus);
    BOOST_CHECK(!result.account_id.IsNull());

    // Verify phases were executed in canonical sequence
    BOOST_REQUIRE(visited_phases.size() >= 4);
    BOOST_CHECK_EQUAL(static_cast<int>(visited_phases[0]), static_cast<int>(cybou::IdentityCreationPhase::CREATING_KEYS));
    BOOST_CHECK_EQUAL(static_cast<int>(visited_phases[1]), static_cast<int>(cybou::IdentityCreationPhase::PERFORMING_WORK));
    BOOST_CHECK_EQUAL(static_cast<int>(visited_phases[2]), static_cast<int>(cybou::IdentityCreationPhase::BROADCASTING));
    BOOST_CHECK_EQUAL(static_cast<int>(visited_phases[3]), static_cast<int>(cybou::IdentityCreationPhase::WAITING_FOR_FINALITY));

    // Verify account state in consensus runtime
    const auto acc = runtime.GetAccountState(result.account_id);
    BOOST_REQUIRE(acc.has_value());
    BOOST_CHECK_EQUAL(acc->creation_height, 1);
    BOOST_CHECK_EQUAL(acc->system_balance, definition.protocol_parameters.onboarding_bonus);
    BOOST_CHECK_EQUAL(acc->balance, 0);

    // Repeated call for the same identity is idempotent and immediately active
    const auto repeat_result = identity_service.CreateIdentitySync();
    BOOST_CHECK(repeat_result.success);
    BOOST_CHECK(repeat_result.account_id == result.account_id);
}

BOOST_AUTO_TEST_CASE(identity_service_loads_existing_identity)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(1);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);

    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-identity-test-2",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };

    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

    cybou::CybouIdentityService identity_service{runtime};

    std::array<unsigned char, 32> user_key{};
    user_key.fill(9);
    BOOST_REQUIRE(identity_service.LoadExistingIdentity(user_key));

    const auto expected_pub = *cybou::DeriveEd25519PublicKey(user_key);
    BOOST_REQUIRE(identity_service.GetAccountId().has_value());
    BOOST_CHECK(identity_service.GetAccountId()->Value() == expected_pub);
    // Not active yet because not registered in genesis
    BOOST_CHECK_EQUAL(static_cast<int>(identity_service.GetPhase()), static_cast<int>(cybou::IdentityCreationPhase::IDLE));
}

BOOST_AUTO_TEST_CASE(identity_service_creates_identity_asynchronously)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(1);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);

    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-identity-test-3",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };

    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

    cybou::CybouIdentityService identity_service{runtime};

    std::promise<cybou::IdentityCreationResult> completion_promise;
    auto completion_future = completion_promise.get_future();

    identity_service.CreateIdentityAsync(
        /*on_phase=*/nullptr,
        /*on_complete=*/[&](const cybou::IdentityCreationResult& res) {
            completion_promise.set_value(res);
        });

    BOOST_REQUIRE(completion_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
    const auto result = completion_future.get();
    BOOST_REQUIRE(result.success);
    BOOST_CHECK_EQUAL(static_cast<int>(result.final_phase), static_cast<int>(cybou::IdentityCreationPhase::ACTIVE));
    BOOST_CHECK_EQUAL(result.creation_height, 1);
    BOOST_CHECK_EQUAL(result.system_balance, definition.protocol_parameters.onboarding_bonus);
}

BOOST_AUTO_TEST_CASE(keystore_encrypts_and_recovers_identity_safely)
{
    cybou::CybouKeyStore ks1;
    BOOST_CHECK(!ks1.HasKey());
    BOOST_REQUIRE(ks1.GenerateNew());
    BOOST_CHECK(ks1.HasKey());

    const auto pub1 = ks1.GetPublicKey();
    const auto acc1 = ks1.GetAccountId();
    BOOST_REQUIRE(pub1.has_value() && acc1.has_value());
    BOOST_CHECK(acc1->Value() == *pub1);

    // Test signing
    const uint256 test_digest{uint256::FromUserHex("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef").value()};
    const auto sig1 = ks1.Sign(test_digest);
    BOOST_REQUIRE(sig1.has_value());
    BOOST_CHECK(cybou::VerifyUserSignature(*pub1, *sig1, test_digest));

    // Save to temp file
    const auto temp_path = std::filesystem::temp_directory_path() / "cybou_test_keystore.key";
    std::filesystem::remove(temp_path);
    BOOST_REQUIRE(ks1.SaveToFile(temp_path));
    BOOST_REQUIRE(std::filesystem::exists(temp_path));

    // Load with fresh keystore
    cybou::CybouKeyStore ks2;
    BOOST_REQUIRE(ks2.LoadFromFile(temp_path));
    BOOST_CHECK(ks2.HasKey());
    BOOST_CHECK_EQUAL(ks2.GetPublicKey()->GetHex(), pub1->GetHex());
    BOOST_CHECK_EQUAL(ks2.GetAccountId()->Value().GetHex(), acc1->Value().GetHex());

    // Verify fresh keystore can sign identically
    const auto sig2 = ks2.Sign(test_digest);
    BOOST_REQUIRE(sig2.has_value());
    BOOST_CHECK(sig1 == sig2);

    // Clean up
    std::filesystem::remove(temp_path);
}

BOOST_AUTO_TEST_CASE(identity_service_fails_early_if_key_cannot_be_persisted_before_pow)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(1);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);
    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-identity-presave-fail-test",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

    // Storage path in an uncreatable / nonexistent root path
#ifdef WIN32
    const std::filesystem::path invalid_path = "Z:\\nonexistent_volume_root_xyz\\test.key";
#else
    const std::filesystem::path invalid_path = "/nonexistent_root_dir_xyz/test.key";
#endif

    cybou::CybouIdentityService service(runtime, invalid_path);
    const auto result = service.CreateIdentitySync();
    BOOST_CHECK(!result.success);
    BOOST_CHECK(result.final_phase == cybou::IdentityCreationPhase::FAILED);
    BOOST_CHECK_EQUAL(result.error_message, "Failed to persist identity key to disk before broadcast");
    // Ensure no blocks or operations were produced/processed
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 0);
}

BOOST_AUTO_TEST_CASE(identity_service_pre_saves_valid_key_before_broadcast)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(1);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);
    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-identity-presave-success-test",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

    const auto temp_dir = std::filesystem::temp_directory_path() / "cybou_presave_test";
    std::filesystem::create_directories(temp_dir);
    const auto key_path = temp_dir / "valid_presave.key";
    std::filesystem::remove(key_path);

    cybou::CybouIdentityService service(runtime, key_path);
    const auto result = service.CreateIdentitySync();
    BOOST_REQUIRE(result.success);
    BOOST_CHECK(result.final_phase == cybou::IdentityCreationPhase::ACTIVE);

    // Verify key exists on disk and loads the exact same account
    BOOST_CHECK(std::filesystem::exists(key_path));
    cybou::CybouKeyStore loaded_ks;
    BOOST_REQUIRE(loaded_ks.LoadFromFile(key_path));
    BOOST_CHECK(loaded_ks.GetAccountId() == result.account_id);

    std::filesystem::remove_all(temp_dir);
}

BOOST_AUTO_TEST_SUITE_END()
