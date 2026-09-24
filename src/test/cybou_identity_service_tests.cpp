// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/identity_service.h>
#include <cybou/identity_material.h>
#include <cybou/network_definition.h>
#include <cybou/name_service.h>
#include <cybou/validator.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>

BOOST_FIXTURE_TEST_SUITE(cybou_identity_service_tests, BasicTestingSetup)

namespace {
struct RuntimeFixture {
    std::array<unsigned char, 32> validator_seed{};
    cybou::CybouState genesis;
    cybou::CybouNetworkDefinition definition;

    RuntimeFixture()
    {
        validator_seed[0] = 0x73;
        const auto keypair = cybou::GenerateValidatorKeyPair(validator_seed);
        genesis = cybou::CreateDevGenesisState(keypair->public_key);
        definition = cybou::CreateDevNetworkDefinition(genesis);
        definition.protocol_parameters.account_creation_work_bits = 0;
    }

    cybou::NodeRuntimeConfig Config() const
    {
        return {
            .network_definition = definition,
            .data_dir = "cybou-identity-service-test",
            .validator_private_key = validator_seed,
            .memory_only = true,
            .wipe_data = true,
        };
    }
};
} // namespace

BOOST_AUTO_TEST_CASE(account_creation_requires_prepared_durable_vault)
{
    RuntimeFixture fixture;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    const auto dir = std::filesystem::temp_directory_path() / "cybou-identity-service-vault-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    std::filesystem::remove(path);

    cybou::CybouIdentityService service{runtime, path};
    BOOST_CHECK(!service.CreateIdentitySync("correct horse battery staple").success);
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 0);

    const auto words = service.PrepareNewIdentity();
    BOOST_REQUIRE(words && cybou::DecodeRecoveryWords(*words));
    const auto result = service.CreateIdentitySync("correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(result.success, result.error_message);
    BOOST_CHECK_EQUAL(result.creation_height, 1);
    BOOST_CHECK_EQUAL(result.system_balance, fixture.definition.protocol_parameters.onboarding_bonus);
    BOOST_CHECK(std::filesystem::exists(path));

    const auto material = cybou::LoadIdentityMaterial(path, "correct horse battery staple");
    BOOST_REQUIRE(material);
    const auto account = cybou::AccountId::FromBytes(material->account_id);
    BOOST_REQUIRE(account);
    BOOST_CHECK(result.account_id == *account);
    BOOST_CHECK(result.account_id.Value() != *service.GetKeyStore().GetPublicKey());

    cybou::CybouIdentityService reopened{runtime, path};
    BOOST_CHECK(!reopened.LoadVault("wrong password"));
    BOOST_REQUIRE(reopened.LoadVault("correct horse battery staple"));
    BOOST_CHECK(reopened.GetAccountId() == account);
    BOOST_CHECK(reopened.GetPhase() == cybou::IdentityCreationPhase::ACTIVE);

    const auto restored_path = dir / "restored.cybou";
    std::filesystem::remove(restored_path);
    cybou::CybouIdentityService restored{runtime, restored_path};
    auto wrong_words = *words;
    wrong_words[0] = "invalid";
    BOOST_CHECK(!restored.RestoreIdentitySync(wrong_words, "another strong vault password").success);
    BOOST_CHECK(!std::filesystem::exists(restored_path));
    const auto restore_result = restored.RestoreIdentitySync(*words, "another strong vault password");
    BOOST_REQUIRE_MESSAGE(restore_result.success, restore_result.error_message);
    BOOST_CHECK(restore_result.account_id == result.account_id);
    BOOST_CHECK_EQUAL(restore_result.system_balance, result.system_balance);
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 2);
    const auto loaded_state = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded_state && loaded_state.state);
    const auto* record = loaded_state.state->identities.Find(result.account_id);
    BOOST_REQUIRE(record);
    BOOST_CHECK_EQUAL(record->devices.size(), 2);
    cybou::CybouIdentityService reopened_restored{runtime, restored_path};
    BOOST_REQUIRE(reopened_restored.LoadVault("another strong vault password"));
    BOOST_CHECK(reopened_restored.GetPhase() == cybou::IdentityCreationPhase::ACTIVE);
    BOOST_CHECK(!reopened_restored.RestoreIdentitySync(*words, "wrong password").success);
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 2);

    std::filesystem::remove(path);
    std::filesystem::remove(restored_path);
}

