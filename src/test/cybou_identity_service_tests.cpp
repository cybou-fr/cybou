// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/operation_submit.h>
#include <cybou/poa_finalizer.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/identity_service.h>
#include <cybou/wallet_service.h>
#include <cybou/identity_material.h>
#include <cybou/network_genesis.h>
#include <cybou/name_service.h>
#include <test/cybou_test_helpers.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

BOOST_FIXTURE_TEST_SUITE(cybou_identity_service_tests, CybouTestSetup)

namespace {
struct RuntimeFixture {
    std::array<unsigned char, 32> validator_seed{};
    cybou::CybouState genesis;
    cybou::VerifiedNetworkGenesis definition{cybou::CreateTestNetworkGenesis(cybou::CreateTestGenesisState(),
        cybou::TestPoaFinalizerPublicKey(), cybou::TestNetworkPublicKey())};
    std::filesystem::path data_dir;

    RuntimeFixture()
    {
        data_dir = std::filesystem::temp_directory_path() / ("cybou-identity-service-runtime-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::remove_all(data_dir);
        validator_seed[0] = 0x73;
        genesis = cybou::CreateTestGenesisState();
        definition = cybou::CreateTestNetworkGenesis(genesis, cybou::TestPoaFinalizerPublicKey(validator_seed[0]), cybou::TestNetworkPublicKey(validator_seed[0]));
        definition = cybou::WithTestGenesisParameters(definition, [](auto& params) { params.account_creation_work_bits = 0; });
    }

    ~RuntimeFixture()
    {
        std::error_code ec;
        std::filesystem::remove_all(data_dir, ec);
    }

    cybou::NodeRuntimeConfig Config() const
    {
        return {
            .network_genesis = definition,
            .data_dir = data_dir,
            .poa_finalizer_recovery_entropy = validator_seed,
            .memory_only = true,
            .wipe_data = true,
            .operation_work_bits = 0,
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
    BOOST_CHECK_EQUAL(result.system_balance, fixture.definition.GetProtocolParameters().onboarding_bonus);
    BOOST_CHECK(std::filesystem::exists(path));

    const auto material = cybou::LoadIdentityMaterial(path, "correct horse battery staple");
    BOOST_REQUIRE(material);
    const auto account = cybou::AccountId::FromBytes(material->account_id);
    BOOST_REQUIRE(account);
    BOOST_CHECK(result.account_id == *account);
    const auto device_key = service.GetKeyStore().GetAuthorizationPublicKey();
    BOOST_REQUIRE(device_key);
    BOOST_CHECK(!std::equal(result.account_id.Value().begin(), result.account_id.Value().end(), device_key->ed25519.begin()));

    // A valid but different local KEM seed must not make a signing-key match ACTIVE.
    auto mismatched_material = cybou::LoadIdentityMaterial(path, "correct horse battery staple");
    BOOST_REQUIRE(mismatched_material);
    mismatched_material->recovery_entropy[0] ^= 0x01;
    const auto mismatched_path = dir / "mismatched-kem.cybou";
    std::filesystem::remove(mismatched_path);
    BOOST_REQUIRE(cybou::SaveNewIdentityMaterial(mismatched_path,
        "another strong vault password", *mismatched_material));
    cybou::CybouIdentityService mismatched{runtime, mismatched_path};
    BOOST_REQUIRE(mismatched.LoadVault("another strong vault password"));
    BOOST_CHECK(mismatched.GetPhase() == cybou::IdentityCreationPhase::IDLE);

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
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 1);
    const auto loaded_state = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded_state && loaded_state.state);
    const auto* record = loaded_state.state->identities.Find(result.account_id);
    BOOST_REQUIRE(record);
    BOOST_CHECK_EQUAL(record->key_epoch, 0U);
    cybou::CybouIdentityService reopened_restored{runtime, restored_path};
    BOOST_REQUIRE(reopened_restored.LoadVault("another strong vault password"));
    BOOST_CHECK(reopened_restored.GetPhase() == cybou::IdentityCreationPhase::ACTIVE);
    BOOST_CHECK(!reopened_restored.RestoreIdentitySync(*words, "wrong password").success);
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 1);

