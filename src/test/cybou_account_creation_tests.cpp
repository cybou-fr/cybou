// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/account_creation.h>

#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_account_creation_tests)

namespace {

const uint256 NETWORK_ID{uint256::ONE};
const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
const uint256 AUTH_KEY{uint256::FromUserHex("42").value()};

cybou::AccountAuthorizationV1 ValidAuth()
{
    return cybou::AccountAuthorizationV1{
        .auth_key_commitment = AUTH_KEY,
    };
}

cybou::AccountCreationWorkV1 ValidWork(const unsigned int required_bits = 0)
{
    cybou::AccountCreationWorkV1 work{
        .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
        .network_id = NETWORK_ID,
        .account_id = ACCOUNT_ID,
        .initial_authorization_commitment = cybou::ComputeAuthCommitment(ValidAuth()),
        .work_epoch = 1,
        .nonce = 0,
    };
    if (required_bits > 0) {
        while (!cybou::CheckAccountCreationWork(work, required_bits)) {
            ++work.nonce;
        }
    }
    return work;
}

cybou::AccountCreateOpV1 ValidOp(const unsigned int required_bits = 0)
{
    return cybou::AccountCreateOpV1{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = ACCOUNT_ID,
        .initial_authorization = ValidAuth(),
        .creation_work = ValidWork(required_bits),
    };
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
    const auto work{ValidWork(4)};
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
    const auto op{ValidOp(1)};
    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 1) == cybou::AccountCreateValidationError::NONE);

    auto wrong_network{op};
    wrong_network.creation_work.network_id = uint256::FromUserHex("ff").value();
    BOOST_CHECK(cybou::ValidateAccountCreateOp(wrong_network, NETWORK_ID, 1) == cybou::AccountCreateValidationError::NETWORK_MISMATCH);

    auto wrong_account{op};
    wrong_account.account_id = cybou::AccountId{uint256::FromUserHex("0b").value()};
    BOOST_CHECK(cybou::ValidateAccountCreateOp(wrong_account, NETWORK_ID, 1) == cybou::AccountCreateValidationError::ACCOUNT_ID_MISMATCH);

    auto wrong_auth{op};
    wrong_auth.initial_authorization.auth_key_commitment = uint256::FromUserHex("99").value();
    BOOST_CHECK(cybou::ValidateAccountCreateOp(wrong_auth, NETWORK_ID, 1) == cybou::AccountCreateValidationError::AUTH_COMMITMENT_MISMATCH);

    BOOST_CHECK(cybou::ValidateAccountCreateOp(op, NETWORK_ID, 255) == cybou::AccountCreateValidationError::INSUFFICIENT_WORK);
}

BOOST_AUTO_TEST_SUITE_END()
