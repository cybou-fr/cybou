// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/identity_registry.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>

BOOST_AUTO_TEST_SUITE(cybou_identity_registry_tests)

BOOST_AUTO_TEST_CASE(root_authorized_device_and_recovery_transitions)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{}, second_seed{}, replacement_seed{};
    for (size_t i{0}; i < 32; ++i) {
        root_seed[i] = static_cast<unsigned char>(i + 1);
        device_seed[i] = static_cast<unsigned char>(i + 33);
        second_seed[i] = static_cast<unsigned char>(i + 65);
        replacement_seed[i] = static_cast<unsigned char>(i + 97);
    }
    uint256 account_bytes{}, network_id{};
    account_bytes.begin()[0] = 1;
    network_id.begin()[0] = 2;
    const AccountId account{account_bytes};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::DEVICE);
    const auto second = DeriveIdentityPublicKey(second_seed, IdentityKeyPurpose::DEVICE);
    const auto replacement = DeriveIdentityPublicKey(replacement_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(root && device && second && replacement);
    const IdentityAuthorization auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitment(auth);
    const auto create_digest = ComputeAccountCreatePopDigest(network_id, account, auth);
    BOOST_REQUIRE(commitment && create_digest);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *create_digest);
    const auto device_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *create_digest);
    BOOST_REQUIRE(root_pop && device_pop);
    AccountCreateOp create{account, auth,
        {.network_id = network_id, .account_id = account, .authorization_commitment = *commitment},
        *root_pop, *device_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    IdentityRegistry registry;
    BOOST_REQUIRE(registry.Register(create, network_id, 0, params) == IdentityRegistryError::NONE);
    BOOST_CHECK(registry.Register(create, network_id, 0, params) == IdentityRegistryError::ACCOUNT_EXISTS);
    const auto root_id = ComputeRecoveryKeyId(*root);
    const auto device_id = ComputeDeviceKeyId(*device);
    const auto second_id = ComputeDeviceKeyId(*second);
    BOOST_REQUIRE(root_id && device_id && second_id);
    BOOST_CHECK(registry.FindByRecoveryKeyId(*root_id) == account);
    BOOST_REQUIRE(registry.Find(account));
    BOOST_CHECK(registry.Find(account)->devices.at(*device_id).next_nonce == 0);

    DeviceAdd add{};
    add.account_id = account;
    add.new_device = *second;
    const auto add_digest = ComputeDeviceAddDigest(network_id, add);
    BOOST_REQUIRE(add_digest);
    add.root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *add_digest);
    add.device_pop = *SignIdentityMessage(second_seed, IdentityKeyPurpose::DEVICE, *add_digest);
    auto damaged_add = add;
    damaged_add.device_pop.ed25519[0] ^= 1;
    BOOST_CHECK(registry.AddDevice(damaged_add, network_id) == IdentityRegistryError::INVALID_SIGNATURE);
    BOOST_CHECK(registry.Find(account)->next_root_nonce == 0);
    BOOST_CHECK(registry.AddDevice(add, network_id) == IdentityRegistryError::NONE);
    BOOST_CHECK(registry.Find(account)->devices.size() == 2);
    BOOST_CHECK(registry.Find(account)->devices.at(*device_id).next_nonce == 0);
    BOOST_CHECK(registry.Find(account)->next_root_nonce == 1);
    BOOST_CHECK(registry.AddDevice(add, network_id) == IdentityRegistryError::DEVICE_EXISTS);

    DeviceAuthorization operation{};
    operation.account_id = account;
    operation.device_id = *second_id;
    operation.activation_nonce = 1;
    operation.kind = DeviceOperationKind::PAYMENT;
    operation.payload_commitment[0] = 0x55;
    const auto operation_digest = ComputeDeviceOperationDigest(network_id, operation);
    BOOST_REQUIRE(operation_digest);
    operation.signature = *SignIdentityMessage(second_seed, IdentityKeyPurpose::DEVICE, *operation_digest);
    auto wrong_kind = operation;
    wrong_kind.kind = DeviceOperationKind::MAIL;
    BOOST_CHECK(registry.AuthorizeDeviceOperation(wrong_kind, network_id) == IdentityRegistryError::INVALID_SIGNATURE);
    auto wrong_payload = operation;
    wrong_payload.payload_commitment[0] ^= 1;
    BOOST_CHECK(registry.AuthorizeDeviceOperation(wrong_payload, network_id) == IdentityRegistryError::INVALID_SIGNATURE);
    BOOST_CHECK(registry.Find(account)->devices.at(*second_id).next_nonce == 0);
    auto candidate = registry;
    BOOST_CHECK(candidate.AuthorizeDeviceOperation(operation, network_id) == IdentityRegistryError::NONE);
    BOOST_CHECK(registry.Find(account)->devices.at(*second_id).next_nonce == 0);
    BOOST_CHECK(registry.AuthorizeDeviceOperation(operation, network_id) == IdentityRegistryError::NONE);
    BOOST_CHECK(registry.Find(account)->devices.at(*second_id).next_nonce == 1);
    BOOST_CHECK(registry.Find(account)->devices.at(*device_id).next_nonce == 0);
    BOOST_CHECK(registry.AuthorizeDeviceOperation(operation, network_id) == IdentityRegistryError::BAD_NONCE);

    DeviceRevoke revoke{};
    revoke.account_id = account;
    revoke.device_id = *device_id;
    revoke.root_nonce = 1;
    const auto revoke_digest = ComputeDeviceRevokeDigest(network_id, revoke);
    BOOST_REQUIRE(revoke_digest);
    revoke.root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *revoke_digest);
    BOOST_CHECK(registry.RevokeDevice(revoke, network_id) == IdentityRegistryError::NONE);
    BOOST_CHECK(!registry.Find(account)->devices.contains(*device_id));
    BOOST_CHECK(registry.Find(account)->devices.contains(*second_id));
    BOOST_CHECK(registry.Find(account)->next_root_nonce == 2);
    BOOST_CHECK(registry.RevokeDevice(revoke, network_id) == IdentityRegistryError::DEVICE_NOT_FOUND);
    DeviceAuthorization old_device_operation{};
    old_device_operation.account_id = account;
    old_device_operation.device_id = *device_id;
    old_device_operation.kind = DeviceOperationKind::PAYMENT;
    old_device_operation.payload_commitment[0] = 0x44;
    const auto old_digest = ComputeDeviceOperationDigest(network_id, old_device_operation);
    BOOST_REQUIRE(old_digest);
    old_device_operation.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *old_digest);
    DeviceAdd readd{};
    readd.account_id = account;
    readd.new_device = *device;
    readd.root_nonce = 2;
    const auto readd_digest = ComputeDeviceAddDigest(network_id, readd);
    BOOST_REQUIRE(readd_digest);
    readd.root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *readd_digest);
    readd.device_pop = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *readd_digest);
    BOOST_CHECK(registry.AddDevice(readd, network_id) == IdentityRegistryError::NONE);
    BOOST_CHECK(registry.Find(account)->devices.at(*device_id).activation_nonce == 3);
    BOOST_CHECK(registry.AuthorizeDeviceOperation(old_device_operation, network_id) == IdentityRegistryError::BAD_NONCE);

    RecoveryRotate rotate{};
    rotate.account_id = account;
    rotate.new_root = *replacement;
    rotate.root_nonce = 3;
    const auto rotate_digest = ComputeRecoveryRotateDigest(network_id, rotate);
    BOOST_REQUIRE(rotate_digest);
    rotate.old_root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    rotate.new_root_pop = *SignIdentityMessage(replacement_seed, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    auto damaged_rotate = rotate;
    damaged_rotate.new_root_pop.ed25519[0] ^= 1;
    BOOST_CHECK(registry.RotateRecovery(damaged_rotate, network_id) == IdentityRegistryError::INVALID_SIGNATURE);
    BOOST_CHECK(registry.FindByRecoveryKeyId(*root_id) == account);
    BOOST_CHECK(registry.RotateRecovery(rotate, network_id) == IdentityRegistryError::NONE);
    const auto replacement_id = ComputeRecoveryKeyId(*replacement);
    BOOST_REQUIRE(replacement_id);
    BOOST_CHECK(!registry.FindByRecoveryKeyId(*root_id));
    BOOST_CHECK(registry.FindByRecoveryKeyId(*replacement_id) == account);
    BOOST_CHECK(registry.Find(account)->next_root_nonce == 4);
    BOOST_CHECK(registry.Find(account)->devices.at(*second_id).next_nonce == 1);

    const auto snapshot = SerializeIdentityRegistry(registry);
    BOOST_REQUIRE(snapshot);
    const auto restored = DeserializeIdentityRegistry(*snapshot);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeIdentityRegistry(*restored) == snapshot);
    BOOST_CHECK(restored->FindByRecoveryKeyId(*replacement_id) == account);
    BOOST_CHECK(restored->Find(account)->devices.at(*device_id).activation_nonce == 3);
    auto damaged_snapshot = *snapshot;
    damaged_snapshot[0] = 1;
    BOOST_CHECK(!DeserializeIdentityRegistry(damaged_snapshot));
    damaged_snapshot = *snapshot;
    damaged_snapshot.push_back(0);
    BOOST_CHECK(!DeserializeIdentityRegistry(damaged_snapshot));
    BOOST_CHECK(!DeserializeIdentityRegistry(std::span{*snapshot}.first(snapshot->size() - 1)));
    // The canonical wire order is DeviceKeyID order, not insertion order.
    damaged_snapshot = *snapshot;
    constexpr size_t first_device_offset{5 + 32 + 1984 + 8 + 1};
    constexpr size_t device_size{1344 + 8 + 8};
    for (size_t i{0}; i < device_size; ++i) {
        std::swap(damaged_snapshot[first_device_offset + i], damaged_snapshot[first_device_offset + device_size + i]);
    }
    BOOST_CHECK(!DeserializeIdentityRegistry(damaged_snapshot));
    damaged_snapshot = *snapshot;
    std::copy_n(damaged_snapshot.begin() + first_device_offset + device_size - 8, 8,
        damaged_snapshot.begin() + first_device_offset + 2 * device_size - 8);
    BOOST_CHECK(!DeserializeIdentityRegistry(damaged_snapshot));
}

BOOST_AUTO_TEST_SUITE_END()
