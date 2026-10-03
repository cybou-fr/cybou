// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block_executor.h>
#include <cybou/network_definition.h>
#include <cybou/state_store.h>
#include <cybou/kv_store.h>
#include <cybou/support_mail.h>
#include <test/cybou_test_helpers.h>
#include "cybou_test_identity_helpers.h"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <limits>

BOOST_AUTO_TEST_SUITE(cybou_state_tests)

BOOST_AUTO_TEST_CASE(support_mail_pads_to_the_support_rate)
{
    using namespace cybou;
    const auto params = DevProtocolParameters();
    RootPublication publication;
    publication.root_chunk_id[0] = 1;
    publication.chunk_authorization_root[0] = 2;
    publication.chunk_count = 1;
    publication.recipient_capsules.resize(2); // recipient + owner
    const auto base = RootPublicationOperationFee(params, publication);
    const auto minimum = SupportMailMinimumFee(params);
    BOOST_REQUIRE(base);
    BOOST_CHECK(*base < minimum);
    BOOST_CHECK_EQUAL(minimum, SUPPORT_MAIL_FEE_MULTIPLIER * (5 * params.root_publication_fee_per_started_kib +
                                                               params.root_publication_fee_per_chunk));
    BOOST_REQUIRE(PadPublicationToFee(params, publication, minimum));
    const auto padded = RootPublicationOperationFee(params, publication);
    BOOST_REQUIRE(padded);
    BOOST_CHECK(*padded >= minimum);
    BOOST_CHECK(publication.recipient_capsules.size() <= ROOT_PUBLICATION_MAX_CAPSULES);
    // The first two capsules are untouched; padding never exceeds what the rate needs.
    publication.recipient_capsules.pop_back();
    BOOST_CHECK(*RootPublicationOperationFee(params, publication) < minimum);

    // The support account is whoever claimed the genesis 'cybou' allocation.
    CybouState state{};
    BOOST_CHECK(!SupportAccount(state));
    uint256 raw{};
    raw.begin()[0] = 42;
    IdentityKeyId recovery_id{};
    recovery_id[0] = 7;
    state.genesis_allocations.emplace(recovery_id, GenesisAllocation{.balance = 1, .label = "cybou"});
    BOOST_CHECK(!SupportAccount(state));
    state.genesis_allocations.at(recovery_id).claimed_by = AccountId{raw};
    BOOST_REQUIRE(SupportAccount(state));
    BOOST_CHECK(*SupportAccount(state) == AccountId{raw});
}

BOOST_AUTO_TEST_CASE(genesis_allocation_is_claimed_once_by_its_recovery_key)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 7;
    device_seed[0] = 8;
    uint256 raw_account{}, network_binding{};
    raw_account.begin()[0] = 9;
    network_binding.begin()[0] = 4;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);
    const auto recovery_id = ComputeRecoveryKeyId(*root);
    BOOST_REQUIRE(recovery_id);
    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_binding = network_binding, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;
    state.genesis_allocations.emplace(*recovery_id,
        GenesisAllocation{.balance = 100'000'000, .authority = 1'000'001, .label = "cybou"});
    BOOST_REQUIRE(ValidateCybouState(state) == StateValidationError::NONE);
    const uint64_t supply = TotalSupply(state);
    const auto genesis_bytes = SerializeCybouState(state);
    BOOST_REQUIRE(genesis_bytes);
    const auto genesis_restored = DeserializeCybouState(*genesis_bytes);
    BOOST_REQUIRE(genesis_restored && genesis_restored->genesis_allocations == state.genesis_allocations);

    BOOST_REQUIRE(ApplyAccountCreate(create, network_binding, 1, params, state) == AccountCreateStateError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(account).balance, 100'000'000u);
    BOOST_CHECK_EQUAL(state.accounts.at(account).authority, 1'000'001u);
    BOOST_CHECK(state.genesis_allocations.at(*recovery_id).claimed_by == account);
    BOOST_REQUIRE(state.names.PrimaryName(account));
    BOOST_CHECK_EQUAL(*state.names.PrimaryName(account), "cybou");
    BOOST_CHECK_EQUAL(TotalSupply(state), supply);
    BOOST_REQUIRE(ValidateCybouState(state) == StateValidationError::NONE);
    const auto bytes = SerializeCybouState(state);
    BOOST_REQUIRE(bytes);
    const auto restored = DeserializeCybouState(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeCybouState(*restored) == bytes);
    BOOST_CHECK_EQUAL(restored->accounts.at(account).authority, 1'000'001u);
    BOOST_CHECK_EQUAL(restored->genesis_allocations.at(*recovery_id).authority, 1'000'001u);

    // AUTH is committed by the state root but excluded from CYBOU supply.
    auto more_auth = state;
    ++more_auth.accounts.at(account).authority;
    BOOST_CHECK_EQUAL(TotalSupply(more_auth), TotalSupply(state));
    BOOST_CHECK(CybouStateHash(more_auth) != CybouStateHash(state));
    auto more_genesis_auth = state;
    ++more_genesis_auth.genesis_allocations.at(*recovery_id).authority;
    BOOST_CHECK_EQUAL(TotalSupply(more_genesis_auth), TotalSupply(state));
    BOOST_CHECK(CybouStateHash(more_genesis_auth) != CybouStateHash(state));
    BOOST_CHECK(bytes->front() == CYBOU_STATE_VERSION);
    auto old_version = *bytes;
    old_version.front() = CYBOU_STATE_VERSION - 1;
    BOOST_CHECK(!DeserializeCybouState(old_version));

    // A second AccountCreate never claims the same genesis AUTH again.
    BOOST_CHECK(ApplyAccountCreate(create, network_binding, 2, params, state) != AccountCreateStateError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(account).authority, 1'000'001u);

    // A reserved label never validates without its genesis grant.
    auto forged = state;
    forged.genesis_allocations.clear();
    BOOST_CHECK(ValidateCybouState(forged) == StateValidationError::INVALID_NAME_REGISTRY);
}