    std::filesystem::remove(path);
    std::filesystem::remove(restored_path);
    std::filesystem::remove(mismatched_path);
}

BOOST_AUTO_TEST_CASE(lock_erases_authority_key_material_and_vault_can_be_reopened)
{
    RuntimeFixture fixture;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    const auto dir = std::filesystem::temp_directory_path() / "cybou-identity-service-lock-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "authority.cybou";
    std::filesystem::remove(path);

    auto material = cybou::GenerateIdentityMaterial();
    BOOST_REQUIRE(material);
    cybou::crypto::CleanseMemory(material->recovery_entropy.data(), material->recovery_entropy.size());
    material->recovery_entropy = fixture.validator_seed;
    cybou::CybouKeyStore writer;
    BOOST_REQUIRE(writer.LoadMaterial(std::move(*material)));
    BOOST_REQUIRE(writer.SaveToFile(path, "correct horse battery staple"));

    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.LoadVault("correct horse battery staple"));
    BOOST_CHECK(identity.IsUnlocked());
    BOOST_CHECK(identity.GetKeyStore().HasKey());
    BOOST_CHECK(identity.IsNetworkAuthority());

    identity.Lock();
    BOOST_CHECK(!identity.IsUnlocked());
    BOOST_CHECK(!identity.GetKeyStore().HasKey());
    BOOST_CHECK(!identity.IsNetworkAuthority());
    BOOST_CHECK(identity.GetPhase() == cybou::IdentityCreationPhase::IDLE);
    BOOST_CHECK(std::filesystem::exists(path));

    BOOST_REQUIRE(identity.LoadVault("correct horse battery staple"));
    BOOST_CHECK(identity.IsUnlocked());
    BOOST_CHECK(identity.IsNetworkAuthority());
    std::filesystem::remove(path);
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

BOOST_AUTO_TEST_CASE(recovery_rotation_promotes_candidate_only_after_finality)
{
    RuntimeFixture fixture;
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-recovery-rotation-service-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    auto candidate_path = path;
    candidate_path += ".rotation-pending";
    std::filesystem::remove(path);
    std::filesystem::remove(candidate_path);

    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    const auto created = identity.CreateIdentitySync("correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(created.success, created.error_message);
    const auto old_root = identity.GetKeyStore().GetRecoveryPublicKey();
    const auto old_authorization = identity.GetKeyStore().GetAuthorizationPublicKey();
    BOOST_REQUIRE(old_root && old_authorization);
    const auto old_root_id = cybou::ComputeRecoveryKeyId(*old_root);
    BOOST_REQUIRE(old_root_id);

    auto new_entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(new_entropy);
    const auto new_words = cybou::EncodeRecoveryWords(*new_entropy);
    const auto new_root = cybou::DeriveIdentityPublicKey(*new_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(new_root);
    cybou::crypto::CleanseMemory(new_entropy->data(), new_entropy->size());

    const auto wrong_password = identity.RotateIdentitySync(new_words, "wrong vault password");
    BOOST_CHECK(wrong_password.phase == cybou::IdentityOperationPhase::REJECTED);
    BOOST_CHECK(!std::filesystem::exists(candidate_path));

    const auto pending = identity.RotateIdentitySync(new_words, "correct horse battery staple");
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(pending.phase),
        static_cast<unsigned>(cybou::IdentityOperationPhase::ACCEPTED));
    BOOST_CHECK(std::filesystem::exists(candidate_path));
    const auto resumed = identity.ResumeIdentityRotationSync("correct horse battery staple");
    BOOST_CHECK(resumed.phase == cybou::IdentityOperationPhase::ACCEPTED);
    BOOST_CHECK(resumed.op_id == pending.op_id);
    const auto still_active = cybou::LoadIdentityMaterial(path, "correct horse battery staple");
    BOOST_REQUIRE(still_active);
    const auto still_active_root = cybou::DeriveIdentityPublicKey(
        still_active->recovery_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(still_active_root);
    BOOST_CHECK(*still_active_root == *old_root);

    const auto candidate = cybou::LoadIdentityMaterial(candidate_path, "correct horse battery staple");
    BOOST_REQUIRE(candidate);
    const auto candidate_root = cybou::DeriveIdentityPublicKey(
        candidate->recovery_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(candidate_root);
    BOOST_CHECK(*candidate_root == *new_root);
    BOOST_CHECK(candidate->account_id == still_active->account_id);
    BOOST_CHECK(candidate->recovery_entropy != still_active->recovery_entropy);

    BOOST_REQUIRE(runtime.ProduceBlock());
    const auto finalized = identity.ResumeIdentityRotationSync("correct horse battery staple");
    BOOST_REQUIRE_EQUAL(static_cast<unsigned>(finalized.phase),
        static_cast<unsigned>(cybou::IdentityOperationPhase::FINALIZED));
    BOOST_CHECK(!std::filesystem::exists(candidate_path));
    BOOST_CHECK(identity.GetKeyStore().GetRecoveryPublicKey() == new_root);
    BOOST_CHECK(identity.GetKeyStore().GetAuthorizationPublicKey() != old_authorization);
    BOOST_CHECK(identity.GetAccountId() == std::optional<cybou::AccountId>{created.account_id});

    const auto loaded = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded && loaded.state);
    BOOST_CHECK(!loaded.state->identities.FindByRecoveryKeyId(*old_root_id));
    BOOST_CHECK(loaded.state->identities.FindByRecoveryKeyId(*cybou::ComputeRecoveryKeyId(*new_root)) ==
        std::optional<cybou::AccountId>{created.account_id});
    BOOST_CHECK_EQUAL(loaded.state->identities.Find(created.account_id)->nonce, 1U);
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(name_claim_saves_secret_before_commit_and_finalizes_owner)
{
    RuntimeFixture fixture;
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 0; });
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
    bool checked_name_operation = false;
    for (uint64_t height = 2; height <= runtime.GetFinalizedHeight().value_or(0); ++height) {
        const auto finalized = runtime.GetBlockAtHeight(height);
        BOOST_REQUIRE(finalized);
        for (const auto& operation : finalized->block.operations) {
            if (std::holds_alternative<cybou::AuthorizedNameCommit>(operation) ||
                std::holds_alternative<cybou::AuthorizedNameReveal>(operation)) {
                BOOST_CHECK(runtime.SubmitOperation(operation).status ==
                    cybou::OperationSubmitStatus::ALREADY_FINALIZED);
                checked_name_operation = true;
            }
        }
    }
    BOOST_CHECK(checked_name_operation);
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
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 0; });
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
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 0; });
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

