// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/account_creation.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>
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

BOOST_AUTO_TEST_SUITE_END()
