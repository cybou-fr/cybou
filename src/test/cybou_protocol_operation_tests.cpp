// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <vector>

BOOST_AUTO_TEST_SUITE(cybou_protocol_operation_tests)

namespace {

struct TestIdentity {
    std::array<unsigned char, 32> root_seed{};
    std::array<unsigned char, 32> dev_seed{};
    cybou::AccountId account_id;
    cybou::IdentityHybridPublicKey recovery_root;
    cybou::IdentityHybridPublicKey initial_device;
    cybou::IdentityKeyId device_id;
    cybou::IdentityAuthorization auth;
};

TestIdentity MakeTestIdentity(unsigned char fill_byte)
{
    TestIdentity id;
    id.root_seed.fill(fill_byte);
    id.dev_seed.fill(static_cast<unsigned char>(fill_byte + 100));

    uint256 acc_bytes{};
    acc_bytes.begin()[0] = fill_byte;
    id.account_id = cybou::AccountId{acc_bytes};

    const auto root = cybou::DeriveIdentityPublicKey(id.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto dev = cybou::DeriveIdentityPublicKey(id.dev_seed, cybou::IdentityKeyPurpose::DEVICE);
    id.recovery_root = *root;
    id.initial_device = *dev;
    id.device_id = *cybou::ComputeDeviceKeyId(id.initial_device);
    id.auth = cybou::IdentityAuthorization{id.recovery_root, id.initial_device};
    return id;
}

uint256 TestNetworkId()
{
    uint256 net_id{};
    net_id.begin()[0] = 0xAA;
    return net_id;
}

cybou::AccountCreateOp MakeTestAccountCreate(const TestIdentity& id)
{
    const auto net_id = TestNetworkId();
    const auto commitment = cybou::ComputeIdentityAuthorizationCommitment(id.auth);
    const auto pop_digest = cybou::ComputeAccountCreatePopDigest(net_id, id.account_id, id.auth);

    const auto root_pop = cybou::SignIdentityMessage(id.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, *pop_digest);
    const auto dev_pop = cybou::SignIdentityMessage(id.dev_seed, cybou::IdentityKeyPurpose::DEVICE, *pop_digest);

    return cybou::AccountCreateOp{
        .account_id = id.account_id,
        .authorization = id.auth,
        .work = {
            .network_id = net_id,
            .account_id = id.account_id,
            .authorization_commitment = *commitment,
            .work_epoch = 0,
            .nonce = 42,
        },
        .recovery_pop = *root_pop,
        .device_pop = *dev_pop,
    };
}

cybou::AuthorizedPayment MakeTestPayment(const TestIdentity& sender, const cybou::AccountId& recipient, uint64_t amount)
{
    const auto net_id = TestNetworkId();
    cybou::PaymentPayload payload{.recipient = recipient, .amount = amount};
    const auto commit = cybou::ComputePaymentPayloadCommitment(payload);

    cybou::DeviceAuthorization auth{
        .account_id = sender.account_id,
        .device_id = sender.device_id,
        .nonce = 0,
        .activation_nonce = 0,
        .kind = cybou::DeviceOperationKind::PAYMENT,
        .payload_commitment = *commit,
    };
    const auto digest = cybou::ComputeDeviceOperationDigest(net_id, auth);
    auth.signature = *cybou::SignIdentityMessage(sender.dev_seed, cybou::IdentityKeyPurpose::DEVICE, *digest);

    return cybou::AuthorizedPayment{.authorization = auth, .payment = payload};
}

cybou::AuthorizedSystemLock MakeTestSystemLock(const TestIdentity& sender, uint64_t amount)
{
    const auto net_id = TestNetworkId();
    cybou::SystemLockPayload payload{.amount = amount};
    const auto commit = cybou::ComputeSystemLockPayloadCommitment(payload);

    cybou::DeviceAuthorization auth{
        .account_id = sender.account_id,
        .device_id = sender.device_id,
        .nonce = 1,
        .activation_nonce = 0,
        .kind = cybou::DeviceOperationKind::SYSTEM_LOCK,
        .payload_commitment = *commit,
    };
    const auto digest = cybou::ComputeDeviceOperationDigest(net_id, auth);
    auth.signature = *cybou::SignIdentityMessage(sender.dev_seed, cybou::IdentityKeyPurpose::DEVICE, *digest);

    return cybou::AuthorizedSystemLock{.authorization = auth, .lock = payload};
}

cybou::AuthorizedMail MakeTestMail(const TestIdentity& sender, const cybou::AccountId& recipient)
{
    const auto net_id = TestNetworkId();
    cybou::MailPayload payload{
        .version = cybou::MAIL_TX_VERSION,
        .recipient = recipient,
        .ciphertext = std::vector<unsigned char>(128, 0x77),
    };
    payload.discovery_tag.begin()[0] = 0x11;
    payload.content_commitment.begin()[0] = 0x22;

    const auto commit = cybou::ComputeMailPayloadCommitment(payload);
    cybou::DeviceAuthorization auth{
        .account_id = sender.account_id,
        .device_id = sender.device_id,
        .nonce = 2,
        .activation_nonce = 0,
        .kind = cybou::DeviceOperationKind::MAIL,
        .payload_commitment = *commit,
    };
    const auto digest = cybou::ComputeDeviceOperationDigest(net_id, auth);
    auth.signature = *cybou::SignIdentityMessage(sender.dev_seed, cybou::IdentityKeyPurpose::DEVICE, *digest);

    return cybou::AuthorizedMail{.authorization = auth, .mail = payload};
}

} // namespace

BOOST_AUTO_TEST_CASE(account_create_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    const auto create_op = MakeTestAccountCreate(alice);
    const cybou::ProtocolOperation op{create_op};

    const auto encoded = cybou::SerializeProtocolOperation(op);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::ACCOUNT_CREATE));
    BOOST_CHECK_EQUAL(encoded->size(), 2 + cybou::ACCOUNT_CREATE_SIZE);

    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(std::holds_alternative<cybou::AccountCreateOp>(*decoded));
    BOOST_CHECK(*decoded == op);

    const auto op_id = cybou::ComputeOperationId(op);
    BOOST_REQUIRE(op_id.has_value());
    BOOST_CHECK(!op_id->IsNull());
    BOOST_CHECK_EQUAL(op_id->GetHex(), cybou::ComputeOperationId(*decoded)->GetHex());
}

