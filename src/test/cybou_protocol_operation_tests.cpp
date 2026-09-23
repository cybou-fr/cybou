// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>

#include <uint256.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_protocol_operation_tests)

BOOST_AUTO_TEST_CASE(account_create_has_canonical_typed_roundtrip)
{
    const cybou::AccountId account_id{uint256::FromUserHex("a1").value()};
    const cybou::AccountCreateOpV1 account_create{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = account_id,
        .initial_authorization{.authorization_descriptor = uint256::FromUserHex("b2").value()},
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = uint256::FromUserHex("c3").value(),
            .account_id = account_id,
            .initial_authorization_commitment = uint256::FromUserHex("d4").value(),
            .work_epoch = 7,
            .nonce = 9,
        },
    };
    const cybou::ProtocolOperationV1 operation{account_create};

    BOOST_CHECK(cybou::OperationType(operation) == cybou::ProtocolOperationType::ACCOUNT_CREATE);
    const auto encoded{cybou::SerializeProtocolOperation(operation)};
    BOOST_REQUIRE_GE(encoded.size(), 2U);
    BOOST_CHECK_EQUAL(encoded[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL(encoded[1], static_cast<uint8_t>(cybou::ProtocolOperationType::ACCOUNT_CREATE));

    const auto decoded{cybou::DeserializeProtocolOperation(encoded)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == operation);
}

BOOST_AUTO_TEST_CASE(unknown_or_malformed_operation_is_rejected)
{
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(std::span<const unsigned char>{}).has_value());

    cybou::ProtocolOperationV1 operation{};
    auto encoded{cybou::SerializeProtocolOperation(operation)};
    encoded[0] = 0xff;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());

    encoded = cybou::SerializeProtocolOperation(operation);
    encoded[1] = 0xff;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());

    encoded = cybou::SerializeProtocolOperation(operation);
    encoded.pop_back();
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());
}

BOOST_AUTO_TEST_CASE(authorized_operation_has_canonical_typed_roundtrip)
{
    const cybou::AccountId sender{uint256::FromUserHex("a1").value()};
    const cybou::AccountId recipient{uint256::FromUserHex("b2").value()};

    const cybou::PaymentOpV1 payment{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = recipient,
        .amount = 500,
        .fee = 10,
    };
    std::array<unsigned char, cybou::USER_SIGNATURE_SIZE> sig{};
    sig.fill(0x77);

    const cybou::AuthorizedOperationV1 auth_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = sender,
        .nonce = 42,
        .payload = payment,
        .signature = sig,
    };
    const cybou::ProtocolOperationV1 operation{auth_op};

    BOOST_CHECK(cybou::OperationType(operation) == cybou::ProtocolOperationType::AUTHORIZED_OPERATION);
    const auto encoded{cybou::SerializeProtocolOperation(operation)};
    BOOST_REQUIRE_GE(encoded.size(), 2U);
    BOOST_CHECK_EQUAL(encoded[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL(encoded[1], static_cast<uint8_t>(cybou::ProtocolOperationType::AUTHORIZED_OPERATION));

    const auto decoded{cybou::DeserializeProtocolOperation(encoded)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == operation);

    // Key update payload roundtrip
    const cybou::KeyUpdateOpV1 key_update{
        .version = cybou::KEY_UPDATE_OP_VERSION,
        .new_authorization{.authorization_descriptor = uint256::FromUserHex("ee").value()},
    };
    const cybou::AuthorizedOperationV1 key_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = sender,
        .nonce = 43,
        .payload = key_update,
        .signature = sig,
    };
    const cybou::ProtocolOperationV1 key_proto_op{key_op};
    const auto key_encoded{cybou::SerializeProtocolOperation(key_proto_op)};
    const auto key_decoded{cybou::DeserializeProtocolOperation(key_encoded)};
    BOOST_REQUIRE(key_decoded.has_value());
    BOOST_CHECK(*key_decoded == key_proto_op);
}

