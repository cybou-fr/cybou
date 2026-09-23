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
    .epoch_blocks = 10,
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

BOOST_AUTO_TEST_CASE(epoch_is_derived_from_height_only)
{
    // Canonical derivation: epoch = height / epoch_blocks, computed in one place.
    BOOST_CHECK_EQUAL(cybou::EpochForHeight(0, TEST_PARAMS), 0);
    BOOST_CHECK_EQUAL(cybou::EpochForHeight(9, TEST_PARAMS), 0);
    BOOST_CHECK_EQUAL(cybou::EpochForHeight(10, TEST_PARAMS), 1);
    BOOST_CHECK_EQUAL(cybou::EpochForHeight(100, TEST_PARAMS), 10);

    const cybou::CybouProtocolParameters default_params{};
    BOOST_CHECK_EQUAL(cybou::EpochForHeight(1023, default_params), 0);
    BOOST_CHECK_EQUAL(cybou::EpochForHeight(1024, default_params), 1);
}

BOOST_AUTO_TEST_CASE(account_create_moves_bonus_and_records_account)
{
    auto state{InitialState()};
    const auto result{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 10, TEST_PARAMS, state)};

    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(state.onboarding_pool, 6000);
    BOOST_REQUIRE(state.accounts.contains(ACCOUNT_ID));
    const auto& acc{state.accounts.at(ACCOUNT_ID)};
    BOOST_CHECK_EQUAL(acc.balance, 0);
    BOOST_CHECK_EQUAL(acc.system_balance, 6000);
    BOOST_CHECK_EQUAL(acc.creation_height, 10);
    BOOST_CHECK_EQUAL(acc.creation_epoch, 1);
    BOOST_CHECK(acc.initial_auth_commitment == AUTH_KEY);
}

BOOST_AUTO_TEST_CASE(account_create_rejects_duplicate_account_without_mutation)
{
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 10, TEST_PARAMS, state));
    const auto snapshot{state};

    const auto result{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 11, TEST_PARAMS, state)};
    BOOST_CHECK(result.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_CHECK(state == snapshot);
}

BOOST_AUTO_TEST_CASE(account_create_rejects_insufficient_onboarding_pool)
{
    auto state{InitialState()};
    state.onboarding_pool = 1000;
    const auto snapshot{state};

    const auto result{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 10, TEST_PARAMS, state)};
    BOOST_CHECK(result.error == cybou::AccountCreateError::INSUFFICIENT_ONBOARDING_POOL);
    BOOST_CHECK(state == snapshot);
}

BOOST_AUTO_TEST_CASE(account_create_rejects_future_or_expired_work_epoch)
{
    auto state{InitialState()};

    // work_epoch = 1; epoch_blocks = 10 → epoch 0 at height 9, epoch 1 at
    // height 10, epoch 2 at height 20 (lag 1 still valid), epoch 3 at height
    // 30 (expired with lag 1).
    const auto future{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 9, TEST_PARAMS, state)};
    BOOST_CHECK(future.error == cybou::AccountCreateError::INVALID_OP);
    BOOST_CHECK(future.validation_error == cybou::AccountCreateValidationError::FUTURE_WORK_EPOCH);

    const auto expired{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 30, TEST_PARAMS, state)};
    BOOST_CHECK(expired.error == cybou::AccountCreateError::INVALID_OP);
    BOOST_CHECK(expired.validation_error == cybou::AccountCreateValidationError::EXPIRED_WORK_EPOCH);

    const auto within_lag{cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 20, TEST_PARAMS, state)};
    BOOST_CHECK(within_lag);
}

BOOST_AUTO_TEST_CASE(state_serialization_round_trip_and_strict)
{
    BOOST_CHECK_EQUAL(cybou::CYBOU_STATE_VERSION, 1);
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 10, TEST_PARAMS, state));

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
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(), NETWORK_ID, 10, TEST_PARAMS, state));
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
