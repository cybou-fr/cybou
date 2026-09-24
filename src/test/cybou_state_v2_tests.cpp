// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>
#include <cybou/block_executor_v2.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>

namespace {
cybou::ValidatorV2 MakeTestValidator(uint8_t seed_byte)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    const auto pub = cybou::DeriveIdentityPublicKey(seed, cybou::IdentityKeyPurpose::VALIDATOR);
    assert(pub.has_value());
    const auto id = cybou::ComputeValidatorKeyId(*pub);
    assert(id.has_value());
    uint256 val_id;
    std::copy_n(id->begin(), 32, val_id.begin());
    return cybou::ValidatorV2{val_id, *pub, 1};
}
} // namespace

BOOST_AUTO_TEST_SUITE(cybou_state_v2_tests)

BOOST_AUTO_TEST_CASE(account_create_funds_system_balance_and_roundtrips_state)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 1;
    device_seed[0] = 2;
    uint256 raw_account{}, network_id{};
    raw_account.begin()[0] = 3;
    network_id.begin()[0] = 4;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);
    const IdentityAuthorizationV2 auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitmentV2(auth);
    const auto digest = ComputeAccountCreatePopDigestV2(network_id, account, auth);
    BOOST_REQUIRE(commitment && digest);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *digest);
    const auto device_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *digest);
    BOOST_REQUIRE(root_pop && device_pop);
    const AccountCreateOpV2 create{account, auth,
        {.network_id = network_id, .account_id = account, .authorization_commitment = *commitment},
        *root_pop, *device_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouStateV2 state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(5));
    auto damaged_create = create;
    damaged_create.device_pop.ed25519[0] ^= 1;
    BOOST_CHECK(ApplyAccountCreateV2(damaged_create, network_id, 0, params, state) == AccountCreateStateErrorV2::INVALID_CREATE);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
    BOOST_CHECK(state.onboarding_pool == params.onboarding_bonus);
    BOOST_CHECK(ApplyAccountCreateV2(create, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);
    BOOST_CHECK(state.onboarding_pool == 0);
    BOOST_CHECK(state.accounts.at(account).system_balance == params.onboarding_bonus);
    BOOST_CHECK(state.accounts.at(account).balance == 0);
    BOOST_CHECK(state.identities.Find(account) != nullptr);
    const auto bytes = SerializeCybouStateV2(state);
    const auto hash = CybouStateHashV2(state);
    BOOST_REQUIRE(bytes && hash);
    const auto restored = DeserializeCybouStateV2(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeCybouStateV2(*restored) == bytes);
    BOOST_CHECK(CybouStateHashV2(*restored) == hash);

    BOOST_CHECK(ApplyAccountCreateV2(create, network_id, 1, params, state) == AccountCreateStateErrorV2::ACCOUNT_EXISTS);
    BOOST_CHECK(SerializeCybouStateV2(state) == bytes);
    auto damaged = *bytes;
    damaged[0] = 1;
    BOOST_CHECK(!DeserializeCybouStateV2(damaged));
    damaged = *bytes;
    damaged.push_back(0);
    BOOST_CHECK(!DeserializeCybouStateV2(damaged));
    BOOST_CHECK(!DeserializeCybouStateV2(std::span{*bytes}.first(bytes->size() - 1)));
    damaged = *bytes;
    damaged[1 + 8 * 3 + 4] ^= 1; // monetary AccountID no longer matches identity registry
    BOOST_CHECK(!DeserializeCybouStateV2(damaged));

    std::array<unsigned char, 32> other_root_seed{}, other_device_seed{};
    other_root_seed[0] = 7;
    other_device_seed[0] = 8;
    uint256 raw_other{};
    raw_other.begin()[0] = 9;
    const AccountId other_account{raw_other};
    const auto other_root = DeriveIdentityPublicKey(other_root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto other_device = DeriveIdentityPublicKey(other_device_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(other_root && other_device);
    const IdentityAuthorizationV2 other_auth{*other_root, *other_device};
    const auto other_commitment = ComputeIdentityAuthorizationCommitmentV2(other_auth);
    const auto other_digest = ComputeAccountCreatePopDigestV2(network_id, other_account, other_auth);
    BOOST_REQUIRE(other_commitment && other_digest);
    const auto other_root_pop = SignIdentityMessage(other_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *other_digest);
    const auto other_device_pop = SignIdentityMessage(other_device_seed, IdentityKeyPurpose::DEVICE, *other_digest);
    BOOST_REQUIRE(other_root_pop && other_device_pop);
    const AccountCreateOpV2 other_create{other_account, other_auth,
        {.network_id = network_id, .account_id = other_account, .authorization_commitment = *other_commitment},
        *other_root_pop, *other_device_pop};
    state.onboarding_pool = params.onboarding_bonus;
    BOOST_REQUIRE(ApplyAccountCreateV2(other_create, network_id, 1, params, state) == AccountCreateStateErrorV2::NONE);
    state.accounts.at(account).balance = 10; // funded fixture; no mint operation in this test
    AuthorizedPaymentV2 payment{};
    payment.payment = PaymentPayloadV2{other_account, 3};
    const auto payment_bytes = SerializePaymentPayloadV2(payment.payment);
    BOOST_REQUIRE(payment_bytes);
    const auto decoded_payment = DeserializePaymentPayloadV2(*payment_bytes);
    BOOST_REQUIRE(decoded_payment);
    BOOST_CHECK(decoded_payment->recipient == other_account);
    BOOST_CHECK(decoded_payment->amount == 3);
    payment.authorization.account_id = account;
    const auto device_id = ComputeDeviceKeyId(*device);
    BOOST_REQUIRE(device_id);
    payment.authorization.device_id = *device_id;
    payment.authorization.kind = DeviceOperationKindV2::PAYMENT;
    const auto payment_commitment = ComputePaymentPayloadCommitmentV2(payment.payment);
    BOOST_REQUIRE(payment_commitment);
    payment.authorization.payload_commitment = *payment_commitment;
    const auto payment_digest = ComputeDeviceOperationDigestV2(network_id, payment.authorization);
    BOOST_REQUIRE(payment_digest);
    const auto payment_signature = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *payment_digest);
    BOOST_REQUIRE(payment_signature);
    payment.authorization.signature = *payment_signature;
    auto tampered_payment = payment;
    tampered_payment.payment.amount = 4;
    BOOST_CHECK(ApplyPaymentV2(tampered_payment, network_id, params, state) == PaymentErrorV2::INVALID_PAYLOAD);
    BOOST_CHECK(state.accounts.at(account).balance == 10);
    BOOST_CHECK(ApplyPaymentV2(payment, network_id, params, state) == PaymentErrorV2::NONE);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    BOOST_CHECK(state.accounts.at(other_account).balance == 3);
    BOOST_CHECK(state.pending_fee_pool == params.payment_fee);
    BOOST_CHECK(state.identities.Find(account)->devices.at(*device_id).next_nonce == 1);
    BOOST_CHECK(ApplyPaymentV2(payment, network_id, params, state) == PaymentErrorV2::INVALID_AUTHORIZATION);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    const ProtocolOperationV2 payment_operation{payment};
    const auto wire = SerializeProtocolOperationV2(payment_operation);
    BOOST_REQUIRE(wire);
    const auto decoded_wire = DeserializeProtocolOperationV2(*wire);
    BOOST_REQUIRE(decoded_wire && std::holds_alternative<AuthorizedPaymentV2>(*decoded_wire));
    BOOST_CHECK(SerializeProtocolOperationV2(*decoded_wire) == wire);
    BOOST_CHECK(ComputeOperationIdV2(*decoded_wire) == ComputeOperationIdV2(payment_operation));
    auto bad_wire = *wire;
    bad_wire[0] = 1;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_wire));
    bad_wire = *wire;
    bad_wire.back() ^= 1;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_wire));
    BOOST_CHECK(!DeserializeProtocolOperationV2(std::span{*wire}.first(wire->size() - 1)));
    const ProtocolOperationV2 create_operation{create};
    const auto create_wire = SerializeProtocolOperationV2(create_operation);
    BOOST_REQUIRE(create_wire);
    const auto decoded_create = DeserializeProtocolOperationV2(*create_wire);
    BOOST_REQUIRE(decoded_create && std::holds_alternative<AccountCreateOpV2>(*decoded_create));
    BOOST_CHECK(SerializeProtocolOperationV2(*decoded_create) == create_wire);

    state.pending_fee_pool = 3; // fixture: next fee completes a 4-unit routing group
    auto next_payment = payment;
    next_payment.payment.amount = 1;
    next_payment.authorization.nonce = 1;
    next_payment.authorization.payload_commitment = *ComputePaymentPayloadCommitmentV2(next_payment.payment);
    const auto next_digest = ComputeDeviceOperationDigestV2(network_id, next_payment.authorization);
    BOOST_REQUIRE(next_digest);
    const auto next_signature = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *next_digest);
    BOOST_REQUIRE(next_signature);
    next_payment.authorization.signature = *next_signature;
    const auto block_result = ExecuteBlockOperationsV2(state, {ProtocolOperationV2{next_payment}}, network_id, 2, params);
    BOOST_REQUIRE(block_result);
    BOOST_CHECK(block_result.state->accounts.at(account).balance == 6);
    BOOST_CHECK(block_result.state->accounts.at(other_account).balance == 4);
    BOOST_CHECK(block_result.state->security_reward_pool == 3);
    BOOST_CHECK(block_result.state->onboarding_pool == 1);
    BOOST_CHECK(block_result.state->pending_fee_pool == 0);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    BOOST_CHECK(state.pending_fee_pool == 3);
    const auto replay_block = ExecuteBlockOperationsV2(*block_result.state,
        {ProtocolOperationV2{next_payment}}, network_id, 3, params);
    BOOST_CHECK(replay_block.error == BlockExecutionErrorV2::INVALID_PAYMENT);
    BOOST_CHECK(replay_block.payment_error == PaymentErrorV2::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(insufficient_pool_does_not_register_identity)
{
    using namespace cybou;
    CybouStateV2 state{};
    auto params = DevProtocolParameters();
    params.onboarding_bonus = 1;
    const AccountCreateOpV2 empty{};
    BOOST_CHECK(ApplyAccountCreateV2(empty, uint256{}, 0, params, state) == AccountCreateStateErrorV2::INSUFFICIENT_ONBOARDING_POOL);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
}

BOOST_AUTO_TEST_CASE(identity_operations_wire_and_block_execution)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{}, second_seed{}, new_root_seed{};
    root_seed[0] = 11;
    device_seed[0] = 12;
    second_seed[0] = 13;
    new_root_seed[0] = 14;
    uint256 raw_account{}, network_id{};
    raw_account.begin()[0] = 15;
    network_id.begin()[0] = 16;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::DEVICE);
    const auto second_dev = DeriveIdentityPublicKey(second_seed, IdentityKeyPurpose::DEVICE);
    const auto new_root = DeriveIdentityPublicKey(new_root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    BOOST_REQUIRE(root && device && second_dev && new_root);

    const IdentityAuthorizationV2 auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitmentV2(auth);
    const auto pop_digest = ComputeAccountCreatePopDigestV2(network_id, account, auth);
    BOOST_REQUIRE(commitment && pop_digest);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *pop_digest);
    const auto device_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *pop_digest);
    BOOST_REQUIRE(root_pop && device_pop);
    const AccountCreateOpV2 create{account, auth,
        {.network_id = network_id, .account_id = account, .authorization_commitment = *commitment},
        *root_pop, *device_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouStateV2 state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(17));
    BOOST_REQUIRE(ApplyAccountCreateV2(create, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);

    // 1. DeviceAddV2
    DeviceAddV2 add{};
    add.account_id = account;
    add.new_device = *second_dev;
    add.root_nonce = 0;
    const auto add_digest = ComputeDeviceAddDigestV2(network_id, add);
    BOOST_REQUIRE(add_digest);
    add.root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *add_digest);
    add.device_pop = *SignIdentityMessage(second_seed, IdentityKeyPurpose::DEVICE, *add_digest);

    const ProtocolOperationV2 add_op{add};
    const auto add_wire = SerializeProtocolOperationV2(add_op);
    BOOST_REQUIRE(add_wire);
    BOOST_CHECK(add_wire->size() == 2 + DEVICE_ADD_V2_SIZE);
    const auto decoded_add = DeserializeProtocolOperationV2(*add_wire);
    BOOST_REQUIRE(decoded_add && std::holds_alternative<DeviceAddV2>(*decoded_add));
    BOOST_CHECK(SerializeProtocolOperationV2(*decoded_add) == add_wire);
    BOOST_CHECK(ComputeOperationIdV2(*decoded_add) == ComputeOperationIdV2(add_op));

    // Damaged wires
    auto bad_add_wire = *add_wire;
    bad_add_wire[0] = 1;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_add_wire));
    bad_add_wire = *add_wire;
    bad_add_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_add_wire));
    BOOST_CHECK(!DeserializeProtocolOperationV2(std::span{*add_wire}.first(add_wire->size() - 1)));

    // Execute DeviceAdd in block
    const auto add_block = ExecuteBlockOperationsV2(state, {add_op}, network_id, 1, params);
    BOOST_REQUIRE(add_block);
    BOOST_CHECK(add_block.state->identities.Find(account)->devices.size() == 2);
    BOOST_CHECK(add_block.state->identities.Find(account)->next_root_nonce == 1);

    // Replay of same device add fails with DEVICE_EXISTS
    const auto replay_add = ExecuteBlockOperationsV2(*add_block.state, {add_op}, network_id, 2, params);
    BOOST_CHECK(replay_add.error == BlockExecutionErrorV2::INVALID_DEVICE_ADD);
    BOOST_CHECK(replay_add.identity_error == IdentityRegistryErrorV2::DEVICE_EXISTS);

    // Stale nonce with new device fails with BAD_NONCE
    std::array<unsigned char, 32> third_seed{};
    third_seed[0] = 99;
    const auto third_dev = DeriveIdentityPublicKey(third_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(third_dev);
    DeviceAddV2 stale_add{};
    stale_add.account_id = account;
    stale_add.new_device = *third_dev;
    stale_add.root_nonce = 0; // stale nonce (current is 1)
    const auto stale_digest = ComputeDeviceAddDigestV2(network_id, stale_add);
    BOOST_REQUIRE(stale_digest);
    stale_add.root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *stale_digest);
    stale_add.device_pop = *SignIdentityMessage(third_seed, IdentityKeyPurpose::DEVICE, *stale_digest);
    const auto stale_add_block = ExecuteBlockOperationsV2(*add_block.state, {ProtocolOperationV2{stale_add}}, network_id, 2, params);
    BOOST_CHECK(stale_add_block.error == BlockExecutionErrorV2::INVALID_DEVICE_ADD);
    BOOST_CHECK(stale_add_block.identity_error == IdentityRegistryErrorV2::BAD_NONCE);

    // 2. DeviceRevokeV2
    const auto first_device_id = ComputeDeviceKeyId(*device);
    BOOST_REQUIRE(first_device_id);
    DeviceRevokeV2 revoke{};
    revoke.account_id = account;
    revoke.device_id = *first_device_id;
    revoke.root_nonce = 1;
    const auto revoke_digest = ComputeDeviceRevokeDigestV2(network_id, revoke);
    BOOST_REQUIRE(revoke_digest);
    revoke.root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *revoke_digest);

    const ProtocolOperationV2 revoke_op{revoke};
    const auto revoke_wire = SerializeProtocolOperationV2(revoke_op);
    BOOST_REQUIRE(revoke_wire);
    BOOST_CHECK(revoke_wire->size() == 2 + DEVICE_REVOKE_V2_SIZE);
    const auto decoded_revoke = DeserializeProtocolOperationV2(*revoke_wire);
    BOOST_REQUIRE(decoded_revoke && std::holds_alternative<DeviceRevokeV2>(*decoded_revoke));
    BOOST_CHECK(SerializeProtocolOperationV2(*decoded_revoke) == revoke_wire);
    BOOST_CHECK(ComputeOperationIdV2(*decoded_revoke) == ComputeOperationIdV2(revoke_op));

    // Damaged wires
    auto bad_revoke_wire = *revoke_wire;
    bad_revoke_wire[0] = 9;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_revoke_wire));
    bad_revoke_wire = *revoke_wire;
    bad_revoke_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_revoke_wire));

    // Execute DeviceRevoke in block
    const auto revoke_block = ExecuteBlockOperationsV2(*add_block.state, {revoke_op}, network_id, 2, params);
    BOOST_REQUIRE(revoke_block);
    BOOST_CHECK(revoke_block.state->identities.Find(account)->devices.size() == 1);
    BOOST_CHECK(revoke_block.state->identities.Find(account)->next_root_nonce == 2);
    BOOST_CHECK(!revoke_block.state->identities.Find(account)->devices.contains(*first_device_id));

    // 3. RecoveryRotateV2
    RecoveryRotateV2 rotate{};
    rotate.account_id = account;
    rotate.new_root = *new_root;
    rotate.root_nonce = 2;
    const auto rotate_digest = ComputeRecoveryRotateDigestV2(network_id, rotate);
    BOOST_REQUIRE(rotate_digest);
    rotate.old_root_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    rotate.new_root_pop = *SignIdentityMessage(new_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);

    const ProtocolOperationV2 rotate_op{rotate};
    const auto rotate_wire = SerializeProtocolOperationV2(rotate_op);
    BOOST_REQUIRE(rotate_wire);
    BOOST_CHECK(rotate_wire->size() == 2 + RECOVERY_ROTATE_V2_SIZE);
    const auto decoded_rotate = DeserializeProtocolOperationV2(*rotate_wire);
    BOOST_REQUIRE(decoded_rotate && std::holds_alternative<RecoveryRotateV2>(*decoded_rotate));
    BOOST_CHECK(SerializeProtocolOperationV2(*decoded_rotate) == rotate_wire);
    BOOST_CHECK(ComputeOperationIdV2(*decoded_rotate) == ComputeOperationIdV2(rotate_op));

    // Damaged wires
    auto bad_rotate_wire = *rotate_wire;
    bad_rotate_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_rotate_wire));
    BOOST_CHECK(!DeserializeProtocolOperationV2(std::span{*rotate_wire}.first(rotate_wire->size() - 1)));

    // Execute RecoveryRotate in block
    const auto rotate_block = ExecuteBlockOperationsV2(*revoke_block.state, {rotate_op}, network_id, 3, params);
    BOOST_REQUIRE(rotate_block);
    const auto new_root_id = ComputeRecoveryKeyId(*new_root);
    BOOST_REQUIRE(new_root_id);
    BOOST_CHECK(rotate_block.state->identities.FindByRecoveryKeyId(*new_root_id) == account);
    BOOST_CHECK(rotate_block.state->identities.Find(account)->recovery_root.ed25519 == new_root->ed25519);
    BOOST_CHECK(rotate_block.state->identities.Find(account)->next_root_nonce == 3);
}