BOOST_AUTO_TEST_CASE(name_claim_rejects_stale_local_label_without_new_commit)
{
    RuntimeFixture fixture;
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 0; });
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-stale-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    auto claim_path = path;
    claim_path += ".nameclaim";
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    BOOST_REQUIRE(identity.CreateIdentitySync("correct horse battery staple").success);
    cybou::CybouNameService first{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!first.ClaimSync("stanislav", "correct horse battery staple",
        [&](cybou::NameClaimPhase phase, const std::string&) {
            if (phase == cybou::NameClaimPhase::WAITING_FOR_COMMIT) first.Cancel();
        }).success);
    const auto height = runtime.GetFinalizedHeight();
    cybou::CybouNameService second{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!second.ClaimSync("anothername", "correct horse battery staple").success);
    BOOST_CHECK(runtime.GetFinalizedHeight() == height);
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
}

BOOST_AUTO_TEST_CASE(name_work_cancellation_keeps_commit_but_not_name)
{
    RuntimeFixture fixture;
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 255; });
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-work-cancel-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    auto claim_path = path;
    claim_path += ".nameclaim";
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    BOOST_REQUIRE(identity.CreateIdentitySync("correct horse battery staple").success);
    cybou::CybouNameService names{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!names.ClaimSync("stanislav", "correct horse battery staple",
        [&](cybou::NameClaimPhase phase, const std::string&) {
            if (phase == cybou::NameClaimPhase::WORKING) names.Cancel();
        }).success);
    const auto loaded = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded && loaded.state);
    BOOST_CHECK_EQUAL(loaded.state->names.pending_commits.size(), 1U);
    BOOST_CHECK(!loaded.state->names.Resolve("stanislav"));
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
}

