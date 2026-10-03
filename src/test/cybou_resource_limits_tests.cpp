// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block_executor.h>
#include <cybou/operation_work.h>
#include <cybou/protocol_limits.h>
#include <test/cybou_test_helpers.h>
#include "cybou_test_identity_helpers.h"

#include <boost/test/unit_test.hpp>

#include <array>

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
            .balance = 100'000'000, .authority = 0, .label = std::string{CENTRAL_AUTHORITY_NAME}});
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

    ProtocolOperation Publish(uint32_t chunks, unsigned char tag)
    {
        RootPublication publication;
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

    BlockExecutionResult Execute(const std::vector<ProtocolOperation>& operations, uint64_t height)
    {
        auto result = ExecuteBlockOperations(state, operations, network, height, params);
        if (result) state = *result.state;
        return result;
    }
};

constexpr uint32_t GIB_CHUNKS{static_cast<uint32_t>(1024ULL * 1024ULL * 1024ULL / QUOTA_CHUNK_BYTES)};

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_resource_limits_tests)

BOOST_AUTO_TEST_CASE(tier_table_is_monotonic_and_bounded)
{
    BOOST_CHECK(ComputeAuthorityTier(9'999) == AuthorityTier::T0);
    BOOST_CHECK(ComputeAuthorityTier(10'000) == AuthorityTier::T1);
    BOOST_CHECK(ComputeAuthorityTier(10'000'000) == AuthorityTier::T3);
    BOOST_CHECK(ComputeAuthorityTier(10'000'001) == AuthorityTier::VALIDATOR);
    const uint64_t tiers[]{0, 10'000, 100'000, 1'000'000, 10'000'001};
    for (size_t i{1}; i < std::size(tiers); ++i) {
        const auto lower = ComputeAuthorityTierLimits(tiers[i - 1]);
        const auto upper = ComputeAuthorityTierLimits(tiers[i]);
        BOOST_CHECK_GT(upper.operations_per_block, lower.operations_per_block);
        BOOST_CHECK_GT(upper.operations_per_epoch, lower.operations_per_epoch);
        BOOST_CHECK_GT(upper.storage_quota_chunks, lower.storage_quota_chunks);
        BOOST_CHECK_GT(upper.max_publication_chunks, lower.max_publication_chunks);
        BOOST_CHECK_LT(upper.operation_work_bits, lower.operation_work_bits);
        // A single file never takes more than a fifth of the tier's quota.
        BOOST_CHECK_LE(uint64_t{upper.max_publication_chunks} * 5, upper.storage_quota_chunks);
    }
    BOOST_CHECK_EQUAL(ComputeAuthorityTierLimits(0).max_publication_chunks, GIB_CHUNKS);
}

BOOST_AUTO_TEST_CASE(t0_identity_gets_one_operation_per_block)
{
    LimitsFixture f;
    const auto first = f.Lock();
    const auto second = f.Lock();
    const auto refused = f.Execute({first, second}, 1);
    BOOST_CHECK(refused.error == BlockExecutionError::OPERATION_LIMIT_EXCEEDED);
    BOOST_CHECK_EQUAL(refused.failed_operation_index, 1U);
    BOOST_REQUIRE(f.Execute({first}, 1));
    // The same height cannot take another one (candidate pool executes one by one).
    BOOST_CHECK(f.Execute({second}, 1).error == BlockExecutionError::OPERATION_LIMIT_EXCEEDED);
    BOOST_CHECK(f.Execute({second}, 2));
}

BOOST_AUTO_TEST_CASE(epoch_limit_resets_in_the_next_epoch)
{
    LimitsFixture f;
    const auto per_epoch = ComputeAuthorityTierLimits(0).operations_per_epoch;
    uint64_t height{1};
    for (uint32_t i{0}; i < per_epoch; ++i) BOOST_REQUIRE(f.Execute({f.Lock()}, height++));
    BOOST_CHECK_EQUAL(f.state.usage.at(f.account).epoch_operations, per_epoch);
    const auto over = f.Lock();
    BOOST_CHECK(f.Execute({over}, height).error == BlockExecutionError::OPERATION_LIMIT_EXCEEDED);
    // First block of the next epoch opens a fresh window; the stale record is swept away first.
    const uint64_t next_epoch = (EpochForHeight(height, f.params) + 1) * f.params.epoch_blocks;
    BOOST_REQUIRE(f.Execute({}, next_epoch));
    BOOST_CHECK(!f.state.usage.contains(f.account));
    BOOST_CHECK(f.Execute({over}, next_epoch + 1));
    BOOST_CHECK_EQUAL(f.state.usage.at(f.account).epoch_operations, 1U);
}

BOOST_AUTO_TEST_CASE(publication_size_and_quota_follow_the_tier)
{
    LimitsFixture f;
    const auto t0 = ComputeAuthorityTierLimits(0);
    BOOST_CHECK(f.Execute({f.Publish(t0.max_publication_chunks + 1, 0x10)}, 1).error ==
        BlockExecutionError::PUBLICATION_TOO_LARGE);
    --f.nonce; // the refused operation never consumed its nonce
    // Five 1 GiB files fill the 5 GiB onboarding credit exactly; the sixth is refused.
    uint64_t height{1};
    for (unsigned char i{0}; i < 5; ++i) BOOST_REQUIRE(f.Execute({f.Publish(t0.max_publication_chunks, 0x20 + 2 * i)}, height++));
    BOOST_CHECK_EQUAL(f.state.usage.at(f.account).stored_chunks, t0.storage_quota_chunks);
    BOOST_CHECK_EQUAL(f.state.publications.size(), 5U);
    BOOST_CHECK(f.Execute({f.Publish(1, 0x40)}, height).error == BlockExecutionError::STORAGE_QUOTA_EXCEEDED);
    --f.nonce;
    // Reaching T1 raises both limits.
    f.state.accounts.at(f.account).authority = 10'000;
    BOOST_CHECK(f.Execute({f.Publish(t0.max_publication_chunks + 1, 0x42)}, height));
}

BOOST_AUTO_TEST_CASE(revoke_publication_frees_quota_and_is_owner_only)
{
    LimitsFixture f;
    const auto publication = f.Publish(100, 0x50);
    const auto publication_id = *ComputeOperationId(publication);
    BOOST_REQUIRE(f.Execute({publication}, 1));
    BOOST_REQUIRE(f.state.publications.contains(publication_id));
    BOOST_CHECK(f.state.publications.at(publication_id).owner == f.account);
    BOOST_CHECK_EQUAL(f.state.usage.at(f.account).stored_chunks, 100U);

    cybou::Hash256 unknown{};
    unknown.begin()[0] = 0x99;
    const auto missing = f.Execute({f.Revoke(unknown)}, 2);
    BOOST_CHECK(missing.error == BlockExecutionError::INVALID_REVOKE_PUBLICATION);
    BOOST_CHECK(missing.revoke_error == RevokePublicationError::PUBLICATION_NOT_FOUND);
    --f.nonce;

    // Another Identity cannot revoke it.
    auto foreign = f.state;
    foreign.usage[f.other] = foreign.usage.at(f.account);
    foreign.usage.erase(f.account);
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

BOOST_AUTO_TEST_CASE(resource_section_keeps_empty_state_root_and_roundtrips)
{
    LimitsFixture f;
    const auto empty = SerializeCybouState(f.state);
    BOOST_REQUIRE(empty);
    // No resource record: the bytes are the same as before resource accounting existed.
    BOOST_CHECK(f.state.usage.empty() && f.state.publications.empty());
    auto with_empty_section = *empty;
    with_empty_section.insert(with_empty_section.end(), 8, 0);
    BOOST_CHECK(!DeserializeCybouState(with_empty_section));

    BOOST_REQUIRE(f.Execute({f.Publish(7, 0x60)}, 5));
    const auto bytes = SerializeCybouState(f.state);
    BOOST_REQUIRE(bytes);
    const auto decoded = DeserializeCybouState(*bytes);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(decoded->usage == f.state.usage);
    BOOST_CHECK(decoded->publications == f.state.publications);
    BOOST_CHECK(CybouStateHash(*decoded) == CybouStateHash(f.state));

    // Quota must equal the register: a mismatch is an invalid state.
    auto broken = f.state;
    broken.usage.at(f.account).stored_chunks += 1;
    BOOST_CHECK(ValidateCybouState(broken) == StateValidationError::INVALID_RESOURCE_USAGE);
}

BOOST_AUTO_TEST_CASE(operation_work_is_bound_and_tiered)
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

    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(operation, f.state), 22U);
    f.state.accounts.at(f.account).authority = 10'000'001;
    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(operation, f.state), 18U);
    AuthorizedNameCommit commit{};
    commit.authorization.account_id = f.account;
    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(ProtocolOperation{commit}, f.state), 18U + NAME_OPERATION_EXTRA_WORK_BITS);
    // AccountCreate and PoaAuthAdjustment carry their own protection.
    BOOST_CHECK_EQUAL(RequiredOperationWorkBits(ProtocolOperation{PoaAuthAdjustment{}}, f.state), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
