// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/protocol_operation.h>
#include <cybou/operation_relay.h>
#include <cybou/protocol_limits.h>
#include "cybou_test_identity_helpers.h"
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
    cybou::IdentityHybridPublicKey authorization_key;
    cybou::IdentityAuthorization auth;
};

TestIdentity MakeTestIdentity(unsigned char fill_byte)
{
    TestIdentity id;
    id.root_seed.fill(fill_byte);
    id.dev_seed = id.root_seed;

    uint256 acc_bytes{};
    acc_bytes.begin()[0] = fill_byte;
    id.account_id = cybou::AccountId{acc_bytes};

    const auto root = cybou::DeriveIdentityPublicKey(id.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto dev = cybou::DeriveIdentityPublicKey(id.dev_seed, cybou::IdentityKeyPurpose::AUTHORIZATION);
    id.recovery_root = *root;
    id.authorization_key = *dev;
    id.auth = cybou::IdentityAuthorization{id.recovery_root, id.authorization_key};
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
    const auto binding = cybou::test::MakeIdentityKemBinding(net_id, id.account_id, id.auth);

    const auto root_pop = cybou::SignIdentityMessage(id.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto dev_pop = cybou::SignIdentityMessage(id.dev_seed, cybou::IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);

    return cybou::AccountCreateOp{
        .account_id = id.account_id,
        .authorization = id.auth,
        .kem_package = binding.package,
        .work = {
            .network_id = net_id,
            .account_id = id.account_id,
            .authorization_commitment = binding.authorization_commitment,
            .work_epoch = 0,
            .nonce = 42,
        },
        .recovery_pop = *root_pop,
        .authorization_pop = *dev_pop,
    };
}

cybou::AuthorizedPayment MakeTestPayment(const TestIdentity& sender, const cybou::AccountId& recipient, uint64_t amount)
{
    const auto net_id = TestNetworkId();
    cybou::PaymentPayload payload{.recipient = recipient, .amount = amount};
    const auto commit = cybou::ComputePaymentPayloadCommitment(payload);

    cybou::IdentityOperationAuthorization auth{
        .account_id = sender.account_id,
        .nonce = 0,
        .key_epoch = 0,
        .kind = cybou::IdentityOperationKind::PAYMENT,
        .payload_commitment = *commit,
    };
    const auto digest = cybou::ComputeIdentityOperationDigest(net_id, auth);
    auth.signature = *cybou::SignIdentityMessage(sender.dev_seed, cybou::IdentityKeyPurpose::AUTHORIZATION, *digest);

    return cybou::AuthorizedPayment{.authorization = auth, .payment = payload};
}

cybou::AuthorizedSystemLock MakeTestSystemLock(const TestIdentity& sender, uint64_t amount)
{
    const auto net_id = TestNetworkId();
    cybou::SystemLockPayload payload{.amount = amount};
    const auto commit = cybou::ComputeSystemLockPayloadCommitment(payload);

    cybou::IdentityOperationAuthorization auth{
        .account_id = sender.account_id,
        .nonce = 1,
        .key_epoch = 0,
        .kind = cybou::IdentityOperationKind::SYSTEM_LOCK,
        .payload_commitment = *commit,
    };
    const auto digest = cybou::ComputeIdentityOperationDigest(net_id, auth);
    auth.signature = *cybou::SignIdentityMessage(sender.dev_seed, cybou::IdentityKeyPurpose::AUTHORIZATION, *digest);

    return cybou::AuthorizedSystemLock{.authorization = auth, .lock = payload};
}


} // namespace

BOOST_AUTO_TEST_CASE(operation_relay_is_volatile_bounded_and_mesh_deduplicated)
{
    cybou::OperationRelay relay{1, cybou::MAX_OPERATION_PAYLOAD_BYTES};
    auto first = MakeTestAccountCreate(MakeTestIdentity(1));
    auto first_bytes = cybou::SerializeProtocolOperation(cybou::ProtocolOperation{first});
    BOOST_REQUIRE(first_bytes);
    BOOST_CHECK(relay.Enqueue(*first_bytes) == cybou::OperationRelayEnqueueStatus::QUEUED);

    const auto session = relay.AttachAuthenticatedFinalizer();
    BOOST_REQUIRE(session);
    BOOST_CHECK(!relay.AttachAuthenticatedFinalizer());
    BOOST_CHECK(relay.Enqueue(*first_bytes) == cybou::OperationRelayEnqueueStatus::DUPLICATE);
    BOOST_CHECK_EQUAL(relay.QueuedOperations(), 1U);
    BOOST_CHECK_EQUAL(relay.QueuedBytes(), first_bytes->size());

    const auto first_id = cybou::ComputeOperationId(cybou::ProtocolOperation{first});
    BOOST_REQUIRE(first_id);
    const auto peeked = relay.Peek();
    BOOST_REQUIRE(peeked);
    BOOST_CHECK(peeked->exact_bytes == *first_bytes);
    const auto unrelated_id = cybou::ComputeOperationId(
        cybou::ProtocolOperation{MakeTestSystemLock(MakeTestIdentity(2), 1)});
    BOOST_REQUIRE(unrelated_id);
    BOOST_CHECK(!relay.Acknowledge(*unrelated_id));
    const auto claimed = relay.Claim();
    BOOST_REQUIRE(claimed);
    BOOST_CHECK(claimed->operation_id == *first_id);
    BOOST_CHECK(!relay.Claim()); // A second peer cannot race the same FIFO head.
    relay.Release(*first_id); // Failed transfer makes it available to the next peer.
    BOOST_CHECK(!relay.Acknowledge(*first_id));
    BOOST_REQUIRE(relay.Claim());

    auto second = MakeTestAccountCreate(MakeTestIdentity(3));
    second.work.nonce = 43;
    const auto second_bytes = cybou::SerializeProtocolOperation(cybou::ProtocolOperation{second});
    BOOST_REQUIRE(second_bytes);
    BOOST_CHECK(relay.Enqueue(*second_bytes) == cybou::OperationRelayEnqueueStatus::QUEUE_FULL);

    BOOST_CHECK(relay.Acknowledge(*first_id));
    BOOST_CHECK_EQUAL(relay.QueuedOperations(), 0U);
    BOOST_CHECK(relay.Enqueue(*first_bytes) == cybou::OperationRelayEnqueueStatus::DUPLICATE);
    BOOST_CHECK(relay.Enqueue(*first_bytes, true) == cybou::OperationRelayEnqueueStatus::QUEUED);
    BOOST_REQUIRE(relay.Claim());
    BOOST_CHECK(relay.Acknowledge(*first_id));
    BOOST_CHECK(relay.Enqueue(*second_bytes) == cybou::OperationRelayEnqueueStatus::QUEUED);
    relay.DetachFinalizer(*session);
    BOOST_CHECK(!relay.HasAuthenticatedFinalizer());
    BOOST_CHECK_EQUAL(relay.QueuedOperations(), 1U);
    BOOST_CHECK_EQUAL(relay.QueuedBytes(), second_bytes->size());
    BOOST_REQUIRE(relay.Peek());
    BOOST_CHECK(relay.Enqueue(*second_bytes) == cybou::OperationRelayEnqueueStatus::DUPLICATE);

    const auto next_session = relay.AttachAuthenticatedFinalizer();
    BOOST_REQUIRE(next_session);
    const auto second_id = cybou::ComputeOperationId(cybou::ProtocolOperation{second});
    BOOST_REQUIRE(second_id);
    BOOST_REQUIRE(relay.Claim());
    BOOST_CHECK(relay.Acknowledge(*second_id));
    BOOST_CHECK(!relay.HasQueued(*second_id));
    BOOST_CHECK(relay.Enqueue(*second_bytes) == cybou::OperationRelayEnqueueStatus::DUPLICATE);
    BOOST_CHECK(relay.Enqueue(*second_bytes, true) == cybou::OperationRelayEnqueueStatus::QUEUED);
    BOOST_CHECK(relay.HasQueued(*second_id));
    BOOST_REQUIRE(relay.Claim());
    relay.ForgetFinalized(*second_id);
    BOOST_CHECK(!relay.HasQueued(*second_id));
    BOOST_CHECK_EQUAL(relay.QueuedOperations(), 0U);
    relay.DetachFinalizer(*session);
    BOOST_CHECK(relay.HasAuthenticatedFinalizer());
    BOOST_CHECK(!relay.Peek());
    relay.DetachFinalizer(*next_session);
    BOOST_CHECK(!relay.HasAuthenticatedFinalizer());
}

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

BOOST_AUTO_TEST_CASE(relay_precheck_requires_valid_identity_signatures)
{
    const auto alice = MakeTestIdentity(0x31);
    const auto create = MakeTestAccountCreate(alice);
    cybou::IdentityRegistry identities;
    auto parameters = cybou::DevProtocolParameters();
    parameters.account_creation_work_bits = 0;
    BOOST_REQUIRE(identities.Register(create, TestNetworkId(), 0, parameters) ==
        cybou::IdentityRegistryError::NONE);

    const auto bob = MakeTestIdentity(0x32);
    auto payment = MakeTestPayment(alice, bob.account_id, 17);
    const cybou::ProtocolOperation valid{payment};
    BOOST_CHECK(cybou::VerifyProtocolOperationRelayProofs(valid, TestNetworkId(), identities));

    payment.authorization.signature.ed25519[0] ^= 1;
    BOOST_CHECK(!cybou::VerifyProtocolOperationRelayProofs(
        cybou::ProtocolOperation{payment}, TestNetworkId(), identities));
    BOOST_CHECK(!cybou::VerifyProtocolOperationRelayProofs(valid, uint256{}, identities));
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

BOOST_AUTO_TEST_CASE(root_publication_is_a_typed_identity_authorized_operation)
{
    const auto alice = MakeTestIdentity(1);
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(0x31);
    publication.chunk_authorization_root.fill(0x42);
    publication.chunk_count = 1;
    cybou::RootRecipientCapsule capsule;
    capsule.key_epoch = 0;
    capsule.encapsulation.fill(0x53);
    capsule.wrapped_content_key.fill(0x64);
    publication.recipient_capsules.push_back(capsule);

    cybou::IdentityOperationAuthorization auth{
        .account_id = alice.account_id,
        .nonce = 3,
        .key_epoch = 0,
        .kind = cybou::IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *cybou::ComputeRootPublicationPayloadCommitment(publication),
    };
    const auto digest = cybou::ComputeIdentityOperationDigest(TestNetworkId(), auth);
    BOOST_REQUIRE(digest);
    auth.signature = *cybou::SignIdentityMessage(alice.dev_seed,
        cybou::IdentityKeyPurpose::AUTHORIZATION, *digest);
    const cybou::ProtocolOperation operation{cybou::AuthorizedRootPublication{
        .authorization = auth, .publication = publication}};

    const auto encoded = cybou::SerializeProtocolOperation(operation);
    BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::ROOT_PUBLICATION));
    BOOST_CHECK(encoded->size() <= cybou::ROOT_PUBLICATION_MAX_OPERATION_BYTES);
    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(*decoded == operation);

    auto altered = *encoded;
    altered.back() ^= 1;
    BOOST_CHECK(!cybou::DeserializeProtocolOperation(altered));
}

