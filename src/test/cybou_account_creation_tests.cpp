// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/account_creation.h>
#include <cybou/signing.h>

#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_account_creation_tests)

namespace {

const uint256 NETWORK_ID{uint256::ONE};
const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
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

cybou::AccountCreationWorkV1 ValidWork(const uint64_t epoch = 1, const unsigned int required_bits = 0)
{
    cybou::AccountCreationWorkV1 work{
        .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
        .network_id = NETWORK_ID,
        .account_id = ACCOUNT_ID,
        .initial_authorization_commitment = cybou::ComputeAuthCommitment(ValidAuth()),
        .work_epoch = epoch,
        .nonce = 0,
    };
    if (required_bits > 0) {
        while (!cybou::CheckAccountCreationWork(work, required_bits)) {
            ++work.nonce;
        }
    }
    return work;
}

cybou::AccountCreateOpV1 ValidOp(const uint64_t epoch = 1, const unsigned int required_bits = 0)
{
    cybou::AccountCreateOpV1 op{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = ACCOUNT_ID,
        .initial_authorization = ValidAuth(),
        .creation_work = ValidWork(epoch, required_bits),
    };
    const uint256 pop_digest = cybou::ComputeAccountPopDigest(
        NETWORK_ID, op.account_id, op.initial_authorization.authorization_descriptor);
    op.proof_of_possession = *cybou::SignUserMessage(AUTH_PRIVKEY, pop_digest);
    return op;
}

} // namespace

BOOST_AUTO_TEST_CASE(account_creation_work_serialization_round_trip)
{
    const auto work{ValidWork()};
    const auto bytes{cybou::SerializeAccountCreationWork(work)};
    BOOST_CHECK_EQUAL(bytes.size(), cybou::ACCOUNT_CREATION_WORK_SERIALIZED_SIZE);

    const auto decoded{cybou::DeserializeAccountCreationWork(bytes)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == work);

    auto malformed{bytes};
    malformed[0] = 0xff;
    BOOST_CHECK(!cybou::DeserializeAccountCreationWork(malformed));

    malformed = bytes;
    malformed.pop_back();
    BOOST_CHECK(!cybou::DeserializeAccountCreationWork(malformed));
}

BOOST_AUTO_TEST_CASE(account_creation_work_hash_is_sensitive_to_all_fields)
{
    const auto work{ValidWork()};
    const auto hash{cybou::ComputeAccountCreationWorkHash(work)};

    auto mutated{work};
    mutated.network_id = uint256::FromUserHex("99").value();
    BOOST_CHECK(cybou::ComputeAccountCreationWorkHash(mutated) != hash);

    mutated = work;
    mutated.account_id = cybou::AccountId{uint256::FromUserHex("99").value()};
    BOOST_CHECK(cybou::ComputeAccountCreationWorkHash(mutated) != hash);

    mutated = work;
    mutated.initial_authorization_commitment = uint256::FromUserHex("99").value();
    BOOST_CHECK(cybou::ComputeAccountCreationWorkHash(mutated) != hash);

    mutated = work;
    mutated.work_epoch = 2;
    BOOST_CHECK(cybou::ComputeAccountCreationWorkHash(mutated) != hash);

    mutated = work;
    mutated.nonce = 1;
    BOOST_CHECK(cybou::ComputeAccountCreationWorkHash(mutated) != hash);
}

BOOST_AUTO_TEST_CASE(check_account_creation_work_verifies_leading_zero_bits)
{
    const auto work{ValidWork(1, 4)};
    BOOST_CHECK(cybou::CheckAccountCreationWork(work, 4));
    BOOST_CHECK(cybou::CheckAccountCreationWork(work, 0));

    auto mutated{work};
    mutated.nonce ^= 0xdeadbeef;
    // With very high probability or check against bit count
    const auto bits{cybou::CountLeadingZeroBits(cybou::ComputeAccountCreationWorkHash(mutated))};
    BOOST_CHECK_EQUAL(cybou::CheckAccountCreationWork(mutated, bits + 1), false);
}

BOOST_AUTO_TEST_CASE(account_create_op_serialization_round_trip)
{
    const auto op{ValidOp()};
    const auto bytes{cybou::SerializeAccountCreateOp(op)};
    const auto decoded{cybou::DeserializeAccountCreateOp(bytes)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == op);
}

BOOST_AUTO_TEST_CASE(validate_account_create_op_enforces_all_bindings)
{
    cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 1,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = 6000,
        .epoch_blocks = 10,
    };

    const auto op{ValidOp(1, 1)};
    // Heights map to epochs via height / epoch_blocks (10): 10 -> epoch 1,
    // 20 -> epoch 2 (within lag 1), 30 -> epoch 3 (expired), 0 -> epoch 0
    // (work from the future).
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 10, params) == cybou::AccountCreateValidationError::NONE);
    // Within allowed epoch lag window
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 20, params) == cybou::AccountCreateValidationError::NONE);
    // Expired: lag is 2 > 1
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 30, params) == cybou::AccountCreateValidationError::EXPIRED_WORK_EPOCH);
    // Future epoch: work_epoch 1 > epoch(0) = 0
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 0, params) == cybou::AccountCreateValidationError::FUTURE_WORK_EPOCH);

    auto wrong_network{op};
    wrong_network.creation_work.network_id = uint256::FromUserHex("ff").value();
    BOOST_CHECK(cybou::ValidateAccountCreateOp(wrong_network, NETWORK_ID, 10, params) == cybou::AccountCreateValidationError::NETWORK_MISMATCH);

    auto wrong_account{op};
    wrong_account.account_id = cybou::AccountId{uint256::FromUserHex("0b").value()};
    BOOST_CHECK(cybou::ValidateAccountCreateOp(wrong_account, NETWORK_ID, 10, params) == cybou::AccountCreateValidationError::ACCOUNT_ID_MISMATCH);

    auto wrong_auth{op};
    wrong_auth.initial_authorization.authorization_descriptor = uint256::FromUserHex("99").value();
    BOOST_CHECK(cybou::ValidateAccountCreateOp(wrong_auth, NETWORK_ID, 10, params) == cybou::AccountCreateValidationError::AUTH_COMMITMENT_MISMATCH);

    auto high_diff_params{params};
    high_diff_params.account_creation_work_bits = 255;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 10, high_diff_params) == cybou::AccountCreateValidationError::INSUFFICIENT_WORK);

    auto bad_pop{op};
    bad_pop.proof_of_possession[0] ^= 0x55;
    BOOST_CHECK(cybou::ValidateAccountCreateOp(bad_pop, NETWORK_ID, 10, params) == cybou::AccountCreateValidationError::INVALID_PROOF_OF_POSSESSION);
}

BOOST_AUTO_TEST_SUITE_END()
