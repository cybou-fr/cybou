// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/account_creation.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_node_runtime_tests, BasicTestingSetup)

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

cybou::CybouNetworkDefinitionV1 CreateTestNetworkDefinition(const cybou::CybouState& genesis)
{
    auto params = cybou::DevProtocolParameters();
    params.account_creation_work_bits = 0;
    return cybou::CybouNetworkDefinitionV1{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = cybou::CybouStateHash(genesis),
        .genesis_state_root = cybou::CybouStateHash(genesis),
        .protocol_parameters = params,
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(genesis.validator_set),
        .operator_authority = std::nullopt,
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(runtime_initializes_genesis_and_reports_status)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(1);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);

    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-runtime-test-1",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };

    cybou::CybouNodeRuntime runtime{std::move(config)};
    auto status = runtime.GetStatus();
    BOOST_CHECK(!status.is_initialized);
    BOOST_CHECK(status.network_id == runtime.GetNetworkId());

    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));
    status = runtime.GetStatus();
    BOOST_CHECK(status.is_initialized);
    BOOST_CHECK_EQUAL(status.finalized_height, 0);
    BOOST_CHECK_EQUAL(status.validator_count, 1);
    BOOST_CHECK(status.state_root == cybou::CybouStateHash(genesis));

    // Empty block production in authority mode
    const auto block1 = runtime.ProduceBlock();
    BOOST_REQUIRE(block1.has_value());
    BOOST_CHECK_EQUAL(block1->block.height, 1);
    BOOST_CHECK_EQUAL(*runtime.GetFinalizedHeight(), 1);
    BOOST_CHECK_EQUAL(runtime.GetStatus().finalized_height, 1);
}