BOOST_AUTO_TEST_CASE(identity_rotate_canonical_typed_roundtrip)
{
    const auto alice = MakeTestIdentity(1);
    std::array<unsigned char, 32> next_entropy{};
    next_entropy.fill(0x55);
    const auto recovery = cybou::DeriveIdentityPublicKey(next_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization = cybou::DeriveIdentityPublicKey(next_entropy, cybou::IdentityKeyPurpose::AUTHORIZATION);
    const auto kem_seed = cybou::DeriveIdentityXWingSeed(next_entropy);
    const auto kem_public = kem_seed ? cybou::DeriveXWingPublicKey(*kem_seed) : std::nullopt;
    const auto package = kem_public ? cybou::EncodeIdentityKemPackage(*kem_public) : std::nullopt;
    BOOST_REQUIRE(recovery && authorization && package);
    cybou::IdentityRotate rotate{
        .account_id = alice.account_id,
        .new_recovery_key = *recovery,
        .new_authorization_key = *authorization,
        .new_kem_package = *package,
        .nonce = 0,
        .key_epoch = 1,
    };
    const auto digest = cybou::ComputeIdentityRotateDigest(TestNetworkId(), rotate);
    BOOST_REQUIRE(digest);
    rotate.old_recovery_signature = *cybou::SignIdentityMessage(alice.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, *digest);
    rotate.new_recovery_pop = *cybou::SignIdentityMessage(next_entropy, cybou::IdentityKeyPurpose::RECOVERY_ROOT, *digest);
    rotate.new_authorization_pop = *cybou::SignIdentityMessage(next_entropy, cybou::IdentityKeyPurpose::AUTHORIZATION, *digest);
    const cybou::ProtocolOperation operation{rotate};
    const auto encoded = cybou::SerializeProtocolOperation(operation);
    BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL((*encoded)[0], cybou::PROTOCOL_OPERATION_VERSION);
    BOOST_CHECK_EQUAL((*encoded)[1], static_cast<uint8_t>(cybou::ProtocolOperationKind::IDENTITY_ROTATE));
    BOOST_CHECK_EQUAL(encoded->size(), 2 + cybou::IDENTITY_ROTATE_SIZE);
    const auto decoded = cybou::DeserializeProtocolOperation(*encoded);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(std::holds_alternative<cybou::IdentityRotate>(*decoded));
    BOOST_CHECK(*decoded == operation);
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
