// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>
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

const std::array<unsigned char, 32> AUTH_PRIVKEY = []{
    std::array<unsigned char, 32> k{};
    k[0] = 0x42;
    return k;
}();
const uint256 AUTH_KEY = *cybou::DeriveEd25519PublicKey(AUTH_PRIVKEY);

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
    const uint256 pop_digest = cybou::ComputeAccountPopDigest(
        NETWORK_ID, op.account_id, op.initial_authorization.authorization_descriptor);
    op.proof_of_possession = *cybou::SignUserMessage(AUTH_PRIVKEY, pop_digest);
    return op;
}

cybou::CybouState InitialState()
{
    return cybou::CybouState{
        .onboarding_pool = 12000,
        .security_reward_pool = 1000,
        .pending_fee_pool = 500,
        .accounts{},
        .validator_set{},
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
    BOOST_CHECK(acc.active_authorization_key == AUTH_KEY);
    BOOST_CHECK_EQUAL(acc.next_nonce, 0);
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

    changed = state;
    ++changed.accounts.at(ACCOUNT_ID).active_authorization_key.begin()[0];
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    ++changed.accounts.at(ACCOUNT_ID).next_nonce;
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);

    changed = state;
    changed.validator_set.validators.push_back(cybou::ValidatorV1{
        .validator_id = uint256::ONE,
        .consensus_public_key = uint256::ONE,
        .weight = 1,
    });
    BOOST_CHECK(cybou::CybouStateHash(changed) != root);
}

BOOST_AUTO_TEST_CASE(payment_transfers_balance_and_accrues_fee)
{
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID), NETWORK_ID, 10, TEST_PARAMS, state));
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID_2), NETWORK_ID, 10, TEST_PARAMS, state));

    // Fund sender balance
    state.accounts.at(ACCOUNT_ID).balance = 1000;
    const uint64_t initial_fee_pool{state.pending_fee_pool};
    const uint64_t initial_system_balance{state.accounts.at(ACCOUNT_ID).system_balance};

    const auto res{cybou::ApplyPayment(ACCOUNT_ID, ACCOUNT_ID_2, 400, 50, state)};
    BOOST_REQUIRE(res);

    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).balance, 600); // 1000 - 400
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).system_balance, initial_system_balance - 50); // fee debited from system_balance
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).next_nonce, 1);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID_2).balance, 400);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID_2).next_nonce, 0); // recipient nonce unmodified
    BOOST_CHECK_EQUAL(state.pending_fee_pool, initial_fee_pool + 50);
}

BOOST_AUTO_TEST_CASE(payment_validates_invariants)
{
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID), NETWORK_ID, 10, TEST_PARAMS, state));
    state.accounts.at(ACCOUNT_ID).balance = 100;
    const auto snapshot{state};

    // Zero amount
    BOOST_CHECK(cybou::ApplyPayment(ACCOUNT_ID, ACCOUNT_ID_2, 0, 10, state).error ==
        cybou::PaymentError::ZERO_AMOUNT);
    BOOST_CHECK(state == snapshot);

    // Self payment
    BOOST_CHECK(cybou::ApplyPayment(ACCOUNT_ID, ACCOUNT_ID, 50, 10, state).error ==
        cybou::PaymentError::SELF_PAYMENT);
    BOOST_CHECK(state == snapshot);

    // Recipient not found
    BOOST_CHECK(cybou::ApplyPayment(ACCOUNT_ID, ACCOUNT_ID_2, 50, 10, state).error ==
        cybou::PaymentError::RECIPIENT_NOT_FOUND);
    BOOST_CHECK(state == snapshot);

    // Sender not found
    BOOST_CHECK(cybou::ApplyPayment(ACCOUNT_ID_2, ACCOUNT_ID, 50, 10, state).error ==
        cybou::PaymentError::SENDER_NOT_FOUND);
    BOOST_CHECK(state == snapshot);

    // Insufficient balance (balance is 100, amount is 101)
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID_2), NETWORK_ID, 10, TEST_PARAMS, state));
    BOOST_CHECK(cybou::ApplyPayment(ACCOUNT_ID, ACCOUNT_ID_2, 101, 1, state).error ==
        cybou::PaymentError::INSUFFICIENT_BALANCE);

    // Insufficient system balance for fee
    state.accounts.at(ACCOUNT_ID).system_balance = 0;
    BOOST_CHECK(cybou::ApplyPayment(ACCOUNT_ID, ACCOUNT_ID_2, 50, 10, state).error ==
        cybou::PaymentError::INSUFFICIENT_SYSTEM_BALANCE);
}

BOOST_AUTO_TEST_CASE(key_update_rotates_authorization_key)
{
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID), NETWORK_ID, 10, TEST_PARAMS, state));

    const uint256 new_key{uint256::FromUserHex("9988").value()};
    const auto res{cybou::ApplyKeyUpdate(ACCOUNT_ID, new_key, state)};
    BOOST_REQUIRE(res);

    BOOST_CHECK(state.accounts.at(ACCOUNT_ID).active_authorization_key == new_key);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).next_nonce, 1);

    // Null key rejected
    BOOST_CHECK(cybou::ApplyKeyUpdate(ACCOUNT_ID, uint256{}, state).error ==
        cybou::KeyUpdateError::NULL_KEY);

    // Unknown account
    BOOST_CHECK(cybou::ApplyKeyUpdate(ACCOUNT_ID_2, new_key, state).error ==
        cybou::KeyUpdateError::ACCOUNT_NOT_FOUND);
}