BOOST_AUTO_TEST_CASE(system_lock_wire_and_execution)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 21;
    device_seed[0] = 22;
    uint256 raw_account{}, network_id{};
    raw_account.begin()[0] = 23;
    network_id.begin()[0] = 24;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorizationV2 auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitmentV2(auth);
    const auto pop_digest = ComputeAccountCreatePopDigestV2(network_id, account, auth);
    BOOST_REQUIRE(commitment && pop_digest);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *pop_digest);
    const auto device_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *pop_digest);
    BOOST_REQUIRE(root_pop && device_pop);
    const AccountCreateOpV2 create{account, auth,
        {.network_id = network_id, .account_id = account, .authorization_commitment = *commitment},
        *root_pop, *device_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouStateV2 state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(25));
    BOOST_REQUIRE(ApplyAccountCreateV2(create, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);

    // Fund account balance
    state.accounts.at(account).balance = 50;

    const auto device_id = ComputeDeviceKeyId(*device);
    BOOST_REQUIRE(device_id);

    AuthorizedSystemLockV2 lock{};
    lock.lock.amount = 20;
    const auto lock_bytes = SerializeSystemLockPayloadV2(lock.lock);
    BOOST_REQUIRE(lock_bytes);
    const auto decoded_lock = DeserializeSystemLockPayloadV2(*lock_bytes);
    BOOST_REQUIRE(decoded_lock && decoded_lock->amount == 20);

    const auto lock_commitment = ComputeSystemLockPayloadCommitmentV2(lock.lock);
    BOOST_REQUIRE(lock_commitment);

    lock.authorization.account_id = account;
    lock.authorization.device_id = *device_id;
    lock.authorization.nonce = 0;
    lock.authorization.activation_nonce = 0;
    lock.authorization.kind = DeviceOperationKindV2::SYSTEM_LOCK;
    lock.authorization.payload_commitment = *lock_commitment;
    const auto lock_digest = ComputeDeviceOperationDigestV2(network_id, lock.authorization);
    BOOST_REQUIRE(lock_digest);
    lock.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *lock_digest);

    // Wire serialization
    const ProtocolOperationV2 lock_op{lock};
    const auto lock_wire = SerializeProtocolOperationV2(lock_op);
    BOOST_REQUIRE(lock_wire);
    BOOST_CHECK(lock_wire->size() == 2 + AUTHORIZED_SYSTEM_LOCK_V2_SIZE);
    const auto decoded_wire = DeserializeProtocolOperationV2(*lock_wire);
    BOOST_REQUIRE(decoded_wire && std::holds_alternative<AuthorizedSystemLockV2>(*decoded_wire));
    BOOST_CHECK(SerializeProtocolOperationV2(*decoded_wire) == lock_wire);
    BOOST_CHECK(ComputeOperationIdV2(*decoded_wire) == ComputeOperationIdV2(lock_op));

    // Damaged wires
    auto bad_lock_wire = *lock_wire;
    bad_lock_wire[0] = 0;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_lock_wire));
    bad_lock_wire = *lock_wire;
    bad_lock_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_lock_wire));
    BOOST_CHECK(!DeserializeProtocolOperationV2(std::span{*lock_wire}.first(lock_wire->size() - 1)));

    // Execute in block
    const auto block_res = ExecuteBlockOperationsV2(state, {lock_op}, network_id, 1, params);
    BOOST_REQUIRE(block_res);
    BOOST_CHECK(block_res.state->accounts.at(account).balance == 30);
    BOOST_CHECK(block_res.state->accounts.at(account).system_balance == params.onboarding_bonus + 20);
    BOOST_CHECK(block_res.state->pending_fee_pool == 0); // no fee for lock
    BOOST_CHECK(block_res.state->identities.Find(account)->devices.at(*device_id).next_nonce == 1);

    // Replay stale nonce fails
    const auto replay = ExecuteBlockOperationsV2(*block_res.state, {lock_op}, network_id, 2, params);
    BOOST_CHECK(replay.error == BlockExecutionErrorV2::INVALID_SYSTEM_LOCK);
    BOOST_CHECK(replay.lock_error == SystemLockErrorV2::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(state_validation_invariants)
{
    using namespace cybou;
    CybouStateV2 state{};
    state.validator_set.validators.push_back(MakeTestValidator(31));
    BOOST_CHECK(ValidateCybouStateV2(state) == StateValidationErrorV2::NONE);

    // Mismatched account count
    uint256 raw{};
    raw.begin()[0] = 33;
    const AccountId acc{raw};
    state.accounts.emplace(acc, AccountStateV2{.balance = 10});
    BOOST_CHECK(ValidateCybouStateV2(state) == StateValidationErrorV2::ACCOUNT_IDENTITY_COUNT_MISMATCH);

    // Balance overflow
    state.accounts.clear();
    state.onboarding_pool = 100'000'000'001ULL;
    BOOST_CHECK(ValidateCybouStateV2(state) == StateValidationErrorV2::BALANCE_OVERFLOW);
}

BOOST_AUTO_TEST_CASE(post_quantum_validator_set_and_bft_certificate)
{
    using namespace cybou;
    std::array<std::array<unsigned char, 32>, 4> seeds{};
    for (size_t i = 0; i < 4; ++i) {
        seeds[i][0] = static_cast<unsigned char>(101 + i);
    }
    std::vector<ValidatorV2> validators;
    for (size_t i = 0; i < 4; ++i) {
        validators.push_back(MakeTestValidator(static_cast<uint8_t>(101 + i)));
    }

    ValidatorSetV2 val_set{.validators = validators};
    BOOST_CHECK(val_set.version == VALIDATOR_SET_VERSION_V2);
    BOOST_CHECK_EQUAL(val_set.Size(), 4U);
    BOOST_CHECK_EQUAL(val_set.TotalWeight(), 4U);
    BOOST_CHECK_EQUAL(val_set.FaultTolerance(), 1U);
    BOOST_CHECK_EQUAL(val_set.QuorumThreshold(), 3U);
    BOOST_CHECK(val_set.Mode() == ConsensusMode::BFT);
    BOOST_CHECK(ValidateValidatorSetV2(val_set) == ValidatorSetValidationError::NONE);

    // Hardening check: duplicate ML-DSA-65 key rejection
    auto dup_ml_set = val_set;
    dup_ml_set.validators[1].consensus_public_key.ml_dsa = dup_ml_set.validators[0].consensus_public_key.ml_dsa;
    const auto dup_val_id = ComputeValidatorKeyId(dup_ml_set.validators[1].consensus_public_key);
    BOOST_REQUIRE(dup_val_id.has_value());
    std::copy_n(dup_val_id->begin(), 32, dup_ml_set.validators[1].validator_id.begin());
    BOOST_CHECK(ValidateValidatorSetV2(dup_ml_set) == ValidatorSetValidationError::DUPLICATE_CONSENSUS_KEY);

    // Serialization roundtrip
    const auto serialized = SerializeValidatorSetV2(val_set);
    BOOST_CHECK_EQUAL(serialized.size(), 5 + 4 * VALIDATOR_V2_ENTRY_SIZE);
    const auto deserialized = DeserializeValidatorSetV2(serialized);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK(*deserialized == val_set);

    const auto commitment = ComputeValidatorSetCommitmentV2(val_set);
    BOOST_CHECK(!commitment.IsNull());

    // Build BFT Finality Certificate with 3 votes (quorum threshold = 3)
    uint256 network_id{}, block_id{};
    network_id.begin()[0] = 77;
    block_id.begin()[0] = 88;
    const uint64_t height = 1000;
    const uint32_t round = 0;

    const uint256 commit_digest = ComputeBftCommitDigestV2(network_id, block_id, height, round, commitment);
    std::array<unsigned char, 32> digest_bytes{};
    std::copy_n(commit_digest.begin(), 32, digest_bytes.begin());

    BftFinalityCertificateV2 cert{};
    cert.network_id = network_id;
    cert.block_id = block_id;
    cert.height = height;
    cert.round = round;
    cert.validator_set_commitment = commitment;

    for (size_t i = 0; i < 3; ++i) {
        const auto sig = SignIdentityMessage(seeds[i], IdentityKeyPurpose::VALIDATOR, digest_bytes);
        BOOST_REQUIRE(sig.has_value());
        cert.commit_votes.push_back(BftCommitVoteV2{
            .validator_id = validators[i].validator_id,
            .signature = *sig,
        });
    }

    // Verification succeeds
    BOOST_CHECK(VerifyFinalityCertificateV2(cert, val_set, network_id) == FinalityVerificationError::NONE);

    // Serialization roundtrip
    const auto cert_bytes = SerializeFinalityCertificateV2(cert);
    BOOST_REQUIRE(cert_bytes.has_value());
    BOOST_CHECK_EQUAL(cert_bytes->size(), 113 + 3 * BFT_COMMIT_VOTE_V2_SIZE);
    const auto decoded_cert = DeserializeFinalityCertificateV2(*cert_bytes);
    BOOST_REQUIRE(decoded_cert.has_value());
    BOOST_CHECK(*decoded_cert == cert);

    // Fail-closed malformed certificate serialization
    auto bad_ml_cert = cert;
    bad_ml_cert.commit_votes[0].signature.ml_dsa.pop_back(); // 3308 != 3309
    BOOST_CHECK(!SerializeFinalityCertificateV2(bad_ml_cert).has_value());

    // Adversarial verification checks
    // 1. Wrong network
    uint256 wrong_net = network_id;
    wrong_net.begin()[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificateV2(cert, val_set, wrong_net) == FinalityVerificationError::NETWORK_MISMATCH);

    // 2. Wrong validator set commitment
    auto bad_commitment_cert = cert;
    bad_commitment_cert.validator_set_commitment.begin()[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificateV2(bad_commitment_cert, val_set, network_id) == FinalityVerificationError::VALIDATOR_SET_MISMATCH);

    // 3. Insufficient votes (2 < 3)
    auto short_cert = cert;
    short_cert.commit_votes.pop_back();
    BOOST_CHECK(VerifyFinalityCertificateV2(short_cert, val_set, network_id) == FinalityVerificationError::INSUFFICIENT_VOTES);

    // 4. Duplicate vote
    auto dup_cert = cert;
    dup_cert.commit_votes[2] = dup_cert.commit_votes[0];
    BOOST_CHECK(VerifyFinalityCertificateV2(dup_cert, val_set, network_id) == FinalityVerificationError::DUPLICATE_VOTE);

    // 5. Unknown validator
    auto unknown_cert = cert;
    unknown_cert.commit_votes[0].validator_id.begin()[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificateV2(unknown_cert, val_set, network_id) == FinalityVerificationError::UNKNOWN_VALIDATOR);

    // 6. Invalid signature
    auto bad_sig_cert = cert;
    bad_sig_cert.commit_votes[0].signature.ed25519[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificateV2(bad_sig_cert, val_set, network_id) == FinalityVerificationError::INVALID_SIGNATURE);
}

BOOST_AUTO_TEST_CASE(name_registry_validation_and_lifecycle)
{
    using namespace cybou;

    // 1. Syntax validation tests
    BOOST_CHECK(ValidateNameLabel("alice") == NameValidationError::NONE);
    BOOST_CHECK(ValidateNameLabel("stanislav") == NameValidationError::NONE);
    BOOST_CHECK(ValidateNameLabel("node-01") == NameValidationError::NONE);
    BOOST_CHECK(ValidateNameLabel("a123456789012345678901234567890b") == NameValidationError::NONE); // 32 chars
    BOOST_CHECK(ValidateNameLabel("") == NameValidationError::EMPTY);
    BOOST_CHECK(ValidateNameLabel("four") == NameValidationError::TOO_SHORT);
    BOOST_CHECK(ValidateNameLabel("a123456789012345678901234567890bc") == NameValidationError::TOO_LONG); // 33 chars
    BOOST_CHECK(ValidateNameLabel("-alice") == NameValidationError::INVALID_START_END);
    BOOST_CHECK(ValidateNameLabel("alice-") == NameValidationError::INVALID_START_END);
    BOOST_CHECK(ValidateNameLabel("al--ce") == NameValidationError::CONSECUTIVE_HYPHENS);
    BOOST_CHECK(ValidateNameLabel("Alice") == NameValidationError::INVALID_CHARACTER);
    BOOST_CHECK(ValidateNameLabel("ali ce") == NameValidationError::INVALID_CHARACTER);
    BOOST_CHECK(ValidateNameLabel("ali_ce") == NameValidationError::INVALID_CHARACTER);
    BOOST_CHECK(ValidateNameLabel("xn--alice") == NameValidationError::IDN_PREFIX);
    BOOST_CHECK(ValidateNameLabel("12345") == NameValidationError::ALL_DIGITS);
    BOOST_CHECK(ValidateNameLabel("cybou") == NameValidationError::RESERVED_NAME);
    BOOST_CHECK(ValidateNameLabel("admin") == NameValidationError::RESERVED_NAME);
    BOOST_CHECK(ValidateNameLabel("system") == NameValidationError::RESERVED_NAME);
    BOOST_CHECK(ValidateNameLabel("operator") == NameValidationError::RESERVED_NAME);
    BOOST_CHECK(ValidateNameLabel("validator") == NameValidationError::RESERVED_NAME);

    // 2. Setup state with an account
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 51;
    device_seed[0] = 52;
    uint256 raw_account{}, network_id{};
    raw_account.begin()[0] = 53;
    network_id.begin()[0] = 54;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorizationV2 auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitmentV2(auth);
    const auto pop_digest = ComputeAccountCreatePopDigestV2(network_id, account, auth);
    BOOST_REQUIRE(commitment && pop_digest);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *pop_digest);
    const auto device_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *pop_digest);
    BOOST_REQUIRE(root_pop && device_pop);
    const AccountCreateOpV2 create{account, auth,
        {.network_id = network_id, .account_id = account, .authorization_commitment = *commitment},
        *root_pop, *device_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    params.name_claim_work_bits = 0;
    params.name_commit_min_depth = 1;
    params.name_commit_max_lifetime = 100;
    CybouStateV2 state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(55));
    BOOST_REQUIRE(ApplyAccountCreateV2(create, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);

    const auto device_id = ComputeDeviceKeyId(*device);
    BOOST_REQUIRE(device_id.has_value());

    // 3. NameCommit
    const std::string label = "stanislav";
    std::array<unsigned char, 32> salt{};
    salt[0] = 77;
    const uint256 name_commit_hash = ComputeNameCommitment(network_id, account, label, salt);

    AuthorizedNameCommit commit_op{};
    commit_op.commit.commitment = name_commit_hash;
    const auto commit_payload_bytes = SerializeNameCommitPayload(commit_op.commit);
    BOOST_REQUIRE(commit_payload_bytes.has_value());
    const auto commit_payload_commitment = ComputeNameCommitPayloadCommitment(commit_op.commit);
    BOOST_REQUIRE(commit_payload_commitment.has_value());

    commit_op.authorization.account_id = account;
    commit_op.authorization.device_id = *device_id;
    commit_op.authorization.nonce = 0;
    commit_op.authorization.activation_nonce = 0;
    commit_op.authorization.kind = DeviceOperationKindV2::NAME_COMMIT;
    commit_op.authorization.payload_commitment = *commit_payload_commitment;
    const auto commit_digest = ComputeDeviceOperationDigestV2(network_id, commit_op.authorization);
    BOOST_REQUIRE(commit_digest.has_value());
    commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *commit_digest);

    // Wire serialization check
    const ProtocolOperationV2 commit_proto_op{commit_op};
    const auto commit_wire = SerializeProtocolOperationV2(commit_proto_op);
    BOOST_REQUIRE(commit_wire.has_value());
    BOOST_CHECK_EQUAL(commit_wire->size(), 2 + AUTHORIZED_NAME_COMMIT_V2_SIZE);
    const auto decoded_commit_wire = DeserializeProtocolOperationV2(*commit_wire);
    BOOST_REQUIRE(decoded_commit_wire.has_value());
    BOOST_CHECK(std::holds_alternative<AuthorizedNameCommit>(*decoded_commit_wire));
    BOOST_CHECK(ComputeOperationIdV2(*decoded_commit_wire) == ComputeOperationIdV2(commit_proto_op));

    // Execute NameCommit at height 10
    const auto commit_block = ExecuteBlockOperationsV2(state, {commit_proto_op}, network_id, 10, params);
    BOOST_REQUIRE(commit_block);
    BOOST_REQUIRE(commit_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_CHECK_EQUAL(commit_block.state->names.pending_commits.at(name_commit_hash).commit_height, 10ULL);
    BOOST_CHECK_EQUAL(commit_block.state->identities.Find(account)->devices.at(*device_id).next_nonce, 1ULL);

    // 4. NameReveal
    AuthorizedNameReveal reveal_op{};
    reveal_op.reveal.label = label;
    reveal_op.reveal.salt = salt;
    reveal_op.reveal.work.network_id = network_id;
    reveal_op.reveal.work.account_id = account;
    reveal_op.reveal.work.commitment = name_commit_hash;
    reveal_op.reveal.work.work_epoch = EpochForHeight(12, params);
    reveal_op.reveal.work.nonce = 0;

    const auto reveal_payload_bytes = SerializeNameRevealPayload(reveal_op.reveal);
    BOOST_REQUIRE(reveal_payload_bytes.has_value());
    const auto reveal_payload_commitment = ComputeNameRevealPayloadCommitment(reveal_op.reveal);
    BOOST_REQUIRE(reveal_payload_commitment.has_value());

    reveal_op.authorization.account_id = account;
    reveal_op.authorization.device_id = *device_id;
    reveal_op.authorization.nonce = 1;
    reveal_op.authorization.activation_nonce = 0;
    reveal_op.authorization.kind = DeviceOperationKindV2::NAME_REVEAL;
    reveal_op.authorization.payload_commitment = *reveal_payload_commitment;
    const auto reveal_digest = ComputeDeviceOperationDigestV2(network_id, reveal_op.authorization);
    BOOST_REQUIRE(reveal_digest.has_value());
    reveal_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *reveal_digest);

    // Wire serialization check
    const ProtocolOperationV2 reveal_proto_op{reveal_op};
    const auto reveal_wire = SerializeProtocolOperationV2(reveal_proto_op);
    BOOST_REQUIRE(reveal_wire.has_value());
    BOOST_CHECK_EQUAL(reveal_wire->size(), 2 + AUTHORIZED_NAME_REVEAL_V2_SIZE);
    const auto decoded_reveal_wire = DeserializeProtocolOperationV2(*reveal_wire);
    BOOST_REQUIRE(decoded_reveal_wire.has_value());
    BOOST_CHECK(std::holds_alternative<AuthorizedNameReveal>(*decoded_reveal_wire));
    BOOST_CHECK(ComputeOperationIdV2(*decoded_reveal_wire) == ComputeOperationIdV2(reveal_proto_op));

    // Adversarial: Premature reveal at height 10 (needs min depth 1 -> height >= 11)
    const auto premature_res = ExecuteBlockOperationsV2(*commit_block.state, {reveal_proto_op}, network_id, 10, params);
    BOOST_CHECK(premature_res.error == BlockExecutionErrorV2::INVALID_NAME_REVEAL);
    BOOST_CHECK(premature_res.name_reveal_error == NameRevealError::INSUFFICIENT_COMMIT_DEPTH);

    // Successful reveal at height 12
    const auto reveal_block = ExecuteBlockOperationsV2(*commit_block.state, {reveal_proto_op}, network_id, 12, params);
    BOOST_REQUIRE(reveal_block);
    BOOST_CHECK(!reveal_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_REQUIRE(reveal_block.state->names.Resolve(label) != nullptr);
    BOOST_CHECK(*reveal_block.state->names.Resolve(label) == account);
    BOOST_REQUIRE(reveal_block.state->names.PrimaryName(account) != nullptr);
    BOOST_CHECK(*reveal_block.state->names.PrimaryName(account) == label);
    BOOST_CHECK_EQUAL(reveal_block.state->identities.Find(account)->devices.at(*device_id).next_nonce, 2ULL);

    // State serialization roundtrip with names
    const auto state_bytes = SerializeCybouStateV2(*reveal_block.state);
    BOOST_REQUIRE(state_bytes.has_value());
    const auto restored_state = DeserializeCybouStateV2(*state_bytes);
    BOOST_REQUIRE(restored_state.has_value());
    BOOST_CHECK(*restored_state->names.Resolve(label) == account);
    BOOST_CHECK(*restored_state->names.PrimaryName(account) == label);
    BOOST_CHECK(SerializeCybouStateV2(*restored_state) == state_bytes);

    // 5. Adversarial checks
    // A0. Account already has a pending commitment -> cannot commit another
    auto second_commit_op = commit_op;
    second_commit_op.authorization.nonce = 1;
    const auto sec_digest0 = ComputeDeviceOperationDigestV2(network_id, second_commit_op.authorization);
    BOOST_REQUIRE(sec_digest0.has_value());
    second_commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *sec_digest0);
    const auto pending_commit_res = ExecuteBlockOperationsV2(*commit_block.state, {second_commit_op}, network_id, 11, params);
    BOOST_CHECK(pending_commit_res.error == BlockExecutionErrorV2::INVALID_NAME_COMMIT);
    BOOST_CHECK(pending_commit_res.name_commit_error == NameCommitError::ACCOUNT_HAS_PENDING_COMMIT);

    // A. Account already has a name -> cannot commit another name
    second_commit_op.authorization.nonce = 2;
    const auto sec_digest = ComputeDeviceOperationDigestV2(network_id, second_commit_op.authorization);
    BOOST_REQUIRE(sec_digest.has_value());
    second_commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::DEVICE, *sec_digest);
    const auto double_commit_res = ExecuteBlockOperationsV2(*reveal_block.state, {second_commit_op}, network_id, 13, params);
    BOOST_CHECK(double_commit_res.error == BlockExecutionErrorV2::INVALID_NAME_COMMIT);
    BOOST_CHECK(double_commit_res.name_commit_error == NameCommitError::ACCOUNT_ALREADY_HAS_NAME);

    // B. Expired commit & deterministic block pruning
    params.name_commit_max_lifetime = 5;
    // Block at height 16 automatically prunes the expired commit from height 10 (16 > 10 + 5)
    const auto prune_block = ExecuteBlockOperationsV2(*commit_block.state, {}, network_id, 16, params);
    BOOST_REQUIRE(prune_block);
    BOOST_CHECK(!prune_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_CHECK(prune_block.state->names.pending_commits.empty());

    // Once pruned, the commitment is no longer found for reveal
    const auto expired_res = ExecuteBlockOperationsV2(*commit_block.state, {reveal_proto_op}, network_id, 16, params);
    BOOST_CHECK(expired_res.error == BlockExecutionErrorV2::INVALID_NAME_REVEAL);
    BOOST_CHECK(expired_res.name_reveal_error == NameRevealError::COMMITMENT_NOT_FOUND);

    // Direct ApplyNameReveal check for COMMIT_EXPIRED on unpruned state
    auto unpruned_state = *commit_block.state;
    BOOST_CHECK(ApplyNameReveal(reveal_op, network_id, 16, params, unpruned_state) == NameRevealError::COMMIT_EXPIRED);
    params.name_commit_max_lifetime = 100;

    // C. Tampered commit wire
    auto bad_commit_wire = *commit_wire;
    bad_commit_wire[0] = 0;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_commit_wire));
    bad_commit_wire = *commit_wire;
    bad_commit_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_commit_wire));
    BOOST_CHECK(!DeserializeProtocolOperationV2(std::span{*commit_wire}.first(commit_wire->size() - 1)));

    // D. Tampered reveal wire
    auto bad_reveal_wire = *reveal_wire;
    bad_reveal_wire[0] = 0;
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_reveal_wire));
    bad_reveal_wire = *reveal_wire;
    bad_reveal_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperationV2(bad_reveal_wire));
    BOOST_CHECK(!DeserializeProtocolOperationV2(std::span{*reveal_wire}.first(reveal_wire->size() - 1)));

    // E. State corruption: name references account not in monetary state
    auto corrupted_state = *reveal_block.state;
    corrupted_state.accounts.clear();
    BOOST_CHECK(ValidateCybouStateV2(corrupted_state) == StateValidationErrorV2::ACCOUNT_IDENTITY_COUNT_MISMATCH);
}

