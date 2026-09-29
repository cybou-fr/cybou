// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>
#include <cybou/block_executor.h>
#include <cybou/network_definition.h>
#include <test/cybou_test_helpers.h>
#include "cybou_test_identity_helpers.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>

namespace {
cybou::Validator MakeTestValidator(uint8_t seed_byte)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    const auto pub = cybou::DeriveIdentityPublicKey(seed, cybou::IdentityKeyPurpose::VALIDATOR);
    assert(pub.has_value());
    const auto id = cybou::ComputeValidatorKeyId(*pub);
    assert(id.has_value());
    uint256 val_id;
    std::copy_n(id->begin(), 32, val_id.begin());
    return cybou::Validator{val_id, *pub, 1};
}
} // namespace

BOOST_AUTO_TEST_SUITE(cybou_state_tests)

BOOST_AUTO_TEST_CASE(network_id_commits_to_name_rules)
{
    using namespace cybou;
    std::array<unsigned char, 32> seed{};
    seed[0] = 0x51;
    const auto validator = GenerateValidatorKeyPair(seed);
    BOOST_REQUIRE(validator);
    const auto definition = CreateDevNetworkDefinition(
        CreateDevGenesisState(validator->public_key), cybou::TestPoaFinalizerPublicKey());
    BOOST_CHECK(ValidateNetworkDefinition(definition) == NetworkDefinitionError::NONE);
    const auto encoded = SerializeNetworkDefinition(definition);
    const auto decoded = DeserializeNetworkDefinition(encoded);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(SerializeNetworkDefinition(*decoded) == encoded);

    auto changed = definition;
    ++changed.protocol_parameters.name_claim_work_bits;
    BOOST_CHECK(NetworkId(changed) != NetworkId(definition));
    changed = definition;
    ++changed.protocol_parameters.name_commit_min_depth;
    BOOST_CHECK(NetworkId(changed) != NetworkId(definition));
    changed = definition;
    ++changed.protocol_parameters.name_commit_max_lifetime;
    BOOST_CHECK(NetworkId(changed) != NetworkId(definition));
    changed = definition;
    --changed.protocol_parameters.max_pending_name_commits;
    BOOST_CHECK(NetworkId(changed) != NetworkId(definition));
}

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
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);
    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_id = network_id, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(5));
    auto damaged_create = create;
    damaged_create.authorization_pop.ed25519[0] ^= 1;
    BOOST_CHECK(ApplyAccountCreate(damaged_create, network_id, 0, params, state) == AccountCreateStateError::INVALID_CREATE);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
    BOOST_CHECK(state.onboarding_pool == params.onboarding_bonus);
    BOOST_CHECK(ApplyAccountCreate(create, network_id, 0, params, state) == AccountCreateStateError::NONE);
    BOOST_CHECK(state.onboarding_pool == 0);
    BOOST_CHECK(state.accounts.at(account).system_balance == params.onboarding_bonus);
    BOOST_CHECK(state.accounts.at(account).balance == 0);
    BOOST_CHECK(state.identities.Find(account) != nullptr);
    const auto bytes = SerializeCybouState(state);
    const auto hash = CybouStateHash(state);
    BOOST_REQUIRE(bytes && hash);
    const auto restored = DeserializeCybouState(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeCybouState(*restored) == bytes);
    BOOST_CHECK(CybouStateHash(*restored) == hash);

    BOOST_CHECK(ApplyAccountCreate(create, network_id, 1, params, state) == AccountCreateStateError::ACCOUNT_EXISTS);
    BOOST_CHECK(SerializeCybouState(state) == bytes);
    auto damaged = *bytes;
    damaged[0] = 1;
    BOOST_CHECK(!DeserializeCybouState(damaged));
    damaged = *bytes;
    damaged.push_back(0);
    BOOST_CHECK(!DeserializeCybouState(damaged));
    BOOST_CHECK(!DeserializeCybouState(std::span{*bytes}.first(bytes->size() - 1)));
    damaged = *bytes;
    damaged[1 + 8 * 3 + 4] ^= 1; // monetary AccountID no longer matches identity registry
    BOOST_CHECK(!DeserializeCybouState(damaged));

    std::array<unsigned char, 32> other_root_seed{}, other_device_seed{};
    other_root_seed[0] = 7;
    other_device_seed[0] = 8;
    uint256 raw_other{};
    raw_other.begin()[0] = 9;
    const AccountId other_account{raw_other};
    const auto other_root = DeriveIdentityPublicKey(other_root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto other_device = DeriveIdentityPublicKey(other_device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(other_root && other_device);
    const IdentityAuthorization other_auth{*other_root, *other_device};
    const auto other_binding = test::MakeIdentityKemBinding(network_id, other_account, other_auth);
    const auto other_root_pop = SignIdentityMessage(other_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, other_binding.pop_digest);
    const auto other_authorization_pop = SignIdentityMessage(other_device_seed, IdentityKeyPurpose::AUTHORIZATION, other_binding.pop_digest);
    BOOST_REQUIRE(other_root_pop && other_authorization_pop);
    const AccountCreateOp other_create{other_account, other_auth, other_binding.package,
        {.network_id = network_id, .account_id = other_account, .authorization_commitment = other_binding.authorization_commitment},
        *other_root_pop, *other_authorization_pop};
    state.onboarding_pool = params.onboarding_bonus;
    BOOST_REQUIRE(ApplyAccountCreate(other_create, network_id, 1, params, state) == AccountCreateStateError::NONE);
    state.accounts.at(account).balance = 10; // funded fixture; no mint operation in this test
    AuthorizedPayment payment{};
    payment.payment = PaymentPayload{other_account, 3};
    const auto payment_bytes = SerializePaymentPayload(payment.payment);
    BOOST_REQUIRE(payment_bytes);
    const auto decoded_payment = DeserializePaymentPayload(*payment_bytes);
    BOOST_REQUIRE(decoded_payment);
    BOOST_CHECK(decoded_payment->recipient == other_account);
    BOOST_CHECK(decoded_payment->amount == 3);
    payment.authorization.account_id = account;
    payment.authorization.kind = IdentityOperationKind::PAYMENT;
    const auto payment_commitment = ComputePaymentPayloadCommitment(payment.payment);
    BOOST_REQUIRE(payment_commitment);
    payment.authorization.payload_commitment = *payment_commitment;
    const auto payment_digest = ComputeIdentityOperationDigest(network_id, payment.authorization);
    BOOST_REQUIRE(payment_digest);
    const auto payment_signature = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *payment_digest);
    BOOST_REQUIRE(payment_signature);
    payment.authorization.signature = *payment_signature;
    auto tampered_payment = payment;
    tampered_payment.payment.amount = 4;
    BOOST_CHECK(ApplyPayment(tampered_payment, network_id, params, state) == PaymentError::INVALID_PAYLOAD);
    BOOST_CHECK(state.accounts.at(account).balance == 10);
    BOOST_CHECK(ApplyPayment(payment, network_id, params, state) == PaymentError::NONE);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    BOOST_CHECK(state.accounts.at(other_account).balance == 3);
    BOOST_CHECK(state.pending_fee_pool == params.payment_fee);
    BOOST_CHECK(state.identities.Find(account)->nonce == 1);
    BOOST_CHECK(ApplyPayment(payment, network_id, params, state) == PaymentError::INVALID_AUTHORIZATION);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    const ProtocolOperation payment_operation{payment};
    const auto wire = SerializeProtocolOperation(payment_operation);
    BOOST_REQUIRE(wire);
    const auto decoded_wire = DeserializeProtocolOperation(*wire);
    BOOST_REQUIRE(decoded_wire && std::holds_alternative<AuthorizedPayment>(*decoded_wire));
    BOOST_CHECK(SerializeProtocolOperation(*decoded_wire) == wire);
    BOOST_CHECK(ComputeOperationId(*decoded_wire) == ComputeOperationId(payment_operation));
    auto bad_wire = *wire;
    bad_wire[0] = 1;
    BOOST_CHECK(!DeserializeProtocolOperation(bad_wire));
    bad_wire = *wire;
    bad_wire.back() ^= 1;
    BOOST_CHECK(!DeserializeProtocolOperation(bad_wire));
    BOOST_CHECK(!DeserializeProtocolOperation(std::span{*wire}.first(wire->size() - 1)));
    const ProtocolOperation create_operation{create};
    const auto create_wire = SerializeProtocolOperation(create_operation);
    BOOST_REQUIRE(create_wire);
    const auto decoded_create = DeserializeProtocolOperation(*create_wire);
    BOOST_REQUIRE(decoded_create && std::holds_alternative<AccountCreateOp>(*decoded_create));
    BOOST_CHECK(SerializeProtocolOperation(*decoded_create) == create_wire);

    state.pending_fee_pool = 3; // fixture: next fee completes a 4-unit routing group
    auto next_payment = payment;
    next_payment.payment.amount = 1;
    next_payment.authorization.nonce = 1;
    next_payment.authorization.payload_commitment = *ComputePaymentPayloadCommitment(next_payment.payment);
    const auto next_digest = ComputeIdentityOperationDigest(network_id, next_payment.authorization);
    BOOST_REQUIRE(next_digest);
    const auto next_signature = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *next_digest);
    BOOST_REQUIRE(next_signature);
    next_payment.authorization.signature = *next_signature;
    const auto block_result = ExecuteBlockOperations(state, {ProtocolOperation{next_payment}}, network_id, 2, params);
    BOOST_REQUIRE(block_result);
    BOOST_CHECK(block_result.state->accounts.at(account).balance == 6);
    BOOST_CHECK(block_result.state->accounts.at(other_account).balance == 4);
    BOOST_CHECK(block_result.state->security_reward_pool == 3);
    BOOST_CHECK(block_result.state->onboarding_pool == 1);
    BOOST_CHECK(block_result.state->pending_fee_pool == 0);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    BOOST_CHECK(state.pending_fee_pool == 3);
    const auto replay_block = ExecuteBlockOperations(*block_result.state,
        {ProtocolOperation{next_payment}}, network_id, 3, params);
    BOOST_CHECK(replay_block.error == BlockExecutionError::INVALID_PAYMENT);
    BOOST_CHECK(replay_block.payment_error == PaymentError::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(root_publication_is_identity_authorized_and_pays_deterministic_fee)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, authorization_seed{};
    root_seed[0] = 0x71;
    authorization_seed[0] = 0x72;
    uint256 raw_account{}, network_id{};
    raw_account.begin()[0] = 0x73;
    network_id.begin()[0] = 0x74;
    const AccountId account{raw_account};
    const auto root_key = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization_key = DeriveIdentityPublicKey(authorization_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root_key && authorization_key);
    const IdentityAuthorization identity_authorization{*root_key, *authorization_key};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, identity_authorization);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(authorization_seed,
        IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, identity_authorization, binding.package,
        {.network_id = network_id, .account_id = account,
            .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(0x75));
    BOOST_REQUIRE(ApplyAccountCreate(create, network_id, 0, params, state) == AccountCreateStateError::NONE);

    RootPublication publication;
    publication.root_chunk_id.fill(0x31);
    publication.chunk_authorization_root.fill(0x42);
    publication.chunk_count = 1;
    RootRecipientCapsule capsule;
    capsule.encapsulation.fill(0x53);
    capsule.wrapped_content_key.fill(0x64);
    publication.recipient_capsules.push_back(capsule);
    IdentityOperationAuthorization operation_auth{
        .account_id = account,
        .nonce = 0,
        .key_epoch = 0,
        .kind = IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *ComputeRootPublicationPayloadCommitment(publication),
    };
    const auto digest = ComputeIdentityOperationDigest(network_id, operation_auth);
    BOOST_REQUIRE(digest);
    operation_auth.signature = *SignIdentityMessage(authorization_seed,
        IdentityKeyPurpose::AUTHORIZATION, *digest);
    const AuthorizedRootPublication operation{operation_auth, publication};
    const auto encoded = SerializeProtocolOperation(ProtocolOperation{operation});
    const auto fee = encoded ? ComputeRootPublicationFee(params, encoded->size(), publication.chunk_count) : std::nullopt;
    BOOST_REQUIRE(fee);
    const auto starting_balance = state.accounts.at(account).system_balance;

    BOOST_CHECK(ApplyRootPublication(operation, network_id, params, state) == RootPublicationError::NONE);
    BOOST_CHECK(state.accounts.at(account).system_balance == starting_balance - *fee);
    BOOST_CHECK(state.pending_fee_pool == *fee);
    BOOST_CHECK(state.identities.Find(account)->nonce == 1);
    BOOST_CHECK(ApplyRootPublication(operation, network_id, params, state) == RootPublicationError::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(insufficient_pool_does_not_register_identity)
{
    using namespace cybou;
    CybouState state{};
    auto params = DevProtocolParameters();
    params.onboarding_bonus = 1;
    const AccountCreateOp empty{};
    BOOST_CHECK(ApplyAccountCreate(empty, uint256{}, 0, params, state) == AccountCreateStateError::INSUFFICIENT_ONBOARDING_POOL);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
}

BOOST_AUTO_TEST_CASE(identity_rotate_wire_and_block_execution)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, authorization_seed{}, new_entropy{};
    root_seed[0] = 11;
    authorization_seed[0] = 12;
    new_entropy.fill(0x66);
    uint256 raw_account{}, network_id{};
    raw_account.begin()[0] = 15;
    network_id.begin()[0] = 16;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization = DeriveIdentityPublicKey(authorization_seed, IdentityKeyPurpose::AUTHORIZATION);
    const auto new_recovery = DeriveIdentityPublicKey(new_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto new_authorization = DeriveIdentityPublicKey(new_entropy, IdentityKeyPurpose::AUTHORIZATION);
    const auto new_kem_seed = DeriveIdentityXWingSeed(new_entropy);
    const auto new_kem_public = new_kem_seed ? DeriveXWingPublicKey(*new_kem_seed) : std::nullopt;
    const auto new_package = new_kem_public ? EncodeIdentityKemPackage(*new_kem_public) : std::nullopt;
    BOOST_REQUIRE(root && authorization && new_recovery && new_authorization && new_package);

    const IdentityAuthorization auth{*root, *authorization};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(authorization_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_id = network_id, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(17));
    BOOST_REQUIRE(ApplyAccountCreate(create, network_id, 0, params, state) == AccountCreateStateError::NONE);

    const IdentityAuthorization next_auth{*new_recovery, *new_authorization};
    const auto next_binding = test::MakeIdentityKemBinding(network_id, account, next_auth, 1);
    IdentityRotate rotate{
        .account_id = account,
        .new_recovery_key = *new_recovery,
        .new_authorization_key = *new_authorization,
        .new_kem_package = *new_package,
        .nonce = 0,
        .key_epoch = 1,
    };
    const auto rotate_digest = ComputeIdentityRotateDigest(network_id, rotate);
    BOOST_REQUIRE(rotate_digest);
    rotate.old_recovery_signature = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    rotate.new_recovery_pop = *SignIdentityMessage(new_entropy, IdentityKeyPurpose::RECOVERY_ROOT, *rotate_digest);
    rotate.new_authorization_pop = *SignIdentityMessage(new_entropy, IdentityKeyPurpose::AUTHORIZATION, *rotate_digest);

    const ProtocolOperation operation{rotate};
    const auto wire = SerializeProtocolOperation(operation);
    BOOST_REQUIRE(wire);
    BOOST_CHECK_EQUAL(wire->size(), 2 + IDENTITY_ROTATE_SIZE);
    const auto decoded = DeserializeProtocolOperation(*wire);
    BOOST_REQUIRE(decoded && std::holds_alternative<IdentityRotate>(*decoded));
    BOOST_CHECK(SerializeProtocolOperation(*decoded) == wire);
    BOOST_CHECK(ComputeOperationId(*decoded) == ComputeOperationId(operation));
    auto malformed = *wire;
    malformed.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperation(malformed));
    BOOST_CHECK(!DeserializeProtocolOperation(std::span{*wire}.first(wire->size() - 1)));

    const auto rotated = ExecuteBlockOperations(state, {operation}, network_id, 1, params);
    BOOST_REQUIRE(rotated);
    const auto* record = rotated.state->identities.Find(account);
    BOOST_REQUIRE(record);
    BOOST_CHECK(record->recovery_key == *new_recovery);
    BOOST_CHECK(record->authorization_key == *new_authorization);
    BOOST_CHECK(record->kem_package_id == next_binding.package_id);
    BOOST_CHECK_EQUAL(record->key_epoch, 1U);
    BOOST_CHECK_EQUAL(record->nonce, 0U);
    const auto new_recovery_id = ComputeRecoveryKeyId(*new_recovery);
    BOOST_REQUIRE(new_recovery_id);
    BOOST_CHECK(rotated.state->identities.FindByRecoveryKeyId(*new_recovery_id) == account);
    const auto replay = ExecuteBlockOperations(*rotated.state, {operation}, network_id, 2, params);
    BOOST_CHECK(replay.error == BlockExecutionError::INVALID_IDENTITY_ROTATE);
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
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_id = network_id, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(25));
    BOOST_REQUIRE(ApplyAccountCreate(create, network_id, 0, params, state) == AccountCreateStateError::NONE);

    // Fund account balance
    state.accounts.at(account).balance = 50;


    AuthorizedSystemLock lock{};
    lock.lock.amount = 20;
    const auto lock_bytes = SerializeSystemLockPayload(lock.lock);
    BOOST_REQUIRE(lock_bytes);
    const auto decoded_lock = DeserializeSystemLockPayload(*lock_bytes);
    BOOST_REQUIRE(decoded_lock && decoded_lock->amount == 20);

    const auto lock_commitment = ComputeSystemLockPayloadCommitment(lock.lock);
    BOOST_REQUIRE(lock_commitment);

    lock.authorization.account_id = account;
    lock.authorization.nonce = 0;
    lock.authorization.kind = IdentityOperationKind::SYSTEM_LOCK;
    lock.authorization.payload_commitment = *lock_commitment;
    const auto lock_digest = ComputeIdentityOperationDigest(network_id, lock.authorization);
    BOOST_REQUIRE(lock_digest);
    lock.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *lock_digest);

    // Wire serialization
    const ProtocolOperation lock_op{lock};
    const auto lock_wire = SerializeProtocolOperation(lock_op);
    BOOST_REQUIRE(lock_wire);
    BOOST_CHECK(lock_wire->size() == 2 + AUTHORIZED_SYSTEM_LOCK_SIZE);
    const auto decoded_wire = DeserializeProtocolOperation(*lock_wire);
    BOOST_REQUIRE(decoded_wire && std::holds_alternative<AuthorizedSystemLock>(*decoded_wire));
    BOOST_CHECK(SerializeProtocolOperation(*decoded_wire) == lock_wire);
    BOOST_CHECK(ComputeOperationId(*decoded_wire) == ComputeOperationId(lock_op));

    // Damaged wires
    auto bad_lock_wire = *lock_wire;
    bad_lock_wire[0] = 0;
    BOOST_CHECK(!DeserializeProtocolOperation(bad_lock_wire));
    bad_lock_wire = *lock_wire;
    bad_lock_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperation(bad_lock_wire));
    BOOST_CHECK(!DeserializeProtocolOperation(std::span{*lock_wire}.first(lock_wire->size() - 1)));

    // Execute in block
    const auto block_res = ExecuteBlockOperations(state, {lock_op}, network_id, 1, params);
    BOOST_REQUIRE(block_res);
    BOOST_CHECK(block_res.state->accounts.at(account).balance == 30);
    BOOST_CHECK(block_res.state->accounts.at(account).system_balance == params.onboarding_bonus + 20);
    BOOST_CHECK(block_res.state->pending_fee_pool == 0); // no fee for lock
    BOOST_CHECK(block_res.state->identities.Find(account)->nonce == 1);

    // Replay stale nonce fails
    const auto replay = ExecuteBlockOperations(*block_res.state, {lock_op}, network_id, 2, params);
    BOOST_CHECK(replay.error == BlockExecutionError::INVALID_SYSTEM_LOCK);
    BOOST_CHECK(replay.lock_error == SystemLockError::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(state_validation_invariants)
{
    using namespace cybou;
    CybouState state{};
    state.validator_set.validators.push_back(MakeTestValidator(31));
    BOOST_CHECK(ValidateCybouState(state) == StateValidationError::NONE);

    // Mismatched account count
    uint256 raw{};
    raw.begin()[0] = 33;
    const AccountId acc{raw};
    state.accounts.emplace(acc, AccountState{.balance = 10});
    BOOST_CHECK(ValidateCybouState(state) == StateValidationError::ACCOUNT_IDENTITY_COUNT_MISMATCH);

    // Balance overflow
    state.accounts.clear();
    state.onboarding_pool = 100'000'000'001ULL;
    BOOST_CHECK(ValidateCybouState(state) == StateValidationError::BALANCE_OVERFLOW);
}

BOOST_AUTO_TEST_CASE(post_quantum_validator_set_and_bft_certificate)
{
    using namespace cybou;
    std::array<std::array<unsigned char, 32>, 4> seeds{};
    for (size_t i = 0; i < 4; ++i) {
        seeds[i][0] = static_cast<unsigned char>(101 + i);
    }
    std::vector<Validator> validators;
    for (size_t i = 0; i < 4; ++i) {
        validators.push_back(MakeTestValidator(static_cast<uint8_t>(101 + i)));
    }

    ValidatorSet val_set{.validators = validators};
    BOOST_CHECK(val_set.version == VALIDATOR_SET_VERSION);
    BOOST_CHECK_EQUAL(val_set.Size(), 4U);
    BOOST_CHECK_EQUAL(val_set.TotalWeight(), 4U);
    BOOST_CHECK_EQUAL(val_set.FaultTolerance(), 1U);
    BOOST_CHECK_EQUAL(val_set.QuorumThreshold(), 3U);
    BOOST_CHECK(val_set.Mode() == ConsensusMode::BFT);
    BOOST_CHECK(ValidateValidatorSet(val_set) == ValidatorSetValidationError::NONE);

    // Hardening check: duplicate ML-DSA-65 key rejection
    auto dup_ml_set = val_set;
    dup_ml_set.validators[1].consensus_public_key.ml_dsa = dup_ml_set.validators[0].consensus_public_key.ml_dsa;
    const auto dup_val_id = ComputeValidatorKeyId(dup_ml_set.validators[1].consensus_public_key);
    BOOST_REQUIRE(dup_val_id.has_value());
    std::copy_n(dup_val_id->begin(), 32, dup_ml_set.validators[1].validator_id.begin());
    BOOST_CHECK(ValidateValidatorSet(dup_ml_set) == ValidatorSetValidationError::DUPLICATE_CONSENSUS_KEY);

    // Serialization roundtrip
    const auto serialized = SerializeValidatorSet(val_set);
    BOOST_CHECK_EQUAL(serialized.size(), 5 + 4 * VALIDATOR_ENTRY_SIZE);
    const auto deserialized = DeserializeValidatorSet(serialized);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK(*deserialized == val_set);

    const auto commitment = ComputeValidatorSetCommitment(val_set);
    BOOST_CHECK(!commitment.IsNull());

    // Build BFT Finality Certificate with 3 votes (quorum threshold = 3)
    uint256 network_id{}, block_id{};
    network_id.begin()[0] = 77;
    block_id.begin()[0] = 88;
    const uint64_t height = 1000;
    const uint32_t round = 0;

    const uint256 commit_digest = ComputeBftCommitDigest(network_id, block_id, height, round, commitment);
    std::array<unsigned char, 32> digest_bytes{};
    std::copy_n(commit_digest.begin(), 32, digest_bytes.begin());

    BftFinalityCertificate cert{};
    cert.network_id = network_id;
    cert.block_id = block_id;
    cert.height = height;
    cert.round = round;
    cert.validator_set_commitment = commitment;

    for (size_t i = 0; i < 3; ++i) {
        const auto sig = SignIdentityMessage(seeds[i], IdentityKeyPurpose::VALIDATOR, digest_bytes);
        BOOST_REQUIRE(sig.has_value());
        cert.commit_votes.push_back(BftCommitVote{
            .validator_id = validators[i].validator_id,
            .signature = *sig,
        });
    }

    // Verification succeeds
    BOOST_CHECK(VerifyFinalityCertificate(cert, val_set, network_id) == FinalityVerificationError::NONE);

    // Serialization roundtrip
    const auto cert_bytes = SerializeFinalityCertificate(cert);
    BOOST_REQUIRE(cert_bytes.has_value());
    BOOST_CHECK_EQUAL(cert_bytes->size(), 113 + 3 * BFT_COMMIT_VOTE_SIZE);
    const auto decoded_cert = DeserializeFinalityCertificate(*cert_bytes);
    BOOST_REQUIRE(decoded_cert.has_value());
    BOOST_CHECK(*decoded_cert == cert);

    // Fail-closed malformed certificate serialization
    auto bad_ml_cert = cert;
    bad_ml_cert.commit_votes[0].signature.ml_dsa.pop_back(); // 3308 != 3309
    BOOST_CHECK(!SerializeFinalityCertificate(bad_ml_cert).has_value());

    // Adversarial verification checks
    // 1. Wrong network
    uint256 wrong_net = network_id;
    wrong_net.begin()[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificate(cert, val_set, wrong_net) == FinalityVerificationError::NETWORK_MISMATCH);

    // 2. Wrong validator set commitment
    auto bad_commitment_cert = cert;
    bad_commitment_cert.validator_set_commitment.begin()[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificate(bad_commitment_cert, val_set, network_id) == FinalityVerificationError::VALIDATOR_SET_MISMATCH);

    // 3. Insufficient votes (2 < 3)
    auto short_cert = cert;
    short_cert.commit_votes.pop_back();
    BOOST_CHECK(VerifyFinalityCertificate(short_cert, val_set, network_id) == FinalityVerificationError::INSUFFICIENT_VOTES);

    // 4. Duplicate vote
    auto dup_cert = cert;
    dup_cert.commit_votes[2] = dup_cert.commit_votes[0];
    BOOST_CHECK(VerifyFinalityCertificate(dup_cert, val_set, network_id) == FinalityVerificationError::DUPLICATE_VOTE);

    // 5. Unknown validator
    auto unknown_cert = cert;
    unknown_cert.commit_votes[0].validator_id.begin()[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificate(unknown_cert, val_set, network_id) == FinalityVerificationError::UNKNOWN_VALIDATOR);

    // 6. Invalid signature
    auto bad_sig_cert = cert;
    bad_sig_cert.commit_votes[0].signature.ed25519[0] ^= 1;
    BOOST_CHECK(VerifyFinalityCertificate(bad_sig_cert, val_set, network_id) == FinalityVerificationError::INVALID_SIGNATURE);
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
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_id = network_id, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    params.name_claim_work_bits = 0;
    params.name_commit_min_depth = 1;
    params.name_commit_max_lifetime = 100;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.validator_set.validators.push_back(MakeTestValidator(55));
    BOOST_REQUIRE(ApplyAccountCreate(create, network_id, 0, params, state) == AccountCreateStateError::NONE);


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
    commit_op.authorization.nonce = 0;
    commit_op.authorization.kind = IdentityOperationKind::NAME_COMMIT;
    commit_op.authorization.payload_commitment = *commit_payload_commitment;
    const auto commit_digest = ComputeIdentityOperationDigest(network_id, commit_op.authorization);
    BOOST_REQUIRE(commit_digest.has_value());
    commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *commit_digest);

    // Wire serialization check
    const ProtocolOperation commit_proto_op{commit_op};
    const auto commit_wire = SerializeProtocolOperation(commit_proto_op);
    BOOST_REQUIRE(commit_wire.has_value());
    BOOST_CHECK_EQUAL(commit_wire->size(), 2 + AUTHORIZED_NAME_COMMIT_SIZE);
    const auto decoded_commit_wire = DeserializeProtocolOperation(*commit_wire);
    BOOST_REQUIRE(decoded_commit_wire.has_value());
    BOOST_CHECK(std::holds_alternative<AuthorizedNameCommit>(*decoded_commit_wire));
    BOOST_CHECK(ComputeOperationId(*decoded_commit_wire) == ComputeOperationId(commit_proto_op));

    // Execute NameCommit at height 10
    const auto commit_block = ExecuteBlockOperations(state, {commit_proto_op}, network_id, 10, params);
    BOOST_REQUIRE(commit_block);
    BOOST_REQUIRE(commit_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_CHECK_EQUAL(commit_block.state->names.pending_commits.at(name_commit_hash).commit_height, 10ULL);
    BOOST_CHECK_EQUAL(commit_block.state->identities.Find(account)->nonce, 1ULL);

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
    reveal_op.authorization.nonce = 1;
    reveal_op.authorization.kind = IdentityOperationKind::NAME_REVEAL;
    reveal_op.authorization.payload_commitment = *reveal_payload_commitment;
    const auto reveal_digest = ComputeIdentityOperationDigest(network_id, reveal_op.authorization);
    BOOST_REQUIRE(reveal_digest.has_value());
    reveal_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *reveal_digest);

    // Wire serialization check
    const ProtocolOperation reveal_proto_op{reveal_op};
    const auto reveal_wire = SerializeProtocolOperation(reveal_proto_op);
    BOOST_REQUIRE(reveal_wire.has_value());
    BOOST_CHECK_EQUAL(reveal_wire->size(), 2 + AUTHORIZED_NAME_REVEAL_SIZE);
    const auto decoded_reveal_wire = DeserializeProtocolOperation(*reveal_wire);
    BOOST_REQUIRE(decoded_reveal_wire.has_value());
    BOOST_CHECK(std::holds_alternative<AuthorizedNameReveal>(*decoded_reveal_wire));
    BOOST_CHECK(ComputeOperationId(*decoded_reveal_wire) == ComputeOperationId(reveal_proto_op));

    // Adversarial: Premature reveal at height 10 (needs min depth 1 -> height >= 11)
    const auto premature_res = ExecuteBlockOperations(*commit_block.state, {reveal_proto_op}, network_id, 10, params);
    BOOST_CHECK(premature_res.error == BlockExecutionError::INVALID_NAME_REVEAL);
    BOOST_CHECK(premature_res.name_reveal_error == NameRevealError::INSUFFICIENT_COMMIT_DEPTH);

    // Successful reveal at height 12
    const auto reveal_block = ExecuteBlockOperations(*commit_block.state, {reveal_proto_op}, network_id, 12, params);
    BOOST_REQUIRE(reveal_block);
    BOOST_CHECK(!reveal_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_REQUIRE(reveal_block.state->names.Resolve(label) != nullptr);
    BOOST_CHECK(*reveal_block.state->names.Resolve(label) == account);
    BOOST_REQUIRE(reveal_block.state->names.PrimaryName(account) != nullptr);
    BOOST_CHECK(*reveal_block.state->names.PrimaryName(account) == label);
    BOOST_CHECK_EQUAL(reveal_block.state->identities.Find(account)->nonce, 2ULL);

    // State serialization roundtrip with names
    const auto state_bytes = SerializeCybouState(*reveal_block.state);
    BOOST_REQUIRE(state_bytes.has_value());
    const auto restored_state = DeserializeCybouState(*state_bytes);
    BOOST_REQUIRE(restored_state.has_value());
    BOOST_CHECK(*restored_state->names.Resolve(label) == account);
    BOOST_CHECK(*restored_state->names.PrimaryName(account) == label);
    BOOST_CHECK(SerializeCybouState(*restored_state) == state_bytes);

    // 5. Adversarial checks
    // A0. Account already has a pending commitment -> cannot commit another
    auto second_commit_op = commit_op;
    second_commit_op.authorization.nonce = 1;
    const auto sec_digest0 = ComputeIdentityOperationDigest(network_id, second_commit_op.authorization);
    BOOST_REQUIRE(sec_digest0.has_value());
    second_commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *sec_digest0);
    const auto pending_commit_res = ExecuteBlockOperations(*commit_block.state, {second_commit_op}, network_id, 11, params);
    BOOST_CHECK(pending_commit_res.error == BlockExecutionError::INVALID_NAME_COMMIT);
    BOOST_CHECK(pending_commit_res.name_commit_error == NameCommitError::ACCOUNT_HAS_PENDING_COMMIT);

    // A. Account already has a name -> cannot commit another name
    second_commit_op.authorization.nonce = 2;
    const auto sec_digest = ComputeIdentityOperationDigest(network_id, second_commit_op.authorization);
    BOOST_REQUIRE(sec_digest.has_value());
    second_commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *sec_digest);
    const auto double_commit_res = ExecuteBlockOperations(*reveal_block.state, {second_commit_op}, network_id, 13, params);
    BOOST_CHECK(double_commit_res.error == BlockExecutionError::INVALID_NAME_COMMIT);
    BOOST_CHECK(double_commit_res.name_commit_error == NameCommitError::ACCOUNT_ALREADY_HAS_NAME);

    // B. Expired commit & deterministic block pruning
    params.name_commit_max_lifetime = 5;
    // Block at height 16 automatically prunes the expired commit from height 10 (16 > 10 + 5)
    const auto prune_block = ExecuteBlockOperations(*commit_block.state, {}, network_id, 16, params);
    BOOST_REQUIRE(prune_block);
    BOOST_CHECK(!prune_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_CHECK(prune_block.state->names.pending_commits.empty());

    // Once pruned, the commitment is no longer found for reveal
    const auto expired_res = ExecuteBlockOperations(*commit_block.state, {reveal_proto_op}, network_id, 16, params);
    BOOST_CHECK(expired_res.error == BlockExecutionError::INVALID_NAME_REVEAL);
    BOOST_CHECK(expired_res.name_reveal_error == NameRevealError::COMMITMENT_NOT_FOUND);

    // Direct ApplyNameReveal check for COMMIT_EXPIRED on unpruned state
    auto unpruned_state = *commit_block.state;
    BOOST_CHECK(ApplyNameReveal(reveal_op, network_id, 16, params, unpruned_state) == NameRevealError::COMMIT_EXPIRED);
    params.name_commit_max_lifetime = 100;

    // C. Tampered commit wire
    auto bad_commit_wire = *commit_wire;
    bad_commit_wire[0] = 0;
    BOOST_CHECK(!DeserializeProtocolOperation(bad_commit_wire));
    bad_commit_wire = *commit_wire;
    bad_commit_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperation(bad_commit_wire));
    BOOST_CHECK(!DeserializeProtocolOperation(std::span{*commit_wire}.first(commit_wire->size() - 1)));

    // D. Tampered reveal wire
    auto bad_reveal_wire = *reveal_wire;
    bad_reveal_wire[0] = 0;
    BOOST_CHECK(!DeserializeProtocolOperation(bad_reveal_wire));
    bad_reveal_wire = *reveal_wire;
    bad_reveal_wire.push_back(0);
    BOOST_CHECK(!DeserializeProtocolOperation(bad_reveal_wire));
    BOOST_CHECK(!DeserializeProtocolOperation(std::span{*reveal_wire}.first(reveal_wire->size() - 1)));

    // E. State corruption: name references account not in monetary state
    auto corrupted_state = *reveal_block.state;
    corrupted_state.accounts.clear();
    BOOST_CHECK(ValidateCybouState(corrupted_state) == StateValidationError::ACCOUNT_IDENTITY_COUNT_MISMATCH);
}

