// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/device_operation_coordinator.h>
#include <cybou/identity_service.h>
#include <cybou/name_registry.h>
#include <cybou/validator.h>

#include <test/util/setup_common.h>
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

BOOST_FIXTURE_TEST_SUITE(cybou_device_operation_coordinator_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(uncertain_submission_keeps_one_exact_journal_across_restart_and_corruption)
{
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const auto root = std::filesystem::temp_directory_path() /
        ("cybou-device-operation-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    std::array<unsigned char, 32> validator_seed{};
    validator_seed[0] = 0x67;
    const auto validator = cybou::GenerateValidatorKeyPair(validator_seed);
    BOOST_REQUIRE(validator);
    const auto genesis = cybou::CreateDevGenesisState(validator->public_key);
    auto definition = cybou::CreateDevNetworkDefinition(genesis);
    definition.protocol_parameters.account_creation_work_bits = 0;
    const auto network_id = cybou::NetworkId(definition);

    cybou::NodeRuntimeConfig producer_config{
        .network_definition = definition,
        .data_dir = root / "producer",
        .validator_private_key = validator_seed,
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
    std::array<std::vector<unsigned char>, 3> received_frames;
    std::thread remote([&] {
        for (auto& frame : received_frames) {
            tcp::socket socket{server_io};
            acceptor.accept(socket);
            std::array<unsigned char, 40> header{};
            asio::read(socket, asio::buffer(header));
            const uint32_t payload_size = uint32_t{header[36]} |
                (uint32_t{header[37]} << 8) | (uint32_t{header[38]} << 16) |
                (uint32_t{header[39]} << 24);
            frame.assign(header.begin(), header.end());
            const size_t offset = frame.size();
            frame.resize(offset + payload_size);
            asio::read(socket, asio::buffer(frame.data() + offset, payload_size));
            // Closing without the response models a lost acknowledgment after delivery.
        }
    });

    const auto client_data = root / "client";
    cybou::NodeRuntimeConfig client_config{
        .network_definition = definition,
        .data_dir = client_data,
        .submit_endpoint = std::pair<std::string, uint16_t>{"127.0.0.1", port},
        .memory_only = false,
        .wipe_data = true,
    };
    uint256 operation_id;
    {
        cybou::CybouNodeRuntime client{client_config};
        BOOST_REQUIRE(client.InitializeGenesis(genesis));
        BOOST_REQUIRE(client.CommitBlock(*account_block));

        std::array<unsigned char, 32> salt{};
        salt[0] = 0x51;
        const auto commitment = cybou::ComputeNameCommitment(network_id, *account, "alicecy", salt);
        const auto payload = cybou::NameCommitPayload{.commitment = commitment};
        const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
        BOOST_REQUIRE(payload_commitment);
        auto& coordinator = client.GetDeviceOperationCoordinator(identity.GetKeyStore());
        const auto result = coordinator.Execute(cybou::DeviceOperationKind::NAME_COMMIT, *payload_commitment,
            [&](const cybou::DeviceAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, payload}};
            });
        BOOST_CHECK(result.phase == cybou::DeviceOperationPhase::UNCERTAIN);
        BOOST_REQUIRE(!result.op_id.IsNull());
        operation_id = result.op_id;
        BOOST_CHECK(std::filesystem::exists(client_data / "device-operation.cydop"));
    }

    client_config.wipe_data = false;
    {
        cybou::CybouNodeRuntime restarted{client_config};
        BOOST_REQUIRE(restarted.InitializeGenesis(genesis));
        auto& coordinator = restarted.GetDeviceOperationCoordinator(identity.GetKeyStore());
        const auto commitment = cybou::ComputeNameCommitment(
            network_id, *account, "alicecy", std::array<unsigned char, 32>{0x51});
        const auto payload = cybou::NameCommitPayload{.commitment = commitment};
        const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
        BOOST_REQUIRE(payload_commitment);
        const auto result = coordinator.Execute(cybou::DeviceOperationKind::NAME_COMMIT, *payload_commitment,
            [&](const cybou::DeviceAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, payload}};
            });
        BOOST_CHECK(result.phase == cybou::DeviceOperationPhase::UNCERTAIN);
        BOOST_CHECK(result.op_id == operation_id);
        const cybou::PaymentPayload payment{.recipient = *account, .amount = 1};
        const auto payment_commitment = cybou::ComputePaymentPayloadCommitment(payment);
        BOOST_REQUIRE(payment_commitment);
        const auto wallet_attempt = coordinator.Execute(cybou::DeviceOperationKind::PAYMENT, *payment_commitment,
            [&](const cybou::DeviceAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedPayment{authorization, payment}};
            });
        BOOST_CHECK(wallet_attempt.phase == cybou::DeviceOperationPhase::CONFLICT);
        BOOST_CHECK(wallet_attempt.op_id == operation_id);
        BOOST_CHECK(received_frames[0] == received_frames[1]);
        BOOST_CHECK(received_frames[1] == received_frames[2]);
        const auto status = coordinator.GetStatus(operation_id);
        BOOST_CHECK(status.phase == cybou::DeviceOperationPhase::UNCERTAIN);
        const auto loaded = restarted.GetStore().LoadState();
        BOOST_REQUIRE(loaded && loaded.state);
        const auto* record = loaded.state->identities.Find(*account);
        BOOST_REQUIRE(record);
        const auto device_id = identity.GetKeyStore().GetDeviceId();
        BOOST_REQUIRE(device_id);
        BOOST_CHECK_EQUAL(record->devices.at(*device_id).next_nonce, 0U);

        std::array<unsigned char, 32> competing_salt{};
        competing_salt[0] = 0x53;
        const auto competing_commitment = cybou::ComputeNameCommitment(
            network_id, *account, "competingcy", competing_salt);
        const cybou::NameCommitPayload competing_payload{.commitment = competing_commitment};
        const auto competing_digest = cybou::ComputeNameCommitPayloadCommitment(competing_payload);
        BOOST_REQUIRE(competing_digest);
        auto& producer_coordinator = producer.GetDeviceOperationCoordinator(identity.GetKeyStore());
        const auto competing = producer_coordinator.Execute(cybou::DeviceOperationKind::NAME_COMMIT,
            *competing_digest,
            [&](const cybou::DeviceAuthorization& authorization) -> std::optional<cybou::ProtocolOperation> {
                return cybou::ProtocolOperation{cybou::AuthorizedNameCommit{authorization, competing_payload}};
            });
        BOOST_REQUIRE(competing.phase == cybou::DeviceOperationPhase::ACCEPTED);
        const auto nonce_advance_block = producer.ProduceBlock();
        BOOST_REQUIRE(nonce_advance_block);
        BOOST_REQUIRE(restarted.CommitBlock(*nonce_advance_block));
        const auto nonce_advanced_conflict = coordinator.Execute(
            cybou::DeviceOperationKind::PAYMENT, *payment_commitment,
            [&](const cybou::DeviceAuthorization&) -> std::optional<cybou::ProtocolOperation> {
                BOOST_ERROR("A new operation builder must not run after nonce history conflicts");
                return std::nullopt;
            });
        BOOST_CHECK(nonce_advanced_conflict.phase == cybou::DeviceOperationPhase::CONFLICT);
        BOOST_CHECK(nonce_advanced_conflict.op_id == operation_id);
    }
    remote.join();
    acceptor.close();

    const auto journal_path = client_data / "device-operation.cydop";
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
        cybou::CybouNodeRuntime damaged_runtime{client_config};
        BOOST_REQUIRE(damaged_runtime.InitializeGenesis(genesis));
        auto& coordinator = damaged_runtime.GetDeviceOperationCoordinator(identity.GetKeyStore());
        std::array<unsigned char, 32> salt{};
        salt[0] = 0x52;
        const auto name_commitment = cybou::ComputeNameCommitment(network_id, *account, "bobcy", salt);
        const cybou::NameCommitPayload payload{.commitment = name_commitment};
        const auto payload_commitment = cybou::ComputeNameCommitPayloadCommitment(payload);
        BOOST_REQUIRE(payload_commitment);
        bool builder_called{false};
        const auto result = coordinator.Execute(cybou::DeviceOperationKind::NAME_COMMIT, *payload_commitment,
            [&](const cybou::DeviceAuthorization&) -> std::optional<cybou::ProtocolOperation> {
                builder_called = true;
                return std::nullopt;
            });
        BOOST_CHECK(result.phase == cybou::DeviceOperationPhase::CONFLICT);
        BOOST_CHECK(!builder_called);
    };
    auto corrupt_journal = valid_journal;
    corrupt_journal.back() ^= 0x01;
    verify_fail_closed(corrupt_journal);
    verify_fail_closed(std::vector<unsigned char>(valid_journal.begin(), valid_journal.begin() + 12));

    std::array<unsigned char, 32> foreign_validator_seed{};
    foreign_validator_seed[0] = 0x6a;
    const auto foreign_validator = cybou::GenerateValidatorKeyPair(foreign_validator_seed);
    BOOST_REQUIRE(foreign_validator);
    const auto foreign_genesis = cybou::CreateDevGenesisState(foreign_validator->public_key);
    auto foreign_definition = cybou::CreateDevNetworkDefinition(foreign_genesis);
    foreign_definition.protocol_parameters.account_creation_work_bits = 0;
    const auto foreign_data = root / "foreign-client";
    std::filesystem::create_directories(foreign_data);
    {
        std::ofstream output{foreign_data / "device-operation.cydop", std::ios::binary | std::ios::trunc};
        output.write(reinterpret_cast<const char*>(valid_journal.data()),
            static_cast<std::streamsize>(valid_journal.size()));
    }
    {
        cybou::NodeRuntimeConfig foreign_config{
            .network_definition = foreign_definition,
            .data_dir = foreign_data,
            .memory_only = false,
            .wipe_data = false,
        };
        cybou::CybouNodeRuntime foreign_runtime{std::move(foreign_config)};
        BOOST_REQUIRE(foreign_runtime.InitializeGenesis(foreign_genesis));
        BOOST_REQUIRE(foreign_runtime.GetNetworkId() != network_id);
        auto& foreign_coordinator = foreign_runtime.GetDeviceOperationCoordinator(identity.GetKeyStore());
        bool foreign_builder_called{false};
        const auto foreign_result = foreign_coordinator.Execute(cybou::DeviceOperationKind::NAME_COMMIT,
            cybou::IdentityKeyId{}, [&](const cybou::DeviceAuthorization&) -> std::optional<cybou::ProtocolOperation> {
                foreign_builder_called = true;
                return std::nullopt;
            });
        BOOST_CHECK(foreign_result.phase == cybou::DeviceOperationPhase::CONFLICT);
        BOOST_CHECK(!foreign_builder_called);
    }

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    BOOST_CHECK(!ec);
}

