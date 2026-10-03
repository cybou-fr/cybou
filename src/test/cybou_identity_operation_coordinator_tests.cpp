// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_operation_coordinator.h>
#include <cybou/p2p/session.h>
#include <cybou/private_application_store.h>
#include <cybou/recovery_phrase.h>
#include <cybou/publication_service.h>
#include <cybou/identity_service.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/name_registry.h>

#include <test/cybou_test_setup.h>
#include <test/cybou_service_test_fixture.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_identity_operation_coordinator_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(relayed_identity_operation_is_retried_after_volatile_ack)
{
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("relay-retry-identity.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    const auto account_block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(account_block);

    cybou::CybouNodeRuntime relay{{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "ordinary-relay",
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(relay.InitializeGenesis(fixture.genesis));
    BOOST_REQUIRE(relay.CommitBlock(*account_block));

    asio::io_context io;
    const auto loopback = asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const uint16_t port = acceptor.local_endpoint().port();
    std::atomic_bool first_served{false};
    std::atomic_bool retry_served{false};
    std::thread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const bool handshake = session.Handshake({.network_binding = fixture.runtime->GetNetworkBinding(),
            .finalized_height = 1, .finalized_tip = cybou::ComputeBlockId(account_block->block),
            .nonce = 7401});
        if (!handshake) return;
        first_served = session.ServeNext(relay);
        if (first_served) retry_served = session.ServeNext(relay);
    }};

    cybou::CybouNodeRuntime client{{
        .network_genesis = fixture.definition,
        .data_dir = fixture.directory / "retry-origin",
        .configured_peers = {{std::pair<std::string, uint16_t>{loopback.to_string(), port}}},
        .memory_only = true,
        .wipe_data = true,
        .peer_admission_policy = TestLabAdmissionPolicy(),
    }};
    BOOST_REQUIRE(client.InitializeGenesis(fixture.genesis));
    BOOST_REQUIRE(client.CommitBlock(*account_block));

    cybou::IdentityOperationCoordinator coordinator{client, identity->GetKeyStore(),
        fixture.directory / "relay-retry.cyiop", std::chrono::milliseconds{5}};
    std::array<unsigned char, 32> salt{};
    salt[0] = 0x61;
    const auto payload = cybou::NameCommitPayload{
        .commitment = cybou::ComputeNameCommitment(client.GetNetworkBinding(), *account, "relayretry", salt)};
    const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
    BOOST_REQUIRE(payload_commitment);
    const auto submitted = coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT,
        *payload_commitment, [&](const cybou::IdentityOperationAuthorization& authorization)
            -> std::optional<cybou::ProtocolOperation> {
            return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, payload}};
        });

    bool retry_status_accepted{false};
    bool operation_requeued{false};
    if (submitted.phase == cybou::IdentityOperationPhase::ACCEPTED && !submitted.op_id.IsNull()) {
        const auto queued = relay.ClaimRelayedOperation();
        if (queued && queued->operation_id == submitted.op_id) {
            relay.AcknowledgeRelayedOperation(submitted.op_id); // Simulate the volatile relay losing its queued copy.
            std::this_thread::sleep_for(std::chrono::milliseconds{15});
            client.RetryPendingIdentityOperations();
            const auto retried = coordinator.GetStatus(submitted.op_id);
            retry_status_accepted = retried.phase == cybou::IdentityOperationPhase::ACCEPTED;
            const auto restored = relay.ClaimRelayedOperation();
            operation_requeued = restored && restored->operation_id == submitted.op_id &&
                restored->exact_bytes == queued->exact_bytes;
            if (restored) relay.ReleaseRelayedOperation(submitted.op_id);
        }
    }
    server.join();
    acceptor.close();

    BOOST_CHECK(submitted.phase == cybou::IdentityOperationPhase::ACCEPTED);
    BOOST_CHECK(first_served.load());
    BOOST_CHECK(retry_status_accepted);
    BOOST_CHECK(retry_served.load());
    BOOST_CHECK(operation_requeued);
}