BOOST_AUTO_TEST_CASE(expired_name_commit_can_restart_with_saved_claim)
{
    RuntimeFixture fixture;
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 0; });
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_commit_max_lifetime = 1; });
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-expiry-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    auto claim_path = path;
    claim_path += ".nameclaim";
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
    cybou::CybouIdentityService identity{runtime, path};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    BOOST_REQUIRE(identity.CreateIdentitySync("correct horse battery staple").success);
    cybou::CybouNameService first{runtime, identity.GetKeyStore(), path};
    BOOST_CHECK(!first.ClaimSync("stanislav", "correct horse battery staple",
        [&](cybou::NameClaimPhase phase, const std::string&) {
            if (phase == cybou::NameClaimPhase::WAITING_FOR_COMMIT) first.Cancel();
        }).success);
    const auto committed = runtime.GetStore().LoadState();
    BOOST_REQUIRE(committed && committed.state);
    BOOST_REQUIRE_EQUAL(committed.state->names.pending_commits.size(), 1U);
    const uint64_t commit_height = committed.state->names.pending_commits.begin()->second.commit_height;
    while (runtime.GetFinalizedHeight().value_or(0) <= commit_height +
        fixture.definition.GetProtocolParameters().name_commit_max_lifetime) {
        BOOST_REQUIRE(runtime.ProduceBlock());
    }
    const auto pruned = runtime.GetStore().LoadState();
    BOOST_REQUIRE(pruned && pruned.state);
    BOOST_CHECK(pruned.state->names.pending_commits.empty());
    cybou::CybouNameService restarted{runtime, identity.GetKeyStore(), path};
    const auto claimed = restarted.ClaimSync("stanislav", "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(claimed.success, claimed.message);
    BOOST_CHECK(identity.GetFinalizedPrimaryName() == std::optional<std::string>{"stanislav"});
    std::filesystem::remove(path);
    std::filesystem::remove(claim_path);
}