BOOST_AUTO_TEST_CASE(account_create_funds_system_balance_and_roundtrips_state)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 1;
    device_seed[0] = 2;
    uint256 raw_account{}, network_binding{};
    raw_account.begin()[0] = 3;
    network_binding.begin()[0] = 4;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);
    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_binding = network_binding, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;

    auto damaged_create = create;
    damaged_create.authorization_pop.ed25519[0] ^= 1;
    BOOST_CHECK(ApplyAccountCreate(damaged_create, network_binding, 0, params, state) == AccountCreateStateError::INVALID_CREATE);
    BOOST_CHECK(state.accounts.empty());
    BOOST_CHECK(state.identities.Accounts().empty());
    BOOST_CHECK(state.onboarding_pool == params.onboarding_bonus);
    BOOST_CHECK(ApplyAccountCreate(create, network_binding, 0, params, state) == AccountCreateStateError::NONE);
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

    BOOST_CHECK(ApplyAccountCreate(create, network_binding, 1, params, state) == AccountCreateStateError::ACCOUNT_EXISTS);
    BOOST_CHECK(SerializeCybouState(state) == bytes);
    auto damaged = *bytes;
    damaged[0] = 1;
    BOOST_CHECK(!DeserializeCybouState(damaged));
    damaged = *bytes;
    damaged.push_back(0);
    BOOST_CHECK(!DeserializeCybouState(damaged));
    BOOST_CHECK(!DeserializeCybouState(std::span{*bytes}.first(bytes->size() - 1)));
    damaged = *bytes;
    damaged[1 + 8 + 4] ^= 1; // monetary AccountID no longer matches identity registry
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
    const auto other_binding = test::MakeIdentityKemBinding(network_binding, other_account, other_auth);
    const auto other_root_pop = SignIdentityMessage(other_root_seed, IdentityKeyPurpose::RECOVERY_ROOT, other_binding.pop_digest);
    const auto other_authorization_pop = SignIdentityMessage(other_device_seed, IdentityKeyPurpose::AUTHORIZATION, other_binding.pop_digest);
    BOOST_REQUIRE(other_root_pop && other_authorization_pop);
    const AccountCreateOp other_create{other_account, other_auth, other_binding.package,
        {.network_binding = network_binding, .account_id = other_account, .authorization_commitment = other_binding.authorization_commitment},
        *other_root_pop, *other_authorization_pop};
    state.onboarding_pool = params.onboarding_bonus;
    BOOST_REQUIRE(ApplyAccountCreate(other_create, network_binding, 1, params, state) == AccountCreateStateError::NONE);
    state.genesis_allocations.emplace(IdentityKeyId{}, GenesisAllocation{.label = std::string{CENTRAL_AUTHORITY_NAME}});
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
    const auto payment_digest = ComputeIdentityOperationDigest(network_binding, payment.authorization);
    BOOST_REQUIRE(payment_digest);
    const auto payment_signature = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *payment_digest);
    BOOST_REQUIRE(payment_signature);
    payment.authorization.signature = *payment_signature;
    auto tampered_payment = payment;
    tampered_payment.payment.amount = 4;
    BOOST_CHECK(ApplyPayment(tampered_payment, network_binding, params, state) == PaymentError::INVALID_PAYLOAD);
    BOOST_CHECK(state.accounts.at(account).balance == 10);
    BOOST_CHECK(ApplyPayment(payment, network_binding, params, state) == PaymentError::NONE);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    BOOST_CHECK(state.accounts.at(other_account).balance == 3);
    BOOST_CHECK(FindCentralAuthorityAllocation(state)->balance == params.payment_fee);
    BOOST_CHECK(state.identities.Find(account)->nonce == 1);
    BOOST_CHECK(ApplyPayment(payment, network_binding, params, state) == PaymentError::INVALID_AUTHORIZATION);
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

    auto next_payment = payment;
    next_payment.payment.amount = 1;
    next_payment.authorization.nonce = 1;
    next_payment.authorization.payload_commitment = *ComputePaymentPayloadCommitment(next_payment.payment);
    const auto next_digest = ComputeIdentityOperationDigest(network_binding, next_payment.authorization);
    BOOST_REQUIRE(next_digest);
    const auto next_signature = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *next_digest);
    BOOST_REQUIRE(next_signature);
    next_payment.authorization.signature = *next_signature;
    const auto block_result = ExecuteBlockOperations(state, {ProtocolOperation{next_payment}}, network_binding, 2, params);
    BOOST_REQUIRE(block_result);
    BOOST_CHECK(block_result.state->accounts.at(account).balance == 6);
    BOOST_CHECK(block_result.state->accounts.at(other_account).balance == 4);
    BOOST_CHECK(FindCentralAuthorityAllocation(*block_result.state)->balance == 2 * params.payment_fee);
    BOOST_CHECK(block_result.state->onboarding_pool == state.onboarding_pool);
    BOOST_CHECK(state.accounts.at(account).balance == 7);
    BOOST_CHECK(FindCentralAuthorityAllocation(state)->balance == params.payment_fee);
    const auto replay_block = ExecuteBlockOperations(*block_result.state,
        {ProtocolOperation{next_payment}}, network_binding, 3, params);
    BOOST_CHECK(replay_block.error == BlockExecutionError::INVALID_PAYMENT);
    BOOST_CHECK(replay_block.payment_error == PaymentError::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(root_publication_is_identity_authorized_and_pays_deterministic_fee)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, authorization_seed{};
    root_seed[0] = 0x71;
    authorization_seed[0] = 0x72;
    uint256 raw_account{}, network_binding{};
    raw_account.begin()[0] = 0x73;
    network_binding.begin()[0] = 0x74;
    const AccountId account{raw_account};
    const auto root_key = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization_key = DeriveIdentityPublicKey(authorization_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root_key && authorization_key);
    const IdentityAuthorization identity_authorization{*root_key, *authorization_key};
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, identity_authorization);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(authorization_seed,
        IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, identity_authorization, binding.package,
        {.network_binding = network_binding, .account_id = account,
            .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state = CreateTestGenesisState();
    state.onboarding_pool = params.onboarding_bonus;

    BOOST_REQUIRE(ApplyAccountCreate(create, network_binding, 0, params, state) == AccountCreateStateError::NONE);

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
    const auto digest = ComputeIdentityOperationDigest(network_binding, operation_auth);
    BOOST_REQUIRE(digest);
    operation_auth.signature = *SignIdentityMessage(authorization_seed,
        IdentityKeyPurpose::AUTHORIZATION, *digest);
    const AuthorizedRootPublication operation{operation_auth, publication};
    const auto encoded = SerializeProtocolOperation(ProtocolOperation{operation});
    const auto fee = encoded ? ComputeRootPublicationFee(params, encoded->size(), publication.chunk_count) : std::nullopt;
    BOOST_REQUIRE(fee);
    const auto starting_balance = state.accounts.at(account).system_balance;

    BOOST_CHECK(ApplyRootPublication(operation, network_binding, params, state) == RootPublicationError::NONE);
    BOOST_CHECK(state.accounts.at(account).system_balance == starting_balance - *fee);
    BOOST_CHECK(FindCentralAuthorityAllocation(state)->balance == *fee);
    BOOST_CHECK(state.identities.Find(account)->nonce == 1);
    BOOST_CHECK(ApplyRootPublication(operation, network_binding, params, state) == RootPublicationError::INVALID_AUTHORIZATION);
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
    uint256 raw_account{}, network_binding{};
    raw_account.begin()[0] = 15;
    network_binding.begin()[0] = 16;
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
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(authorization_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_binding = network_binding, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;

    BOOST_REQUIRE(ApplyAccountCreate(create, network_binding, 0, params, state) == AccountCreateStateError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(account).authority, 0U);
    state.accounts.at(account).authority = 1'000'001;

    const IdentityAuthorization next_auth{*new_recovery, *new_authorization};
    const auto account_bytes = account.Value();
    const auto next_package_id = ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_binding.begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32}, 1, *new_package);
    BOOST_REQUIRE(next_package_id);
    IdentityRotate rotate{
        .account_id = account,
        .new_recovery_key = *new_recovery,
        .new_authorization_key = *new_authorization,
        .new_kem_package = *new_package,
        .nonce = 0,
        .key_epoch = 1,
    };
    const auto rotate_digest = ComputeIdentityRotateDigest(network_binding, rotate);
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

    const auto rotated = ExecuteBlockOperations(state, {operation}, network_binding, 1, params);
    BOOST_REQUIRE(rotated);
    const auto* record = rotated.state->identities.Find(account);
    BOOST_REQUIRE(record);
    BOOST_CHECK(record->recovery_key == *new_recovery);
    BOOST_CHECK(record->authorization_key == *new_authorization);
    BOOST_CHECK(record->kem_package_id == *next_package_id);
    BOOST_CHECK_EQUAL(record->key_epoch, 1U);
    BOOST_CHECK_EQUAL(record->nonce, 1U);
    const auto new_recovery_id = ComputeRecoveryKeyId(*new_recovery);
    BOOST_REQUIRE(new_recovery_id);
    BOOST_CHECK(rotated.state->identities.FindByRecoveryKeyId(*new_recovery_id) == account);
    // IdentityRotate preserves the account's AUTH and earns 0 AUTH (security maintenance).
    BOOST_CHECK_EQUAL(rotated.state->accounts.at(account).authority, 1'000'001U);
    const auto replay = ExecuteBlockOperations(*rotated.state, {operation}, network_binding, 2, params);
    BOOST_CHECK(replay.error == BlockExecutionError::INVALID_IDENTITY_ROTATE);
}

