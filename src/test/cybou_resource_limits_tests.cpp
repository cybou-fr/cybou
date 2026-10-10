// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/block_executor.h>
#include <test/cybou_settlement_test_helpers.h>
#include <cybou/operation_work.h>
#include <cybou/protocol_limits.h>
#include <test/cybou_test_helpers.h>
#include "cybou_test_identity_helpers.h"

#include <boost/test/unit_test.hpp>

#include <array>
#include <limits>
#include <optional>
#include <random>
#include <vector>

using namespace cybou;

namespace {

/// One funded T0 Identity on a valid genesis state, plus helpers to sign its operations.
struct LimitsFixture {
    cybou::Hash256 network{};
    CybouProtocolParameters params{DevProtocolParameters()};
    std::array<unsigned char, 32> authorization_seed{};
    AccountId account;
    AccountId other;
    CybouState state;
    uint64_t nonce{0};

    LimitsFixture()
    {
        network.begin()[0] = 0x21;
        params.account_creation_work_bits = 0;
        std::array<unsigned char, 32> recovery{};
        recovery[0] = 0x31;
        authorization_seed[0] = 0x32;
        cybou::Hash256 raw{};
        raw.begin()[0] = 0x33;
        account = AccountId{raw};
        const IdentityAuthorization keys{
            *DeriveIdentityPublicKey(recovery, IdentityKeyPurpose::RECOVERY_ROOT),
            *DeriveIdentityPublicKey(authorization_seed, IdentityKeyPurpose::AUTHORIZATION)};
        const auto binding = test::MakeIdentityKemBinding(network, account, keys);
        const AccountCreateOp create{account, keys, binding.package,
            {.network_binding = network, .account_id = account, .authorization_commitment = binding.authorization_commitment},
            *SignIdentityMessage(recovery, IdentityKeyPurpose::RECOVERY_ROOT, binding.pop_digest),
            *SignIdentityMessage(authorization_seed, IdentityKeyPurpose::AUTHORIZATION, binding.pop_digest)};
        state = CreateDevGenesisState();
        state.genesis_allocations.emplace(IdentityKeyId{}, GenesisAllocation{
            .balance = 100'000'000, .label = std::string{CENTRAL_AUTHORITY_NAME}});
        BOOST_REQUIRE(ApplyAccountCreate(create, network, 0, params, state) == AccountCreateStateError::NONE);
        std::array<unsigned char, 32> other_recovery{}, other_authorization{};
        other_recovery[0] = 0x41;
        other_authorization[0] = 0x42;
        cybou::Hash256 other_raw{};
        other_raw.begin()[0] = 0x43;
        other = AccountId{other_raw};
        const IdentityAuthorization other_keys{
            *DeriveIdentityPublicKey(other_recovery, IdentityKeyPurpose::RECOVERY_ROOT),
            *DeriveIdentityPublicKey(other_authorization, IdentityKeyPurpose::AUTHORIZATION)};
        const auto other_binding = test::MakeIdentityKemBinding(network, other, other_keys);
        const AccountCreateOp other_create{other, other_keys, other_binding.package,
            {.network_binding = network, .account_id = other, .authorization_commitment = other_binding.authorization_commitment},
            *SignIdentityMessage(other_recovery, IdentityKeyPurpose::RECOVERY_ROOT, other_binding.pop_digest),
            *SignIdentityMessage(other_authorization, IdentityKeyPurpose::AUTHORIZATION, other_binding.pop_digest)};
        BOOST_REQUIRE(ApplyAccountCreate(other_create, network, 0, params, state) == AccountCreateStateError::NONE);
        state.accounts.at(account).balance = 1'000'000;
        state.accounts.at(account).system_balance = 100'000'000;
    }

    IdentityOperationAuthorization Authorize(IdentityOperationKind kind, const IdentityKeyId& commitment)
    {
        IdentityOperationAuthorization auth{.account_id = account, .nonce = nonce++, .key_epoch = 0,
            .kind = kind, .payload_commitment = commitment};
        auth.signature = *SignIdentityMessage(authorization_seed, IdentityKeyPurpose::AUTHORIZATION,
            *ComputeIdentityOperationDigest(network, auth));
        return auth;
    }

