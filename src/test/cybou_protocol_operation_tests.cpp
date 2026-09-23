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
    cybou::AccountCreateOpV1 create1{
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
    create1.proof_of_possession = *cybou::SignUserMessage(
        priv1, cybou::ComputeAccountPopDigest(network_id, acc1, *pub1));
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create1}, ctx, state));

    cybou::AccountCreateOpV1 create2{
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
    create2.proof_of_possession = *cybou::SignUserMessage(
        priv2, cybou::ComputeAccountPopDigest(network_id, acc2, *pub2));
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create2}, ctx, state));

    // Fund acc1 balance for payments
    state.accounts.at(acc1).balance = 2000;

    // Create payment: acc1 -> acc2, amount 300 (fee 4 debited from system_balance)
    const cybou::PaymentOpV1 payment{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = acc2,
        .amount = 300,
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
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).balance, 1700); // 2000 - 300
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).system_balance, 4999); // 5000 - 1
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).next_nonce, 1);
    BOOST_CHECK_EQUAL(state.accounts.at(acc2).balance, 300);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, 101); // 100 + 1

    // Replay of same operation fails with BAD_NONCE
    const auto replay_res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{auth_op}, ctx, state)};
    BOOST_CHECK(replay_res.error == cybou::OperationExecutionError::BAD_NONCE);

    // Operation signed by wrong key fails with INVALID_SIGNATURE
    const cybou::PaymentOpV1 payment2{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = acc2,
        .amount = 100,
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

BOOST_AUTO_TEST_CASE(system_lock_roundtrip_and_execution)
{
    const cybou::AccountId sender{uint256::FromUserHex("a1").value()};
    const cybou::SystemLockOpV1 lock_op{
        .version = cybou::SYSTEM_LOCK_OP_VERSION,
        .amount = 750,
    };
    std::array<unsigned char, cybou::USER_SIGNATURE_SIZE> sig{};
    sig.fill(0x55);

    const cybou::AuthorizedOperationV1 auth_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = sender,
        .nonce = 10,
        .payload = lock_op,
        .signature = sig,
    };
    const cybou::ProtocolOperationV1 operation{auth_op};

    // Serialization & roundtrip
    const auto encoded{cybou::SerializeProtocolOperation(operation)};
    const auto decoded{cybou::DeserializeProtocolOperation(encoded)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == operation);

    // Execution with cryptographic verification
    std::array<unsigned char, 32> priv{};
    priv.fill(0x42);
    const auto pub{cybou::DeriveEd25519PublicKey(priv)};
    BOOST_REQUIRE(pub.has_value());

    const cybou::AccountId acc{uint256::FromUserHex("11").value()};
    const uint256 network_id{uint256::FromUserHex("77").value()};
    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = 100,
        .epoch_blocks = 10,
    };
    const cybou::ProtocolExecutionContextV1 ctx{
        .network_id = network_id,
        .block_height = 1,
        .params = params,
    };

    cybou::CybouState state{
        .onboarding_pool = 10000,
        .security_reward_pool = 0,
        .pending_fee_pool = 0,
        .accounts{},
    };
    cybou::AccountCreateOpV1 create_op{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = acc,
        .initial_authorization{.authorization_descriptor = *pub},
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = network_id,
            .account_id = acc,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment({*pub}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    create_op.proof_of_possession = *cybou::SignUserMessage(
        priv, cybou::ComputeAccountPopDigest(network_id, acc, *pub));
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create_op}, ctx, state));
    state.accounts.at(acc).balance = 500;

    const cybou::SystemLockOpV1 exec_lock{.version = cybou::SYSTEM_LOCK_OP_VERSION, .amount = 200};
    const uint256 digest{cybou::ComputeUserOperationDigest(network_id, acc, 0, exec_lock)};
    const auto valid_sig{cybou::SignUserMessage(priv, std::span<const unsigned char>{digest.begin(), digest.size()})};
    BOOST_REQUIRE(valid_sig.has_value());

    const cybou::AuthorizedOperationV1 exec_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = acc,
        .nonce = 0,
        .payload = exec_lock,
        .signature = *valid_sig,
    };

    const auto exec_res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{exec_op}, ctx, state)};
    BOOST_REQUIRE(exec_res);
    BOOST_CHECK_EQUAL(state.accounts.at(acc).balance, 300);
    BOOST_CHECK_EQUAL(state.accounts.at(acc).system_balance, 300); // 100 bonus + 200 locked
    BOOST_CHECK_EQUAL(state.accounts.at(acc).next_nonce, 1);
}