BOOST_AUTO_TEST_CASE(system_lock_wire_and_execution)
{
    using namespace cybou;
    std::array<unsigned char, 32> root_seed{}, device_seed{};
    root_seed[0] = 21;
    device_seed[0] = 22;
    uint256 raw_account{}, network_binding{};
    raw_account.begin()[0] = 23;
    network_binding.begin()[0] = 24;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_binding = network_binding, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;

    // A finalized AccountCreate earns no AUTH.
    const auto created = ExecuteBlockOperations(state, {ProtocolOperation{create}}, network_binding, 0, params);
    BOOST_REQUIRE(created);
    BOOST_CHECK_EQUAL(created.state->accounts.at(account).authority, 0U);
    BOOST_REQUIRE(ApplyAccountCreate(create, network_binding, 0, params, state) == AccountCreateStateError::NONE);

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
    const auto lock_digest = ComputeIdentityOperationDigest(network_binding, lock.authorization);
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
    const auto block_res = ExecuteBlockOperations(state, {lock_op}, network_binding, 1, params);
    BOOST_REQUIRE(block_res);
    BOOST_CHECK(block_res.state->accounts.at(account).balance == 30);
    BOOST_CHECK(block_res.state->accounts.at(account).system_balance == params.onboarding_bonus + 20);
    BOOST_CHECK(block_res.state->identities.Find(account)->nonce == 1);
    // A finalized SystemLock earns a flat +1 AUTH regardless of the locked amount.
    BOOST_CHECK_EQUAL(block_res.state->accounts.at(account).authority, 1U);
    BOOST_CHECK_EQUAL(TotalSupply(*block_res.state), TotalSupply(state));
    auto saturated = state;
    saturated.accounts.at(account).authority = std::numeric_limits<uint64_t>::max();
    const auto saturated_res = ExecuteBlockOperations(saturated, {lock_op}, network_binding, 1, params);
    BOOST_REQUIRE(saturated_res);
    BOOST_CHECK_EQUAL(saturated_res.state->accounts.at(account).authority, std::numeric_limits<uint64_t>::max());

    // Replay stale nonce fails
    const auto replay = ExecuteBlockOperations(*block_res.state, {lock_op}, network_binding, 2, params);
    BOOST_CHECK(replay.error == BlockExecutionError::INVALID_SYSTEM_LOCK);
    BOOST_CHECK(replay.lock_error == SystemLockError::INVALID_AUTHORIZATION);
}