BOOST_AUTO_TEST_CASE(user_operation_digest_is_domain_separated)
{
    const uint256 net1{uint256::ONE};
    const uint256 net2{uint256::FromUserHex("02").value()};
    const cybou::AccountId sender{uint256::FromUserHex("a1").value()};
    const cybou::AccountId recipient{uint256::FromUserHex("b2").value()};
    const cybou::PaymentOpV1 payment{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = recipient,
        .amount = 100,
        .fee = 5,
    };

    const auto d1{cybou::ComputeUserOperationDigest(net1, sender, 0, payment)};
    const auto d2{cybou::ComputeUserOperationDigest(net2, sender, 0, payment)};
    const auto d3{cybou::ComputeUserOperationDigest(net1, sender, 1, payment)};

    // Cross-network digest uniqueness
    BOOST_CHECK(d1 != d2);
    // Nonce sensitivity
    BOOST_CHECK(d1 != d3);
}

BOOST_AUTO_TEST_CASE(apply_protocol_operation_verifies_signature_and_advances_state)
{
    std::array<unsigned char, 32> priv1{};
    priv1.fill(0x11);
    const auto pub1{cybou::DeriveEd25519PublicKey(priv1)};
    BOOST_REQUIRE(pub1.has_value());

    std::array<unsigned char, 32> priv2{};
    priv2.fill(0x22);
    const auto pub2{cybou::DeriveEd25519PublicKey(priv2)};
    BOOST_REQUIRE(pub2.has_value());

    const cybou::AccountId acc1{uint256::FromUserHex("01").value()};
    const cybou::AccountId acc2{uint256::FromUserHex("02").value()};
    const uint256 network_id{uint256::ONE};

    cybou::CybouState state{
        .onboarding_pool = 50000,
        .security_reward_pool = 1000,
        .pending_fee_pool = 100,
        .accounts{},
    };
    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = 5000,
        .epoch_blocks = 10,
    };
    const cybou::ProtocolExecutionContextV1 ctx{
        .network_id = network_id,
        .block_height = 1,
        .params = params,
    };

    // Onboard acc1 and acc2
    const cybou::AccountCreateOpV1 create1{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = acc1,
        .initial_authorization{.authorization_descriptor = *pub1},
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = network_id,
            .account_id = acc1,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment({*pub1}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create1}, ctx, state));

    const cybou::AccountCreateOpV1 create2{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = acc2,
        .initial_authorization{.authorization_descriptor = *pub2},
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = network_id,
            .account_id = acc2,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment({*pub2}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create2}, ctx, state));

    // Fund acc1 balance for payments
    state.accounts.at(acc1).balance = 2000;

    // Create payment: acc1 -> acc2, amount 300, fee 20
    const cybou::PaymentOpV1 payment{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = acc2,
        .amount = 300,
        .fee = 20,
    };
    const uint256 digest{cybou::ComputeUserOperationDigest(network_id, acc1, 0, payment)};
    const auto sig{cybou::SignUserMessage(priv1, std::span<const unsigned char>{digest.begin(), digest.size()})};
    BOOST_REQUIRE(sig.has_value());

    const cybou::AuthorizedOperationV1 auth_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = acc1,
        .nonce = 0,
        .payload = payment,
        .signature = *sig,
    };

    // Apply payment operation
    const auto res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{auth_op}, ctx, state)};
    BOOST_REQUIRE(res);
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).balance, 1680);
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).next_nonce, 1);
    BOOST_CHECK_EQUAL(state.accounts.at(acc2).balance, 300);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, 120);

    // Replay of same operation fails with BAD_NONCE
    const auto replay_res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{auth_op}, ctx, state)};
    BOOST_CHECK(replay_res.error == cybou::OperationExecutionError::BAD_NONCE);

    // Operation signed by wrong key fails with INVALID_SIGNATURE
    const cybou::PaymentOpV1 payment2{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = acc2,
        .amount = 100,
        .fee = 5,
    };
    const uint256 digest2{cybou::ComputeUserOperationDigest(network_id, acc1, 1, payment2)};
    const auto wrong_sig{cybou::SignUserMessage(priv2, std::span<const unsigned char>{digest2.begin(), digest2.size()})};
    BOOST_REQUIRE(wrong_sig.has_value());

    const cybou::AuthorizedOperationV1 bad_sig_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = acc1,
        .nonce = 1,
        .payload = payment2,
        .signature = *wrong_sig,
    };
    const auto bad_sig_res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{bad_sig_op}, ctx, state)};
    BOOST_CHECK(bad_sig_res.error == cybou::OperationExecutionError::INVALID_SIGNATURE);
}

BOOST_AUTO_TEST_SUITE_END()