    ProtocolOperation Lock()
    {
        const SystemLockPayload lock{1};
        return AuthorizedSystemLock{Authorize(IdentityOperationKind::SYSTEM_LOCK, *ComputeSystemLockPayloadCommitment(lock)), lock};
    }

    ProtocolOperation Publish(uint32_t chunks, unsigned char tag, uint32_t lease_periods = 0)
    {
        RootPublication publication;
        publication.lease_periods = lease_periods;
        publication.root_chunk_id.fill(tag);
        publication.chunk_authorization_root.fill(static_cast<unsigned char>(tag + 1));
        publication.chunk_count = chunks;
        RootRecipientCapsule capsule;
        capsule.encapsulation.fill(0x53);
        capsule.wrapped_content_key.fill(0x64);
        publication.recipient_capsules.push_back(capsule);
        return AuthorizedRootPublication{Authorize(IdentityOperationKind::ROOT_PUBLICATION,
            *ComputeRootPublicationPayloadCommitment(publication)), publication};
    }

    ProtocolOperation Revoke(const cybou::Hash256& publication_id)
    {
        const RevokePublicationPayload revoke{publication_id};
        return AuthorizedRevokePublication{Authorize(IdentityOperationKind::REVOKE_PUBLICATION,
            *ComputeRevokePublicationPayloadCommitment(revoke)), revoke};
    }

    ProtocolOperation Lease(const cybou::Hash256& publication_id, uint32_t periods)
    {
        const StorageLeasePayload lease{publication_id, periods};
        return AuthorizedStorageLease{Authorize(IdentityOperationKind::STORAGE_LEASE,
            *ComputeStorageLeasePayloadCommitment(lease)), lease};
    }

    /// PoA-signed settlement for the next period, signed by the test PoA key.
    ProtocolOperation Settle(uint64_t start_utc, std::vector<StorageSettlementEntry> entries)
    {
        StorageSettlement settlement{.period = state.settlement.next_period, .period_start_utc = start_utc,
            .entries = std::move(entries)};
        test::PayWindow(settlement, params.storage_settlement_period_seconds);
        test::FixturePayEntries(settlement, state);
        std::array<unsigned char, 32> poa_seed{};
        poa_seed[0] = 0xA7;
        settlement.poa_signature = *SignIdentityMessage(poa_seed, IdentityKeyPurpose::POA_FINALIZER,
            *ComputeStorageSettlementDigest(network, settlement));
        return settlement;
    }

    BlockExecutionResult Execute(const std::vector<ProtocolOperation>& operations, uint64_t height)
    {
        const auto poa_key = TestPoaFinalizerPublicKey();
        auto result = ExecuteBlockOperations(state, operations, network, height, params, &poa_key);
        if (result) state = *result.state;
        return result;
    }
};

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_resource_limits_tests)

