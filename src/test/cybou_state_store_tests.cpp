// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state_store.h>

#include <dbwrapper.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <span>

BOOST_FIXTURE_TEST_SUITE(cybou_state_store_tests, BasicTestingSetup)

namespace {

const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
const cybou::AccountId ACCOUNT_ID_2{uint256::FromUserHex("0b").value()};
const uint256 AUTH_KEY{uint256::FromUserHex("42").value()};
const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};
const std::string HEAD_KEY{"cybou/head/v1"};

cybou::AccountAuthorizationV1 ValidAuth()
{
    return cybou::AccountAuthorizationV1{
        .authorization_descriptor = AUTH_KEY,
    };
}

const cybou::CybouProtocolParameters PARAMS{
    .account_creation_work_bits = 0,
    .account_creation_epoch_lag = 1,
    .max_account_creates_per_block = 128,
    .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
    .epoch_blocks = 10,
};

cybou::CybouNetworkDefinitionV1 TestNetworkDefinition();

cybou::AccountCreateOpV1 ValidOp(const cybou::AccountId& acc = ACCOUNT_ID)
{
    return cybou::AccountCreateOpV1{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = acc,
        .initial_authorization = ValidAuth(),
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = cybou::NetworkId(TestNetworkDefinition()),
            .account_id = acc,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(ValidAuth()),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
}

cybou::CybouState GenesisState()
{
    return cybou::CybouState{
        .onboarding_pool = 20000,
        .security_reward_pool = 1000,
        .pending_fee_pool = 500,
        .accounts{},
    };
}

cybou::CybouNetworkDefinitionV1 TestNetworkDefinition()
{
    return cybou::CybouNetworkDefinitionV1{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = uint256::ONE,
        .genesis_state_root = cybou::CybouStateHash(GenesisState()),
        .protocol_parameters = PARAMS,
        .initial_validator_set_commitment = uint256::FromUserHex("99").value(),
    };
}

CDBWrapper MemoryDb()
{
    return CDBWrapper{{
        .path = "cybou-state-store-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
}

} // namespace

BOOST_AUTO_TEST_CASE(genesis_initializes_once_and_loads)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::NOT_FOUND);
    BOOST_CHECK(!store.GetStateRoot().has_value());
    BOOST_CHECK(!store.GetFinalizedHead().has_value());
    BOOST_CHECK(!store.GetFinalizedTip().has_value());
    BOOST_CHECK(!store.GetFinalizedHeight().has_value());
    BOOST_CHECK(!store.GetStoredNetworkId().has_value());

    const auto genesis{GenesisState()};
    BOOST_REQUIRE(store.InitializeGenesis(genesis));
    BOOST_CHECK(store.InitializeGenesis(genesis).error == cybou::GenesisInitError::ALREADY_INITIALIZED);

    auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == genesis);
    BOOST_REQUIRE(store.GetStateRoot().has_value());
    BOOST_CHECK(*store.GetStateRoot() == cybou::CybouStateHash(genesis));
    BOOST_REQUIRE(store.GetFinalizedHead().has_value());
    BOOST_CHECK(store.GetFinalizedHead()->block_id == definition.genesis_block_id);
    BOOST_CHECK_EQUAL(store.GetFinalizedHead()->height, 0);
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == definition.genesis_block_id);
    BOOST_REQUIRE(store.GetFinalizedHeight().has_value());
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 0);
    BOOST_REQUIRE(store.GetStoredNetworkId().has_value());
    BOOST_CHECK(*store.GetStoredNetworkId() == store.GetNetworkId());
}

BOOST_AUTO_TEST_CASE(network_definition_binds_identity_and_genesis_state)
{
    const auto definition{TestNetworkDefinition()};
    BOOST_CHECK(cybou::ValidateNetworkDefinition(definition) == cybou::NetworkDefinitionError::NONE);
    auto changed_params{definition};
    ++changed_params.protocol_parameters.onboarding_bonus;
    auto changed_validators{definition};
    changed_validators.initial_validator_set_commitment = uint256::FromUserHex("98").value();

    BOOST_CHECK(cybou::NetworkId(definition) != cybou::NetworkId(changed_params));
    BOOST_CHECK(cybou::NetworkId(definition) != cybou::NetworkId(changed_validators));

    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, definition};
    auto wrong_genesis{GenesisState()};
    ++wrong_genesis.onboarding_pool;
    BOOST_CHECK(store.InitializeGenesis(wrong_genesis).error == cybou::GenesisInitError::GENESIS_STATE_MISMATCH);
    BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::NOT_FOUND);
}