BOOST_AUTO_TEST_CASE(recovery_device_add_uses_journaled_root_nonce_coordinator_path)
{
    const auto root = std::filesystem::temp_directory_path() /
        ("cybou-recovery-device-operation-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);

    std::array<unsigned char, 32> validator_seed{};
    validator_seed[0] = 0x68;
    const auto validator = cybou::GenerateValidatorKeyPair(validator_seed);
    BOOST_REQUIRE(validator);
    const auto genesis = cybou::CreateDevGenesisState(validator->public_key);
    auto definition = cybou::CreateDevNetworkDefinition(genesis);
    definition.protocol_parameters.account_creation_work_bits = 0;

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = root / "node",
        .validator_private_key = validator_seed,
        .memory_only = false,
        .wipe_data = true,
    };
    {
        cybou::CybouNodeRuntime runtime{std::move(config)};
        BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

        cybou::CybouIdentityService original{runtime, root / "original.cybou"};
        const auto words = original.PrepareNewIdentity();
        BOOST_REQUIRE(words);
        const auto created = original.CreateIdentitySync("correct horse battery staple");
        BOOST_REQUIRE_MESSAGE(created.success, created.error_message);
        const auto account = original.GetAccountId();
        BOOST_REQUIRE(account);

        cybou::CybouIdentityService recovered{runtime, root / "recovered.cybou"};
        const auto restored = recovered.RestoreIdentitySync(*words, "another correct horse battery staple");
        BOOST_REQUIRE_MESSAGE(restored.success, restored.error_message);
        BOOST_CHECK_EQUAL(static_cast<unsigned>(restored.final_phase),
            static_cast<unsigned>(cybou::IdentityCreationPhase::ACTIVE));

        const auto loaded = runtime.GetStore().LoadState();
        BOOST_REQUIRE(loaded && loaded.state);
        const auto* record = loaded.state->identities.Find(*account);
        BOOST_REQUIRE(record);
        BOOST_CHECK_EQUAL(record->devices.size(), 2U);
        BOOST_CHECK_EQUAL(record->next_root_nonce, 1U);
        const auto recovered_device = recovered.GetKeyStore().GetDeviceId();
        BOOST_REQUIRE(recovered_device);
        BOOST_CHECK(record->devices.contains(*recovered_device));
        BOOST_CHECK(std::filesystem::exists(root / "node" / "device-operation.cydop"));

        const auto original_device = original.GetKeyStore().GetDeviceId();
        BOOST_REQUIRE(original_device);
        auto& coordinator = runtime.GetDeviceOperationCoordinator(recovered.GetKeyStore());
        const auto revoked = coordinator.RevokeDevice(*original_device);
        BOOST_REQUIRE(revoked.phase == cybou::DeviceOperationPhase::ACCEPTED);
        BOOST_REQUIRE(runtime.ProduceBlock());
        const auto revoke_status = coordinator.GetStatus(revoked.op_id);
        BOOST_REQUIRE(revoke_status.phase == cybou::DeviceOperationPhase::FINALIZED);

        const auto after_revoke = runtime.GetStore().LoadState();
        BOOST_REQUIRE(after_revoke && after_revoke.state);
        const auto* final_record = after_revoke.state->identities.Find(*account);
        BOOST_REQUIRE(final_record);
        BOOST_CHECK_EQUAL(final_record->devices.size(), 1U);
        BOOST_CHECK_EQUAL(final_record->next_root_nonce, 2U);
        BOOST_CHECK(!final_record->devices.contains(*original_device));
        BOOST_CHECK(final_record->devices.contains(*recovered_device));
    }

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    BOOST_CHECK(!ec);
}

