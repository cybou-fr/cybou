// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/state.h>

#include <boost/test/unit_test.hpp>
#include <util/strencodings.h>

#include <limits>
#include <span>

BOOST_AUTO_TEST_SUITE(cybou_state_tests)

namespace {

const uint256 NETWORK_ID{uint256::ONE};
const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
const cybou::AccountId ACCOUNT_ID_2{uint256::FromUserHex("0b").value()};
const uint256 AUTH_KEY{uint256::FromUserHex("42").value()};

cybou::AccountAuthorizationV1 ValidAuth()
{
    return cybou::AccountAuthorizationV1{
        .authorization_descriptor = AUTH_KEY,
    };
}

const cybou::CybouProtocolParameters TEST_PARAMS{
    .account_creation_work_bits = 0,
    .account_creation_epoch_lag = 1,
    .max_account_creates_per_block = 128,
    .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
};

cybou::AccountCreateOpV1 ValidOp(const cybou::AccountId& acc = ACCOUNT_ID)
{
    cybou::AccountCreateOpV1 op{
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
    return op;
}

cybou::CybouState InitialState()
{
    return cybou::CybouState{
        .onboarding_pool = 12000,
        .security_reward_pool = 1000,
        .pending_fee_pool = 500,
        .accounts{},
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(account_create_moves_bonus_and_records_account)
{
    auto state{InitialState()};
    cybou::CybouStateDelta delta;
    const auto result{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 100, 1, TEST_PARAMS, state, delta)};

    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(state.onboarding_pool, 6000);
    BOOST_REQUIRE(state.accounts.contains(ACCOUNT_ID));
    const auto& acc{state.accounts.at(ACCOUNT_ID)};
    BOOST_CHECK_EQUAL(acc.balance, 0);
    BOOST_CHECK_EQUAL(acc.system_balance, 6000);
    BOOST_CHECK_EQUAL(acc.creation_height, 100);
    BOOST_CHECK_EQUAL(acc.creation_epoch, 1);
    BOOST_CHECK(acc.initial_auth_commitment == AUTH_KEY);

    BOOST_CHECK_EQUAL(delta.created_accounts.size(), 1);
    BOOST_CHECK_EQUAL(delta.onboarding_pool_debited, 6000);
    BOOST_CHECK_EQUAL(delta.system_balance_credited, 6000);
}

BOOST_AUTO_TEST_CASE(account_create_rejects_duplicate_account_without_mutation)
{
    auto state{InitialState()};
    cybou::CybouStateDelta delta1;
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 100, 1, TEST_PARAMS, state, delta1));
    const auto snapshot{state};

    cybou::CybouStateDelta delta2;
    const auto result{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 101, 1, TEST_PARAMS, state, delta2)};
    BOOST_CHECK(result.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_CHECK(state == snapshot);
    BOOST_CHECK(delta2.created_accounts.empty());
}

BOOST_AUTO_TEST_CASE(account_create_rejects_insufficient_onboarding_pool)
{
    auto state{InitialState()};
    state.onboarding_pool = 1000;
    const auto snapshot{state};

    cybou::CybouStateDelta delta;
    const auto result{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 100, 1, TEST_PARAMS, state, delta)};
    BOOST_CHECK(result.error == cybou::AccountCreateError::INSUFFICIENT_ONBOARDING_POOL);
    BOOST_CHECK(state == snapshot);
}

BOOST_AUTO_TEST_CASE(undo_account_create_delta_restores_state_atomically)
{
    auto state{InitialState()};
    const auto initial{state};
    cybou::CybouStateDelta delta;
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 100, 1, TEST_PARAMS, state, delta));
    BOOST_CHECK(state != initial);

    cybou::UndoAccountCreateDelta(delta, state);
    BOOST_CHECK(state == initial);
}

BOOST_AUTO_TEST_CASE(state_serialization_round_trip_and_strict)
{
    BOOST_CHECK_EQUAL(cybou::CYBOU_STATE_VERSION, 1);
    auto state{InitialState()};
    cybou::CybouStateDelta delta;
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 100, 1, TEST_PARAMS, state, delta));

    const auto bytes{cybou::SerializeCybouState(state)};
    const auto decoded{cybou::DeserializeCybouState(bytes)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == state);
    BOOST_CHECK(cybou::SerializeCybouState(*decoded) == bytes);
    BOOST_CHECK(cybou::CybouStateHash(*decoded) == cybou::CybouStateHash(state));

    auto malformed{bytes};
    malformed[0] = cybou::CYBOU_STATE_VERSION + 1;
    BOOST_CHECK(!cybou::DeserializeCybouState(malformed));

    malformed = bytes;
    malformed.pop_back();
    BOOST_CHECK(!cybou::DeserializeCybouState(malformed));
}

BOOST_AUTO_TEST_CASE(cybou_state_hash_is_sensitive_to_every_field)
{
    auto state{InitialState()};
    cybou::CybouStateDelta delta;
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 100, 1, TEST_PARAMS, state, delta));
    const auto root{cybou::CybouStateHash(state)};

    auto changed{state};
    ++changed.onboarding_pool;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    ++changed.security_reward_pool;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    ++changed.pending_fee_pool;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    ++changed.accounts.at(ACCOUNT_ID).balance;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    ++changed.accounts.at(ACCOUNT_ID).system_balance;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    ++changed.accounts.at(ACCOUNT_ID).creation_height;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);
}

BOOST_AUTO_TEST_SUITE_END()
