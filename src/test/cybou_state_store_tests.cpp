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

const uint256 NETWORK_ID{uint256::ONE};
const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
const cybou::AccountId ACCOUNT_ID_2{uint256::FromUserHex("0b").value()};
const uint256 AUTH_KEY{uint256::FromUserHex("42").value()};
const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};

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

cybou::AccountCreateOpV1 ValidOp(const cybou::AccountId& acc = ACCOUNT_ID)
{
    return cybou::AccountCreateOpV1{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = acc,
        .initial_authorization = ValidAuth(),
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = NETWORK_ID,
            .account_id = acc,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(ValidAuth()),
            .work_epoch = 1,
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
    cybou::CybouStateStore store{db};
    BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::NOT_FOUND);
    BOOST_CHECK(!store.GetStateRoot().has_value());
    BOOST_CHECK(!store.GetFinalizedTip().has_value());

    const auto genesis{GenesisState()};
    BOOST_REQUIRE(store.InitializeGenesis(genesis));
    BOOST_CHECK(store.InitializeGenesis(genesis).error == cybou::GenesisInitError::ALREADY_INITIALIZED);

    auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == genesis);
    BOOST_REQUIRE(store.GetStateRoot().has_value());
    BOOST_CHECK(*store.GetStateRoot() == cybou::CybouStateHash(genesis));
}

BOOST_AUTO_TEST_CASE(state_store_rejects_partial_or_hash_mismatched_snapshots)
{
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db};
        const auto genesis{GenesisState()};

        // State bytes without the matching hash: corrupt.
        db.Write(STATE_KEY, cybou::SerializeCybouState(genesis));
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db};

        // Hash without state bytes: corrupt.
        db.Write(HASH_KEY, uint256::ONE);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db};
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

BOOST_AUTO_TEST_CASE(commit_finalized_block_is_tip_ordered_and_atomic)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, uint256{}, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 10, PARAMS));
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1);

    BOOST_CHECK(store.CommitFinalizedBlock(
        block2, uint256::ONE, {ValidOp(ACCOUNT_ID_2)}, NETWORK_ID, 11, PARAMS).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);
    BOOST_REQUIRE(store.CommitFinalizedBlock(
        block2, block1, {ValidOp(ACCOUNT_ID_2)}, NETWORK_ID, 11, PARAMS));

    const auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK_EQUAL(loaded.state->onboarding_pool, 8000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID_2).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID_2).creation_epoch, 1);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_replay_of_applied_block)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block1{uint256::FromUserHex("11").value()};

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, uint256{}, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 10, PARAMS));
    BOOST_CHECK(store.CommitFinalizedBlock(block1, uint256{}, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 10, PARAMS).error ==
        cybou::BlockTransitionError::BLOCK_ALREADY_APPLIED);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_invalid_operation_without_mutation)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};

    BOOST_REQUIRE(store.CommitFinalizedBlock(block1, uint256{}, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 10, PARAMS));
    const auto snapshot{store.LoadState()};
    BOOST_REQUIRE(snapshot);

    const auto result{store.CommitFinalizedBlock(
        block2, block1, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 11, PARAMS)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::INVALID_OPERATION);
    BOOST_CHECK(result.op_result.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_REQUIRE(store.LoadState());
    BOOST_CHECK(*store.LoadState().state == *snapshot.state);
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_too_many_account_creates)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const uint256 block{uint256::FromUserHex("31").value()};

    cybou::CybouProtocolParameters strict_params{PARAMS};
    strict_params.max_account_creates_per_block = 1;

    std::vector<cybou::AccountCreateOpV1> ops{
        ValidOp(ACCOUNT_ID),
        ValidOp(ACCOUNT_ID_2),
    };

    const auto result{store.CommitFinalizedBlock(block, uint256{}, ops, NETWORK_ID, 10, strict_params)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::TOO_MANY_ACCOUNT_CREATES);
    BOOST_CHECK(store.LoadState().error != cybou::StateLoadError::CORRUPT);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_requires_initialized_state)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    const uint256 block{uint256::FromUserHex("11").value()};
    BOOST_CHECK(store.CommitFinalizedBlock(block, uint256{}, {ValidOp()}, NETWORK_ID, 10, PARAMS).error ==
        cybou::BlockTransitionError::STATE_NOT_INITIALIZED);
}

BOOST_AUTO_TEST_SUITE_END()