BOOST_AUTO_TEST_CASE(runtime_submits_operations_and_updates_account_state)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(2);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);

    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    cybou::NodeRuntimeConfig config{
        .network_definition = definition,
        .data_dir = "cybou-runtime-test-2",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };

    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(genesis));

    // Create an account operation
    std::array<unsigned char, 32> user_priv{};
    user_priv.fill(7);
    const auto user_pub = *cybou::DeriveEd25519PublicKey(user_priv);
    const cybou::AccountId account_id{user_pub};

    const cybou::AccountAuthorizationV1 auth{.authorization_descriptor = user_pub};
    cybou::AccountCreateOpV1 create_op{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = account_id,
        .initial_authorization = auth,
        .creation_work = {
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = runtime.GetNetworkId(),
            .account_id = account_id,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(auth),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    create_op.proof_of_possession = *cybou::SignUserMessage(
        user_priv, cybou::ComputeAccountPopDigest(runtime.GetNetworkId(), account_id, user_pub));

    BOOST_REQUIRE(runtime.SubmitOperation(cybou::ProtocolOperationV1{create_op}));

    // Account does not exist in state prior to block production
    BOOST_CHECK(!runtime.GetAccountState(account_id).has_value());

    // Produce block 1
    const auto block = runtime.ProduceBlock();
    BOOST_REQUIRE(block.has_value());
    BOOST_CHECK_EQUAL(block->block.height, 1);
    BOOST_CHECK_EQUAL(block->block.operations.size(), 1);

    // Account state now exists and is credited with onboarding bonus
    const auto acc = runtime.GetAccountState(account_id);
    BOOST_REQUIRE(acc.has_value());
    BOOST_CHECK_EQUAL(acc->creation_height, 1);
    BOOST_CHECK_EQUAL(acc->system_balance, definition.protocol_parameters.onboarding_bonus);
    BOOST_CHECK_EQUAL(acc->balance, 0);
}

BOOST_AUTO_TEST_CASE(remote_operation_submission_over_tcp_network_transport)
{
    std::array<unsigned char, 32> val_key{};
    val_key.fill(2);
    const auto val_pub = *cybou::DeriveEd25519PublicKey(val_key);

    const auto genesis = CreateTestGenesis(val_pub);
    const auto definition = CreateTestNetworkDefinition(genesis);

    // Authority runtime
    cybou::NodeRuntimeConfig auth_config{
        .network_definition = definition,
        .data_dir = "cybou-node-runtime-test-auth",
        .validator_private_key = val_key,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime auth_runtime{std::move(auth_config)};
    BOOST_REQUIRE(auth_runtime.InitializeGenesis(genesis));

    // Open TCP listener on loopback port 0 (dynamic port)
    boost::asio::io_context io;
    boost::asio::ip::tcp::acceptor acceptor(io, {boost::asio::ip::make_address("127.0.0.1"), 0});
    const uint16_t port = acceptor.local_endpoint().port();

    std::atomic<bool> server_running{true};
    std::thread server_thread([&] {
        while (server_running.load()) {
            boost::asio::ip::tcp::socket socket(io);
            boost::system::error_code ec;
            acceptor.non_blocking(true);
            acceptor.accept(socket, ec);
            if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (ec) break;
            cybou::ServeCybouConnection(auth_runtime, socket);
        }
    });

    // Observer runtime configured with submit_endpoint pointing to authority
    cybou::NodeRuntimeConfig obs_config{
        .network_definition = definition,
        .data_dir = "cybou-node-runtime-test-obs",
        .submit_endpoint = std::make_pair("127.0.0.1", port),
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime obs_runtime{std::move(obs_config)};
    BOOST_REQUIRE(obs_runtime.InitializeGenesis(genesis));
    BOOST_CHECK(obs_runtime.HasSubmitEndpoint());

    // Prepare AccountCreateOp
    std::array<unsigned char, 32> user_priv{};
    user_priv.fill(9);
    const auto user_pub = *cybou::DeriveEd25519PublicKey(user_priv);
    const cybou::AccountId account_id{user_pub};
    const cybou::AccountAuthorizationV1 auth{.authorization_descriptor = user_pub};

    cybou::AccountCreateOpV1 create_op{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = account_id,
        .initial_authorization = auth,
        .creation_work = {
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = obs_runtime.GetNetworkId(),
            .account_id = account_id,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(auth),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    create_op.proof_of_possession = *cybou::SignUserMessage(
        user_priv, cybou::ComputeAccountPopDigest(obs_runtime.GetNetworkId(), account_id, user_pub));

    // Submit operation from observer desktop node over network!
    const auto submit_res = obs_runtime.SubmitOperation(cybou::ProtocolOperationV1{create_op});
    BOOST_CHECK(submit_res);
    BOOST_CHECK(submit_res.status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK(!submit_res.op_id.IsNull());

    // Authority produces block containing the submitted operation
    const auto block = auth_runtime.ProduceBlock();
    BOOST_REQUIRE(block.has_value());
    BOOST_CHECK_EQUAL(block->block.height, 1);
    BOOST_CHECK_EQUAL(block->block.operations.size(), 1);

    // Observer syncs block over network from authority
    const auto sync_res = obs_runtime.SyncFromPeer("127.0.0.1", port, 1);
    BOOST_CHECK_EQUAL(sync_res.blocks_applied, 1);
    BOOST_CHECK(sync_res.status == cybou::SyncPeerStatus::BLOCKS_APPLIED);
    BOOST_CHECK(sync_res.IsConnected());
    BOOST_CHECK_EQUAL(obs_runtime.GetFinalizedHeight().value_or(0), 1);

    // Observer syncs again when no new block exists: must return UP_TO_DATE and IsConnected() == true
    const auto sync_up_to_date = obs_runtime.SyncFromPeer("127.0.0.1", port, 1);
    BOOST_CHECK_EQUAL(sync_up_to_date.blocks_applied, 0);
    BOOST_CHECK(sync_up_to_date.status == cybou::SyncPeerStatus::UP_TO_DATE);
    BOOST_CHECK(sync_up_to_date.IsConnected());

    // Observer now has the account active in its canonical state!
    const auto acc = obs_runtime.GetAccountState(account_id);
    BOOST_REQUIRE(acc.has_value());
    BOOST_CHECK_EQUAL(acc->creation_height, 1);
    BOOST_CHECK_EQUAL(acc->system_balance, definition.protocol_parameters.onboarding_bonus);

    // Clean stop server
    server_running.store(false);
    acceptor.close();
    if (server_thread.joinable()) {
        server_thread.join();
    }
}

BOOST_AUTO_TEST_SUITE_END()
