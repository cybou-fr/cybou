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

cybou::CybouState InitialState()
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

BOOST_AUTO_TEST_CASE(state_store_round_trips_and_overwrites_snapshot)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::NOT_FOUND);

    auto state{InitialState()};
    store.Write(state);
    auto loaded{store.Load()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == state);

    state.onboarding_pool = 14000;
    store.Write(state);
    loaded = store.Load();
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == state);
}

BOOST_AUTO_TEST_CASE(state_store_rejects_partial_or_hash_mismatched_snapshots)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    const auto state{InitialState()};

    db.Write(STATE_KEY, cybou::SerializeCybouState(state));
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::CORRUPT);

    db.Write(HASH_KEY, uint256::ONE);
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::CORRUPT);

    store.Write(state);
    BOOST_REQUIRE(store.Load());
    auto corrupt_bytes{cybou::SerializeCybouState(state)};
    corrupt_bytes.back() ^= 1;
    db.Write(STATE_KEY, corrupt_bytes);
    BOOST_CHECK(store.Load().error == cybou::StateLoadError::CORRUPT);
}

BOOST_AUTO_TEST_CASE(create_account_and_write_keeps_memory_and_database_in_sync)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    auto state{InitialState()};

    const auto result{store.CreateAccountAndWrite(ValidOp(), NETWORK_ID, 1, 1, PARAMS, state)};
    BOOST_REQUIRE(result);
    const auto loaded{store.Load()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == state);

    const auto snapshot{state};
    const auto replay{store.CreateAccountAndWrite(ValidOp(), NETWORK_ID, 2, 1, PARAMS, state)};
    BOOST_CHECK(replay.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_CHECK(state == snapshot);
    BOOST_REQUIRE(store.Load());
    BOOST_CHECK(*store.Load().state == snapshot);
}

BOOST_AUTO_TEST_CASE(apply_block_and_rollback_are_tip_ordered_and_atomic)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    auto state{InitialState()};
    store.Write(state);
    const auto initial{state};
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};

    BOOST_REQUIRE(store.ApplyBlock(block1, uint256{}, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 1, 1, PARAMS, state));
    const auto after_block1{state};
    BOOST_CHECK(store.ApplyBlock(
        block2, uint256::ONE, {ValidOp(ACCOUNT_ID_2)}, NETWORK_ID, 2, 1, PARAMS, state).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);
    BOOST_REQUIRE(store.ApplyBlock(
        block2, block1, {ValidOp(ACCOUNT_ID_2)}, NETWORK_ID, 2, 1, PARAMS, state));
    BOOST_CHECK_EQUAL(state.onboarding_pool, 8000);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).system_balance, 6000);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID_2).system_balance, 6000);

    BOOST_CHECK(store.RollbackBlock(block1, state).error == cybou::BlockTransitionError::NOT_CURRENT_TIP);
    BOOST_REQUIRE(store.RollbackBlock(block2, state));
    BOOST_CHECK(state == after_block1);
    BOOST_REQUIRE(store.RollbackBlock(block1, state));
    BOOST_CHECK(state == initial);
    BOOST_REQUIRE(store.Load());
    BOOST_CHECK(*store.Load().state == initial);
}

BOOST_AUTO_TEST_CASE(apply_block_rejects_duplicate_account)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    auto state{InitialState()};
    store.Write(state);
    const uint256 block1{uint256::FromUserHex("11").value()};
    const uint256 block2{uint256::FromUserHex("12").value()};

    BOOST_REQUIRE(store.ApplyBlock(block1, uint256{}, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 1, 1, PARAMS, state));
    const auto snapshot{state};

    const auto result{store.ApplyBlock(
        block2, block1, {ValidOp(ACCOUNT_ID)}, NETWORK_ID, 2, 1, PARAMS, state)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::INVALID_OPERATION);
    BOOST_CHECK(result.op_result.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_CHECK(state == snapshot);
    BOOST_REQUIRE(store.Load());
    BOOST_CHECK(*store.Load().state == snapshot);
}

BOOST_AUTO_TEST_CASE(apply_block_rejects_too_many_account_creates)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    auto state{InitialState()};
    store.Write(state);
    const uint256 block{uint256::FromUserHex("31").value()};

    cybou::CybouProtocolParameters strict_params{PARAMS};
    strict_params.max_account_creates_per_block = 1;

    std::vector<cybou::AccountCreateOpV1> ops{
        ValidOp(ACCOUNT_ID),
        ValidOp(ACCOUNT_ID_2),
    };

    const auto result{store.ApplyBlock(block, uint256{}, ops, NETWORK_ID, 1, 1, strict_params, state)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::TOO_MANY_ACCOUNT_CREATES);
}

BOOST_AUTO_TEST_CASE(apply_block_rejects_state_mismatch_and_corrupt_undo)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db};
    auto state{InitialState()};
    store.Write(state);
    const uint256 block{uint256::FromUserHex("21").value()};

    auto stale{state};
    --stale.onboarding_pool;
    BOOST_CHECK(store.ApplyBlock(block, uint256{}, {ValidOp()}, NETWORK_ID, 1, 1, PARAMS, stale).error ==
        cybou::BlockTransitionError::STATE_MISMATCH);
    BOOST_REQUIRE(store.ApplyBlock(block, uint256{}, {ValidOp()}, NETWORK_ID, 1, 1, PARAMS, state));

    const std::string undo_key{"cybou/undo/v1/" + block.GetHex()};
    db.Write(undo_key, std::vector<unsigned char>{1, 0});
    const auto snapshot{state};
    BOOST_CHECK(store.RollbackBlock(block, state).error ==
        cybou::BlockTransitionError::MISSING_OR_CORRUPT_UNDO);
    BOOST_CHECK(state == snapshot);
}

BOOST_AUTO_TEST_SUITE_END()
