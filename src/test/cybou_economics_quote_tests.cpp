// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/economics_quote.h>
#include <cybou/protocol_operation.h>
#include <cybou/storage_economy.h>
#include <cybou/storage_lease.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <limits>

BOOST_AUTO_TEST_SUITE(cybou_economics_quote_tests)
BOOST_AUTO_TEST_CASE(publication_quote_separates_fees_and_initial_escrow)
{
    const auto params = cybou::DevProtocolParameters();
    for (const auto chunks : {1U, 2U, 2048U}) {
        const auto quote = cybou::QuotePublicationCost(params, 1025, chunks, 30);
        BOOST_REQUIRE(quote);
        BOOST_CHECK_EQUAL(quote->publication_fee, 8 + 4 * chunks);
        BOOST_CHECK_EQUAL(quote->storage_escrow, *cybou::ComputeStorageLeaseEscrow(params, chunks, 2, 30));
        BOOST_CHECK_EQUAL(quote->other_fees, 0U);
        BOOST_CHECK_EQUAL(quote->total_system_debit, quote->publication_fee + quote->storage_escrow);
        BOOST_CHECK_EQUAL(quote->billing_units, chunks);
        BOOST_CHECK_EQUAL(quote->remote_replicas, 2U);
    }
    const auto without_lease = cybou::QuotePublicationCost(params, 1024, 1, 0);
    BOOST_REQUIRE(without_lease);
    BOOST_CHECK_EQUAL(without_lease->storage_escrow, 0U);
    BOOST_CHECK_EQUAL(without_lease->total_system_debit, 8U);
}
BOOST_AUTO_TEST_CASE(quote_rejects_invalid_inputs_and_combined_overflow)
{
    auto params = cybou::DevProtocolParameters();
    BOOST_CHECK(!cybou::QuotePublicationCost(params, 0, 1, 30));
    BOOST_CHECK(!cybou::QuotePublicationCost(params, cybou::ROOT_PUBLICATION_MAX_OPERATION_BYTES + 1, 1, 30));
    BOOST_CHECK(!cybou::QuotePublicationCost(params, 1, 0, 30));
    BOOST_CHECK(!cybou::QuotePublicationCost(params, 1, cybou::MAX_PUBLICATION_CHUNKS + 1, 30));
    BOOST_CHECK(!cybou::QuotePublicationCost(params, 1, 1, params.max_storage_lease_periods + 1));
    params.root_publication_fee_per_started_kib = std::numeric_limits<std::uint64_t>::max() - 10;
    params.root_publication_fee_per_chunk = 0;
    BOOST_REQUIRE(cybou::ComputeRootPublicationFee(params, 1, 2048));
    BOOST_REQUIRE(cybou::ComputeStorageLeaseEscrow(params, 2048, 2, 30));
    BOOST_CHECK(!cybou::QuotePublicationCost(params, 1, 2048, 30));
}
BOOST_AUTO_TEST_CASE(renewal_quote_uses_consensus_parameters_and_fee)
{
    auto params = cybou::DevProtocolParameters();
    params.storage_rate_per_gib_day_replica = 7;
    for (const auto bytes : {512ULL * 1024, 512ULL * 1024 + 1, 1ULL << 20, 1ULL << 30}) {
        const auto units = static_cast<std::uint32_t>(cybou::StorageBillingUnits(bytes));
        const auto quote = cybou::QuoteStorageLeaseCost(params, units, 2, 30);
        BOOST_REQUIRE(quote);
        BOOST_CHECK_EQUAL(quote->protocol_fee, params.payment_fee);
        BOOST_CHECK_EQUAL(quote->storage_escrow, *cybou::ComputeStorageLeaseEscrow(params, units, 2, 30));
        BOOST_CHECK_EQUAL(quote->total_system_debit, params.payment_fee + quote->storage_escrow);
    }
    BOOST_CHECK(!cybou::QuoteStorageLeaseCost(params, 1, 2, 0));
    params.payment_fee = std::numeric_limits<std::uint64_t>::max();
    BOOST_CHECK(!cybou::QuoteStorageLeaseCost(params, 2048, 2, 30));
}
BOOST_AUTO_TEST_CASE(exact_publication_quote_matches_consensus_debit_and_conservation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("economics-quote.vault");
    const auto snapshot = fixture.runtime->GetStore().GetStateSnapshot();
    BOOST_REQUIRE(snapshot && snapshot.state);
    auto state = *snapshot.state;
    const auto account = *identity->GetKeyStore().GetAccountId();
    const auto* record = state.identities.Find(account);
    BOOST_REQUIRE(record);
    cybou::RootPublication publication;
    publication.root_chunk_id.fill(11);
    publication.chunk_authorization_root.fill(22);
    publication.chunk_count = 2048;
    publication.lease_periods = 30;
    publication.recipient_capsules.resize(1);
    const auto commitment = cybou::ComputeRootPublicationPayloadCommitment(publication);
    BOOST_REQUIRE(commitment);
    cybou::IdentityOperationAuthorization authorization{.account_id = account, .nonce = record->nonce,
        .key_epoch = record->key_epoch, .kind = cybou::IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *commitment};
    const auto digest = cybou::ComputeIdentityOperationDigest(fixture.runtime->GetNetworkBinding(), authorization);
    BOOST_REQUIRE(digest);
    const auto signature = identity->GetKeyStore().SignAuthorization(*digest);
    BOOST_REQUIRE(signature);
    authorization.signature = *signature;
    const cybou::AuthorizedRootPublication operation{authorization, publication};
    const auto& params = fixture.runtime->GetNetworkGenesis().GetProtocolParameters();
    const auto quote = cybou::QuotePublicationCost(params, operation);
    BOOST_REQUIRE(quote);
    const auto before = state.accounts.at(account).system_balance;
    const auto treasury_before = cybou::FindCentralAuthorityAllocation(state)->balance;
    const auto total_before = cybou::TotalCybou(state);
    BOOST_REQUIRE(cybou::ApplyRootPublication(operation, fixture.runtime->GetNetworkBinding(), params, state) == cybou::RootPublicationError::NONE);
    BOOST_CHECK_EQUAL(before - state.accounts.at(account).system_balance, quote->total_system_debit);
    BOOST_CHECK_EQUAL(cybou::FindCentralAuthorityAllocation(state)->balance - treasury_before, quote->publication_fee);
    BOOST_REQUIRE_EQUAL(state.leases.size(), 1U);
    BOOST_CHECK_EQUAL(state.leases.begin()->second.escrow_onboarding + state.leases.begin()->second.escrow_locked, quote->storage_escrow);
    BOOST_CHECK(cybou::TotalCybou(state) == total_before);
    const auto publication_id = cybou::ComputeOperationId(cybou::ProtocolOperation{operation});
    BOOST_REQUIRE(publication_id);
    BOOST_REQUIRE(cybou::RecordPublication(state, *publication_id, account, publication.chunk_authorization_root,
        publication.chunk_count, 1));
    const cybou::StorageLeasePayload renewal{*publication_id, 90};
    const auto renewal_commitment = cybou::ComputeStorageLeasePayloadCommitment(renewal);
    BOOST_REQUIRE(renewal_commitment);
    authorization.nonce = state.identities.Find(account)->nonce;
    authorization.kind = cybou::IdentityOperationKind::STORAGE_LEASE;
    authorization.payload_commitment = *renewal_commitment;
    const auto renewal_digest = cybou::ComputeIdentityOperationDigest(fixture.runtime->GetNetworkBinding(), authorization);
    BOOST_REQUIRE(renewal_digest);
    const auto renewal_signature = identity->GetKeyStore().SignAuthorization(*renewal_digest);
    BOOST_REQUIRE(renewal_signature);
    authorization.signature = *renewal_signature;
    const auto renewal_quote = cybou::QuoteStorageLeaseCost(params, publication.chunk_count, params.storage_replica_target, 90);
    BOOST_REQUIRE(renewal_quote);
    const auto before_renewal = state.accounts.at(account).system_balance;
    const auto treasury_before_renewal = cybou::FindCentralAuthorityAllocation(state)->balance;
    const auto& old_lease = state.leases.at(*publication_id);
    const auto escrow_before_renewal = old_lease.escrow_onboarding + old_lease.escrow_locked;
    BOOST_REQUIRE(cybou::ApplyStorageLease({authorization, renewal}, fixture.runtime->GetNetworkBinding(), params, state) == cybou::StorageLeaseError::NONE);
    BOOST_CHECK_EQUAL(before_renewal - state.accounts.at(account).system_balance, renewal_quote->total_system_debit);
    BOOST_CHECK_EQUAL(cybou::FindCentralAuthorityAllocation(state)->balance - treasury_before_renewal, renewal_quote->protocol_fee);
    const auto& renewed = state.leases.at(*publication_id);
    BOOST_CHECK_EQUAL(renewed.escrow_onboarding + renewed.escrow_locked - escrow_before_renewal, renewal_quote->storage_escrow);
    BOOST_CHECK(cybou::TotalCybou(state) == total_before);
}
BOOST_AUTO_TEST_SUITE_END()