BOOST_AUTO_TEST_CASE(concurrent_execute_reserves_only_one_device_operation)
{
    CybouServiceTestFixture fixture{0x69};
    auto identity = fixture.CreateIdentity("coordinator-concurrent.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    auto& coordinator = fixture.runtime->GetDeviceOperationCoordinator(identity->GetKeyStore());

    std::array<cybou::NameCommitPayload, 2> payloads;
    std::array<cybou::IdentityKeyId, 2> commitments{};
    for (size_t i = 0; i < payloads.size(); ++i) {
        std::array<unsigned char, 32> salt{};
        salt[0] = static_cast<unsigned char>(0x70 + i);
        const std::string label = i == 0 ? "firstcy" : "secondcy";
        payloads[i].commitment = cybou::ComputeNameCommitment(
            fixture.runtime->GetNetworkId(), *account, label, salt);
        const auto digest = cybou::ComputeNameCommitPayloadCommitment(payloads[i]);
        BOOST_REQUIRE(digest);
        commitments[i] = *digest;
    }

    std::barrier start{3};
    std::atomic<unsigned> builders_called{0};
    std::array<cybou::DeviceOperationResult, 2> results;
    std::array<std::thread, 2> callers;
    for (size_t i = 0; i < callers.size(); ++i) {
        callers[i] = std::thread{[&, i] {
            start.arrive_and_wait();
            results[i] = coordinator.Execute(cybou::DeviceOperationKind::NAME_COMMIT, commitments[i],
                [&, i](const cybou::DeviceAuthorization& authorization)
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
        return result.phase == cybou::DeviceOperationPhase::ACCEPTED;
    });
    const auto conflict_count = std::count_if(results.begin(), results.end(), [](const auto& result) {
        return result.phase == cybou::DeviceOperationPhase::CONFLICT;
    });
    BOOST_CHECK_EQUAL(accepted_count, 1U);
    BOOST_CHECK_EQUAL(conflict_count, 1U);
    BOOST_CHECK_EQUAL(builders_called.load(), 1U);
    BOOST_CHECK(!results[0].op_id.IsNull());
    BOOST_CHECK(results[0].op_id == results[1].op_id);
}

BOOST_AUTO_TEST_SUITE_END()