BOOST_AUTO_TEST_CASE(uncertain_submission_keeps_one_exact_journal_across_restart_and_corruption)
{
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const auto root = std::filesystem::temp_directory_path() /
        ("cybou-identity-operation-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    std::array<unsigned char, 32> validator_seed{};
    validator_seed[0] = 0xA7;
    const auto genesis = cybou::CreateTestGenesisState();
    auto definition = cybou::CreateTestNetworkGenesis(genesis, cybou::TestPoaFinalizerPublicKey(), cybou::TestNetworkPublicKey());
    definition = cybou::WithTestGenesisParameters(definition, [](auto& params) { params.account_creation_work_bits = 0; });
    const auto network_binding = cybou::ComputeNetworkBinding(definition.GetNetworkPublicKey());

    cybou::NodeRuntimeConfig producer_config{
        .network_genesis = definition,
        .data_dir = root / "producer",
        .poa_finalizer_recovery_entropy = validator_seed,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime producer{std::move(producer_config)};
    BOOST_REQUIRE(producer.InitializeGenesis(genesis));
    cybou::CybouIdentityService identity{producer, root / "identity.cybou"};
    BOOST_REQUIRE(identity.PrepareNewIdentity());
    const auto created = identity.CreateIdentitySync("correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(created.success, created.error_message);
    const auto account = identity.GetAccountId();
    BOOST_REQUIRE(account);
    const auto account_block = producer.GetBlockAtHeight(1);
    BOOST_REQUIRE(account_block);

    asio::io_context server_io;
    tcp::acceptor acceptor{server_io, tcp::endpoint{asio::ip::address_v4::loopback(), 0}};
    const uint16_t port = acceptor.local_endpoint().port();
    std::thread remote([&] {
        for (int attempt{0}; attempt < 2; ++attempt) {
            tcp::socket socket{server_io};
            acceptor.accept(socket);
            // Model a lost acknowledgment: handshake as an operation-accepting
            // finalizer, then drop the session without answering.
            cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
            (void)session.Handshake({.network_binding = network_binding, .finalized_height = 1,
                .finalized_tip = cybou::ComputeBlockId(account_block->block),
                .nonce = static_cast<std::uint64_t>(4100 + attempt)});
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
        }
    });

    const auto client_data = root / "client";
    const auto make_client_config = [&](const bool wipe_data) {
        return cybou::NodeRuntimeConfig{
            .network_genesis = definition,
            .data_dir = client_data,
            .configured_peers = {{std::pair<std::string, uint16_t>{"127.0.0.1", port}}},
            .memory_only = false,
            .wipe_data = wipe_data,
            .peer_admission_policy = TestLabAdmissionPolicy(),
        };
    };
    uint256 operation_id;
    {
        cybou::CybouNodeRuntime client{make_client_config(true)};
        BOOST_REQUIRE(client.InitializeGenesis(genesis));
        BOOST_REQUIRE(client.CommitBlock(*account_block));

        std::array<unsigned char, 32> salt{};
        salt[0] = 0x51;
        const auto commitment = cybou::ComputeNameCommitment(network_binding, *account, "alicecy", salt);
        const auto payload = cybou::NameCommitPayload{.commitment = commitment};
        const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
        BOOST_REQUIRE(payload_commitment);
        auto& coordinator = client.GetIdentityOperationCoordinator(identity.GetKeyStore());
        const auto result = coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT, *payload_commitment,
            [&](const cybou::IdentityOperationAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, payload}};
            });
        BOOST_CHECK(result.phase == cybou::IdentityOperationPhase::UNCERTAIN);
        BOOST_REQUIRE(!result.op_id.IsNull());
        operation_id = result.op_id;
        BOOST_CHECK(std::filesystem::exists(client_data / "identity-operation.cyiop"));
    }

    {
        cybou::CybouNodeRuntime restarted{make_client_config(false)};
        BOOST_REQUIRE(restarted.InitializeGenesis(genesis));
        auto& coordinator = restarted.GetIdentityOperationCoordinator(identity.GetKeyStore());
        const auto commitment = cybou::ComputeNameCommitment(
            network_binding, *account, "alicecy", std::array<unsigned char, 32>{0x51});
        const auto payload = cybou::NameCommitPayload{.commitment = commitment};
        const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
        BOOST_REQUIRE(payload_commitment);
        const auto result = coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT, *payload_commitment,
            [&](const cybou::IdentityOperationAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, payload}};
            });
        BOOST_CHECK(result.phase == cybou::IdentityOperationPhase::UNCERTAIN);
        BOOST_CHECK(result.op_id == operation_id);
        const cybou::PaymentPayload payment{.recipient = *account, .amount = 1};
        const auto payment_commitment = cybou::ComputePaymentPayloadCommitment(payment);
        BOOST_REQUIRE(payment_commitment);
        const auto wallet_attempt = coordinator.Execute(cybou::IdentityOperationKind::PAYMENT, *payment_commitment,
            [&](const cybou::IdentityOperationAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedPayment{authorization, payment}};
            });
        BOOST_CHECK(wallet_attempt.phase == cybou::IdentityOperationPhase::CONFLICT);
        BOOST_CHECK(wallet_attempt.op_id == operation_id);
        const auto status = coordinator.GetStatus(operation_id);
        BOOST_CHECK(status.phase == cybou::IdentityOperationPhase::UNCERTAIN);
        const auto loaded = restarted.GetStore().LoadState();
        BOOST_REQUIRE(loaded && loaded.state);
        const auto* record = loaded.state->identities.Find(*account);
        BOOST_REQUIRE(record);
        BOOST_CHECK_EQUAL(record->nonce, 0U);

        std::array<unsigned char, 32> competing_salt{};
        competing_salt[0] = 0x53;
        const auto competing_commitment = cybou::ComputeNameCommitment(
            network_binding, *account, "competingcy", competing_salt);
        const cybou::NameCommitPayload competing_payload{.commitment = competing_commitment};
        const auto competing_digest = cybou::ComputeNameCommitPayloadCommitment(competing_payload);
        BOOST_REQUIRE(competing_digest);
        auto& producer_coordinator = producer.GetIdentityOperationCoordinator(identity.GetKeyStore());
        const auto competing = producer_coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT,
            *competing_digest,
            [&](const cybou::IdentityOperationAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, competing_payload}};
            });
        BOOST_REQUIRE(competing.phase == cybou::IdentityOperationPhase::ACCEPTED);
        const auto nonce_advance_block = producer.ProduceBlock();
        BOOST_REQUIRE(nonce_advance_block);
        BOOST_REQUIRE(restarted.CommitBlock(*nonce_advance_block));
        const auto nonce_advanced_conflict = coordinator.Execute(
            cybou::IdentityOperationKind::PAYMENT, *payment_commitment,
            [&](const cybou::IdentityOperationAuthorization&) -> std::optional<cybou::ProtocolOperation> {
                BOOST_ERROR("A new operation builder must not run after nonce history conflicts");
                return std::nullopt;
            });
        BOOST_CHECK(nonce_advanced_conflict.phase == cybou::IdentityOperationPhase::CONFLICT);
        BOOST_CHECK(nonce_advanced_conflict.op_id == operation_id);
    }
    remote.join();
    acceptor.close();

    const auto journal_path = client_data / "identity-operation.cyiop";
    std::ifstream journal_input{journal_path, std::ios::binary};
    const std::vector<unsigned char> valid_journal{std::istreambuf_iterator<char>{journal_input},
        std::istreambuf_iterator<char>{}};
    journal_input.close();
    BOOST_REQUIRE(!valid_journal.empty());
    const auto verify_fail_closed = [&](const std::vector<unsigned char>& bytes) {
        {
            std::ofstream output{journal_path, std::ios::binary | std::ios::trunc};
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        cybou::CybouNodeRuntime damaged_runtime{make_client_config(false)};
        BOOST_REQUIRE(damaged_runtime.InitializeGenesis(genesis));
        auto& coordinator = damaged_runtime.GetIdentityOperationCoordinator(identity.GetKeyStore());
        std::array<unsigned char, 32> salt{};
        salt[0] = 0x52;
        const auto name_commitment = cybou::ComputeNameCommitment(network_binding, *account, "bobcy", salt);
        const cybou::NameCommitPayload payload{.commitment = name_commitment};
        const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
        BOOST_REQUIRE(payload_commitment);
        bool builder_called{false};
        const auto result = coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT, *payload_commitment,
            [&](const cybou::IdentityOperationAuthorization&) -> std::optional<cybou::ProtocolOperation> {
                builder_called = true;
                return std::nullopt;
            });
        BOOST_CHECK(result.phase == cybou::IdentityOperationPhase::CONFLICT);
        BOOST_CHECK(!builder_called);
    };
    auto corrupt_journal = valid_journal;
    corrupt_journal.back() ^= 0x01;
    verify_fail_closed(corrupt_journal);
    verify_fail_closed(std::vector<unsigned char>(valid_journal.begin(), valid_journal.begin() + 12));

    std::array<unsigned char, 32> foreign_validator_seed{};
    foreign_validator_seed[0] = 0x6a;
    const auto foreign_genesis = cybou::CreateTestGenesisState();
    auto foreign_definition = cybou::CreateTestNetworkGenesis(foreign_genesis, cybou::TestPoaFinalizerPublicKey(0xBC), cybou::TestNetworkPublicKey(0xBC));
    foreign_definition = cybou::WithTestGenesisParameters(foreign_definition, [](auto& params) { params.account_creation_work_bits = 0; });
    const auto foreign_data = root / "foreign-client";
    std::filesystem::create_directories(foreign_data);
    {
        std::ofstream output{foreign_data / "identity-operation.cyiop", std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(valid_journal.data()),
            static_cast<std::streamsize>(valid_journal.size()));
    }
    {
        cybou::NodeRuntimeConfig foreign_config{
            .network_genesis = foreign_definition,
            .data_dir = foreign_data,
            .memory_only = false,
            .wipe_data = false,
        };
        cybou::CybouNodeRuntime foreign_runtime{std::move(foreign_config)};
        BOOST_REQUIRE(foreign_runtime.InitializeGenesis(foreign_genesis));
        BOOST_REQUIRE(foreign_runtime.GetNetworkBinding() != network_binding);
        auto& foreign_coordinator = foreign_runtime.GetIdentityOperationCoordinator(identity.GetKeyStore());
        bool foreign_builder_called{false};
        const auto foreign_result = foreign_coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT,
            cybou::IdentityKeyId{}, [&](const cybou::IdentityOperationAuthorization&) -> std::optional<cybou::ProtocolOperation> {
                foreign_builder_called = true;
                return std::nullopt;
            });
        BOOST_CHECK(foreign_result.phase == cybou::IdentityOperationPhase::CONFLICT);
        BOOST_CHECK(!foreign_builder_called);
    }

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    BOOST_CHECK(!ec);
}