BOOST_AUTO_TEST_CASE(network_definition_rejects_invalid_consensus_constants)
{
    const auto valid{TestNetworkDefinition()};
    const auto check_error = [&valid](auto mutate, const cybou::NetworkDefinitionError expected) {
        auto definition{valid};
        mutate(definition);
        BOOST_CHECK(cybou::ValidateNetworkDefinition(definition) == expected);
    };

    check_error([](auto& definition) { ++definition.protocol_version; },
        cybou::NetworkDefinitionError::UNSUPPORTED_VERSION);
    check_error([](auto& definition) { definition.genesis_block_id.SetNull(); },
        cybou::NetworkDefinitionError::NULL_GENESIS_BLOCK_ID);
    check_error([](auto& definition) { definition.genesis_state_root.SetNull(); },
        cybou::NetworkDefinitionError::NULL_GENESIS_STATE_ROOT);
    check_error([](auto& definition) { definition.initial_validator_set_commitment.SetNull(); },
        cybou::NetworkDefinitionError::NULL_VALIDATOR_SET_COMMITMENT);
    check_error([](auto& definition) { definition.protocol_parameters.account_creation_work_bits = 257; },
        cybou::NetworkDefinitionError::INVALID_ACCOUNT_CREATION_WORK_BITS);
    check_error([](auto& definition) { definition.protocol_parameters.max_account_creates_per_block = 0; },
        cybou::NetworkDefinitionError::ZERO_MAX_ACCOUNT_CREATES_PER_BLOCK);
    check_error([](auto& definition) { definition.protocol_parameters.epoch_blocks = 0; },
        cybou::NetworkDefinitionError::ZERO_EPOCH_BLOCKS);

    auto invalid{valid};
    invalid.protocol_parameters.epoch_blocks = 0;
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, invalid};
    BOOST_CHECK(store.InitializeGenesis(GenesisState()).error ==
        cybou::GenesisInitError::INVALID_NETWORK_DEFINITION);
    BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::INVALID_NETWORK_DEFINITION);
    const uint256 block{uint256::FromUserHex("72").value()};
    BOOST_CHECK(store.CommitFinalizedBlock(block, uint256{}, {}).error ==
        cybou::BlockTransitionError::INVALID_NETWORK_DEFINITION);
    BOOST_CHECK(!store.GetStoredNetworkId().has_value());
}

BOOST_AUTO_TEST_CASE(state_store_rejects_partial_or_hash_mismatched_snapshots)
{
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};
        const auto genesis{GenesisState()};

        // State bytes without the matching hash: corrupt.
        db.Write(STATE_KEY, cybou::SerializeCybouState(genesis));
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};

        // Hash without state bytes: corrupt.
        db.Write(HASH_KEY, uint256::ONE);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};
        const auto genesis{GenesisState()};

        // Missing head: corrupt.
        BOOST_REQUIRE(store.InitializeGenesis(genesis));
        db.Erase(HEAD_KEY);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};
        const auto genesis{GenesisState()};

        // Bit-flip in the stored state breaks the hash check.
        BOOST_REQUIRE(store.InitializeGenesis(genesis));
        BOOST_REQUIRE(store.LoadState());
        auto corrupt_bytes{cybou::SerializeCybouState(genesis)};
        corrupt_bytes.back() ^= 1;
        db.Write(STATE_KEY, corrupt_bytes);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
}