BOOST_AUTO_TEST_CASE(vault_save_failure_prevents_broadcast)
{
    RuntimeFixture fixture;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    const auto path = std::filesystem::temp_directory_path() /
        "cybou-nonexistent-parent-for-vault" / "identity.cybou";
    cybou::CybouIdentityService service{runtime, path};
    BOOST_REQUIRE(service.PrepareNewIdentity());
    const auto result = service.CreateIdentitySync("correct horse battery staple");
    BOOST_CHECK(!result.success);
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 0);
    BOOST_CHECK(!std::filesystem::exists(path));
}

BOOST_AUTO_TEST_CASE(name_claim_saves_secret_before_commit_and_finalizes_owner)
{
    RuntimeFixture fixture;
    fixture.definition.protocol_parameters.name_claim_work_bits = 0;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-service-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    const auto claim_path = dir / "identity.cybou.nameclaim";
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    const auto created = identity.CreateIdentitySync("correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(created.success, created.error_message);
    cybou::CybouNameService names{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!names.ClaimSync("bad", "correct horse battery staple").success);
    BOOST_CHECK(!std::filesystem::exists(claim_path));
    const auto claimed = names.ClaimSync("stanislav", "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(claimed.success, claimed.message);
    BOOST_CHECK(std::filesystem::exists(claim_path));
    std::ifstream claim_file{claim_path, std::ios::binary};
    const std::string encrypted{std::istreambuf_iterator<char>{claim_file}, std::istreambuf_iterator<char>{}};
    claim_file.close();
    BOOST_CHECK(encrypted.find("stanislav") == std::string::npos);
    BOOST_CHECK(identity.GetFinalizedPrimaryName() == std::optional<std::string>{"stanislav"});
    const auto loaded = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded && loaded.state);
    const auto* owner = loaded.state->names.Resolve("stanislav");
    BOOST_REQUIRE(owner);
    BOOST_CHECK(*owner == created.account_id);
    BOOST_CHECK(names.ClaimSync("stanislav", "correct horse battery staple").success);
    BOOST_CHECK(!names.ClaimSync("anothername", "correct horse battery staple").success);
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
}

BOOST_AUTO_TEST_CASE(name_commit_requires_durable_encrypted_claim)
{
    RuntimeFixture fixture;
    fixture.definition.protocol_parameters.name_claim_work_bits = 0;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-save-failure-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    std::filesystem::remove(path);
    auto claim_path = path;
    claim_path += ".nameclaim";
    std::filesystem::remove(claim_path);
    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    BOOST_REQUIRE(identity.CreateIdentitySync("correct horse battery staple").success);
    const auto before = runtime.GetFinalizedHeight();
    std::filesystem::create_directory(claim_path);
    cybou::CybouNameService names{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!names.ClaimSync("stanislav", "correct horse battery staple").success);
    BOOST_CHECK(runtime.GetFinalizedHeight() == before);
    const auto loaded = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded && loaded.state);
    BOOST_CHECK(loaded.state->names.pending_commits.empty());
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
}

BOOST_AUTO_TEST_CASE(name_claim_resumes_after_commit_with_correct_password)
{
    RuntimeFixture fixture;
    fixture.definition.protocol_parameters.name_claim_work_bits = 0;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-resume-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    auto claim_path = path;
    claim_path += ".nameclaim";
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    BOOST_REQUIRE(identity.CreateIdentitySync("correct horse battery staple").success);
    cybou::CybouNameService interrupted{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!interrupted.ClaimSync("stanislav", "correct horse battery staple",
        [&](cybou::NameClaimPhase phase, const std::string&) {
            if (phase == cybou::NameClaimPhase::WAITING_FOR_COMMIT) interrupted.Cancel();
        }).success);
    BOOST_CHECK(std::filesystem::exists(claim_path));
    const auto height = runtime.GetFinalizedHeight();
    cybou::CybouNameService resumed{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!resumed.ClaimSync("stanislav", "wrong password").success);
    BOOST_CHECK(runtime.GetFinalizedHeight() == height);
    const auto claimed = resumed.ClaimSync("stanislav", "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(claimed.success, claimed.message);
    BOOST_CHECK(identity.GetFinalizedPrimaryName() == std::optional<std::string>{"stanislav"});
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
}

BOOST_AUTO_TEST_SUITE_END()