BOOST_AUTO_TEST_CASE(restore_is_local_and_rotation_uses_the_identity_journal)
{
    CybouServiceTestFixture fixture{0x68};
    const auto identity = fixture.CreateIdentity("identity-restore-rotation.cybou");
    const auto account = identity->GetAccountId();
    const auto phrase = identity->GetKeyStore().GetRecoveryWords();
    BOOST_REQUIRE(account && phrase);
    const uint64_t before_restore = fixture.runtime->GetFinalizedHeight().value_or(0);

    const auto restore_path = fixture.directory / "identity-restored.cybou";
    cybou::CybouIdentityService restored{*fixture.runtime, restore_path};
    const auto restore_result = restored.RestoreIdentitySync(*phrase, "another correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(restore_result.success, restore_result.error_message);
    BOOST_CHECK_EQUAL(fixture.runtime->GetFinalizedHeight().value_or(0), before_restore);
    const auto state_before = fixture.runtime->GetStore().LoadState();
    BOOST_REQUIRE(state_before && state_before.state);
    const auto* record_before = state_before.state->identities.Find(*account);
    BOOST_REQUIRE(record_before);
    BOOST_CHECK_EQUAL(record_before->nonce, 0U);
    BOOST_CHECK_EQUAL(record_before->key_epoch, 0U);

    auto entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(entropy);
    const auto new_phrase = cybou::EncodeRecoveryWords(*entropy);
    cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
    const auto pending = identity->RotateIdentitySync(new_phrase, "correct horse battery staple");
    BOOST_REQUIRE(pending.phase == cybou::IdentityOperationPhase::ACCEPTED);
    BOOST_REQUIRE(std::filesystem::exists(fixture.directory / "runtime" / "identity-operation.cyiop"));
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto finalized = identity->ResumeIdentityRotationSync("correct horse battery staple");
    BOOST_REQUIRE(finalized.phase == cybou::IdentityOperationPhase::FINALIZED);
    BOOST_CHECK(!std::filesystem::exists(fixture.directory / "runtime" / "identity-operation.cyiop"));
    const auto state_after = fixture.runtime->GetStore().LoadState();
    BOOST_REQUIRE(state_after && state_after.state);
    const auto* record_after = state_after.state->identities.Find(*account);
    BOOST_REQUIRE(record_after);
    BOOST_CHECK_EQUAL(record_after->nonce, 1U);
    BOOST_CHECK_EQUAL(record_after->key_epoch, 1U);
}
BOOST_AUTO_TEST_CASE(concurrent_execute_reserves_only_one_identity_operation)
{
    CybouServiceTestFixture fixture{0x69};
    auto identity = fixture.CreateIdentity("coordinator-concurrent.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());

    std::array<cybou::NameCommitPayload, 2> payloads;
    std::array<cybou::IdentityKeyId, 2> commitments{};
    for (size_t i = 0; i < payloads.size(); ++i) {
        std::array<unsigned char, 32> salt{};
        salt[0] = static_cast<unsigned char>(0x70 + i);
        const std::string label = i == 0 ? "firstcy" : "secondcy";
        payloads[i].commitment = cybou::ComputeNameCommitment(
            fixture.runtime->GetNetworkBinding(), *account, label, salt);
        const auto digest = cybou::ComputeNameCommitPayloadCommitment(payloads[i]);
        BOOST_REQUIRE(digest);
        commitments[i] = *digest;
    }

    std::barrier start{3};
    std::atomic<unsigned> builders_called{0};
    std::array<cybou::IdentityOperationResult, 2> results;
    std::array<std::thread, 2> callers;
    for (size_t i = 0; i < callers.size(); ++i) {
        callers[i] = std::thread{[&, i] {
            start.arrive_and_wait();
            results[i] = coordinator.Execute(cybou::IdentityOperationKind::NAME_COMMIT, commitments[i],
                [&, i](const cybou::IdentityOperationAuthorization& authorization)
                    -> std::optional<cybou::ProtocolOperation> {
                    builders_called.fetch_add(1);
                    return cybou::ProtocolOperation{
                        cybou::AuthorizedNameCommit{authorization, payloads[i]}};
                });
        }};
    }
    start.arrive_and_wait();
    for (auto& caller : callers) caller.join();

    const auto accepted_count = std::count_if(results.begin(), results.end(), [](const auto& result) {
        return result.phase == cybou::IdentityOperationPhase::ACCEPTED;
    });
    const auto conflict_count = std::count_if(results.begin(), results.end(), [](const auto& result) {
        return result.phase == cybou::IdentityOperationPhase::CONFLICT;
    });
    BOOST_CHECK_EQUAL(accepted_count, 1U);
    BOOST_CHECK_EQUAL(conflict_count, 1U);
    BOOST_CHECK_EQUAL(builders_called.load(), 1U);
    BOOST_CHECK(!results[0].op_id.IsNull());
    BOOST_CHECK(results[0].op_id == results[1].op_id);
}

BOOST_AUTO_TEST_CASE(each_key_store_gets_its_own_coordinator)
{
    CybouServiceTestFixture fixture;
    auto first = fixture.CreateIdentity("coordinator-first.cybou");
    auto second = fixture.CreateIdentity("coordinator-second.cybou");
    auto& a = fixture.runtime->GetIdentityOperationCoordinator(first->GetKeyStore());
    auto& b = fixture.runtime->GetIdentityOperationCoordinator(second->GetKeyStore());
    // A coordinator signs with its own key store, so it is never shared.
    BOOST_CHECK(&a != &b);
    BOOST_CHECK(&a == &fixture.runtime->GetIdentityOperationCoordinator(first->GetKeyStore()));
    BOOST_CHECK(&b == &fixture.runtime->GetIdentityOperationCoordinator(second->GetKeyStore()));
}

BOOST_AUTO_TEST_CASE(finalized_earlier_operation_does_not_block_rotation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("rotate-after-operation.cybou");
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
    // Leave a finalized RootPublication (for example a RecoveryBridge) in the journal.
    cybou::KVStore staging{cybou::KVStoreOptions{.memory_only = true}};
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "app"};
    cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(), db, coordinator};
    cybou::FilesMutationBatch batch;
    const auto item = *cybou::NewPrivateItemId();
    batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item,
        cybou::FileItem{.item_id = item, .kind = cybou::FileItemKind::FOLDER, .name = "before-rotation"}});
    BOOST_REQUIRE(publication.PublishFiles("before-rotation", batch).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());

    auto next = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(next);
    const auto words = cybou::EncodeRecoveryWords(*next);
    cybou::crypto::CleanseMemory(next->data(), next->size());
    const auto rotated = identity->RotateIdentitySync(words, "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(rotated.phase == cybou::IdentityOperationPhase::ACCEPTED, rotated.error);
}

BOOST_AUTO_TEST_SUITE_END()