BOOST_AUTO_TEST_CASE(mail_operation_fee_commitment_and_execution)
{
    const cybou::CybouProtocolParameters default_params{};
    // Deterministic size-aware fee tiers from protocol parameters
    BOOST_CHECK_EQUAL(default_params.MailFeeForSize(0), 4);
    BOOST_CHECK_EQUAL(default_params.MailFeeForSize(500), 5); // 4 + 1
    BOOST_CHECK_EQUAL(default_params.MailFeeForSize(1024), 5); // 4 + 1
    BOOST_CHECK_EQUAL(default_params.MailFeeForSize(1025), 6); // 4 + 2
    BOOST_CHECK_EQUAL(default_params.MailFeeForSize(65536), 68); // 4 + 64

    // Content commitment
    const uint256 salt{uint256::FromUserHex("5a17").value()};
    const std::vector<unsigned char> ciphertext{0x01, 0x02, 0x03, 0x04};
    const uint256 commitment = cybou::ComputeMailContentCommitment(salt, ciphertext);
    BOOST_CHECK(!commitment.IsNull());

    const cybou::AccountId sender{uint256::FromUserHex("a1").value()};
    const cybou::AccountId recipient{uint256::FromUserHex("b2").value()};

    const cybou::MailOpV1 mail_op{
        .version = cybou::MAIL_OP_VERSION,
        .recipient = recipient,
        .content_commitment = commitment,
        .discovery_tag = uint256::FromUserHex("d15c").value(),
        .ciphertext = ciphertext,
    };
    std::array<unsigned char, cybou::USER_SIGNATURE_SIZE> sig{};
    sig.fill(0x33);

    const cybou::AuthorizedOperationV1 auth_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = sender,
        .nonce = 0,
        .payload = mail_op,
        .signature = sig,
    };
    const cybou::ProtocolOperationV1 operation{auth_op};

    // Serialization & roundtrip
    const auto encoded{cybou::SerializeProtocolOperation(operation)};
    const auto decoded{cybou::DeserializeProtocolOperation(encoded)};
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(*decoded == operation);

    // Execution with cryptographic verification
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
    const uint256 network_id{uint256::FromUserHex("42").value()};

    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = 6000,
        .epoch_blocks = 10,
    };
    const cybou::ProtocolExecutionContextV1 ctx{
        .network_id = network_id,
        .block_height = 1,
        .params = params,
    };

    cybou::CybouState state{
        .onboarding_pool = 100000,
        .security_reward_pool = 0,
        .pending_fee_pool = 0,
        .accounts{},
    };
    cybou::AccountCreateOpV1 create1{
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
    create1.proof_of_possession = *cybou::SignUserMessage(
        priv1, cybou::ComputeAccountPopDigest(network_id, acc1, *pub1));
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create1}, ctx, state));

    cybou::AccountCreateOpV1 create2{
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
    create2.proof_of_possession = *cybou::SignUserMessage(
        priv2, cybou::ComputeAccountPopDigest(network_id, acc2, *pub2));
    BOOST_REQUIRE(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{create2}, ctx, state));

    const size_t state_accounts_before = state.accounts.size();

    // 1. Oversized mail rejected
    cybou::MailOpV1 oversized_mail = mail_op;
    oversized_mail.recipient = acc2;
    oversized_mail.ciphertext.resize(params.max_mail_ciphertext_size + 1);
    const uint256 over_digest{cybou::ComputeUserOperationDigest(network_id, acc1, 0, oversized_mail)};
    const auto over_sig{cybou::SignUserMessage(priv1, std::span<const unsigned char>{over_digest.begin(), over_digest.size()})};
    const cybou::AuthorizedOperationV1 over_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = acc1,
        .nonce = 0,
        .payload = oversized_mail,
        .signature = *over_sig,
    };
    BOOST_CHECK(cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{over_op}, ctx, state).error ==
                cybou::OperationExecutionError::MAIL_OVERSIZED);

    // 2. Insufficient system balance rejected
    state.accounts.at(acc1).system_balance = 0;
    cybou::MailOpV1 valid_mail{
        .version = cybou::MAIL_OP_VERSION,
        .recipient = acc2,
        .content_commitment = commitment,
        .discovery_tag = uint256::FromUserHex("d1").value(),
        .ciphertext = ciphertext,
    };
    const uint256 valid_digest{cybou::ComputeUserOperationDigest(network_id, acc1, 0, valid_mail)};
    const auto valid_mail_sig{cybou::SignUserMessage(priv1, std::span<const unsigned char>{valid_digest.begin(), valid_digest.size()})};
    const cybou::AuthorizedOperationV1 valid_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = acc1,
        .nonce = 0,
        .payload = valid_mail,
        .signature = *valid_mail_sig,
    };

    const auto no_sys_res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{valid_op}, ctx, state)};
    BOOST_CHECK(no_sys_res.error == cybou::OperationExecutionError::MAIL_FAILED);
    BOOST_CHECK(no_sys_res.mail_result.error == cybou::MailError::INSUFFICIENT_SYSTEM_BALANCE);

    // 3. Valid mail operation execution with restored system balance
    state.accounts.at(acc1).system_balance = 6000;
    const auto mail_exec_res{cybou::ApplyProtocolOperation(cybou::ProtocolOperationV1{valid_op}, ctx, state)};
    BOOST_REQUIRE(mail_exec_res);

    // System balance consumed (deterministic fee: 4 base + 1 = 5 CYBOU)
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).system_balance, 5995); // 6000 - 5
    BOOST_CHECK_EQUAL(state.accounts.at(acc1).next_nonce, 1);
    BOOST_CHECK_EQUAL(state.pending_fee_pool, 5);

    // Hard Rule: Consensus state remains bounded, NO permanent per-mail object
    BOOST_CHECK_EQUAL(state.accounts.size(), state_accounts_before);
}

BOOST_AUTO_TEST_SUITE_END()