BOOST_AUTO_TEST_CASE(system_lock_transfers_balance_to_system_balance)
{
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID), NETWORK_ID, 10, TEST_PARAMS, state));

    state.accounts.at(ACCOUNT_ID).balance = 1000;
    const uint64_t initial_system_balance{state.accounts.at(ACCOUNT_ID).system_balance};

    // Lock 400 CYBOU
    const auto res{cybou::ApplySystemLock(ACCOUNT_ID, 400, state)};
    BOOST_REQUIRE(res);

    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).balance, 600);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).system_balance, initial_system_balance + 400);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).next_nonce, 1);

    // Invariants
    BOOST_CHECK(cybou::ApplySystemLock(ACCOUNT_ID, 0, state).error ==
        cybou::SystemLockError::ZERO_AMOUNT);
    BOOST_CHECK(cybou::ApplySystemLock(ACCOUNT_ID, 1000, state).error ==
        cybou::SystemLockError::INSUFFICIENT_BALANCE);
    BOOST_CHECK(cybou::ApplySystemLock(ACCOUNT_ID_2, 100, state).error ==
        cybou::SystemLockError::ACCOUNT_NOT_FOUND);
}

BOOST_AUTO_TEST_CASE(mail_operation_deducts_fee_and_keeps_state_bounded)
{
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID), NETWORK_ID, 10, TEST_PARAMS, state));
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID_2), NETWORK_ID, 10, TEST_PARAMS, state));

    const size_t initial_account_count{state.accounts.size()};
    const uint64_t initial_fee_pool{state.pending_fee_pool};

    // Case 1: Fee fully covered by system_balance
    state.accounts.at(ACCOUNT_ID).system_balance = 100;
    state.accounts.at(ACCOUNT_ID).balance = 50;

    const auto res1{cybou::ApplyMail(ACCOUNT_ID, ACCOUNT_ID_2, 10, 10, TEST_PARAMS, state)};
    BOOST_REQUIRE(res1);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).system_balance, 90);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).balance, 50); // user balance untouched
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).next_nonce, 1);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, initial_fee_pool + 10);

    // Bounded state invariant: no per-mail consensus object created
    BOOST_CHECK_EQUAL(state.accounts.size(), initial_account_count);

    // Case 2: Fee exceeds system_balance, rejected with INSUFFICIENT_SYSTEM_BALANCE
    state.accounts.at(ACCOUNT_ID).system_balance = 4;
    state.accounts.at(ACCOUNT_ID).balance = 20;

    const auto res2{cybou::ApplyMail(ACCOUNT_ID, ACCOUNT_ID_2, 10, 10, TEST_PARAMS, state)};
    BOOST_CHECK(res2.error == cybou::MailError::INSUFFICIENT_SYSTEM_BALANCE);
    // User balance untouched, system_balance untouched
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).system_balance, 4);
    BOOST_CHECK_EQUAL(state.accounts.at(ACCOUNT_ID).balance, 20);

    // Case 3: Invariant violations
    BOOST_CHECK(cybou::ApplyMail(ACCOUNT_ID, ACCOUNT_ID, 10, 10, TEST_PARAMS, state).error ==
        cybou::MailError::SELF_MAIL);
    BOOST_CHECK(cybou::ApplyMail(ACCOUNT_ID, cybou::AccountId{uint256::FromUserHex("ff").value()}, 10, 10, TEST_PARAMS, state).error ==
        cybou::MailError::RECIPIENT_NOT_FOUND);
    BOOST_CHECK(cybou::ApplyMail(cybou::AccountId{uint256::FromUserHex("ff").value()}, ACCOUNT_ID_2, 10, 10, TEST_PARAMS, state).error ==
        cybou::MailError::SENDER_NOT_FOUND);
}

BOOST_AUTO_TEST_CASE(fee_routing_settles_four_cybou_chunks_without_loss)
{
    auto state{InitialState()};
    state.security_reward_pool = 1000;
    state.onboarding_pool = 5000;
    state.pending_fee_pool = 15; // 3 chunks of 4 + 3 remainder

    cybou::RoutePendingFees(state);

    // 3 * 3 = 9 to security, 3 * 1 = 3 to onboarding, 3 remainder
    BOOST_CHECK_EQUAL(state.security_reward_pool, 1009);
    BOOST_CHECK_EQUAL(state.onboarding_pool, 5003);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, 3);

    // Re-routing with remainder < 4 produces no changes
    cybou::RoutePendingFees(state);
    BOOST_CHECK_EQUAL(state.security_reward_pool, 1009);
    BOOST_CHECK_EQUAL(state.onboarding_pool, 5003);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, 3);

    // Adding 1 CYBOU forms another chunk (3 + 1 = 4)
    state.pending_fee_pool += 1;
    cybou::RoutePendingFees(state);
    BOOST_CHECK_EQUAL(state.security_reward_pool, 1012);
    BOOST_CHECK_EQUAL(state.onboarding_pool, 5004);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, 0);
}