BOOST_AUTO_TEST_CASE(accepted_assignment_state_codec_vectors)
{
    // Fixed key/account fixture: byte-layout vectors, independent of randomized runtime identities.
    LimitsFixture f;
    const Hash256 publication{91}; ChunkId root{}; root[0] = 7;
    BOOST_REQUIRE(RecordPublication(f.state, publication, f.account, root, 1, 1));
    const auto escrow = ComputeStorageLeaseEscrow(f.params, 1, 1, 30); BOOST_REQUIRE(escrow);
    FundStorageLease(f.state, publication, f.account, 1, 1, 30, *escrow, f.params, publication);
    auto& term = f.state.leases.at(publication).funded_terms[0];
    std::array<unsigned char,32> storage{}; storage[0] = 8;
    term.declarations.push_back({Hash256{93}, 1, 10, {{storage, f.other, 0, 10}}});
    term.next_assignment_epoch = 2;
    BOOST_REQUIRE(ValidateCybouState(f.state) == StateValidationError::NONE);
    const auto prepared = CybouStateHash(f.state); BOOST_REQUIRE(prepared);
    BOOST_CHECK_EQUAL(prepared->GetHex(), "0762dfdeed75bda796b79112da2c3076d0913729e0435222220de1592066b557");
    term.assignments.push_back({Hash256{92}, Hash256{93}, Hash256{94}, 1, 0, {{0,storage,f.other,1}}});
    BOOST_REQUIRE(ValidateCybouState(f.state) == StateValidationError::NONE);
    const auto activated = CybouStateHash(f.state); BOOST_REQUIRE(activated);
    BOOST_CHECK_EQUAL(activated->GetHex(), "3742f90006ab43ac62b1336f93a83260696e9341b75afdafd894eebb14c7bb60");
    const auto bytes = SerializeCybouState(f.state); BOOST_REQUIRE(bytes);
    const auto decoded = DeserializeCybouState(*bytes); BOOST_REQUIRE(decoded);
    BOOST_CHECK(CybouStateHash(*decoded) == activated);
    BOOST_CHECK(SerializeCybouState(*decoded) == bytes);
    auto corrupt = *bytes; corrupt.push_back(0); BOOST_CHECK(!DeserializeCybouState(corrupt));
    corrupt = *bytes; corrupt.pop_back(); BOOST_CHECK(!DeserializeCybouState(corrupt));
    corrupt = *bytes; std::fill_n(corrupt.end() - 20 - 69 - 4, 4, 255); BOOST_CHECK(!DeserializeCybouState(corrupt));
    corrupt = *bytes; std::fill_n(corrupt.end() - 20 - 4, 4, 0); BOOST_CHECK(!DeserializeCybouState(corrupt));
}

BOOST_AUTO_TEST_CASE(publications_are_not_limited_by_auth)
{
    // DEC-274: a T0 Identity publishes beyond the former 5 GiB credit; only the safety bound applies.
    LimitsFixture f;
    uint64_t height{1};
    for (unsigned char i{0}; i < 6; ++i) BOOST_REQUIRE(f.Execute({f.Publish(2048, 0x20 + 2 * i)}, height++));
    BOOST_CHECK_EQUAL(f.state.publications.size(), 6U);
    const auto oversized = f.Execute({f.Publish(MAX_PUBLICATION_CHUNKS + 1, 0x40)}, height);
    BOOST_CHECK(oversized.root_publication_error == RootPublicationError::INVALID_PAYLOAD);
}

BOOST_AUTO_TEST_CASE(revoke_publication_frees_quota_and_is_owner_only)
{
    LimitsFixture f;
    const auto publication = f.Publish(100, 0x50);
    const auto publication_id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    BOOST_REQUIRE(f.state.publications.contains(publication_id));
    BOOST_CHECK(f.state.publications.at(publication_id).owner == f.account);

    cybou::Hash256 unknown{};
    unknown.begin()[0] = 0x99;
    const auto missing = f.Execute({f.Revoke(unknown)}, 2);
    BOOST_CHECK(missing.error == BlockExecutionError::INVALID_REVOKE_PUBLICATION);
    BOOST_CHECK(missing.revoke_error == RevokePublicationError::PUBLICATION_NOT_FOUND);
    --f.nonce;

    // Another Identity cannot revoke it.
    auto foreign = f.state;
    foreign.publications.at(publication_id).owner = f.other;
    BOOST_REQUIRE(ValidateCybouState(foreign) == StateValidationError::NONE);
    const auto stolen = ExecuteBlockOperations(foreign, {f.Revoke(publication_id)}, f.network, 2, f.params);
    BOOST_CHECK(stolen.revoke_error == RevokePublicationError::NOT_OWNER);
    --f.nonce;

    const auto system_before = f.state.accounts.at(f.account).system_balance;
    const auto revoke = f.Revoke(publication_id);
    BOOST_REQUIRE(f.Execute({revoke}, 2));
    BOOST_CHECK(!f.state.publications.contains(publication_id));
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.account).system_balance, system_before - f.params.payment_fee);
    // Wire round-trip.
    const auto bytes = SerializeProtocolOperation(revoke);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), 1 + AUTHORIZED_REVOKE_PUBLICATION_SIZE);
    BOOST_CHECK(DeserializeProtocolOperation(*bytes) == std::optional<ProtocolOperation>{revoke});
    // Once revoked it cannot be revoked again.
    BOOST_CHECK(f.Execute({f.Revoke(publication_id)}, 3).revoke_error == RevokePublicationError::PUBLICATION_NOT_FOUND);
}

