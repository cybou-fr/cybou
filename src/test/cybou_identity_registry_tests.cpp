// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/identity_registry.h>
#include "cybou_test_identity_helpers.h"
#include <boost/test/unit_test.hpp>

#include <array>

BOOST_AUTO_TEST_SUITE(cybou_identity_registry_tests)

BOOST_AUTO_TEST_CASE(identity_authorization_nonce_rotation_and_snapshot)
{
    using namespace cybou;
    std::array<unsigned char, 32> entropy{}, next_entropy{};
    entropy.fill(0x21);
    next_entropy.fill(0x42);
    uint256 account_bytes{}, network_id{};
    account_bytes.begin()[0] = 1;
    network_id.begin()[0] = 2;
    const AccountId account{account_bytes};
    const auto root = DeriveIdentityPublicKey(entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization = DeriveIdentityPublicKey(entropy, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && authorization);
    const IdentityAuthorization auth{*root, *authorization};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, auth);
    const auto root_pop = SignIdentityMessage(entropy, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(entropy, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    AccountCreateOp create{
        .account_id = account, .authorization = auth, .kem_package = binding.package,
        .work = {.network_id = network_id, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        .recovery_pop = *root_pop, .authorization_pop = *authorization_pop,
    };
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    IdentityRegistry registry;
    BOOST_REQUIRE(registry.Register(create, network_id, 0, params) == IdentityRegistryError::NONE);
    const auto recovery_id = ComputeRecoveryKeyId(*root);
    BOOST_REQUIRE(recovery_id);
    BOOST_CHECK(registry.FindByRecoveryKeyId(*recovery_id) == account);

    IdentityOperationAuthorization operation{
        .account_id = account, .nonce = 0, .key_epoch = 0,
        .kind = IdentityOperationKind::PAYMENT, .payload_commitment = IdentityKeyId{1},
    };
    const auto op_digest = ComputeIdentityOperationDigest(network_id, operation);
    BOOST_REQUIRE(op_digest);
    operation.signature = *SignIdentityMessage(entropy, IdentityKeyPurpose::AUTHORIZATION, *op_digest);
    BOOST_CHECK(registry.AuthorizeOperation(operation, network_id) == IdentityRegistryError::NONE);
    BOOST_CHECK_EQUAL(registry.Find(account)->nonce, 1U);
    BOOST_CHECK(registry.AuthorizeOperation(operation, network_id) == IdentityRegistryError::BAD_NONCE);

    const auto new_root = DeriveIdentityPublicKey(next_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto new_authorization = DeriveIdentityPublicKey(next_entropy, IdentityKeyPurpose::AUTHORIZATION);
    const auto new_kem_seed = DeriveIdentityXWingSeed(next_entropy);
    const auto new_kem_public = new_kem_seed ? DeriveXWingPublicKey(*new_kem_seed) : std::nullopt;
    const auto new_package = new_kem_public ? EncodeIdentityKemPackage(*new_kem_public) : std::nullopt;
    BOOST_REQUIRE(new_root && new_authorization && new_package);
    IdentityRotate rotate{
        .account_id = account, .new_recovery_key = *new_root,
        .new_authorization_key = *new_authorization, .new_kem_package = *new_package,
        .nonce = 1, .key_epoch = 1,
    };
    const auto rotate_digest = ComputeIdentityRotateDigest(network_id, rotate);
    BOOST_REQUIRE(rotate_digest);
    rotate.old_recovery_signature = *SignIdentityMessage(entropy, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    rotate.new_recovery_pop = *SignIdentityMessage(next_entropy, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    rotate.new_authorization_pop = *SignIdentityMessage(next_entropy, IdentityKeyPurpose::AUTHORIZATION, *rotate_digest);
    BOOST_CHECK(registry.RotateIdentity(rotate, network_id) == IdentityRegistryError::NONE);
    const auto new_id = ComputeRecoveryKeyId(*new_root);
    BOOST_REQUIRE(new_id);
    BOOST_CHECK(!registry.FindByRecoveryKeyId(*recovery_id));
    BOOST_CHECK(registry.FindByRecoveryKeyId(*new_id) == account);
    BOOST_CHECK_EQUAL(registry.Find(account)->nonce, 2U);
    BOOST_CHECK_EQUAL(registry.Find(account)->key_epoch, 1U);

    const auto snapshot = SerializeIdentityRegistry(registry);
    BOOST_REQUIRE(snapshot);
    const auto restored = DeserializeIdentityRegistry(*snapshot);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeIdentityRegistry(*restored) == snapshot);
    auto invalid = *snapshot;
    invalid[0] = 1;
    BOOST_CHECK(!DeserializeIdentityRegistry(invalid));
}

BOOST_AUTO_TEST_SUITE_END()