BOOST_AUTO_TEST_CASE(state_store_rejects_reopen_with_different_network_definition)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore original{db, definition};
    BOOST_REQUIRE(original.InitializeGenesis(GenesisState()));

    cybou::CybouStateStore compatible{db, definition};
    BOOST_REQUIRE(compatible.LoadState());
    BOOST_CHECK(*compatible.GetStoredNetworkId() == compatible.GetNetworkId());

    auto incompatible_definition{definition};
    ++incompatible_definition.protocol_parameters.onboarding_bonus;
    cybou::CybouStateStore incompatible{db, incompatible_definition};
    BOOST_CHECK(incompatible.LoadState().error == cybou::StateLoadError::NETWORK_MISMATCH);

    const uint256 block{uint256::FromUserHex("71").value()};
    BOOST_CHECK(incompatible.CommitFinalizedBlock(block, uint256{}, {}).error ==
        cybou::BlockTransitionError::NETWORK_MISMATCH);
    BOOST_REQUIRE(original.GetFinalizedTip().has_value());
    BOOST_CHECK(*original.GetFinalizedTip() == definition.genesis_block_id);
    BOOST_CHECK_EQUAL(*original.GetFinalizedHeight(), 0);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_is_tip_ordered_and_atomic)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};
    const auto genesis_id{definition.genesis_block_id};

    BOOST_CHECK(store.CommitFinalizedBlock(
        block1, uint256{}, {ValidOp(ACCOUNT_ID)}).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);
    BOOST_REQUIRE(store.CommitFinalizedBlock(
        block1, genesis_id, {ValidOp(ACCOUNT_ID)}));
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);

    BOOST_CHECK(store.CommitFinalizedBlock(
        block2, genesis_id, {ValidOp(ACCOUNT_ID_2)}).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);
    BOOST_REQUIRE(store.CommitFinalizedBlock(
        block2, block1, {ValidOp(ACCOUNT_ID_2)}));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);

    const auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK_EQUAL(loaded.state->onboarding_pool, 8000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID_2).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID_2).creation_epoch, 0);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_replay_of_applied_block)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block1{uint256::FromUserHex("11").value()};
    const auto genesis_id{definition.genesis_block_id};

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, genesis_id, {ValidOp(ACCOUNT_ID)}));
    BOOST_CHECK(store.CommitFinalizedBlock(block1, genesis_id, {ValidOp(ACCOUNT_ID)}).error ==
        cybou::BlockTransitionError::BLOCK_ALREADY_APPLIED);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_invalid_operation_without_mutation)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};
    const auto genesis_id{definition.genesis_block_id};

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, genesis_id, {ValidOp(ACCOUNT_ID)}));
    const auto snapshot{store.LoadState()};
    BOOST_REQUIRE(snapshot);

    const auto result{store.CommitFinalizedBlock(
        block2, block1, {ValidOp(ACCOUNT_ID)})};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::INVALID_OPERATION);
    BOOST_CHECK(result.op_result.account_create_result.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_REQUIRE(store.LoadState());
    BOOST_CHECK(*store.LoadState().state == *snapshot.state);
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_too_many_account_creates)
{
    auto db{MemoryDb()};
    cybou::CybouProtocolParameters strict_params{PARAMS};
    strict_params.max_account_creates_per_block = 1;
    auto strict_network{TestNetworkDefinition()};
    strict_network.protocol_parameters = strict_params;
    cybou::CybouStateStore store{db, strict_network};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block{uint256::FromUserHex("31").value()};
    const auto genesis_id{strict_network.genesis_block_id};

    std::vector<cybou::ProtocolOperationV1> ops{
        ValidOp(ACCOUNT_ID),
        ValidOp(ACCOUNT_ID_2),
    };

    const auto result{store.CommitFinalizedBlock(block, genesis_id, ops)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::TOO_MANY_ACCOUNT_CREATES);
    BOOST_CHECK(store.LoadState().error != cybou::StateLoadError::CORRUPT);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_requires_initialized_state)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, TestNetworkDefinition()};
    const uint256 block{uint256::FromUserHex("11").value()};
    BOOST_CHECK(store.CommitFinalizedBlock(block, uint256{}, {ValidOp()}).error ==
        cybou::BlockTransitionError::STATE_NOT_INITIALIZED);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_advances_height_monotonically)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const auto genesis_id{definition.genesis_block_id};
    const uint256 block1{uint256::FromUserHex("41").value()};
    const uint256 block2{uint256::FromUserHex("42").value()};

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, genesis_id, {ValidOp(ACCOUNT_ID)}));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);
    BOOST_CHECK(*store.GetFinalizedTip() == block1);

    BOOST_REQUIRE(store.CommitFinalizedBlock(block2, block1, {ValidOp(ACCOUNT_ID_2)}));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);
    BOOST_CHECK(*store.GetFinalizedTip() == block2);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_height_overflow)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, TestNetworkDefinition()};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const cybou::FinalizedHeadV1 max_head{
        .block_id = uint256::FromUserHex("51").value(),
        .height = std::numeric_limits<uint64_t>::max(),
    };
    db.Write(HEAD_KEY, max_head);
    const uint256 block{uint256::FromUserHex("52").value()};
    BOOST_CHECK(store.CommitFinalizedBlock(block, max_head.block_id, {}).error ==
        cybou::BlockTransitionError::INVALID_HEIGHT);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_executes_authorized_payment)
{
    std::array<unsigned char, 32> priv1{};
    priv1.fill(0x55);
    const auto pub1{cybou::DeriveEd25519PublicKey(priv1)};
    BOOST_REQUIRE(pub1.has_value());

    std::array<unsigned char, 32> priv2{};
    priv2.fill(0x66);
    const auto pub2{cybou::DeriveEd25519PublicKey(priv2)};
    BOOST_REQUIRE(pub2.has_value());

    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    const auto net_id{cybou::NetworkId(definition)};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    const auto genesis_id{definition.genesis_block_id};
    const uint256 block1{uint256::FromUserHex("61").value()};
    const uint256 block2{uint256::FromUserHex("62").value()};

    // Block 1: Onboard Account 1 and Account 2
    auto op1{ValidOp(ACCOUNT_ID)};
    op1.initial_authorization = cybou::AccountAuthorizationV1{.authorization_descriptor = *pub1};
    op1.creation_work.initial_authorization_commitment = cybou::ComputeAuthCommitment(op1.initial_authorization);

    auto op2{ValidOp(ACCOUNT_ID_2)};
    op2.initial_authorization = cybou::AccountAuthorizationV1{.authorization_descriptor = *pub2};
    op2.creation_work.initial_authorization_commitment = cybou::ComputeAuthCommitment(op2.initial_authorization);

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, genesis_id, {op1, op2}));

    // In state, fund Account 1 balance
    auto current_state{*store.LoadState().state};
    current_state.accounts.at(ACCOUNT_ID).balance = 5000;
    // Update store state for test
    db.Write(STATE_KEY, cybou::SerializeCybouState(current_state));
    db.Write(HASH_KEY, cybou::CybouStateHash(current_state));

    // Block 2: Authorized payment from Account 1 -> Account 2
    const cybou::PaymentOpV1 payment{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = ACCOUNT_ID_2,
        .amount = 1500,
        .fee = 100,
    };
    const uint256 digest{cybou::ComputeUserOperationDigest(net_id, ACCOUNT_ID, 0, payment)};
    const auto sig{cybou::SignUserMessage(priv1, std::span<const unsigned char>{digest.begin(), digest.size()})};
    BOOST_REQUIRE(sig.has_value());

    const cybou::AuthorizedOperationV1 auth_payment{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = ACCOUNT_ID,
        .nonce = 0,
        .payload = payment,
        .signature = *sig,
    };

    BOOST_REQUIRE(store.CommitFinalizedBlock(block2, block1, {auth_payment}));

    const auto final_state{store.LoadState()};
    BOOST_REQUIRE(final_state);
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID).balance, 3400); // 5000 - 1500 - 100
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID).next_nonce, 1);
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID_2).balance, 1500);
    BOOST_CHECK_EQUAL(final_state.state->pending_fee_pool, 600); // initial 500 + 100
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);
    BOOST_CHECK(*store.GetFinalizedTip() == block2);
}

BOOST_AUTO_TEST_SUITE_END()