BOOST_AUTO_TEST_CASE(resource_and_storage_sections_roundtrip_exactly)
{
    LimitsFixture f;
    const auto empty = SerializeCybouState(f.state);
    BOOST_REQUIRE(empty);
    auto trailing = *empty;
    trailing.push_back(0);
    BOOST_CHECK(!DeserializeCybouState(trailing));

    BOOST_REQUIRE(f.Execute({f.Publish(7, 0x60)}, 5));
    const auto published = *ComputeOperationId(f.Publish(7, 0x60));
    --f.nonce;
    (void)published;
    const auto bytes = SerializeCybouState(f.state);
    BOOST_REQUIRE(bytes);
    const auto decoded = DeserializeCybouState(*bytes);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(decoded->publications == f.state.publications);
    BOOST_CHECK(CybouStateHash(*decoded) == CybouStateHash(f.state));
}

// ---- Storage economy (DEC-274..DEC-283) ----

BOOST_AUTO_TEST_CASE(account_create_moves_onboarding_bonus_from_treasury_without_minting)
{
    LimitsFixture f;
    // Both fixture accounts were onboarded from the 100,000,000 Treasury allocation.
    const auto* treasury = FindCentralAuthorityAllocation(f.state);
    BOOST_REQUIRE(treasury);
    BOOST_CHECK_EQUAL(treasury->balance, 100'000'000U - 2 * ONBOARDING_BONUS);
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.other).system_balance, ONBOARDING_BONUS);
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.other).onboarding_system_balance, ONBOARDING_BONUS);
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.other).balance, 0U);
}

BOOST_AUTO_TEST_CASE(onboarding_origin_is_spent_first)
{
    AccountState account{.system_balance = 100, .onboarding_system_balance = 30};
    BOOST_CHECK_EQUAL(DebitSystemBalance(account, 20), 20U);
    BOOST_CHECK_EQUAL(account.onboarding_system_balance, 10U);
    BOOST_CHECK_EQUAL(DebitSystemBalance(account, 50), 10U);
    BOOST_CHECK_EQUAL(account.onboarding_system_balance, 0U);
    BOOST_CHECK_EQUAL(account.system_balance, 30U);
}

BOOST_AUTO_TEST_CASE(storage_lease_locks_rent_in_escrow_and_conserves_cybou)
{
    LimitsFixture f;
    const auto publication = f.Publish(2048, 0x70); // 1 GiB
    const auto publication_id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    const auto total = TotalCybou(f.state);
    const auto system_before = f.state.accounts.at(f.account).system_balance;
    const auto* treasury = FindCentralAuthorityAllocation(f.state);
    const auto treasury_before = treasury->balance;

    BOOST_REQUIRE(f.Execute({f.Lease(publication_id, 3)}, 2));
    // 1 GiB x 2 replicas x 5 CYBOU x 3 days = 30 CYBOU in escrow, plus the ordinary fee to Treasury.
    const auto& lease = f.state.leases.at(publication_id);
    BOOST_CHECK_EQUAL(lease.escrow_onboarding + lease.escrow_locked, 30U);
    BOOST_CHECK_EQUAL(lease.units, 2048U);
    BOOST_CHECK_EQUAL(lease.replicas, 2U);
    BOOST_CHECK_EQUAL(lease.first_period, 0U);
    BOOST_CHECK_EQUAL(lease.end_period, 3U);
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.account).system_balance, system_before - 30 - f.params.payment_fee);
    BOOST_CHECK_EQUAL(FindCentralAuthorityAllocation(f.state)->balance, treasury_before + f.params.payment_fee);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);
    // Onboarding-origin CYBOU went first into escrow and keeps its origin.
    BOOST_CHECK_EQUAL(lease.escrow_onboarding, 30U);

    // A second lease extends the same record.
    BOOST_REQUIRE(f.Execute({f.Lease(publication_id, 2)}, 3));
    BOOST_CHECK_EQUAL(f.state.leases.at(publication_id).end_period, 5U);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);

    // Only the owner can lease, and only a live publication.
    cybou::Hash256 unknown{};
    unknown.begin()[0] = 0x77;
    BOOST_CHECK(f.Execute({f.Lease(unknown, 1)}, 4).lease_error == StorageLeaseError::PUBLICATION_NOT_FOUND);
    --f.nonce;

    // Wire round-trip.
    const auto op = f.Lease(publication_id, 1);
    const auto bytes = SerializeProtocolOperation(op);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), 1 + AUTHORIZED_STORAGE_LEASE_SIZE);
    BOOST_CHECK(DeserializeProtocolOperation(*bytes) == std::optional<ProtocolOperation>{op});
}