BOOST_AUTO_TEST_CASE(mail_tx_execution_and_quotas)
{
    using namespace cybou;

    // 1. Fee calculation logic
    auto params = DevProtocolParameters();
    BOOST_CHECK_EQUAL(params.MailFeeForSize(100), 5ULL);     // 4 + 1
    BOOST_CHECK_EQUAL(params.MailFeeForSize(1024), 5ULL);    // 4 + 1
    BOOST_CHECK_EQUAL(params.MailFeeForSize(1025), 6ULL);    // 4 + 2
    BOOST_CHECK_EQUAL(params.MailFeeForSize(2048), 6ULL);    // 4 + 2
    BOOST_CHECK_EQUAL(params.MailFeeForSize(64 * 1024), 68ULL); // 4 + 64

    // 2. Setup state with Alice (sender) and Bob (recipient)
    std::array<unsigned char, 32> alice_root_seed{}, alice_dev_seed{};
    alice_root_seed[0] = 61;
    alice_dev_seed[0] = 62;
    uint256 alice_raw{}, bob_raw{}, network_id{};
    alice_raw.begin()[0] = 63;
    bob_raw.begin()[0] = 64;
    network_id.begin()[0] = 65;
    const AccountId alice{alice_raw};
    const AccountId bob{bob_raw};

    std::array<unsigned char, 32> bob_root_seed{}, bob_dev_seed{};
    bob_root_seed[0] = 71;
    bob_dev_seed[0] = 72;

    const auto alice_root = DeriveIdentityPublicKey(alice_root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto alice_dev = DeriveIdentityPublicKey(alice_dev_seed, IdentityKeyPurpose::DEVICE);
    const auto bob_root = DeriveIdentityPublicKey(bob_root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto bob_dev = DeriveIdentityPublicKey(bob_dev_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(alice_root && alice_dev && bob_root && bob_dev);

    const auto alice_dev_id = ComputeDeviceKeyId(*alice_dev);
    const auto bob_dev_id = ComputeDeviceKeyId(*bob_dev);
    BOOST_REQUIRE(alice_dev_id && bob_dev_id);

    const IdentityAuthorizationV2 alice_auth{*alice_root, *alice_dev};
    const auto alice_commit = ComputeIdentityAuthorizationCommitmentV2(alice_auth);
    const auto alice_pop = ComputeAccountCreatePopDigestV2(network_id, alice, alice_auth);
    const AccountCreateOpV2 create_alice{alice, alice_auth,
        {.network_id = network_id, .account_id = alice, .authorization_commitment = *alice_commit},
        *SignIdentityMessage(alice_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *alice_pop),
        *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *alice_pop)};

    const IdentityAuthorizationV2 bob_auth{*bob_root, *bob_dev};
    const auto bob_commit = ComputeIdentityAuthorizationCommitmentV2(bob_auth);
    const auto bob_pop = ComputeAccountCreatePopDigestV2(network_id, bob, bob_auth);
    const AccountCreateOpV2 create_bob{bob, bob_auth,
        {.network_id = network_id, .account_id = bob, .authorization_commitment = *bob_commit},
        *SignIdentityMessage(bob_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *bob_pop),
        *SignIdentityMessage(bob_dev_seed, IdentityKeyPurpose::DEVICE, *bob_pop)};

    params.account_creation_work_bits = 0;
    CybouStateV2 state{};
    state.onboarding_pool = params.onboarding_bonus * 10;
    state.validator_set.validators.push_back(MakeTestValidator(88));
    BOOST_REQUIRE(ApplyAccountCreateV2(create_alice, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);
    BOOST_REQUIRE(ApplyAccountCreateV2(create_bob, network_id, 0, params, state) == AccountCreateStateErrorV2::NONE);

    // Initial state check
    BOOST_CHECK_EQUAL(state.accounts.at(alice).system_balance, params.onboarding_bonus);
    BOOST_CHECK_EQUAL(state.accounts.at(alice).mail_count_in_epoch, 0U);

    // 3. Create MailTx from Alice to Bob
    MailPayload payload{};
    payload.recipient = bob;
    payload.discovery_tag.begin()[0] = 99;
    payload.content_commitment.begin()[0] = 100;
    payload.ciphertext = std::vector<unsigned char>(300, 0xAA); // 300 bytes ciphertext -> tier 1 -> fee = 5

    const auto payload_commitment = ComputeMailPayloadCommitment(payload);
    BOOST_REQUIRE(payload_commitment.has_value());

    AuthorizedMail mail_op{};
    mail_op.mail = payload;
    mail_op.authorization.account_id = alice;
    mail_op.authorization.device_id = *alice_dev_id;
    mail_op.authorization.nonce = 0;
    mail_op.authorization.activation_nonce = 0;
    mail_op.authorization.kind = DeviceOperationKindV2::MAIL;
    mail_op.authorization.payload_commitment = *payload_commitment;
    const auto op_digest = ComputeDeviceOperationDigestV2(network_id, mail_op.authorization);
    BOOST_REQUIRE(op_digest.has_value());
    mail_op.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *op_digest);

    // Wire serialization roundtrip
    const ProtocolOperationV2 proto_op{mail_op};
    const auto wire_bytes = SerializeProtocolOperationV2(proto_op);
    BOOST_REQUIRE(wire_bytes.has_value());
    BOOST_CHECK_EQUAL(wire_bytes->size(), 2 + 2597 + MAIL_PAYLOAD_HEADER_SIZE + payload.ciphertext.size());

    const auto decoded_proto_op = DeserializeProtocolOperationV2(*wire_bytes);
    BOOST_REQUIRE(decoded_proto_op.has_value());
    BOOST_CHECK(std::holds_alternative<AuthorizedMail>(*decoded_proto_op));
    BOOST_CHECK(ComputeOperationIdV2(*decoded_proto_op) == ComputeOperationIdV2(proto_op));

    // 4. Execute mail at block height 10 (epoch 0)
    const auto block_res = ExecuteBlockOperationsV2(state, {proto_op}, network_id, 10, params);
    BOOST_REQUIRE(block_res);
    const auto& post_state = *block_res.state;

    // Check balance deduction and counter updates
    const uint64_t expected_fee = params.MailFeeForSize(payload.ciphertext.size()); // 5
    BOOST_CHECK_EQUAL(expected_fee, 5ULL);
    BOOST_CHECK_EQUAL(post_state.accounts.at(alice).system_balance, params.onboarding_bonus - expected_fee);
    BOOST_CHECK_EQUAL(post_state.accounts.at(alice).last_mail_epoch, 0ULL);
    BOOST_CHECK_EQUAL(post_state.accounts.at(alice).mail_count_in_epoch, 1U);
    BOOST_CHECK_EQUAL(post_state.identities.Find(alice)->devices.at(*alice_dev_id).next_nonce, 1ULL);

    // Fee routing check (4 fees -> 3 security + 1 onboarding)
    // Fee = 5: chunks = 5/4 = 1. security += 3, onboarding += 1, pending %= 4 -> 1 remainder
    BOOST_CHECK_EQUAL(post_state.pending_fee_pool, 1ULL);
    BOOST_CHECK_EQUAL(post_state.security_reward_pool, 3ULL);
    BOOST_CHECK_EQUAL(post_state.onboarding_pool, state.onboarding_pool + 1);

    // State serialization roundtrip: no mail body stored in state
    const auto state_serialized = SerializeCybouStateV2(post_state);
    BOOST_REQUIRE(state_serialized.has_value());
    const auto state_deserialized = DeserializeCybouStateV2(*state_serialized);
    BOOST_REQUIRE(state_deserialized.has_value());
    BOOST_CHECK(SerializeCybouStateV2(*state_deserialized) == state_serialized);
    BOOST_CHECK(state_deserialized->accounts.at(alice) == post_state.accounts.at(alice));
    BOOST_CHECK(state_deserialized->accounts.at(bob) == post_state.accounts.at(bob));

    // 5. Test quota enforcement: send 24 more mails in epoch 0 to reach 25
    auto current_state = post_state;
    uint64_t nonce = 1;
    for (uint32_t i = 2; i <= 25; ++i) {
        AuthorizedMail next_mail = mail_op;
        next_mail.authorization.nonce = nonce++;
        const auto dig = ComputeDeviceOperationDigestV2(network_id, next_mail.authorization);
        BOOST_REQUIRE(dig.has_value());
        next_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *dig);
        const auto res = ExecuteBlockOperationsV2(current_state, {ProtocolOperationV2{next_mail}}, network_id, 10 + i, params);
        BOOST_REQUIRE(res);
        current_state = *res.state;
        BOOST_CHECK_EQUAL(current_state.accounts.at(alice).mail_count_in_epoch, i);
    }
    BOOST_CHECK_EQUAL(current_state.accounts.at(alice).mail_count_in_epoch, 25U);

    // 26th mail in epoch 0: must fail with MAIL_QUOTA_EXCEEDED
    AuthorizedMail quota_exceed_mail = mail_op;
    quota_exceed_mail.authorization.nonce = nonce++;
    const auto dig_exceed = ComputeDeviceOperationDigestV2(network_id, quota_exceed_mail.authorization);
    BOOST_REQUIRE(dig_exceed.has_value());
    quota_exceed_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *dig_exceed);
    const auto quota_fail_res = ExecuteBlockOperationsV2(current_state, {ProtocolOperationV2{quota_exceed_mail}}, network_id, 50, params);
    BOOST_CHECK(quota_fail_res.error == BlockExecutionErrorV2::INVALID_MAIL);
    BOOST_CHECK(quota_fail_res.mail_error == MailError::MAIL_QUOTA_EXCEEDED);

    // 6. Reset in epoch 1: block height 1024 -> epoch 1
    const uint64_t epoch1_height = params.epoch_blocks;
    BOOST_CHECK_EQUAL(EpochForHeight(epoch1_height, params), 1ULL);
    // Reuse the same mail op with correct nonce
    quota_exceed_mail.authorization.nonce = nonce - 1; // nonce was not consumed by failed op
    const auto dig_epoch1 = ComputeDeviceOperationDigestV2(network_id, quota_exceed_mail.authorization);
    BOOST_REQUIRE(dig_epoch1.has_value());
    quota_exceed_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *dig_epoch1);
    const auto epoch1_res = ExecuteBlockOperationsV2(current_state, {ProtocolOperationV2{quota_exceed_mail}}, network_id, epoch1_height, params);
    BOOST_REQUIRE(epoch1_res);
    BOOST_CHECK_EQUAL(epoch1_res.state->accounts.at(alice).last_mail_epoch, 1ULL);
    BOOST_CHECK_EQUAL(epoch1_res.state->accounts.at(alice).mail_count_in_epoch, 1U);

    // 7. Adversarial checks
    // A. Recipient not found
    AuthorizedMail bad_recipient_mail = mail_op;
    uint256 unknown_raw{};
    unknown_raw.begin()[0] = 0xFE;
    bad_recipient_mail.mail.recipient = AccountId{unknown_raw};
    const auto bad_recip_commit = ComputeMailPayloadCommitment(bad_recipient_mail.mail);
    BOOST_REQUIRE(bad_recip_commit.has_value());
    bad_recipient_mail.authorization.payload_commitment = *bad_recip_commit;
    bad_recipient_mail.authorization.nonce = nonce;
    const auto dig_bad_recip = ComputeDeviceOperationDigestV2(network_id, bad_recipient_mail.authorization);
    BOOST_REQUIRE(dig_bad_recip.has_value());
    bad_recipient_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *dig_bad_recip);
    const auto bad_recip_res = ExecuteBlockOperationsV2(*epoch1_res.state, {ProtocolOperationV2{bad_recipient_mail}}, network_id, epoch1_height + 1, params);
    BOOST_CHECK(bad_recip_res.error == BlockExecutionErrorV2::INVALID_MAIL);
    BOOST_CHECK(bad_recip_res.mail_error == MailError::RECIPIENT_NOT_FOUND);

    // B. Insufficient system balance
    auto poor_state = *epoch1_res.state;
    poor_state.accounts.at(alice).system_balance = 0;
    AuthorizedMail poor_mail = mail_op;
    poor_mail.authorization.nonce = nonce;
    const auto dig_poor = ComputeDeviceOperationDigestV2(network_id, poor_mail.authorization);
    BOOST_REQUIRE(dig_poor.has_value());
    poor_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *dig_poor);
    const auto poor_res = ExecuteBlockOperationsV2(poor_state, {ProtocolOperationV2{poor_mail}}, network_id, epoch1_height + 1, params);
    BOOST_CHECK(poor_res.error == BlockExecutionErrorV2::INVALID_MAIL);
    BOOST_CHECK(poor_res.mail_error == MailError::INSUFFICIENT_SYSTEM_BALANCE);

    // C. Ciphertext size limits: wire limit vs consensus execution limit
    MailPayload wire_oversized_payload = payload;
    wire_oversized_payload.ciphertext = std::vector<unsigned char>(MAX_MAIL_WIRE_CIPHERTEXT_SIZE + 1, 0xFF);
    BOOST_CHECK(!SerializeMailPayload(wire_oversized_payload));
    MailPayload empty_payload = payload;
    empty_payload.ciphertext.clear();
    BOOST_CHECK(!SerializeMailPayload(empty_payload));

    // Consensus parameter execution check: payload within wire framing (e.g. 65 KiB)
    // but exceeding params.max_mail_ciphertext_size (64 KiB) is rejected by consensus
    MailPayload param_oversized_payload = payload;
    param_oversized_payload.ciphertext = std::vector<unsigned char>(params.max_mail_ciphertext_size + 1, 0xEE);
    const auto param_commit = ComputeMailPayloadCommitment(param_oversized_payload);
    BOOST_REQUIRE(param_commit.has_value());
    AuthorizedMail param_oversized_mail = mail_op;
    param_oversized_mail.mail = param_oversized_payload;
    param_oversized_mail.authorization.payload_commitment = *param_commit;
    param_oversized_mail.authorization.nonce = nonce;
    const auto dig_oversized = ComputeDeviceOperationDigestV2(network_id, param_oversized_mail.authorization);
    BOOST_REQUIRE(dig_oversized.has_value());
    param_oversized_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::DEVICE, *dig_oversized);
    const auto param_res = ExecuteBlockOperationsV2(*epoch1_res.state, {ProtocolOperationV2{param_oversized_mail}}, network_id, epoch1_height + 1, params);
    BOOST_CHECK(param_res.error == BlockExecutionErrorV2::INVALID_MAIL);
    BOOST_CHECK(param_res.mail_error == MailError::INVALID_PAYLOAD);
}