BOOST_AUTO_TEST_CASE(state_validation_invariants)
{
    using namespace cybou;
    CybouState state{};

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
    uint256 raw_account{}, network_binding{};
    raw_account.begin()[0] = 53;
    network_binding.begin()[0] = 54;
    const AccountId account{raw_account};
    const auto root = DeriveIdentityPublicKey(root_seed, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto device = DeriveIdentityPublicKey(device_seed, IdentityKeyPurpose::AUTHORIZATION);
    BOOST_REQUIRE(root && device);

    const IdentityAuthorization auth{*root, *device};
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, auth);
    const auto root_pop = SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest);
    const auto authorization_pop = SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest);
    BOOST_REQUIRE(root_pop && authorization_pop);
    const AccountCreateOp create{account, auth, binding.package,
        {.network_binding = network_binding, .account_id = account, .authorization_commitment = binding.authorization_commitment},
        *root_pop, *authorization_pop};

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    params.name_claim_work_bits = 0;
    params.name_commit_min_depth = 1;
    params.name_commit_max_lifetime = 100;
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus;

    BOOST_REQUIRE(ApplyAccountCreate(create, network_binding, 0, params, state) == AccountCreateStateError::NONE);


    // 3. NameCommit
    const std::string label = "stanislav";
    std::array<unsigned char, 32> salt{};
    salt[0] = 77;
    const uint256 name_commit_hash = ComputeNameCommitment(network_binding, account, label, salt);

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
    const auto commit_digest = ComputeIdentityOperationDigest(network_binding, commit_op.authorization);
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
    const auto commit_block = ExecuteBlockOperations(state, {commit_proto_op}, network_binding, 10, params);
    BOOST_REQUIRE(commit_block);
    BOOST_REQUIRE(commit_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_CHECK_EQUAL(commit_block.state->names.pending_commits.at(name_commit_hash).commit_height, 10ULL);
    BOOST_CHECK_EQUAL(commit_block.state->identities.Find(account)->nonce, 1ULL);

    // 4. NameReveal
    AuthorizedNameReveal reveal_op{};
    reveal_op.reveal.label = label;
    reveal_op.reveal.salt = salt;
    reveal_op.reveal.work.network_binding = network_binding;
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
    const auto reveal_digest = ComputeIdentityOperationDigest(network_binding, reveal_op.authorization);
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
    const auto premature_res = ExecuteBlockOperations(*commit_block.state, {reveal_proto_op}, network_binding, 10, params);
    BOOST_CHECK(premature_res.error == BlockExecutionError::INVALID_NAME_REVEAL);
    BOOST_CHECK(premature_res.name_reveal_error == NameRevealError::INSUFFICIENT_COMMIT_DEPTH);

    // Successful reveal at height 12
    const auto reveal_block = ExecuteBlockOperations(*commit_block.state, {reveal_proto_op}, network_binding, 12, params);
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
    const auto sec_digest0 = ComputeIdentityOperationDigest(network_binding, second_commit_op.authorization);
    BOOST_REQUIRE(sec_digest0.has_value());
    second_commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *sec_digest0);
    const auto pending_commit_res = ExecuteBlockOperations(*commit_block.state, {second_commit_op}, network_binding, 11, params);
    BOOST_CHECK(pending_commit_res.error == BlockExecutionError::INVALID_NAME_COMMIT);
    BOOST_CHECK(pending_commit_res.name_commit_error == NameCommitError::ACCOUNT_HAS_PENDING_COMMIT);

    // A. Account already has a name -> cannot commit another name
    second_commit_op.authorization.nonce = 2;
    const auto sec_digest = ComputeIdentityOperationDigest(network_binding, second_commit_op.authorization);
    BOOST_REQUIRE(sec_digest.has_value());
    second_commit_op.authorization.signature = *SignIdentityMessage(device_seed, IdentityKeyPurpose::AUTHORIZATION, *sec_digest);
    const auto double_commit_res = ExecuteBlockOperations(*reveal_block.state, {second_commit_op}, network_binding, 13, params);
    BOOST_CHECK(double_commit_res.error == BlockExecutionError::INVALID_NAME_COMMIT);
    BOOST_CHECK(double_commit_res.name_commit_error == NameCommitError::ACCOUNT_ALREADY_HAS_NAME);

    // B. Expired commit & deterministic block pruning
    params.name_commit_max_lifetime = 5;
    // Block at height 16 automatically prunes the expired commit from height 10 (16 > 10 + 5)
    const auto prune_block = ExecuteBlockOperations(*commit_block.state, {}, network_binding, 16, params);
    BOOST_REQUIRE(prune_block);
    BOOST_CHECK(!prune_block.state->names.pending_commits.contains(name_commit_hash));
    BOOST_CHECK(prune_block.state->names.pending_commits.empty());

    // Once pruned, the commitment is no longer found for reveal
    const auto expired_res = ExecuteBlockOperations(*commit_block.state, {reveal_proto_op}, network_binding, 16, params);
    BOOST_CHECK(expired_res.error == BlockExecutionError::INVALID_NAME_REVEAL);
    BOOST_CHECK(expired_res.name_reveal_error == NameRevealError::COMMITMENT_NOT_FOUND);

    // Direct ApplyNameReveal check for COMMIT_EXPIRED on unpruned state
    auto unpruned_state = *commit_block.state;
    BOOST_CHECK(ApplyNameReveal(reveal_op, network_binding, 16, params, unpruned_state) == NameRevealError::COMMIT_EXPIRED);
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

