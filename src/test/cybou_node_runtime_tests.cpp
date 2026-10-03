// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/operation_submit.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/hex.h>
#include <cybou/identity_material.h>
#include <cybou/keystore.h>
#include <cybou/kv_store.h>
#include <cybou/name_service.h>
#include <cybou/block_executor.h>
#include <cybou/poa_finalizer.h>
#include <cybou/secret_file.h>
#include <cybou/validation_attestation.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_test_setup.h>

#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>

#include <fstream>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#endif

BOOST_FIXTURE_TEST_SUITE(cybou_node_runtime_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(secret_files_are_private_and_reject_links)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "secret.bin";
    const std::array<unsigned char, 4> secret{1, 2, 3, 4};
    BOOST_REQUIRE(cybou::CreateSecretFile(path, secret));
    const auto read = cybou::ReadSecretFile(path, 4);
    BOOST_REQUIRE(read);
    BOOST_CHECK(*read == std::vector<unsigned char>(secret.begin(), secret.end()));
    BOOST_CHECK(!cybou::CreateSecretFile(path, secret));
#ifndef _WIN32
    struct stat info{};
    BOOST_REQUIRE_EQUAL(::stat(path.c_str(), &info), 0);
    BOOST_CHECK_EQUAL(info.st_mode & 0777, 0600);
    const auto link = fixture.directory / "secret-link.bin";
    std::filesystem::create_symlink(path, link);
    BOOST_CHECK(!cybou::ReadSecretFile(link, 4));
#endif
}

BOOST_AUTO_TEST_CASE(event_log_privacy_modes_filter_sensitive_identifiers)
{
    CybouServiceTestFixture fixture;
    const auto minimal_path = fixture.directory / "minimal-events.jsonl";
    const auto detailed_path = fixture.directory / "detailed-events.jsonl";
    const cybou::EventFields fields{{"operation_id", std::string{"operation-secret"}},
        {"account_id", std::string{"account-secret"}}, {"peer", std::string{"peer-secret"}},
        {"network_binding", std::string{"public-network"}}};
    {
        cybou::EventWriter writer{minimal_path};
        writer.Write(cybou::NodeEvent::operation_accepted, fields);
    }
    {
        cybou::EventWriter writer{detailed_path, cybou::EventLogMode::DETAILED};
        writer.Write(cybou::NodeEvent::operation_accepted, fields);
    }
    std::ifstream minimal_file{minimal_path};
    std::ifstream detailed_file{detailed_path};
    const std::string minimal{std::istreambuf_iterator<char>{minimal_file}, {}};
    const std::string detailed{std::istreambuf_iterator<char>{detailed_file}, {}};
    BOOST_CHECK(minimal.find("operation-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("account-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("peer-secret") == std::string::npos);
    BOOST_CHECK(minimal.find("public-network") != std::string::npos);
    BOOST_CHECK(detailed.find("operation-secret") != std::string::npos);
    BOOST_CHECK(detailed.find("account-secret") != std::string::npos);
    BOOST_CHECK(detailed.find("peer-secret") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(public_event_writer_rejects_secret_fields)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "events.jsonl";
    cybou::EventWriter writer{path};
    BOOST_CHECK_THROW(writer.Write(cybou::NodeEvent::node_started, {{"mnemonic", std::string{"secret"}}}), std::invalid_argument);
    BOOST_CHECK_THROW(writer.Write(cybou::NodeEvent::node_started, {{"role", std::string(257, 'x')}}), std::invalid_argument);
    writer.Write(cybou::NodeEvent::node_status, {{"network_binding", fixture.runtime->GetNetworkBinding().GetHex()},{"height",std::uint64_t{1}}});
    BOOST_CHECK(writer.Good());
    const auto snapshot=fixture.runtime->GetDiagnostics();
    BOOST_CHECK(snapshot.initialized);
    BOOST_CHECK_EQUAL(snapshot.node_type, "Full Node");
    BOOST_CHECK_EQUAL(snapshot.height,fixture.runtime->GetStatus().finalized_height);
}

BOOST_AUTO_TEST_CASE(runtime_finalizes_account_and_observer_verifies_block)
{
    CybouServiceTestFixture fixture;
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(status.is_initialized);
    BOOST_CHECK(status.poa_signer_active);
    const auto alice = fixture.CreateIdentity("alice.cybou");
    const auto account = alice->GetAccountId();
    BOOST_REQUIRE(account);
    BOOST_CHECK(fixture.runtime->GetAccountState(*account));
    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);

    cybou::NodeRuntimeConfig observer_config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "observer",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!observer.GetStatus().poa_signer_active);
    BOOST_REQUIRE(observer.CommitBlock(*block));
    BOOST_CHECK(observer.GetAccountState(*account) == fixture.runtime->GetAccountState(*account));
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 1);
    BOOST_REQUIRE_EQUAL(block->block.operations.size(), 1U);
    const auto op_id = cybou::ComputeOperationId(block->block.operations.front());
    BOOST_REQUIRE(op_id);
    const auto found = observer.FindFinalizedOperation(*op_id);
    BOOST_CHECK(found.status == cybou::FinalizedOperationLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(found.height, 1U);
    BOOST_CHECK_EQUAL(found.operation_index, 0U);
    BOOST_CHECK(found.block_id == cybou::ComputeBlockId(block->block));
    const auto missing_id = *op_id == cybou::Hash256::ONE ? cybou::Hash256{uint8_t{2}} : cybou::Hash256::ONE;
    const auto missing = observer.FindFinalizedOperation(missing_id);
    BOOST_CHECK(missing.status == cybou::FinalizedOperationLookupStatus::NOT_FOUND);
    BOOST_CHECK_EQUAL(missing.scanned_height, 1U);
}