BOOST_AUTO_TEST_CASE(unversioned_canonical_api_workflow)
{
    using namespace cybou;

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;

    // Build unversioned CybouState
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus * 5;
    state.validator_set.validators.push_back(MakeTestValidator(111));

    uint256 network_id{};
    network_id.begin()[0] = 0xAA;
    uint256 acc_raw{};
    acc_raw.begin()[0] = 0xBB;
    const AccountId account{acc_raw};

    std::array<unsigned char, 32> root_seed{}, dev_seed{};
    root_seed[0] = 1;
    dev_seed[0] = 2;
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(dev_seed, IdentityKeyPurpose::DEVICE);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorization auth{*root, *device};
    const auto commitment = ComputeIdentityAuthorizationCommitment(auth);
    BOOST_REQUIRE(commitment.has_value());
    const auto pop_digest = ComputeAccountCreatePopDigest(network_id, account, auth);
    BOOST_REQUIRE(pop_digest.has_value());

    const AccountCreateOp create_op{
        .account_id = account,
        .authorization = auth,
        .work = {
            .network_id = network_id,
            .account_id = account,
            .authorization_commitment = *commitment,
            .work_epoch = 0,
            .nonce = 0,
        },
        .recovery_pop = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *pop_digest),
        .device_pop = *SignIdentityMessage(dev_seed, IdentityKeyPurpose::DEVICE, *pop_digest),
    };

    // Apply unversioned AccountCreate
    BOOST_CHECK(ApplyAccountCreate(create_op, network_id, 0, params, state) == AccountCreateStateError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(account).system_balance, params.onboarding_bonus);

    // Unversioned state validation & hashing
    BOOST_CHECK(ValidateCybouState(state) == StateValidationError::NONE);
    const auto state_hash = CybouStateHash(state);
    BOOST_REQUIRE(state_hash.has_value());
    BOOST_CHECK(!state_hash->IsNull());

    // Unversioned state serialization & deserialization
    const auto state_bytes = SerializeCybouState(state);
    BOOST_REQUIRE(state_bytes.has_value());
    const auto restored_state = DeserializeCybouState(*state_bytes);
    BOOST_REQUIRE(restored_state.has_value());
    BOOST_CHECK(SerializeCybouState(*restored_state) == state_bytes);

    // Execute via unversioned BlockExecutor
    const ProtocolOperation proto_create{create_op};
    const auto proto_wire = SerializeProtocolOperation(proto_create);
    BOOST_REQUIRE(proto_wire.has_value());
    const auto decoded_proto = DeserializeProtocolOperation(*proto_wire);
    BOOST_REQUIRE(decoded_proto.has_value());
    const auto op_id = ComputeOperationId(*decoded_proto);
    BOOST_REQUIRE(op_id.has_value());

    // Execute block with parent state
    CybouState parent{};
    parent.onboarding_pool = params.onboarding_bonus * 5;
    parent.validator_set.validators.push_back(MakeTestValidator(111));
    const auto block_res = ExecuteBlockOperations(parent, {*decoded_proto}, network_id, 0, params);
    BOOST_REQUIRE(block_res);
    BOOST_CHECK_EQUAL(block_res.state->accounts.at(account).system_balance, params.onboarding_bonus);
    BOOST_CHECK(*block_res.state_root == *state_hash);
    BOOST_CHECK_EQUAL(TotalSupply(*block_res.state), TotalSupply(parent));
}

BOOST_AUTO_TEST_CASE(supply_conservation_invariant_check)
{
    using namespace cybou;

    CybouState state{};
    state.onboarding_pool = 1'000'000;
    state.security_reward_pool = 2'000'000;
    state.pending_fee_pool = 500;

    uint256 acc_raw{};
    acc_raw.begin()[0] = 0x11;
    const AccountId acc{acc_raw};
    state.accounts.emplace(acc, AccountState{
        .balance = 3'000'000,
        .system_balance = 500'000,
    });

    // Total supply calculation
    BOOST_CHECK_EQUAL(TotalSupply(state), 1'000'000ULL + 2'000'000ULL + 500ULL + 3'000'000ULL + 500'000ULL);

    // Over-supply check (with valid zero-account state and valid validator set)
    CybouState overflow_state{};
    overflow_state.validator_set.validators.push_back(MakeTestValidator(1));
    overflow_state.onboarding_pool = 100'000'000'001ULL;
    BOOST_CHECK(ValidateCybouState(overflow_state) == StateValidationError::BALANCE_OVERFLOW);
}

BOOST_AUTO_TEST_SUITE_END()
