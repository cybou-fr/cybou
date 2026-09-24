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
    BOOST_CHECK_EQUAL(cert_bytes.size(), 113 + 3 * BFT_COMMIT_VOTE_V2_SIZE);
    const auto decoded_cert = DeserializeFinalityCertificateV2(cert_bytes);
    BOOST_REQUIRE(decoded_cert.has_value());
    BOOST_CHECK(*decoded_cert == cert);

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

BOOST_AUTO_TEST_SUITE_END()