BOOST_AUTO_TEST_CASE(poa_auth_adjustment_grants_and_burns_with_floor)
{
    CybouServiceTestFixture fixture;
    const auto alice = fixture.CreateIdentity("alice.cybou");
    const auto account = alice->GetAccountId();
    BOOST_REQUIRE(account);
    BOOST_CHECK_EQUAL(fixture.runtime->GetAccountState(*account)->authority, 0U);

    const auto grant = fixture.runtime->SubmitPoaAuthAdjustment(cybou::PoaAuthAction::GRANT, *account, 900'000);
    BOOST_REQUIRE(grant.status == cybou::OperationSubmitStatus::ACCEPTED);
    const auto granted = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(granted);
    // GRANT mints exactly N; the PoA operation itself earns no +1.
    BOOST_CHECK_EQUAL(fixture.runtime->GetAccountState(*account)->authority, 900'000U);

    cybou::CybouNodeRuntime observer{{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "auth-observer",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    }};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    for (uint64_t height{1}; height <= granted->block.height; ++height) {
        BOOST_REQUIRE(observer.CommitBlock(*fixture.runtime->GetBlockAtHeight(height)));
    }
    BOOST_CHECK(observer.GetAccountState(*account) == fixture.runtime->GetAccountState(*account));

    BOOST_REQUIRE(fixture.runtime->SubmitPoaAuthAdjustment(cybou::PoaAuthAction::BURN, *account, 2'000'000).status ==
        cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    BOOST_CHECK_EQUAL(fixture.runtime->GetAccountState(*account)->authority, 0U);

    // An adjustment is bound to one block height, the PoA key and one use per block.
    const auto head = *fixture.runtime->GetFinalizedHeight();
    BOOST_REQUIRE(fixture.runtime->SubmitPoaAuthAdjustment(cybou::PoaAuthAction::GRANT, *account, 5).status ==
        cybou::OperationSubmitStatus::ACCEPTED);
    const auto pending = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(pending);
    const auto signed_op = pending->block.operations.front();
    const auto loaded = fixture.runtime->GetStore().LoadState();
    BOOST_REQUIRE(loaded.state);
    const auto& params = fixture.definition.GetProtocolParameters();
    const auto& network_binding = fixture.runtime->GetNetworkBinding();
    const auto& poa_key = fixture.definition.GetPoaPublicKey();
    BOOST_CHECK(cybou::ExecuteBlockOperations(*loaded.state, {signed_op}, network_binding, head + 2, params, &poa_key).error ==
        cybou::BlockExecutionError::INVALID_POA_AUTH_ADJUSTMENT);
    BOOST_CHECK(cybou::ExecuteBlockOperations(*loaded.state, {signed_op}, network_binding, head + 1, params).error ==
        cybou::BlockExecutionError::INVALID_POA_AUTH_ADJUSTMENT);
    BOOST_CHECK(cybou::ExecuteBlockOperations(*loaded.state, {signed_op, signed_op}, network_binding, head + 1, params,
        &poa_key).error == cybou::BlockExecutionError::INVALID_POA_AUTH_ADJUSTMENT);
    auto forged = std::get<cybou::PoaAuthAdjustment>(signed_op);
    forged.amount = 6;
    BOOST_CHECK(cybou::ExecuteBlockOperations(*loaded.state, {cybou::ProtocolOperation{forged}}, network_binding, head + 1,
        params, &poa_key).poa_auth_error == cybou::PoaAuthAdjustmentError::INVALID_SIGNATURE);
    const auto wire = cybou::SerializeProtocolOperation(signed_op);
    BOOST_REQUIRE(wire);
    const auto decoded = cybou::DeserializeProtocolOperation(*wire);
    BOOST_REQUIRE(decoded && *decoded == signed_op);
    BOOST_CHECK(!cybou::AuthorizingAccount(signed_op));
}

BOOST_AUTO_TEST_CASE(ordinary_node_executes_candidates_before_relay)
{
    CybouServiceTestFixture fixture;
    const auto alice = fixture.CreateIdentity("alice.cybou");
    cybou::CybouNameService names{*fixture.runtime, alice->GetKeyStore(), fixture.directory / "alice.cybou"};
    const auto claimed = names.ClaimSync("alice", "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(claimed.success, claimed.message);
    std::optional<cybou::ProtocolOperation> commit, reveal;
    uint64_t commit_height{0};
    for (uint64_t height{2}; height <= fixture.runtime->GetFinalizedHeight().value_or(0); ++height) {
        const auto block = fixture.runtime->GetBlockAtHeight(height);
        BOOST_REQUIRE(block);
        for (const auto& operation : block->block.operations) {
            if (std::holds_alternative<cybou::AuthorizedNameCommit>(operation)) { commit = operation; commit_height = height; }
            if (std::holds_alternative<cybou::AuthorizedNameReveal>(operation)) reveal = operation;
        }
    }
    BOOST_REQUIRE(commit && reveal);

    cybou::CybouNodeRuntime ordinary{{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "ordinary-candidates",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    }};
    BOOST_REQUIRE(ordinary.InitializeGenesis(fixture.genesis));
    for (uint64_t height{1}; height < commit_height; ++height) {
        BOOST_REQUIRE(ordinary.CommitBlock(*fixture.runtime->GetBlockAtHeight(height)));
    }
    const auto commit_bytes = cybou::SerializeProtocolOperation(*commit);
    const auto reveal_bytes = cybou::SerializeProtocolOperation(*reveal);
    const auto commit_id = cybou::ComputeOperationId(*commit);
    const auto reveal_id = cybou::ComputeOperationId(*reveal);
    BOOST_REQUIRE(commit_bytes && reveal_bytes && commit_id && reveal_id);

    // Correctly signed but not executable on this node's finalized state: never relayed.
    BOOST_CHECK(ordinary.EnqueueRelayedOperation(*reveal_bytes, 0) == cybou::OperationRelayEnqueueStatus::INVALID_OPERATION);
    BOOST_CHECK(!ordinary.HasRelayedOperation(*reveal_id));
    BOOST_CHECK(!ordinary.HasCandidateOperation(*reveal_id));

    BOOST_CHECK(ordinary.EnqueueRelayedOperation(*commit_bytes, 0) == cybou::OperationRelayEnqueueStatus::QUEUED);
    BOOST_CHECK(ordinary.HasRelayedOperation(*commit_id));
    BOOST_CHECK(ordinary.HasCandidateOperation(*commit_id));
    // An ordinary node's own pool is not finalizer acceptance.
    BOOST_CHECK(ordinary.GetOperationStatus(*commit_id).kind != cybou::OperationStatusKind::LOCAL_PENDING);

    BOOST_REQUIRE(ordinary.CommitBlock(*fixture.runtime->GetBlockAtHeight(commit_height)));
    BOOST_CHECK(!ordinary.HasCandidateOperation(*commit_id));
    BOOST_CHECK(!ordinary.HasRelayedOperation(*commit_id));
    BOOST_CHECK_EQUAL(ordinary.CandidateOperationCount(), 0U);
    BOOST_CHECK(ordinary.EnqueueRelayedOperation(*commit_bytes, 0) == cybou::OperationRelayEnqueueStatus::DUPLICATE);
}

BOOST_AUTO_TEST_CASE(validation_attestation_requires_finalized_auth_above_ten_million)
{
    CybouServiceTestFixture fixture;
    const auto alice = fixture.CreateIdentity("alice.cybou");
    const auto account = alice->GetAccountId();
    BOOST_REQUIRE(account);
    const cybou::CybouKeyStoreValidationSigner signer{alice->GetKeyStore()};
    const auto& network_binding = fixture.runtime->GetNetworkBinding();
    cybou::Hash256 operation_id;
    operation_id.begin()[0] = 0x42;
    const auto finalized = [&] {
        const auto loaded = fixture.runtime->GetStore().LoadState();
        BOOST_REQUIRE(loaded.state);
        return std::make_pair(*loaded.state, *fixture.runtime->GetFinalizedTip());
    };
    const auto grant = [&](uint64_t amount) {
        BOOST_REQUIRE(fixture.runtime->SubmitPoaAuthAdjustment(cybou::PoaAuthAction::GRANT, *account, amount).status ==
            cybou::OperationSubmitStatus::ACCEPTED);
        BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    };

    auto [state, tip] = finalized();
    BOOST_CHECK(!cybou::IsValidationEligible(state, *account));
    BOOST_CHECK(!cybou::SignValidationAttestation(signer, network_binding, operation_id, tip, state));

    grant(10'000'000);
    std::tie(state, tip) = finalized();
    BOOST_CHECK_EQUAL(state.accounts.at(*account).authority, 10'000'000U);
    BOOST_CHECK(!cybou::IsValidationEligible(state, *account));
    BOOST_CHECK(!cybou::SignValidationAttestation(signer, network_binding, operation_id, tip, state));

    grant(1);
    std::tie(state, tip) = finalized();
    BOOST_REQUIRE(cybou::IsValidationEligible(state, *account));
    const auto attestation = cybou::SignValidationAttestation(signer, network_binding, operation_id, tip, state);
    BOOST_REQUIRE(attestation);
    BOOST_CHECK(cybou::VerifyValidationAttestation(*attestation, network_binding, tip, state) ==
        cybou::ValidationAttestationError::NONE);
    const auto bytes = cybou::SerializeValidationAttestation(*attestation);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), cybou::VALIDATION_ATTESTATION_SIZE);
    BOOST_CHECK(cybou::DeserializeValidationAttestation(*bytes) == attestation);
    auto truncated = *bytes;
    truncated.pop_back();
    BOOST_CHECK(!cybou::DeserializeValidationAttestation(truncated));

    cybou::Hash256 other;
    other.begin()[0] = 0x43;
    BOOST_CHECK(cybou::VerifyValidationAttestation(*attestation, other, tip, state) ==
        cybou::ValidationAttestationError::WRONG_NETWORK);
    BOOST_CHECK(cybou::VerifyValidationAttestation(*attestation, network_binding, other, state) ==
        cybou::ValidationAttestationError::STALE_BASE);
    auto tampered = *attestation;
    tampered.operation_id = other;
    BOOST_CHECK(cybou::VerifyValidationAttestation(tampered, network_binding, tip, state) ==
        cybou::ValidationAttestationError::INVALID_SIGNATURE);

    // Eligibility is read from the verifier's own finalized state, not from the claim.
    BOOST_REQUIRE(fixture.runtime->SubmitPoaAuthAdjustment(cybou::PoaAuthAction::BURN, *account, 1).status ==
        cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto [burned_state, burned_tip] = finalized();
    auto rebased = *attestation;
    rebased.finalized_base_block_id = burned_tip;
    BOOST_CHECK(cybou::VerifyValidationAttestation(rebased, network_binding, burned_tip, burned_state) ==
        cybou::ValidationAttestationError::NOT_ELIGIBLE);

    // A locked vault signs nothing.
    grant(1);
    std::tie(state, tip) = finalized();
    alice->GetKeyStore().Clear();
    BOOST_CHECK(!cybou::SignValidationAttestation(signer, network_binding, operation_id, tip, state));
}

BOOST_AUTO_TEST_CASE(eligible_node_attests_its_own_executed_candidates)
{
    CybouServiceTestFixture fixture;
    const auto alice = fixture.CreateIdentity("alice.cybou");
    BOOST_REQUIRE(fixture.runtime->SubmitPoaAuthAdjustment(cybou::PoaAuthAction::GRANT, *alice->GetAccountId(),
        10'000'001).status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto bob = fixture.CreateIdentity("bob.cybou");
    cybou::CybouNameService names{*fixture.runtime, bob->GetKeyStore(), fixture.directory / "bob.cybou"};
    const auto claimed = names.ClaimSync("bobby", "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(claimed.success, claimed.message);
    std::optional<cybou::ProtocolOperation> commit;
    uint64_t commit_height{0};
    for (uint64_t height{1}; height <= fixture.runtime->GetFinalizedHeight().value_or(0); ++height) {
        const auto block = fixture.runtime->GetBlockAtHeight(height);
        BOOST_REQUIRE(block);
        for (const auto& operation : block->block.operations) {
            if (std::holds_alternative<cybou::AuthorizedNameCommit>(operation)) { commit = operation; commit_height = height; }
        }
    }
    BOOST_REQUIRE(commit);
    const auto commit_bytes = cybou::SerializeProtocolOperation(*commit);
    const auto commit_id = cybou::ComputeOperationId(*commit);
    BOOST_REQUIRE(commit_bytes && commit_id);

    const auto make_node = [&](const std::string& name) {
        auto node = std::make_unique<cybou::CybouNodeRuntime>(cybou::NodeRuntimeConfig{
            .network_genesis = fixture.definition,
            .data_dir = fixture.directory / name,
            .memory_only = true,
            .wipe_data = true, .operation_work_bits = 0
        });
        BOOST_REQUIRE(node->InitializeGenesis(fixture.genesis));
        for (uint64_t height{1}; height < commit_height; ++height) {
            BOOST_REQUIRE(node->CommitBlock(*fixture.runtime->GetBlockAtHeight(height)));
        }
        return node;
    };

    // Eligible validator node: executes, then signs.
    const auto validator = make_node("validator-node");
    validator->SetValidationSigner(std::make_shared<cybou::CybouKeyStoreValidationSigner>(alice->GetKeyStore()));
    BOOST_CHECK(validator->IsLocalValidationEligible());
    BOOST_REQUIRE(validator->EnqueueRelayedOperation(*commit_bytes, 0) == cybou::OperationRelayEnqueueStatus::QUEUED);
    const auto attestations = validator->GetValidationAttestations(*commit_id);
    BOOST_REQUIRE_EQUAL(attestations.size(), 1U);
    BOOST_CHECK(attestations.front().validator_account_id == *alice->GetAccountId());
    BOOST_CHECK(attestations.front().finalized_base_block_id == *validator->GetFinalizedTip());
    BOOST_CHECK(validator->GetOperationStatus(*commit_id).IsValidated());

    // Ordinary node: an attestation never replaces its own execution.
    const auto ordinary = make_node("ordinary-node");
    BOOST_CHECK(!ordinary->IsLocalValidationEligible());
    BOOST_CHECK(ordinary->AcceptValidationAttestation(attestations.front()) ==
        cybou::ValidationAcceptStatus::NOT_CANDIDATE);
    BOOST_REQUIRE(ordinary->EnqueueRelayedOperation(*commit_bytes, 0) == cybou::OperationRelayEnqueueStatus::QUEUED);
    BOOST_CHECK(ordinary->GetValidationAttestations(*commit_id).empty());
    BOOST_CHECK(!ordinary->GetOperationStatus(*commit_id).IsValidated());
    auto forged = attestations.front();
    forged.validator_account_id = *bob->GetAccountId();
    BOOST_CHECK(ordinary->AcceptValidationAttestation(forged) == cybou::ValidationAcceptStatus::INVALID);
    BOOST_CHECK(ordinary->AcceptValidationAttestation(attestations.front()) == cybou::ValidationAcceptStatus::ADDED);
    BOOST_CHECK(ordinary->AcceptValidationAttestation(attestations.front()) == cybou::ValidationAcceptStatus::DUPLICATE);
    const auto validated = ordinary->GetOperationStatus(*commit_id);
    BOOST_CHECK(validated.IsValidated());
    BOOST_CHECK_EQUAL(validated.validation_signatures, 1U);
    const auto balances_before = ordinary->GetAccountState(*bob->GetAccountId());

    // PoA finality is what changes state; Validation changed nothing.
    BOOST_REQUIRE(ordinary->CommitBlock(*fixture.runtime->GetBlockAtHeight(commit_height)));
    const auto finalized = ordinary->GetOperationStatus(*commit_id);
    BOOST_CHECK(finalized.kind == cybou::OperationStatusKind::FINALIZED);
    BOOST_CHECK(!finalized.IsValidated());
    BOOST_CHECK(ordinary->GetValidationAttestations(*commit_id).empty());
    BOOST_CHECK(ordinary->AcceptValidationAttestation(attestations.front()) ==
        cybou::ValidationAcceptStatus::NOT_CANDIDATE);
    BOOST_CHECK_EQUAL(ordinary->GetAccountState(*bob->GetAccountId())->balance, balances_before->balance);
}

BOOST_AUTO_TEST_CASE(runtime_finalizer_can_be_armed_and_disarmed_with_a_vault_signer)
{
    CybouServiceTestFixture fixture;
    cybou::CybouNodeRuntime runtime{cybou::NodeRuntimeConfig{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "vault-finalizer-runtime",
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0
    }};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!runtime.GetStatus().poa_signer_active);
    BOOST_CHECK(!runtime.ProduceBlock());

    auto material = cybou::GenerateIdentityMaterial();
    BOOST_REQUIRE(material);
    cybou::crypto::CleanseMemory(material->recovery_entropy.data(), material->recovery_entropy.size());
    material->recovery_entropy = fixture.validator_seed;
    cybou::CybouKeyStore keystore;
    BOOST_REQUIRE(keystore.LoadMaterial(std::move(*material)));
    auto signer = std::make_shared<cybou::CybouKeyStorePoaSigner>(keystore);
    BOOST_REQUIRE(runtime.EnablePoaSigner(signer));
    BOOST_CHECK(runtime.GetStatus().poa_signer_active);
    BOOST_REQUIRE(runtime.ProduceBlock());

    runtime.DisablePoaSigner();
    BOOST_CHECK(!runtime.GetStatus().poa_signer_active);
    BOOST_CHECK(!runtime.ProduceBlock());

    cybou::CybouKeyStore wrong_keystore;
    BOOST_REQUIRE(wrong_keystore.GenerateNew());
    auto wrong_signer = std::make_shared<cybou::CybouKeyStorePoaSigner>(wrong_keystore);
    BOOST_CHECK(!runtime.EnablePoaSigner(wrong_signer));
    BOOST_CHECK(!runtime.GetStatus().poa_signer_active);
}

BOOST_AUTO_TEST_CASE(runtime_resolves_valid_poa_equivocation_deterministically)
{
    CybouServiceTestFixture fixture;
    const auto canonical = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(canonical);

    // Both competing certificates must cover independently executable blocks.
    // Merely changing state_root produces an invalid block, regardless of its ID.
    CybouServiceTestFixture alternate{fixture.validator_seed[0]};
    auto identity = alternate.CreateIdentity("equivocation.cybou");
    const auto conflicting = alternate.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(conflicting);
    const auto& conflicting_block = conflicting->block;
    const auto result = fixture.runtime->CommitBlock(*conflicting);
    const bool conflicting_wins = cybou::ComputeBlockId(conflicting_block) < cybou::ComputeBlockId(canonical->block);
    if (conflicting_wins) {
        BOOST_CHECK(result.error == cybou::BlockTransitionError::NONE);
        BOOST_CHECK(fixture.runtime->GetFinalizedTip() == cybou::ComputeBlockId(conflicting_block));
    } else {
        BOOST_CHECK(result.error == cybou::BlockTransitionError::POA_EQUIVOCATION_DETECTED);
        BOOST_CHECK(fixture.runtime->GetFinalizedTip() == cybou::ComputeBlockId(canonical->block));
    }
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(!status.poa_safety_halted);
    BOOST_CHECK(status.runtime_state != cybou::NodeRuntimeState::SAFETY_HALTED);
    // A signer whose durable journal belongs to the losing history must fail closed.
    BOOST_CHECK(fixture.runtime->ProduceBlock().has_value() == !conflicting_wins);
    const auto evidence = fixture.runtime->ReadPoaSafetyEvidence();
    BOOST_CHECK(evidence.status == cybou::PoaEvidenceReadStatus::EQUIVOCATION);
    BOOST_REQUIRE(evidence.equivocation);
    BOOST_CHECK(evidence.equivocation->first.block_id != evidence.equivocation->second.block_id);
}

BOOST_AUTO_TEST_CASE(runtime_resolves_only_finalized_root_publications)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("publication-owner.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x31);
    publication.chunk_authorization_root.fill(0x42);
    publication.chunk_count = 1;
    cybou::RootRecipientCapsule capsule;
    capsule.encapsulation.fill(0x53);
    capsule.wrapped_content_key.fill(0x64);
    publication.recipient_capsules.push_back(capsule);
    const auto commitment = cybou::ComputeRootPublicationPayloadCommitment(publication);
    BOOST_REQUIRE(commitment);
    const auto loaded = fixture.runtime->GetStore().LoadState();
    const auto* record = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    BOOST_REQUIRE(record);
    cybou::IdentityOperationAuthorization auth{
        .account_id = *account,
        .nonce = record->nonce,
        .key_epoch = record->key_epoch,
        .kind = cybou::IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *commitment,
    };
    const auto digest = cybou::ComputeIdentityOperationDigest(fixture.runtime->GetNetworkBinding(), auth);
    BOOST_REQUIRE(digest);
    const auto signature = identity->GetKeyStore().SignAuthorization(*digest);
    BOOST_REQUIRE(signature);
    auth.signature = *signature;
    const cybou::ProtocolOperation operation{cybou::AuthorizedRootPublication{auth, publication}};
    const auto submitted = fixture.runtime->SubmitOperation(operation);
    BOOST_REQUIRE(submitted);
    BOOST_CHECK(!fixture.runtime->FindFinalizedRootPublication(submitted.op_id));
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    BOOST_CHECK(fixture.runtime->FindFinalizedRootPublication(submitted.op_id) == publication);
    BOOST_CHECK(!fixture.runtime->FindFinalizedRootPublication(cybou::Hash256::ONE));
}

BOOST_AUTO_TEST_CASE(runtime_resolves_current_identity_kem_package_by_key_epoch)
{
    CybouServiceTestFixture fixture;
    const auto path = fixture.directory / "identity-kem-lookup.cybou";
    std::filesystem::remove(path);
    cybou::CybouIdentityService identity{*fixture.runtime, path};
    const auto words = identity.PrepareNewIdentity();
    BOOST_REQUIRE(words);
    const auto created = identity.CreateIdentitySync("correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(created.success, created.error_message);
    const auto account = identity.GetAccountId();
    BOOST_REQUIRE(account);

    const auto initial = fixture.runtime->FindIdentityKemPackage(*account, 0);
    BOOST_REQUIRE(initial.status == cybou::IdentityKemPackageLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(initial.operation_height, 1U);
    const auto create_block = fixture.runtime->GetBlockAtHeight(initial.operation_height);
    BOOST_REQUIRE(create_block);
    const auto* create = std::get_if<cybou::AccountCreateOp>(&create_block->block.operations[initial.operation_index]);
    BOOST_REQUIRE(create);
    BOOST_CHECK(create->kem_package == initial.package);
    const auto account_bytes = account->Value();
    const auto expected_initial = cybou::ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{fixture.runtime->GetNetworkBinding().begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32}, 0, initial.package);
    BOOST_REQUIRE(expected_initial);
    BOOST_CHECK(*expected_initial == initial.package_id);

    const auto restore_path = fixture.directory / "identity-kem-restored.cybou";
    std::filesystem::remove(restore_path);
    cybou::CybouIdentityService recovered{*fixture.runtime, restore_path};
    const auto restored = recovered.RestoreIdentitySync(*words, "another correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(restored.success, restored.error_message);
    BOOST_CHECK_EQUAL(fixture.runtime->GetFinalizedHeight().value_or(0), 1U);

    auto next_entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(next_entropy);
    const auto next_words = cybou::EncodeRecoveryWords(*next_entropy);
    cybou::crypto::CleanseMemory(next_entropy->data(), next_entropy->size());
    const auto pending = identity.RotateIdentitySync(next_words, "correct horse battery staple");
    BOOST_REQUIRE(pending.phase == cybou::IdentityOperationPhase::ACCEPTED);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto finalized = identity.ResumeIdentityRotationSync("correct horse battery staple");
    BOOST_REQUIRE(finalized.phase == cybou::IdentityOperationPhase::FINALIZED);

    const auto rotated = fixture.runtime->FindIdentityKemPackage(*account, 1);
    BOOST_REQUIRE(rotated.status == cybou::IdentityKemPackageLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(rotated.operation_height, 2U);
    BOOST_CHECK(rotated.package != initial.package);
    const auto future = fixture.runtime->FindIdentityKemPackage(*account, 2);
    BOOST_CHECK(future.status == cybou::IdentityKemPackageLookupStatus::KEY_EPOCH_UNAVAILABLE);
    std::filesystem::remove(path);
    std::filesystem::remove(restore_path);
}
BOOST_AUTO_TEST_CASE(runtime_rejects_foreign_genesis_and_block)
{
    CybouServiceTestFixture fixture;
    std::array<unsigned char, 32> foreign_seed{};
    foreign_seed[0] = 0xBC;
    auto foreign_genesis = cybou::CreateTestGenesisState();
    ++foreign_genesis.onboarding_pool;
    cybou::NodeRuntimeConfig config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "foreign-observer",
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_CHECK(!observer.InitializeGenesis(foreign_genesis));
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto foreign_definition = cybou::CreateTestNetworkGenesis(foreign_genesis, cybou::TestPoaFinalizerPublicKey(0xBC), cybou::TestNetworkPublicKey(0xBC));
    cybou::NodeRuntimeConfig foreign_config{
        .network_genesis = foreign_definition,
        .data_dir = fixture.directory / "foreign-producer",
        .poa_finalizer_recovery_entropy = foreign_seed,
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime foreign{std::move(foreign_config)};
    BOOST_REQUIRE(foreign.InitializeGenesis(foreign_genesis));
    const auto block = foreign.ProduceBlock();
    BOOST_REQUIRE(block);
    BOOST_CHECK(!observer.CommitBlock(*block));
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0);
}

BOOST_AUTO_TEST_CASE(runtime_explicit_peers_take_priority_over_discovered)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "peer-priority",
        .advertised_endpoint = std::make_pair("127.0.0.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    // Operator-approved validator endpoints.
    runtime.SetConfiguredPeerEndpoints({{"10.0.0.10", 8333}, {"10.0.0.11", 8333}});
    // Malicious flood: lexicographically smaller addresses that would eclipse
    // the validator topology in a single sorted set, plus this node's own
    // listener, plus out-of-scope targets.
    std::vector<std::pair<std::string, uint16_t>> flood;
    for (int i = 1; i <= 40; ++i) {
        flood.emplace_back("1.1.1." + std::to_string(i), 7000);
    }
    flood.emplace_back("127.0.0.1", 29001); // own listener
    flood.emplace_back("127.0.0.1", 29002); // own address, other port
    flood.emplace_back("0.0.0.0", 8333);    // unspecified
    flood.emplace_back("224.0.0.1", 8333);  // multicast
    flood.emplace_back("169.254.1.1", 8333); // link-local
    runtime.AddDiscoveredPeerEndpoints(flood);

    const auto gossip = runtime.GetPeerEndpointsForGossip();
    // Capped at 32 targets: explicit peers first, discovered flood behind them.
    BOOST_REQUIRE_EQUAL(gossip.size(), 32U);
    // Explicit validator peers always come first, in front of any discovered
    // lexicographically-smaller hint.
    BOOST_CHECK_EQUAL(gossip[0].first, "10.0.0.10");
    BOOST_CHECK_EQUAL(gossip[1].first, "10.0.0.11");
    BOOST_CHECK_EQUAL(gossip[0].second, 8333);
    // Flood addresses are present but strictly behind the explicit peers.
    for (size_t i = 2; i < gossip.size(); ++i) {
        BOOST_CHECK(gossip[i].first.rfind("1.1.1.", 0) == 0);
    }
}

BOOST_AUTO_TEST_CASE(runtime_discovery_filters_self_and_out_of_scope_addresses)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "peer-policy",
        .advertised_endpoint = std::make_pair("203.0.113.5", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    runtime.SetConfiguredPeerEndpoints({
        {"10.0.0.7", 8333}, {"172.16.0.7", 8333}, {"192.168.0.7", 8333},
    });

    // With a public listener, loopback/link-local/unspecified/multicast
    // and private discovered targets must not be dialed (SSRF-style pivot).
    runtime.AddDiscoveredPeerEndpoints({
        {"203.0.113.5", 29001},   // own listener: always rejected
        {"127.0.0.1", 8333},      // loopback rejected: listener is public
        {"::1", 8333},            // v6 loopback rejected
        {"169.254.10.20", 8333},  // v4 link-local rejected
        {"fe80::1", 8333},        // v6 link-local rejected
        {"10.1.2.3", 8333},       // RFC1918 rejected: listener is public
        {"172.31.2.3", 8333},     // RFC1918 rejected: listener is public
        {"192.168.2.3", 8333},    // RFC1918 rejected: listener is public
        {"fd00::1", 8333},        // unique-local rejected: listener is public
        {"::ffff:10.1.2.3", 8333}, // mapped RFC1918 rejected as well
        {"0.0.0.0", 8333},        // unspecified rejected
        {"::", 8333},             // unspecified v6 rejected
        {"224.0.0.1", 8333},      // multicast rejected
        {"198.51.100.7", 8333},   // public: accepted
    });
    const auto gossip = runtime.GetPeerEndpointsForGossip();
    BOOST_REQUIRE_EQUAL(gossip.size(), 4U);
    BOOST_CHECK_EQUAL(gossip[0].first, "10.0.0.7");
    BOOST_CHECK_EQUAL(gossip[1].first, "172.16.0.7");
    BOOST_CHECK_EQUAL(gossip[2].first, "192.168.0.7");
    BOOST_CHECK_EQUAL(gossip[3].first, "198.51.100.7");
}

BOOST_AUTO_TEST_CASE(runtime_private_listener_accepts_private_discovery)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "private-peer-policy",
        .advertised_endpoint = std::make_pair("10.1.1.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true, .operation_work_bits = 0
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));
    runtime.AddDiscoveredPeerEndpoints({
        {"10.1.1.1", 29001},    // own listener remains rejected
        {"10.1.1.2", 29002},    // private discovery is allowed on private nodes
        {"172.16.2.3", 29003},
        {"192.168.2.4", 29004},
        {"198.51.100.5", 29005},
    });

    const auto gossip = runtime.GetPeerEndpointsForGossip();
    BOOST_REQUIRE_EQUAL(gossip.size(), 4U);
    BOOST_CHECK_EQUAL(gossip[0].first, "10.1.1.2");
    BOOST_CHECK_EQUAL(gossip[1].first, "172.16.2.3");
    BOOST_CHECK_EQUAL(gossip[2].first, "192.168.2.4");
    BOOST_CHECK_EQUAL(gossip[3].first, "198.51.100.5");
}

BOOST_AUTO_TEST_CASE(sync_completion_is_advisory_for_ordinary_peers)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor finalizer_acceptor{io, tcp::endpoint{loopback, 0}};
    const auto finalizer_port = finalizer_acceptor.local_endpoint().port();
    const auto tip = fixture.runtime->GetFinalizedTip();
    BOOST_REQUIRE(tip);
    std::atomic_bool finalizer_served{false};
    std::jthread finalizer_server{[&] {
        tcp::socket socket{io};
        finalizer_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = fixture.HandshakeAsPeer(session, {.network_binding = fixture.runtime->GetNetworkBinding(),
            .finalized_height = 1, .finalized_tip = *tip,
            .nonce = 1301});
        finalizer_served = handshake;
        for (int i = 0; i < 5 && finalizer_served; ++i) finalizer_served = session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig observer_config{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "finalizer-tip-observer",
        .configured_peers = {{std::make_pair(loopback.to_string(), finalizer_port)}},
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto finalizer_sync = observer.SyncFromConfiguredPeer(10);
    finalizer_server.join();
    BOOST_CHECK(finalizer_served.load());
    BOOST_CHECK_EQUAL(finalizer_sync.blocks_applied, 1U);
    BOOST_CHECK(finalizer_sync.caught_up_with_known_peers);

    tcp::acceptor storage_acceptor{io, tcp::endpoint{loopback, 0}};
    const auto storage_port = storage_acceptor.local_endpoint().port();
    std::atomic_bool storage_served{false};
    std::jthread storage_server{[&] {
        tcp::socket socket{io};
        storage_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = session.Handshake({.network_binding = fixture.runtime->GetNetworkBinding(),
            .finalized_height = 1, .finalized_tip = *tip,
            .nonce = 1302});
        storage_served = handshake;
        for (int i = 0; i < 5 && storage_served; ++i) storage_served = session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig storage_observer_config{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "provider-tip-observer",
        .configured_peers = {{std::make_pair(loopback.to_string(), storage_port)}},
        .memory_only = true, .wipe_data = true, .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0};
    cybou::CybouNodeRuntime storage_observer{std::move(storage_observer_config)};
    BOOST_REQUIRE(storage_observer.InitializeGenesis(fixture.genesis));
    const auto storage_sync = storage_observer.SyncFromConfiguredPeer(10);
    storage_server.join();
    BOOST_CHECK(storage_served.load());
    BOOST_CHECK_EQUAL(storage_sync.blocks_applied, 1U);
    BOOST_CHECK(storage_sync.caught_up_with_known_peers);
}

BOOST_AUTO_TEST_SUITE_END()