BOOST_AUTO_TEST_CASE(unversioned_canonical_api_workflow)
{
    using namespace cybou;

    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;

    // Build unversioned CybouState
    CybouState state{};
    state.onboarding_pool = params.onboarding_bonus * 5;

    uint256 network_binding{};
    network_binding.begin()[0] = 0xAA;
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
    const auto binding = test::MakeIdentityKemBinding(network_binding, account, auth);

    const AccountCreateOp create_op{
        .account_id = account,
        .authorization = auth,
        .kem_package = binding.package,
        .work = {
            .network_binding = network_binding,
            .account_id = account,
            .authorization_commitment = binding.authorization_commitment,
            .work_epoch = 0,
            .nonce = 0,
        },
        .recovery_pop = *SignIdentityMessage(root_seed, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest),
        .authorization_pop = *SignIdentityMessage(dev_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest),
    };

    // Apply unversioned AccountCreate
    BOOST_CHECK(ApplyAccountCreate(create_op, network_binding, 0, params, state) == AccountCreateStateError::NONE);
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

    const auto block_res = ExecuteBlockOperations(parent, {*decoded_proto}, network_binding, 0, params);
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
    state.genesis_allocations.emplace(IdentityKeyId{}, GenesisAllocation{.balance = 2'000'000});

    uint256 acc_raw{};
    acc_raw.begin()[0] = 0x11;
    const AccountId acc{acc_raw};
    state.accounts.emplace(acc, AccountState{
        .balance = 3'000'000,
        .system_balance = 500'000,
    });

    // Total supply calculation
    BOOST_CHECK_EQUAL(TotalSupply(state), 1'000'000ULL + 2'000'000ULL + 3'000'000ULL + 500'000ULL);

    // Over-supply check (with valid zero-account state and valid zero-account state)
    CybouState overflow_state{};

    overflow_state.onboarding_pool = 100'000'000'001ULL;
    BOOST_CHECK(ValidateCybouState(overflow_state) == StateValidationError::BALANCE_OVERFLOW);
}


BOOST_AUTO_TEST_CASE(central_authority_fee_lifecycle_and_atomic_failures)
{
    using namespace cybou;
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;
    uint256 network{};
    network.begin()[0] = 0xCA;
    const auto make_create = [&](unsigned char tag) {
        std::array<unsigned char, 32> recovery{}, authorization{};
        recovery[0] = tag;
        authorization[0] = tag + 1;
        uint256 raw{};
        raw.begin()[0] = tag;
        const AccountId account{raw};
        const IdentityAuthorization keys{
            *DeriveIdentityPublicKey(recovery, IdentityKeyPurpose::RECOVERY_ROOT),
            *DeriveIdentityPublicKey(authorization, IdentityKeyPurpose::AUTHORIZATION)};
        const auto binding = test::MakeIdentityKemBinding(network, account, keys);
        return AccountCreateOp{account, keys, binding.package,
            {.network_binding = network, .account_id = account, .authorization_commitment = binding.authorization_commitment},
            *SignIdentityMessage(recovery, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest),
            *SignIdentityMessage(authorization, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest)};
    };
    const auto sender_create = make_create(0x31);
    const auto authority_create = make_create(0x41);
    const auto sender = sender_create.account_id;
    const auto authority = authority_create.account_id;
    const auto recovery_id = *ComputeRecoveryKeyId(authority_create.authorization.recovery_root);
    auto state = CreateDevGenesisState();
    BOOST_CHECK_EQUAL(state.onboarding_pool, 100'000'000U);
    state.genesis_allocations.emplace(recovery_id, GenesisAllocation{
        .balance = 100'000'000, .authority = 1'000'001, .label = std::string{CENTRAL_AUTHORITY_NAME}});
    const auto initial_supply = TotalSupply(state);
    BOOST_REQUIRE(ApplyAccountCreate(sender_create, network, 0, params, state) == AccountCreateStateError::NONE);
    BOOST_CHECK_EQUAL(state.onboarding_pool, DEV_ONBOARDING_POOL - params.onboarding_bonus);
    RootPublication publication;
    publication.root_chunk_id[0] = 1;
    publication.chunk_authorization_root[0] = 2;
    publication.chunk_count = 1;
    publication.recipient_capsules.resize(1);
    AuthorizedRootPublication op{{.account_id = sender, .kind = IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *ComputeRootPublicationPayloadCommitment(publication)}, publication};
    std::array<unsigned char, 32> authorization_seed{};
    authorization_seed[0] = 0x32;
    const auto sign_publication = [&] {
        op.authorization.signature = *SignIdentityMessage(authorization_seed, IdentityKeyPurpose::AUTHORIZATION,
            *ComputeIdentityOperationDigest(network, op.authorization));
    };
    sign_publication();
    const auto fee = *ComputeRootPublicationFee(params, SerializeProtocolOperation(ProtocolOperation{op})->size(), 1);
    const auto assert_rejected_unchanged = [&](CybouState damaged, RootPublicationError expected) {
        const auto accounts = damaged.accounts;
        const auto allocations = damaged.genesis_allocations;
        const auto identities = SerializeIdentityRegistry(damaged.identities);
        const auto pool = damaged.onboarding_pool;
        BOOST_CHECK(ApplyRootPublication(op, network, params, damaged) == expected);
        BOOST_CHECK(damaged.accounts == accounts);
        BOOST_CHECK(damaged.genesis_allocations == allocations);
        BOOST_CHECK(SerializeIdentityRegistry(damaged.identities) == identities);
        BOOST_CHECK_EQUAL(damaged.onboarding_pool, pool);
    };
    auto damaged = state;
    damaged.accounts.at(sender).system_balance = fee - 1;
    assert_rejected_unchanged(damaged, RootPublicationError::INSUFFICIENT_SYSTEM_BALANCE);
    damaged = state;
    damaged.genesis_allocations.clear();
    assert_rejected_unchanged(damaged, RootPublicationError::FEE_TRANSFER_FAILED);
    damaged = state;
    damaged.genesis_allocations.at(recovery_id).balance = std::numeric_limits<uint64_t>::max() - fee + 1;
    assert_rejected_unchanged(damaged, RootPublicationError::FEE_TRANSFER_FAILED);
    damaged = state;
    damaged.genesis_allocations.emplace(IdentityKeyId{}, damaged.genesis_allocations.at(recovery_id));
    assert_rejected_unchanged(damaged, RootPublicationError::FEE_TRANSFER_FAILED);
    damaged = state;
    damaged.genesis_allocations.at(recovery_id).claimed_by = authority;
    assert_rejected_unchanged(damaged, RootPublicationError::FEE_TRANSFER_FAILED);

    const auto pool = state.onboarding_pool;
    const auto payer_budget = state.accounts.at(sender).system_balance;
    const auto finalized = ExecuteBlockOperations(state, {ProtocolOperation{op}}, network, 1, params);
    BOOST_REQUIRE(finalized);
    state = *finalized.state;
    BOOST_CHECK_EQUAL(state.genesis_allocations.at(recovery_id).balance, 100'000'000 + fee);
    BOOST_CHECK_EQUAL(state.accounts.at(sender).system_balance, payer_budget - fee);
    BOOST_CHECK_EQUAL(state.onboarding_pool, pool);
    BOOST_CHECK_EQUAL(TotalSupply(state), initial_supply);
    BOOST_REQUIRE(ApplyAccountCreate(authority_create, network, 2, params, state) == AccountCreateStateError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(authority).balance, 100'000'000 + fee);
    BOOST_CHECK_EQUAL(state.onboarding_pool, pool - params.onboarding_bonus);
    BOOST_CHECK_EQUAL(TotalSupply(state), initial_supply);
    const auto claimed_allocation_balance = state.genesis_allocations.at(recovery_id).balance;
    op.authorization.nonce = 1;
    sign_publication();
    damaged = state;
    damaged.accounts.at(authority).balance = std::numeric_limits<uint64_t>::max() - fee + 1;
    assert_rejected_unchanged(damaged, RootPublicationError::FEE_TRANSFER_FAILED);
    damaged = state;
    damaged.names.names.erase(std::string{CENTRAL_AUTHORITY_NAME});
    assert_rejected_unchanged(damaged, RootPublicationError::FEE_TRANSFER_FAILED);
    BOOST_REQUIRE(ApplyRootPublication(op, network, params, state) == RootPublicationError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(authority).balance, 100'000'000 + 2 * fee);
    BOOST_CHECK_EQUAL(state.genesis_allocations.at(recovery_id).balance, claimed_allocation_balance);
    BOOST_CHECK_EQUAL(state.onboarding_pool, pool - params.onboarding_bonus);
    BOOST_CHECK_EQUAL(TotalSupply(state), initial_supply);
    const auto bytes = SerializeCybouState(state);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->front(), 12);
    const auto restored = DeserializeCybouState(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(SerializeCybouState(*restored) == bytes);
    BOOST_CHECK(CybouStateHash(*restored) == CybouStateHash(state));
    auto v11 = *bytes;
    v11[0] = 11;
    v11.insert(v11.begin() + 9, 16, 0); // actual obsolete pool layout
    BOOST_CHECK(!DeserializeCybouState(v11));

    // Payment to Central Authority credits both the amount and its entire fee.
    state.accounts.at(sender).balance = 5;
    AuthorizedPayment payment{{.account_id = sender, .nonce = 2, .kind = IdentityOperationKind::PAYMENT}, {authority, 5}};
    payment.authorization.payload_commitment = *ComputePaymentPayloadCommitment(payment.payment);
    payment.authorization.signature = *SignIdentityMessage(authorization_seed, IdentityKeyPurpose::AUTHORIZATION,
        *ComputeIdentityOperationDigest(network, payment.authorization));
    damaged = state;
    damaged.accounts.at(authority).balance = std::numeric_limits<uint64_t>::max() - 5;
    const auto previous_accounts = damaged.accounts;
    const auto previous_identities = SerializeIdentityRegistry(damaged.identities);
    BOOST_CHECK(ApplyPayment(payment, network, params, damaged) == PaymentError::FEE_TRANSFER_FAILED);
    BOOST_CHECK(damaged.accounts == previous_accounts);
    BOOST_CHECK(SerializeIdentityRegistry(damaged.identities) == previous_identities);
    const auto payment_supply = TotalSupply(state);
    const auto ca_balance = state.accounts.at(authority).balance;
    BOOST_REQUIRE(ApplyPayment(payment, network, params, state) == PaymentError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(authority).balance, ca_balance + 5 + params.payment_fee);
    BOOST_CHECK_EQUAL(TotalSupply(state), payment_supply);
    BOOST_CHECK_EQUAL(state.genesis_allocations.at(recovery_id).balance, claimed_allocation_balance);

    // When Central Authority pays, its own protocol fee still returns to Balance.
    AuthorizedPayment return_payment{{.account_id = authority, .kind = IdentityOperationKind::PAYMENT}, {sender, 5}};
    return_payment.authorization.payload_commitment = *ComputePaymentPayloadCommitment(return_payment.payment);
    std::array<unsigned char, 32> ca_seed{};
    ca_seed[0] = 0x42;
    return_payment.authorization.signature = *SignIdentityMessage(ca_seed, IdentityKeyPurpose::AUTHORIZATION,
        *ComputeIdentityOperationDigest(network, return_payment.authorization));
    const auto previous_ca_balance = state.accounts.at(authority).balance;
    const auto previous_ca_budget = state.accounts.at(authority).system_balance;
    const auto previous_supply = TotalSupply(state);
    BOOST_REQUIRE(ApplyPayment(return_payment, network, params, state) == PaymentError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(authority).balance, previous_ca_balance - 5 + params.payment_fee);
    BOOST_CHECK_EQUAL(state.accounts.at(authority).system_balance, previous_ca_budget - params.payment_fee);
    BOOST_CHECK_EQUAL(TotalSupply(state), previous_supply);
}

BOOST_AUTO_TEST_CASE(authority_earning_utility_bound_and_velocity_capped)
{
    using namespace cybou;
    uint256 network{};
    network.begin()[0] = 0x11;
    auto params = DevProtocolParameters();
    params.account_creation_work_bits = 0;

    const auto make_account = [&](unsigned char seed_byte) {
        std::array<unsigned char, 32> recovery{}, authorization{};
        recovery[0] = seed_byte;
        authorization[0] = static_cast<unsigned char>(seed_byte + 1);
        uint256 raw{};
        raw.begin()[0] = seed_byte;
        const AccountId account{raw};
        const IdentityAuthorization keys{
            *DeriveIdentityPublicKey(recovery, IdentityKeyPurpose::RECOVERY_ROOT),
            *DeriveIdentityPublicKey(authorization, IdentityKeyPurpose::AUTHORIZATION)};
        const auto binding = test::MakeIdentityKemBinding(network, account, keys);
        AccountCreateOp create{account, keys, binding.package,
            {.network_binding = network, .account_id = account, .authorization_commitment = binding.authorization_commitment},
            *SignIdentityMessage(recovery, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest),
            *SignIdentityMessage(authorization, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest)};
        return std::make_pair(create, authorization);
    };

    const auto [sender_create, sender_auth_seed] = make_account(0x51);
    const auto [recipient_create, recipient_auth_seed] = make_account(0x61);
    const auto account_id = sender_create.account_id;
    const auto recipient_id = recipient_create.account_id;

    auto state = CreateDevGenesisState();
    state.genesis_allocations.emplace(IdentityKeyId{}, GenesisAllocation{
        .balance = 100'000'000, .authority = 1'000'001, .label = std::string{CENTRAL_AUTHORITY_NAME}});
    BOOST_REQUIRE(ApplyAccountCreate(sender_create, network, 0, params, state) == AccountCreateStateError::NONE);
    BOOST_REQUIRE(ApplyAccountCreate(recipient_create, network, 0, params, state) == AccountCreateStateError::NONE);

    state.accounts.at(account_id).balance = 1000;
    state.accounts.at(recipient_id).balance = 100;

    // 1. Payment does NOT earn AUTH
    AuthorizedPayment payment{
        {.account_id = account_id, .nonce = 0, .kind = IdentityOperationKind::PAYMENT},
        {recipient_id, 10}
    };
    payment.authorization.payload_commitment = *ComputePaymentPayloadCommitment(payment.payment);
    payment.authorization.signature = *SignIdentityMessage(sender_auth_seed, IdentityKeyPurpose::AUTHORIZATION,
        *ComputeIdentityOperationDigest(network, payment.authorization));

    const auto payment_res = ExecuteBlockOperations(state, {payment}, network, 1, params);
    BOOST_REQUIRE(payment_res);
    BOOST_CHECK_EQUAL(payment_res.state->accounts.at(account_id).authority, 0U);

    // 2. Velocity limit: two SystemLock operations by the same account in the SAME block
    AuthorizedSystemLock lock1{
        {.account_id = account_id, .nonce = 1, .kind = IdentityOperationKind::SYSTEM_LOCK},
        SystemLockPayload{5}
    };
    lock1.authorization.payload_commitment = *ComputeSystemLockPayloadCommitment(lock1.lock);
    lock1.authorization.signature = *SignIdentityMessage(sender_auth_seed, IdentityKeyPurpose::AUTHORIZATION,
        *ComputeIdentityOperationDigest(network, lock1.authorization));

    AuthorizedSystemLock lock2{
        {.account_id = account_id, .nonce = 2, .kind = IdentityOperationKind::SYSTEM_LOCK},
        SystemLockPayload{5}
    };
    lock2.authorization.payload_commitment = *ComputeSystemLockPayloadCommitment(lock2.lock);
    lock2.authorization.signature = *SignIdentityMessage(sender_auth_seed, IdentityKeyPurpose::AUTHORIZATION,
        *ComputeIdentityOperationDigest(network, lock2.authorization));

    // Execute both locks in block height 2: authority must increase by only +1 (not +2)
    const auto double_lock_res = ExecuteBlockOperations(*payment_res.state, {lock1, lock2}, network, 2, params);
    BOOST_REQUIRE(double_lock_res);
    BOOST_CHECK_EQUAL(double_lock_res.state->accounts.at(account_id).authority, 1U);

    // 3. In the next block, another SystemLock grants +1 AUTH
    AuthorizedSystemLock lock3{
        {.account_id = account_id, .nonce = 3, .kind = IdentityOperationKind::SYSTEM_LOCK},
        SystemLockPayload{5}
    };
    lock3.authorization.payload_commitment = *ComputeSystemLockPayloadCommitment(lock3.lock);
    lock3.authorization.signature = *SignIdentityMessage(sender_auth_seed, IdentityKeyPurpose::AUTHORIZATION,
        *ComputeIdentityOperationDigest(network, lock3.authorization));

    const auto next_block_res = ExecuteBlockOperations(*double_lock_res.state, {lock3}, network, 3, params);
    BOOST_REQUIRE(next_block_res);
    BOOST_CHECK_EQUAL(next_block_res.state->accounts.at(account_id).authority, 2U);

    // 4. Verify AuthorityEarningAccount behavior across op types
    BOOST_CHECK(!AuthorityEarningAccount(payment));
    BOOST_CHECK(AuthorityEarningAccount(lock1) == account_id);
}

BOOST_AUTO_TEST_SUITE_END()