BOOST_AUTO_TEST_CASE(mail_tx_execution_and_quotas)
{
    using namespace cybou;

    // 1. Fee calculation logic
    auto params = DevProtocolParameters();
    BOOST_CHECK_EQUAL(MailFeeForSize(100), 5ULL);     // 4 + 1
    BOOST_CHECK_EQUAL(MailFeeForSize(1024), 5ULL);    // 4 + 1
    BOOST_CHECK_EQUAL(MailFeeForSize(1025), 6ULL);    // 4 + 2
    BOOST_CHECK_EQUAL(MailFeeForSize(2048), 6ULL);    // 4 + 2
    BOOST_CHECK_EQUAL(MailFeeForSize(64 * 1024), 68ULL); // 4 + 64

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
    const auto alice_dev = DeriveIdentityPublicKey(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION);
    const auto bob_root = DeriveIdentityPublicKey(bob_root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto bob_dev = DeriveIdentityPublicKey(bob_dev_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(alice_root && alice_dev && bob_root && bob_dev);


    const IdentityAuthorization alice_auth{*alice_root, *alice_dev};
    const auto alice_binding = test::MakeIdentityKemBinding(network_id, alice, alice_auth);
    const AccountCreateOp create_alice{alice, alice_auth, alice_binding.package,
        {.network_id = network_id, .account_id = alice, .authorization_commitment = alice_binding.authorization_commitment},
        *SignIdentityMessage(alice_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, alice_binding.pop_digest),
        *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, alice_binding.pop_digest)};

    const IdentityAuthorization bob_auth{*bob_root, *bob_dev};
    const auto bob_binding = test::MakeIdentityKemBinding(network_id, bob, bob_auth);
    const AccountCreateOp create_bob{bob, bob_auth, bob_binding.package,
        {.network_id = network_id, .account_id = bob, .authorization_commitment = bob_binding.authorization_commitment},
        *SignIdentityMessage(bob_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, bob_binding.pop_digest),
        *SignIdentityMessage(bob_dev_seed, IdentityKeyPurpose::AUTHORIZATION, bob_binding.pop_digest)};

    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus * 10;
    state.validator_set.validators.push_back(MakeTestValidator(88));
    BOOST_REQUIRE(ApplyAccountCreate(create_alice, network_id, 0, params, state) == AccountCreateStateError::NONE);
    BOOST_REQUIRE(ApplyAccountCreate(create_bob, network_id, 0, params, state) == AccountCreateStateError::NONE);

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
    mail_op.authorization.nonce = 0;
    mail_op.authorization.kind = IdentityOperationKind::MAIL;
    mail_op.authorization.payload_commitment = *payload_commitment;
    const auto op_digest = ComputeIdentityOperationDigest(network_id, mail_op.authorization);
    BOOST_REQUIRE(op_digest.has_value());
    mail_op.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *op_digest);

    // Wire serialization roundtrip
    const ProtocolOperation proto_op{mail_op};
    const auto wire_bytes = SerializeProtocolOperation(proto_op);
    BOOST_REQUIRE(wire_bytes.has_value());
    BOOST_CHECK_EQUAL(wire_bytes->size(), 2 + 2597 + MAIL_PAYLOAD_HEADER_SIZE + payload.ciphertext.size());

    const auto decoded_proto_op = DeserializeProtocolOperation(*wire_bytes);
    BOOST_REQUIRE(decoded_proto_op.has_value());
    BOOST_CHECK(std::holds_alternative<AuthorizedMail>(*decoded_proto_op));
    BOOST_CHECK(ComputeOperationId(*decoded_proto_op) == ComputeOperationId(proto_op));

    // 4. Execute mail at block height 10 (epoch 0)
    const auto block_res = ExecuteBlockOperations(state, {proto_op}, network_id, 10, params);
    BOOST_REQUIRE(block_res);
    const auto& post_state = *block_res.state;

    // Check balance deduction and counter updates
    const uint64_t expected_fee = MailFeeForSize(payload.ciphertext.size()); // 5
    BOOST_CHECK_EQUAL(expected_fee, 5ULL);
    BOOST_CHECK_EQUAL(post_state.accounts.at(alice).system_balance, params.onboarding_bonus - expected_fee);
    BOOST_CHECK_EQUAL(post_state.accounts.at(alice).last_mail_epoch, 0ULL);
    BOOST_CHECK_EQUAL(post_state.accounts.at(alice).mail_count_in_epoch, 1U);
    BOOST_CHECK_EQUAL(post_state.identities.Find(alice)->nonce, 1ULL);

    // Fee routing check (4 fees -> 3 security + 1 onboarding)
    // Fee = 5: chunks = 5/4 = 1. security += 3, onboarding += 1, pending %= 4 -> 1 remainder
    BOOST_CHECK_EQUAL(post_state.pending_fee_pool, 1ULL);
    BOOST_CHECK_EQUAL(post_state.security_reward_pool, 3ULL);
    BOOST_CHECK_EQUAL(post_state.onboarding_pool, state.onboarding_pool + 1);

    // State serialization roundtrip: no mail body stored in state
    const auto state_serialized = SerializeCybouState(post_state);
    BOOST_REQUIRE(state_serialized.has_value());
    const auto state_deserialized = DeserializeCybouState(*state_serialized);
    BOOST_REQUIRE(state_deserialized.has_value());
    BOOST_CHECK(SerializeCybouState(*state_deserialized) == state_serialized);
    BOOST_CHECK(state_deserialized->accounts.at(alice) == post_state.accounts.at(alice));
    BOOST_CHECK(state_deserialized->accounts.at(bob) == post_state.accounts.at(bob));

    // 5. Test quota enforcement: send 24 more mails in epoch 0 to reach 25
    auto current_state = post_state;
    uint64_t nonce = 1;
    for (uint32_t i = 2; i <= 25; ++i) {
        AuthorizedMail next_mail = mail_op;
        next_mail.authorization.nonce = nonce++;
        const auto dig = ComputeIdentityOperationDigest(network_id, next_mail.authorization);
        BOOST_REQUIRE(dig.has_value());
        next_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *dig);
        const auto res = ExecuteBlockOperations(current_state, {ProtocolOperation{next_mail}}, network_id, 10 + i, params);
        BOOST_REQUIRE(res);
        current_state = *res.state;
        BOOST_CHECK_EQUAL(current_state.accounts.at(alice).mail_count_in_epoch, i);
    }
    BOOST_CHECK_EQUAL(current_state.accounts.at(alice).mail_count_in_epoch, 25U);

    // 26th mail in epoch 0: must fail with MAIL_QUOTA_EXCEEDED
    AuthorizedMail quota_exceed_mail = mail_op;
    quota_exceed_mail.authorization.nonce = nonce++;
    const auto dig_exceed = ComputeIdentityOperationDigest(network_id, quota_exceed_mail.authorization);
    BOOST_REQUIRE(dig_exceed.has_value());
    quota_exceed_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *dig_exceed);
    const auto quota_fail_res = ExecuteBlockOperations(current_state, {ProtocolOperation{quota_exceed_mail}}, network_id, 50, params);
    BOOST_CHECK(quota_fail_res.error == BlockExecutionError::INVALID_MAIL);
    BOOST_CHECK(quota_fail_res.mail_error == MailError::MAIL_QUOTA_EXCEEDED);

    // 6. Reset in epoch 1: block height 1024 -> epoch 1
    const uint64_t epoch1_height = params.epoch_blocks;
    BOOST_CHECK_EQUAL(EpochForHeight(epoch1_height, params), 1ULL);
    // Reuse the same mail op with correct nonce
    quota_exceed_mail.authorization.nonce = nonce - 1; // nonce was not consumed by failed op
    const auto dig_epoch1 = ComputeIdentityOperationDigest(network_id, quota_exceed_mail.authorization);
    BOOST_REQUIRE(dig_epoch1.has_value());
    quota_exceed_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *dig_epoch1);
    const auto epoch1_res = ExecuteBlockOperations(current_state, {ProtocolOperation{quota_exceed_mail}}, network_id, epoch1_height, params);
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
    const auto dig_bad_recip = ComputeIdentityOperationDigest(network_id, bad_recipient_mail.authorization);
    BOOST_REQUIRE(dig_bad_recip.has_value());
    bad_recipient_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *dig_bad_recip);
    const auto bad_recip_res = ExecuteBlockOperations(*epoch1_res.state, {ProtocolOperation{bad_recipient_mail}}, network_id, epoch1_height + 1, params);
    BOOST_CHECK(bad_recip_res.error == BlockExecutionError::INVALID_MAIL);
    BOOST_CHECK(bad_recip_res.mail_error == MailError::RECIPIENT_NOT_FOUND);

    // B. Insufficient system balance
    auto poor_state = *epoch1_res.state;
    poor_state.accounts.at(alice).system_balance = 0;
    AuthorizedMail poor_mail = mail_op;
    poor_mail.authorization.nonce = nonce;
    const auto dig_poor = ComputeIdentityOperationDigest(network_id, poor_mail.authorization);
    BOOST_REQUIRE(dig_poor.has_value());
    poor_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *dig_poor);
    const auto poor_res = ExecuteBlockOperations(poor_state, {ProtocolOperation{poor_mail}}, network_id, epoch1_height + 1, params);
    BOOST_CHECK(poor_res.error == BlockExecutionError::INVALID_MAIL);
    BOOST_CHECK(poor_res.mail_error == MailError::INSUFFICIENT_SYSTEM_BALANCE);

    // C. Ciphertext size limits: wire limit vs consensus execution limit
    MailPayload wire_oversized_payload = payload;
    wire_oversized_payload.ciphertext = std::vector<unsigned char>(MAX_MAIL_WIRE_CIPHERTEXT_SIZE + 1, 0xFF);
    BOOST_CHECK(!SerializeMailPayload(wire_oversized_payload));
    MailPayload empty_payload = payload;
    empty_payload.ciphertext.clear();
    BOOST_CHECK(!SerializeMailPayload(empty_payload));

    // Consensus parameter execution check: payload within wire framing (e.g. 65 KiB)
    // but exceeding the transitional Mail ciphertext cap is rejected
    MailPayload param_oversized_payload = payload;
    param_oversized_payload.ciphertext = std::vector<unsigned char>(MAX_MAIL_CIPHERTEXT_SIZE + 1, 0xEE);
    const auto param_commit = ComputeMailPayloadCommitment(param_oversized_payload);
    BOOST_REQUIRE(param_commit.has_value());
    AuthorizedMail param_oversized_mail = mail_op;
    param_oversized_mail.mail = param_oversized_payload;
    param_oversized_mail.authorization.payload_commitment = *param_commit;
    param_oversized_mail.authorization.nonce = nonce;
    const auto dig_oversized = ComputeIdentityOperationDigest(network_id, param_oversized_mail.authorization);
    BOOST_REQUIRE(dig_oversized.has_value());
    param_oversized_mail.authorization.signature = *SignIdentityMessage(alice_dev_seed, IdentityKeyPurpose::AUTHORIZATION, *dig_oversized);
    const auto param_res = ExecuteBlockOperations(*epoch1_res.state, {ProtocolOperation{param_oversized_mail}}, network_id, epoch1_height + 1, params);
    BOOST_CHECK(param_res.error == BlockExecutionError::INVALID_MAIL);
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
    const auto device = DeriveIdentityPublicKey(dev_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_id, account, auth);

    const AccountCreateOp create_op{
        .account_id = account,
        .authorization = auth,
        .kem_package = binding.package,
        .work = {
            .network_id = network_id,
            .account_id = account,
            .authorization_commitment = binding.authorization_commitment,
            .work_epoch = 0,
            .nonce = 0,
        },
        .recovery_pop = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest),
        .authorization_pop = *SignIdentityMessage(dev_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest),
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