BOOST_AUTO_TEST_CASE(settlement_pays_providers_by_origin_and_refunds_expired_escrow)
{
    LimitsFixture f;
    f.params.storage_replica_target = 1;
    const auto publication = f.Publish(2048, 0x80);
    const auto publication_id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    // Spend the onboarding origin first so the escrow mixes both origins: 20,000 - fees.
    auto& owner = f.state.accounts.at(f.account);
    owner.onboarding_system_balance = 5; // 5 onboarding + rest SystemLock origin
    BOOST_REQUIRE(f.Execute({f.Lease(publication_id, 2)}, 2));
    test::AcceptFixtureAllocation(f.state, publication_id, f.other);
    const auto total = TotalCybou(f.state);
    BOOST_CHECK_EQUAL(f.state.leases.at(publication_id).escrow_onboarding, 4U); // fee took 1 first

    const auto provider_before = f.state.accounts.at(f.other);
    // One assigned replica earns five CYBOU from one verified GiB-day.
    BOOST_CHECK(f.Execute({f.Settle(1'700'000'000, {{publication_id, f.other, 6}})}, 3).settlement_error ==
        StorageSettlementError::PAYOUT_EXCEEDS_ESCROW);
    // The payer is never paid for its own lease.
    BOOST_CHECK(f.Execute({f.Settle(1'700'000'000, {{publication_id, f.account, 1}})}, 3).settlement_error ==
        StorageSettlementError::SELF_PAYOUT);
    BOOST_REQUIRE(f.Execute({f.Settle(1'700'000'000, {{publication_id, f.other, 5}})}, 3));
    const auto& provider = f.state.accounts.at(f.other);
    // Four onboarding CYBOU credit System Balance; one locked CYBOU becomes spendable.
    BOOST_CHECK_EQUAL(provider.system_balance, provider_before.system_balance + 4);
    BOOST_CHECK_EQUAL(provider.onboarding_system_balance, provider_before.onboarding_system_balance + 4);
    BOOST_CHECK_EQUAL(provider.balance, provider_before.balance + 1);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);
    BOOST_CHECK_EQUAL(f.state.settlement.next_period, 1U);
    BOOST_CHECK_EQUAL(f.state.settlement.next_period_start_utc, 1'700'000'000U + 86'400);

    // Periods are contiguous: a replay or a gap is refused.
    BOOST_CHECK(f.Execute({f.Settle(1'700'000'000, {})}, 4).settlement_error == StorageSettlementError::WRONG_PERIOD_START);
    // The last period pays nothing here; the lease ends and its escrow returns to the payer's System Balance.
    const auto payer_before = f.state.accounts.at(f.account).system_balance;
    BOOST_REQUIRE(f.Execute({f.Settle(1'700'086'400, {})}, 4));
    BOOST_CHECK_EQUAL(f.state.leases.at(publication_id).escrow_onboarding + f.state.leases.at(publication_id).escrow_locked, 0U);
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.account).system_balance, payer_before + 5);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);

    // A settlement without the genesis PoA signature is refused.
    auto forged = std::get<StorageSettlement>(f.Settle(1'700'172'800, {}));
    forged.poa_signature.ed25519[0] ^= 1;
    BOOST_CHECK(f.Execute({forged}, 5).settlement_error == StorageSettlementError::INVALID_SIGNATURE);

    // Wire round-trip.
    const auto op = f.Settle(1'700'172'800, {});
    const auto bytes = SerializeProtocolOperation(op);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK(DeserializeProtocolOperation(*bytes) == std::optional<ProtocolOperation>{op});
}

BOOST_AUTO_TEST_CASE(publication_pays_its_initial_lease_atomically)
{
    LimitsFixture f;
    const auto publication = f.Publish(2048, 0xA0, 7);
    const auto publication_id = *ComputeOperationId(publication);
    const auto total = TotalCybou(f.state);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    // 1 GiB x 2 replicas x 5 CYBOU x 7 days = 70 CYBOU escrowed in the same block as the publication.
    const auto& lease = f.state.leases.at(publication_id);
    BOOST_CHECK_EQUAL(lease.escrow_onboarding + lease.escrow_locked, 70U);
    BOOST_CHECK_EQUAL(lease.end_period, 7U);
    BOOST_CHECK(lease.payer == f.account);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);
    // With the fee covered but one CYBOU of escrow missing, the publication is refused whole.
    const auto next = f.Publish(2048, 0xA2, 7);
    const auto fee = *ComputeRootPublicationFee(f.params, SerializeProtocolOperation(next)->size(), 2048);
    f.state.accounts.at(f.account).system_balance = fee + 69;
    f.state.accounts.at(f.account).onboarding_system_balance = 0;
    const auto poor = f.Execute({next}, 2);
    BOOST_CHECK(poor.root_publication_error == RootPublicationError::INSUFFICIENT_SYSTEM_BALANCE);
    BOOST_CHECK(!f.state.leases.contains(*ComputeOperationId(next)));
}

BOOST_AUTO_TEST_CASE(revoked_publication_closes_its_lease_after_the_current_period)
{
    LimitsFixture f;
    f.params.storage_replica_target = 1;
    const auto publication = f.Publish(2048, 0x90);
    const auto publication_id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    BOOST_REQUIRE(f.Execute({f.Lease(publication_id, 30)}, 2));
    test::AcceptFixtureAllocation(f.state, publication_id, f.other);
    const auto total = TotalCybou(f.state);
    BOOST_REQUIRE(f.Execute({f.Revoke(publication_id)}, 3));
    BOOST_CHECK_EQUAL(f.state.leases.at(publication_id).end_period, 1U);
    const auto payer_before = f.state.accounts.at(f.account).system_balance;
    BOOST_REQUIRE(f.Execute({f.Settle(1'700'000'000, {{publication_id, f.other, 5}})}, 4));
    BOOST_CHECK_EQUAL(f.state.leases.at(publication_id).escrow_onboarding + f.state.leases.at(publication_id).escrow_locked, 0U);
    // One replica funded 150 CYBOU; one verified day earns five and refunds 145.
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.account).system_balance, payer_before + 145);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);
}

BOOST_AUTO_TEST_CASE(operation_work_is_bound_and_flat)
{
    LimitsFixture f;
    const auto operation = f.Lock();
    const auto id = *ComputeOperationId(operation);
    const auto nonce = SolveOperationWork(f.network, id, 8);
    BOOST_REQUIRE(nonce);
    BOOST_CHECK(CheckOperationWork(f.network, id, *nonce, 8));
    // Bound to network and operation.
    cybou::Hash256 other_network = f.network;
    other_network.begin()[1] ^= 1;
    BOOST_CHECK(!CheckOperationWork(other_network, id, *nonce, 24) || !CheckOperationWork(f.network, id, *nonce, 24));
    BOOST_CHECK(CheckOperationWork(f.network, id, 12345, 0));

    // One difficulty for every Identity (DEC-273); names cost more.
    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(operation), OPERATION_WORK_BITS);
    AuthorizedNameCommit commit{};
    commit.authorization.account_id = f.account;
    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(ProtocolOperation{commit}), OPERATION_WORK_BITS + NAME_OPERATION_EXTRA_WORK_BITS);
    // AccountCreate and StorageSettlement carry their own protection.
    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(ProtocolOperation{StorageSettlement{}}), 0U);
}

// ---- M6: adversarial storage-economy tests (DEC-283 release gate) ----

BOOST_AUTO_TEST_CASE(settlement_payout_attacks_are_refused)
{
    LimitsFixture f;
    f.params.storage_replica_target = 1;
    const auto publication = f.Publish(2048, 0xB0, 3);
    const auto id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    test::AcceptFixtureAllocation(f.state, id, f.other);
    const auto total = TotalCybou(f.state);
    constexpr uint64_t start{1'700'000'000};
    uint64_t height{2};
    const auto refused = [&](const ProtocolOperation& op) {
        const auto result = f.Execute({op}, height);
        BOOST_CHECK(!result);
        BOOST_CHECK_EQUAL(TotalCybou(f.state), total);
        return result.settlement_error;
    };

    cybou::Hash256 unknown{};
    unknown.begin()[0] = 0xEE;
    BOOST_CHECK(refused(f.Settle(start, {{unknown, f.other, 1}})) == StorageSettlementError::LEASE_NOT_FOUND);
    cybou::Hash256 ghost_raw{};
    ghost_raw.begin()[0] = 0xEF;
    BOOST_CHECK(refused(f.Settle(start, {{id, AccountId{ghost_raw}, 1}})) == StorageSettlementError::PAYOUT_ACCOUNT_NOT_FOUND);
    // Duplicate or unordered entries have no canonical encoding at all.
    auto duplicate = std::get<StorageSettlement>(f.Settle(start, {}));
    duplicate.entries = {{id, f.other, 1}, {id, f.other, 1}};
    BOOST_CHECK(!ComputeStorageSettlementDigest(f.network, duplicate));
    BOOST_CHECK(!SerializeProtocolOperation(ProtocolOperation{duplicate}));
    // A zero payout is not a payout.
    auto zero = std::get<StorageSettlement>(f.Settle(start, {}));
    zero.entries = {{id, f.other, 0}};
    BOOST_CHECK(!ComputeStorageSettlementDigest(f.network, zero));
    // Period start arithmetic cannot overflow.
    BOOST_CHECK(refused(f.Settle(std::numeric_limits<uint64_t>::max() - 10, {})) == StorageSettlementError::INVALID_PAYLOAD);
    // Another key cannot sign settlements, and an executor without the genesis PoA key refuses them.
    auto foreign = std::get<StorageSettlement>(f.Settle(start, {}));
    std::array<unsigned char, 32> foreign_seed{};
    foreign_seed[0] = 0xB7;
    foreign.poa_signature = *SignIdentityMessage(foreign_seed, IdentityKeyPurpose::POA_FINALIZER,
        *ComputeStorageSettlementDigest(f.network, foreign));
    BOOST_CHECK(refused(foreign) == StorageSettlementError::INVALID_SIGNATURE);
    const auto keyless = ExecuteBlockOperations(f.state, {f.Settle(start, {})}, f.network, height, f.params);
    BOOST_CHECK(keyless.settlement_error == StorageSettlementError::INVALID_SIGNATURE);

    // A valid settlement cannot be replayed for the same period.
    const auto good = f.Settle(start, {{id, f.other, 5}});
    BOOST_REQUIRE(f.Execute({good}, height++));
    BOOST_CHECK(refused(good) == StorageSettlementError::WRONG_PERIOD);
    // Even across many periods a lease never pays out more than its escrow.
    uint64_t paid{5};
    for (uint64_t period{1}; period < 3; ++period) {
        BOOST_REQUIRE(f.Execute({f.Settle(start + period * 86'400, {{id, f.other, 5}})}, height++));
        paid += 5;
    }
    BOOST_CHECK_EQUAL(paid, 15U);
    BOOST_CHECK_EQUAL(f.state.leases.at(id).funded_terms[0].paid_onboarding + f.state.leases.at(id).funded_terms[0].paid_locked, 15U);
    BOOST_CHECK_EQUAL(TotalCybou(f.state), total);
}

BOOST_AUTO_TEST_CASE(lease_payload_and_extension_bounds_are_enforced)
{
    LimitsFixture f;
    // Create a valid funded term near the period limit through canonical execution.
    f.state.settlement.next_period = std::numeric_limits<uint64_t>::max() - 2;
    const auto publication = f.Publish(2048, 0xC0, 1);
    const auto id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    // Zero periods have no encoding; more than the genesis maximum is refused.
    BOOST_CHECK(!SerializeStorageLeasePayload({id, 0}));
    BOOST_CHECK(f.Execute({f.Lease(id, f.params.max_storage_lease_periods + 1)}, 2).lease_error ==
        StorageLeaseError::INVALID_PAYLOAD);
    f.nonce = f.state.identities.Find(f.account)->nonce;
    // An extension that would overflow the period range is refused before any debit.
    BOOST_CHECK_EQUAL(f.state.leases.at(id).end_period, std::numeric_limits<uint64_t>::max() - 1);
    BOOST_REQUIRE(ValidateCybouState(f.state) == StateValidationError::NONE);
    const auto system_before = f.state.accounts.at(f.account).system_balance;
    BOOST_CHECK(f.Execute({f.Lease(id, 5)}, 2).lease_error == StorageLeaseError::ESCROW_OVERFLOW);
    BOOST_CHECK_EQUAL(f.state.accounts.at(f.account).system_balance, system_before);
}

BOOST_AUTO_TEST_CASE(randomized_economy_conserves_cybou_and_valid_state)
{
    // Deterministic pseudo-random walk over every money-moving operation. Whatever succeeds
    // or fails, TotalCybou never changes and every state stays canonical (DEC-277).
    LimitsFixture f;
    std::mt19937_64 rng{0xC1B0};
    const auto total = TotalCybou(f.state);
    std::vector<cybou::Hash256> publications;
    uint64_t start{1'700'000'000};
    unsigned successes{0};
    for (uint64_t height{1}; height <= 400; ++height) {
        std::optional<ProtocolOperation> op;
        switch (rng() % 5) {
        case 0: op = f.Lock(); break;
        case 1: {
            op = f.Publish(1 + static_cast<uint32_t>(rng() % 4096), static_cast<unsigned char>(rng()),
                static_cast<uint32_t>(rng() % 4));
            break;
        }
        case 2:
            if (publications.empty()) continue;
            op = f.Lease(publications[rng() % publications.size()], 1 + static_cast<uint32_t>(rng() % 3));
            break;
        case 3:
            if (publications.empty()) continue;
            op = f.Revoke(publications[rng() % publications.size()]);
            break;
        default: {
            std::vector<StorageSettlementEntry> entries;
            for (const auto& [id, lease] : f.state.leases) {
                if (rng() % 2 == 0 || lease.first_period > f.state.settlement.next_period) continue;
                entries.push_back({id, f.other, 1 + rng() % 12});
            }
            // Exercise legitimate zero-service periods as well as unattested
            // payout attacks; absent assignments never authorize a payment.
            if (height % 2 == 0) entries.clear();
            const auto period_start = f.state.settlement.next_period_start_utc == 0 ? start
                : f.state.settlement.next_period_start_utc;
            op = f.Settle(period_start, std::move(entries));
        }
        }
        const auto id = ComputeOperationId(*op);
        const auto result = f.Execute({*op}, height);
        if (result) {
            ++successes;
            if (std::holds_alternative<AuthorizedRootPublication>(*op)) publications.push_back(*id);
        }
        // A refused identity operation did not consume its nonce.
        f.nonce = f.state.identities.Find(f.account)->nonce;
        BOOST_REQUIRE_EQUAL(TotalCybou(f.state), total);
        BOOST_REQUIRE(ValidateCybouState(f.state) == StateValidationError::NONE);
        for (const auto& [lease_id, lease] : f.state.leases) {
            BOOST_REQUIRE(f.state.accounts.at(lease.payer).onboarding_system_balance <=
                f.state.accounts.at(lease.payer).system_balance);
        }
    }
    BOOST_CHECK_GT(successes, 100U);
    BOOST_CHECK_GT(f.state.settlement.next_period, 10U);
}

BOOST_AUTO_TEST_SUITE_END()