BOOST_AUTO_TEST_CASE(competing_claim_loser_never_gains_ownership)
{
    RuntimeFixture fixture;
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) { params.name_claim_work_bits = 0; });
    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    const auto dir = std::filesystem::temp_directory_path() / "cybou-name-race-test";
    std::filesystem::create_directories(dir);
    const auto first_path = dir / "first.cybou";
    const auto second_path = dir / "second.cybou";
    auto first_claim_path = first_path;
    auto second_claim_path = second_path;
    first_claim_path += ".nameclaim";
    second_claim_path += ".nameclaim";
    for (const auto& path : {first_path, second_path, first_claim_path, second_claim_path}) std::filesystem::remove(path);
    cybou::CybouIdentityService first_identity{runtime, first_path};
    cybou::CybouIdentityService second_identity{runtime, second_path};
    BOOST_REQUIRE(first_identity.PrepareNewIdentity());
    BOOST_REQUIRE(second_identity.PrepareNewIdentity());
    BOOST_REQUIRE(first_identity.CreateIdentitySync("correct horse battery staple").success);
    BOOST_REQUIRE(second_identity.CreateIdentitySync("correct horse battery staple").success);
    cybou::CybouNameService first{runtime, first_identity.GetKeyStore(), first_path};
    cybou::CybouNameService second{runtime, second_identity.GetKeyStore(), second_path};
    const auto stop_after_commit = [](cybou::CybouNameService& service) {
        return [&service](cybou::NameClaimPhase phase, const std::string&) {
            if (phase == cybou::NameClaimPhase::WAITING_FOR_COMMIT) service.Cancel();
        };
    };
    BOOST_CHECK(!first.ClaimSync("stanislav", "correct horse battery staple", stop_after_commit(first)).success);
    BOOST_CHECK(!second.ClaimSync("stanislav", "correct horse battery staple", stop_after_commit(second)).success);
    BOOST_REQUIRE(first.ClaimSync("stanislav", "correct horse battery staple").success);
    BOOST_CHECK(!second.ClaimSync("stanislav", "correct horse battery staple").success);
    BOOST_CHECK(!second_identity.GetFinalizedPrimaryName());
    const auto loaded = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded && loaded.state);
    const auto* owner = loaded.state->names.Resolve("stanislav");
    BOOST_REQUIRE(owner);
    BOOST_CHECK(*owner == *first_identity.GetAccountId());
    for (const auto& path : {first_path, second_path, first_claim_path, second_claim_path}) std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(genesis_allocation_claim_e2e)
{
    const auto entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(entropy);
    const auto words = cybou::EncodeRecoveryWords(*entropy);
    const auto recovery_key = cybou::DeriveIdentityPublicKey(*entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(recovery_key);
    const auto recovery_id = cybou::ComputeRecoveryKeyId(*recovery_key);
    BOOST_REQUIRE(recovery_id);

    RuntimeFixture fixture;
    fixture.genesis.genesis_allocations.clear();
    fixture.genesis.genesis_allocations[*recovery_id] = cybou::GenesisAllocation{
        .balance = 100'000'000,
        .label = "cybou",
        .claimed_by = std::nullopt,
    };
    fixture.definition = cybou::CreateTestNetworkGenesis(fixture.genesis,
        cybou::TestPoaFinalizerPublicKey(fixture.validator_seed[0]),
        cybou::TestNetworkPublicKey(fixture.validator_seed[0]));
    fixture.definition = cybou::WithTestGenesisParameters(fixture.definition, [](auto& params) {
        params.account_creation_work_bits = 0;
    });

    cybou::CybouNodeRuntime runtime{fixture.Config()};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    const auto dir = std::filesystem::temp_directory_path() / "cybou-genesis-claim-vault-test";
    std::filesystem::create_directories(dir);
    const auto path = dir / "identity.cybou";
    std::filesystem::remove(path);

    cybou::CybouIdentityService service{runtime, path};
    const auto result = service.RestoreIdentitySync(words, "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(result.success, result.error_message);
    BOOST_CHECK(!result.account_id.IsNull());
    BOOST_CHECK_EQUAL(result.creation_height, 1);
    BOOST_CHECK_EQUAL(runtime.GetFinalizedHeight().value_or(0), 1);

    const auto loaded = runtime.GetStore().LoadState();
    BOOST_REQUIRE(loaded && loaded.state);
    const auto it = loaded.state->accounts.find(result.account_id);
    BOOST_REQUIRE(it != loaded.state->accounts.end());
    BOOST_CHECK_EQUAL(it->second.balance, 100'000'000u);
    // Claiming the Treasury allocation `cybou` brings no onboarding bonus: it is its source (DEC-277).
    BOOST_CHECK_EQUAL(it->second.system_balance, 0u);
    BOOST_CHECK(runtime.IsTreasuryClaimant(result.account_id));
    // Its wallet history therefore shows no welcome credit.
    cybou::CybouWalletService wallet{runtime, service.GetKeyStore()};
    wallet.SyncLedger();
    const auto ledger = wallet.GetLedgerEntries();
    BOOST_CHECK(std::none_of(ledger.begin(), ledger.end(),
        [](const auto& entry) { return entry.kind == cybou::WalletEntryKind::ONBOARDING_BONUS; }));

    const auto alloc_it = loaded.state->genesis_allocations.find(*recovery_id);
    BOOST_REQUIRE(alloc_it != loaded.state->genesis_allocations.end());
    BOOST_REQUIRE(alloc_it->second.claimed_by.has_value());
    BOOST_CHECK(*alloc_it->second.claimed_by == result.account_id);

    BOOST_REQUIRE(loaded.state->names.PrimaryName(result.account_id));
    BOOST_CHECK_EQUAL(*loaded.state->names.PrimaryName(result.account_id), "cybou");
    BOOST_CHECK(loaded.state->identities.FindByRecoveryKeyId(*recovery_id) == result.account_id);

    // Second restore with the same 24 words should find the existing on-chain identity
    const auto second_path = dir / "restored.cybou";
    std::filesystem::remove(second_path);
    cybou::CybouIdentityService second_service{runtime, second_path};
    const auto second_result = second_service.RestoreIdentitySync(words, "another password");
    BOOST_REQUIRE_MESSAGE(second_result.success, second_result.error_message);
    BOOST_CHECK(second_result.account_id == result.account_id);

    std::filesystem::remove(path);
    std::filesystem::remove(second_path);
}

BOOST_AUTO_TEST_SUITE_END()