BOOST_AUTO_TEST_CASE(payment_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    const auto bob = MakeTestIdentity(2);
    const auto payment_op = MakeTestPayment(alice, bob.account_id, 1000);
    const cybou::ProtocolOperation op{payment_op};

    const auto encoded = cybou::SerializeProtocolOperation(op);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::PAYMENT));
    BOOST_CHECK_EQUAL(encoded->size(), 2 + cybou::AUTHORIZED_PAYMENT_SIZE);

    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(std::holds_alternative<cybou::AuthorizedPayment>(*decoded));
    BOOST_CHECK(*decoded == op);

    const auto op_id = cybou::ComputeOperationId(op);
    BOOST_REQUIRE(op_id.has_value());
    BOOST_CHECK(!op_id->IsNull());
}

BOOST_AUTO_TEST_CASE(system_lock_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    const auto lock_op = MakeTestSystemLock(alice, 500);
    const cybou::ProtocolOperation op{lock_op};

    const auto encoded = cybou::SerializeProtocolOperation(op);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::SYSTEM_LOCK));
    BOOST_CHECK_EQUAL(encoded->size(), 2 + cybou::AUTHORIZED_SYSTEM_LOCK_SIZE);

    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(std::holds_alternative<cybou::AuthorizedSystemLock>(*decoded));
    BOOST_CHECK(*decoded == op);
}

BOOST_AUTO_TEST_CASE(mail_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    const auto bob = MakeTestIdentity(2);
    const auto mail_op = MakeTestMail(alice, bob.account_id);
    const cybou::ProtocolOperation op{mail_op};

    const auto encoded = cybou::SerializeProtocolOperation(op);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::MAIL));

    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(std::holds_alternative<cybou::AuthorizedMail>(*decoded));
    BOOST_CHECK(*decoded == op);
}

BOOST_AUTO_TEST_CASE(device_add_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    std::array<unsigned char, 32> dev2_seed{};
    dev2_seed.fill(0x55);
    const auto dev2_pk = cybou::DeriveIdentityPublicKey(dev2_seed, cybou::IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(dev2_pk.has_value());

    const auto net_id = TestNetworkId();
    cybou::DeviceAdd add{
        .account_id = alice.account_id,
        .new_device = *dev2_pk,
        .root_nonce = 0,
    };
    const auto add_digest = cybou::ComputeDeviceAddDigest(net_id, add);
    BOOST_REQUIRE(add_digest.has_value());
    add.root_signature = *cybou::SignIdentityMessage(alice.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, *add_digest);
    add.device_pop = *cybou::SignIdentityMessage(dev2_seed, cybou::IdentityKeyPurpose::DEVICE, *add_digest);

    const cybou::ProtocolOperation op{add};
    const auto encoded = cybou::SerializeProtocolOperation(op);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::DEVICE_ADD));
    BOOST_CHECK_EQUAL(encoded->size(), 2 + cybou::DEVICE_ADD_SIZE);

    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(std::holds_alternative<cybou::DeviceAdd>(*decoded));
    BOOST_CHECK(*decoded == op);
}

BOOST_AUTO_TEST_CASE(device_revoke_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    const auto net_id = TestNetworkId();
    cybou::DeviceRevoke revoke{
        .account_id = alice.account_id,
        .device_id = alice.device_id,
        .root_nonce = 1,
    };
    const auto revoke_digest = cybou::ComputeDeviceRevokeDigest(net_id, revoke);
    BOOST_REQUIRE(revoke_digest.has_value());
    revoke.root_signature = *cybou::SignIdentityMessage(alice.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, *revoke_digest);

    const cybou::ProtocolOperation op{revoke};
    const auto encoded = cybou::SerializeProtocolOperation(op);
    BOOST_REQUIRE(encoded.has_value());
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::DEVICE_REVOKE));
    BOOST_CHECK_EQUAL(encoded->size(), 2 + cybou::DEVICE_REVOKE_SIZE);

    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded.has_value());
    BOOST_CHECK(std::holds_alternative<cybou::DeviceRevoke>(*decoded));
    BOOST_CHECK(*decoded == op);
}

BOOST_AUTO_TEST_CASE(unknown_or_malformed_operation_is_rejected)
{
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(std::span<const unsigned char>{}).has_value());

    const auto alice = MakeTestIdentity(1);
    const auto payment_op = MakeTestPayment(alice, alice.account_id, 100);
    auto encoded = *cybou::SerializeProtocolOperation(cybou::ProtocolOperation{payment_op});

    // Invalid version
    encoded[0] = 0xff;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());
    encoded[0] = cybou::PROTOCOL_OPERATION_VERSION;

    // Unknown operation kind
    encoded[1] = 0xff;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());

    // Truncated payload
    encoded.pop_back();
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(encoded).has_value());
}

BOOST_AUTO_TEST_SUITE_END()