BOOST_AUTO_TEST_CASE(pot_score_calculation_and_mail_rate_limit_enforcement)
{
    // 1. Integer PoT score calculation checks
    cybou::AccountState acc{
        .balance = 0,
        .system_balance = 0,
        .creation_height = 0,
        .creation_epoch = 0,
        .active_authorization_key = AUTH_KEY,
        .next_nonce = 0,
        .last_mail_epoch = 0,
        .mail_count_in_epoch = 0,
    };

    // New account at epoch 0 with 0 system balance: base score = 100
    BOOST_CHECK_EQUAL(cybou::ComputeProofOfTrustScore(acc, 0), 100);
    BOOST_CHECK_EQUAL(cybou::CalculateMailRateLimit(100, TEST_PARAMS), 25);

    // System balance contribution capped at 50 (1 per 100 CYBOU)
    acc.system_balance = 3000;
    BOOST_CHECK_EQUAL(cybou::ComputeProofOfTrustScore(acc, 0), 130); // 100 + 30
    BOOST_CHECK_EQUAL(cybou::CalculateMailRateLimit(130, TEST_PARAMS), 25);

    acc.system_balance = 5000;
    BOOST_CHECK_EQUAL(cybou::ComputeProofOfTrustScore(acc, 0), 150); // 100 + 50 (cap reached)
    BOOST_CHECK_EQUAL(cybou::CalculateMailRateLimit(150, TEST_PARAMS), 50);

    acc.system_balance = 100000; // Large balance cannot exceed 50 cap
    BOOST_CHECK_EQUAL(cybou::ComputeProofOfTrustScore(acc, 0), 150);

    // Account age contribution: 5 points per epoch, capped at 100
    // At epoch 10 with creation_epoch 0: age = 10 epochs -> 50 points
    BOOST_CHECK_EQUAL(cybou::ComputeProofOfTrustScore(acc, 10), 200); // 100 + 50 (sb cap) + 50 (age)
    BOOST_CHECK_EQUAL(cybou::CalculateMailRateLimit(200, TEST_PARAMS), 100);

    // At epoch 30: age contribution reaches max 100 points
    BOOST_CHECK_EQUAL(cybou::ComputeProofOfTrustScore(acc, 30), 250); // 100 + 50 + 100

    // 2. Rate limit enforcement in ApplyMail:
    // Create new account with base limit = 25 at height 10 (epoch 1)
    auto state{InitialState()};
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID), NETWORK_ID, 10, TEST_PARAMS, state));
    BOOST_REQUIRE(cybou::ApplyAccountCreate(ValidOp(ACCOUNT_ID_2), NETWORK_ID, 10, TEST_PARAMS, state));

    auto& sender = state.accounts.at(ACCOUNT_ID);
    sender.system_balance = 1000; // PoT score = 100 + 10 = 110 < 150 (baseline tier = 25)

    // Send exactly 25 mails at epoch 1 (height 10..19)
    for (uint32_t i = 0; i < 25; ++i) {
        const auto res = cybou::ApplyMail(ACCOUNT_ID, ACCOUNT_ID_2, 1, 10, TEST_PARAMS, state);
        BOOST_REQUIRE(res);
        BOOST_CHECK_EQUAL(sender.mail_count_in_epoch, i + 1);
    }
    BOOST_CHECK_EQUAL(sender.mail_count_in_epoch, 25);

    // 26th mail at epoch 1 (height 15) is strictly rejected with RATE_LIMIT_EXCEEDED!
    const auto over_res = cybou::ApplyMail(ACCOUNT_ID, ACCOUNT_ID_2, 1, 15, TEST_PARAMS, state);
    BOOST_CHECK(over_res.error == cybou::MailError::RATE_LIMIT_EXCEEDED);
    // Counter not incremented, funds not deducted
    BOOST_CHECK_EQUAL(sender.mail_count_in_epoch, 25);

    // 3. Epoch rollover:
    // Height 20 is epoch 2 (since epoch_blocks = 10). Limit counter resets!
    const auto next_epoch_res = cybou::ApplyMail(ACCOUNT_ID, ACCOUNT_ID_2, 1, 20, TEST_PARAMS, state);
    BOOST_CHECK(next_epoch_res);
    BOOST_CHECK_EQUAL(sender.last_mail_epoch, 2);
    BOOST_CHECK_EQUAL(sender.mail_count_in_epoch, 1);

    // 4. State serialization roundtrip preserves rate limiting state
    const auto serialized = cybou::SerializeCybouState(state);
    const auto deserialized = cybou::DeserializeCybouState(serialized);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK(*deserialized == state);
    BOOST_CHECK_EQUAL(deserialized->accounts.at(ACCOUNT_ID).last_mail_epoch, 2);
    BOOST_CHECK_EQUAL(deserialized->accounts.at(ACCOUNT_ID).mail_count_in_epoch, 1);
}

BOOST_AUTO_TEST_SUITE_END()
