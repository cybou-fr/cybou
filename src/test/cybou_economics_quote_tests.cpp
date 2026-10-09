// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/economics_quote.h>
#include <cybou/protocol_operation.h>
#include <cybou/storage_economy.h>
#include <cybou/storage_lease.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_economics_scenarios.h>
#include <boost/test/unit_test.hpp>
#include <limits>

BOOST_AUTO_TEST_SUITE(cybou_economics_quote_tests)
BOOST_AUTO_TEST_CASE(cost_model_chunk_bounds_include_real_root_index_and_data_chunks)
{
    std::array<unsigned char, 32> binding{};
    binding[0] = 1;
    for (const auto bytes : {0ULL, 1ULL << 20, 41ULL << 20}) {
        auto remaining = bytes;
        const auto tree = cybou::BuildEncryptedChunkTree(binding,
            [&](std::span<unsigned char> out) -> std::optional<std::size_t> {
                const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, out.size()));
                std::fill_n(out.begin(), count, 42);
                remaining -= count;
                return count;
            }, [](std::uint32_t, const cybou::EncryptedChunk&) { return true; });
        BOOST_REQUIRE(tree);
        const auto [low, high] = cybou::test::PublicationChunkBounds(bytes);
        if (bytes) {
            BOOST_CHECK_GE(tree->chunk_count + 1, low); // additional application metadata ROOT
            BOOST_CHECK_LE(tree->chunk_count + 1, high);
        } else BOOST_CHECK_EQUAL(tree->chunk_count, low);
    }
}
BOOST_AUTO_TEST_CASE(tiny_lease_funds_each_replica_but_daily_cap_still_pays_early)
{
    const auto p = cybou::DevProtocolParameters();
    BOOST_CHECK_EQUAL(*cybou::ComputeStorageLeaseEscrow(p, 1, 2, 30), 2U);
    BOOST_CHECK_EQUAL(*cybou::ComputeStorageLeasePeriodCap(p, 1, 2), 1U);
    cybou::StorageRentAccumulator whole, split;
    BOOST_REQUIRE(cybou::AccrueStorageRent(whole, 1, 30 * p.storage_settlement_period_seconds, 2));
    for (int i = 0; i < 30; ++i) BOOST_REQUIRE(cybou::AccrueStorageRent(split, 1, p.storage_settlement_period_seconds, 2));
    BOOST_CHECK_EQUAL(whole.cybou, 0U);
    BOOST_CHECK_EQUAL(split.cybou, whole.cybou);
    BOOST_CHECK_EQUAL(split.remainder, whole.remainder);
    CybouServiceTestFixture fixture;
    auto payer = fixture.CreateIdentity("tiny-rent-payer.vault");
    auto provider = fixture.CreateIdentity("tiny-rent-provider.vault");
    const auto snapshot = fixture.runtime->GetStore().GetStateSnapshot();
    BOOST_REQUIRE(snapshot && snapshot.state);
    auto state = *snapshot.state;
    const auto payer_id = *payer->GetKeyStore().GetAccountId();
    const auto provider_id = *provider->GetKeyStore().GetAccountId();
    cybou::Hash256 publication_id;
    publication_id.begin()[0] = 1;
    cybou::FundStorageLease(state, publication_id, payer_id, 1, 2, 30, 2, p, publication_id);
    const auto before = cybou::TotalCybou(state);
    const auto provider_before = state.accounts.at(provider_id).system_balance;
    cybou::StorageSettlement settlement{.period = state.settlement.next_period, .period_start_utc = 1700000000,
        .entries = {{publication_id, provider_id, 1}}};
    const auto digest = cybou::ComputeStorageSettlementDigest(fixture.runtime->GetNetworkBinding(), settlement);
    BOOST_REQUIRE(digest);
    const auto signature = cybou::SignIdentityMessage(fixture.validator_seed, cybou::IdentityKeyPurpose::POA_FINALIZER, *digest);
    BOOST_REQUIRE(signature);
    settlement.poa_signature = *signature;
    BOOST_REQUIRE(cybou::ApplyStorageSettlement(settlement, fixture.runtime->GetNetworkBinding(),
        fixture.definition.GetProtocolParameters(), fixture.definition.GetPoaPublicKey(), state) == cybou::StorageSettlementError::NONE);
    BOOST_CHECK_EQUAL(state.leases.at(publication_id).escrow_onboarding + state.leases.at(publication_id).escrow_locked, 1U);
    BOOST_CHECK_EQUAL(state.accounts.at(provider_id).system_balance - provider_before, 1U);
    BOOST_CHECK(cybou::TotalCybou(state) == before);
    BOOST_REQUIRE_EQUAL(state.leases.at(publication_id).funded_terms.size(), 1U);
    BOOST_CHECK_EQUAL(state.leases.at(publication_id).funded_terms.front().initial_onboarding, 2U);
    BOOST_CHECK_EQUAL(state.leases.at(publication_id).funded_terms.front().paid_onboarding, 1U);
    BOOST_CHECK_EQUAL(state.leases.at(publication_id).funded_terms.front().paid_locked, 0U);
    BOOST_REQUIRE(cybou::ValidateCybouState(state) == cybou::StateValidationError::NONE);
}
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
BOOST_AUTO_TEST_CASE(settlement_debits_only_active_term_and_preserves_origin_after_restart)
{
    CybouServiceTestFixture fixture;
    auto payer = fixture.CreateIdentity("term-debit-payer.vault");
    auto provider = fixture.CreateIdentity("term-debit-provider.vault");
    const auto snapshot = fixture.runtime->GetStore().GetStateSnapshot();
    BOOST_REQUIRE(snapshot && snapshot.state);
    auto state = *snapshot.state;
    const auto payer_id = *payer->GetKeyStore().GetAccountId();
    const auto provider_id = *provider->GetKeyStore().GetAccountId();
    const auto params = fixture.definition.GetProtocolParameters();
    cybou::Hash256 publication, renewal;
    publication.begin()[0] = 55;
    renewal.begin()[0] = 56;
    // Fixture provenance: the initial term mixes origins; renewal is onboarding-only.
    state.accounts.at(payer_id).onboarding_system_balance = 1;
    const auto total = cybou::TotalCybou(state);
    BOOST_REQUIRE_EQUAL(*cybou::ComputeStorageLeaseEscrow(params, 1, 2, 3), 2U);
    cybou::FundStorageLease(state, publication, payer_id, 1, 2, 3, 2, params, publication);
    state.accounts.at(payer_id).onboarding_system_balance = state.accounts.at(payer_id).system_balance;
    cybou::FundStorageLease(state, publication, payer_id, 1, 2, 3, 2, params, renewal);
    BOOST_REQUIRE(cybou::ValidateCybouState(state) == cybou::StateValidationError::NONE);
    const auto provider_before = state.accounts.at(provider_id);
    const auto payer_after_funding = state.accounts.at(payer_id);
    const auto settle = [&](uint64_t period, uint64_t amount) {
        cybou::StorageSettlement operation{.period = period,
            .period_start_utc = 1700000000 + period * params.storage_settlement_period_seconds};
        if (amount) operation.entries.push_back({publication, provider_id, amount});
        const auto digest = cybou::ComputeStorageSettlementDigest(fixture.runtime->GetNetworkBinding(), operation);
        BOOST_REQUIRE(digest);
        const auto signature = cybou::SignIdentityMessage(fixture.validator_seed,
            cybou::IdentityKeyPurpose::POA_FINALIZER, *digest);
        BOOST_REQUIRE(signature);
        operation.poa_signature = *signature;
        return cybou::ApplyStorageSettlement(operation, fixture.runtime->GetNetworkBinding(), params,
            fixture.definition.GetPoaPublicKey(), state);
    };
    BOOST_REQUIRE(settle(0, 1) == cybou::StorageSettlementError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(provider_id).system_balance - provider_before.system_balance, 1U);
    BOOST_CHECK_EQUAL(state.leases.at(publication).funded_terms.front().paid_onboarding, 1U);
    const auto bytes = cybou::SerializeCybouState(state);
    BOOST_REQUIRE(bytes);
    const auto restored = cybou::DeserializeCybouState(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(cybou::CybouStateHash(state) == cybou::CybouStateHash(*restored));
    state = *restored;
    BOOST_CHECK(settle(0, 1) == cybou::StorageSettlementError::WRONG_PERIOD);
    BOOST_CHECK(cybou::SerializeCybouState(state) == bytes);
    BOOST_REQUIRE(settle(1, 1) == cybou::StorageSettlementError::NONE);
    BOOST_CHECK_EQUAL(state.accounts.at(provider_id).balance - provider_before.balance, 1U);
    BOOST_CHECK_EQUAL(state.leases.at(publication).escrow_onboarding, 2U); // future origin remains untouched
    BOOST_CHECK_EQUAL(state.leases.at(publication).funded_terms.front().paid_locked, 1U);
    BOOST_CHECK_EQUAL(state.leases.at(publication).funded_terms.back().paid_onboarding, 0U);
    const auto exhausted = cybou::SerializeCybouState(state);
    BOOST_REQUIRE(exhausted);
    BOOST_CHECK(settle(2, 1) == cybou::StorageSettlementError::PAYOUT_EXCEEDS_ESCROW);
    BOOST_CHECK(cybou::SerializeCybouState(state) == exhausted);
    auto corrupt = state;
    corrupt.leases.at(publication).funded_terms.front().paid_locked = 0;
    BOOST_CHECK(cybou::ValidateCybouState(corrupt) == cybou::StateValidationError::INVALID_STORAGE_LEASE);
    corrupt = state;
    corrupt.leases.at(publication).funded_terms.front().paid_locked = 2;
    BOOST_CHECK(cybou::ValidateCybouState(corrupt) == cybou::StateValidationError::INVALID_STORAGE_LEASE);
    corrupt = state;
    corrupt.leases.at(publication).funded_terms.front().paid_onboarding = 0;
    ++corrupt.leases.at(publication).escrow_onboarding;
    BOOST_CHECK(cybou::ValidateCybouState(corrupt) == cybou::StateValidationError::INVALID_STORAGE_LEASE);
    BOOST_REQUIRE(settle(2, 0) == cybou::StorageSettlementError::NONE);
    BOOST_REQUIRE(settle(3, 1) == cybou::StorageSettlementError::NONE);
    BOOST_CHECK_EQUAL(state.leases.at(publication).funded_terms.back().paid_onboarding, 1U);
    BOOST_REQUIRE(cybou::ValidateCybouState(state) == cybou::StateValidationError::NONE);
    BOOST_REQUIRE(settle(4, 0) == cybou::StorageSettlementError::NONE);
    BOOST_REQUIRE(settle(5, 0) == cybou::StorageSettlementError::NONE);
    BOOST_CHECK(!state.leases.contains(publication));
    BOOST_CHECK_EQUAL(state.accounts.at(payer_id).system_balance - payer_after_funding.system_balance, 1U);
    BOOST_CHECK_EQUAL(state.accounts.at(payer_id).onboarding_system_balance - payer_after_funding.onboarding_system_balance, 1U);
    BOOST_CHECK(cybou::TotalCybou(state) == total);
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
    BOOST_CHECK(!cybou::QuoteStorageLeaseCost(params, 1, 2, params.max_storage_lease_periods + 1));
    params.payment_fee = std::numeric_limits<std::uint64_t>::max();
    BOOST_CHECK(!cybou::QuoteStorageLeaseCost(params, 2048, 2, 30));
}
BOOST_AUTO_TEST_CASE(exact_publication_quote_matches_consensus_debit_and_conservation)
{
    for (const auto units : {1U, 2048U}) {
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
        publication.chunk_count = units;
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
        BOOST_REQUIRE_EQUAL(old_lease.funded_terms.size(), 1U);
        const auto initial_term = old_lease.funded_terms.front();
        BOOST_CHECK(initial_term.funding_operation_id == *publication_id);
        BOOST_CHECK_EQUAL(initial_term.first_period, 0U);
        BOOST_CHECK_EQUAL(initial_term.end_period, 30U);
        const auto escrow_before_renewal = old_lease.escrow_onboarding + old_lease.escrow_locked;
        BOOST_REQUIRE(cybou::ApplyStorageLease({authorization, renewal}, fixture.runtime->GetNetworkBinding(), params, state) == cybou::StorageLeaseError::NONE);
        BOOST_CHECK_EQUAL(before_renewal - state.accounts.at(account).system_balance, renewal_quote->total_system_debit);
        BOOST_CHECK_EQUAL(cybou::FindCentralAuthorityAllocation(state)->balance - treasury_before_renewal, renewal_quote->protocol_fee);
        const auto& renewed = state.leases.at(*publication_id);
        BOOST_CHECK_EQUAL(renewed.escrow_onboarding + renewed.escrow_locked - escrow_before_renewal, renewal_quote->storage_escrow);
        BOOST_CHECK(cybou::TotalCybou(state) == total_before);
        BOOST_REQUIRE_EQUAL(renewed.funded_terms.size(), 2U);
        BOOST_CHECK(renewed.funded_terms.front() == initial_term);
        const auto renewal_id = cybou::ComputeOperationId(cybou::ProtocolOperation{
            cybou::AuthorizedStorageLease{authorization, renewal}});
        BOOST_REQUIRE(renewal_id);
        BOOST_CHECK(renewed.funded_terms.back().funding_operation_id == *renewal_id);
        BOOST_CHECK_EQUAL(renewed.funded_terms.back().first_period, 30U);
        BOOST_CHECK_EQUAL(renewed.funded_terms.back().end_period, 120U);
        BOOST_CHECK_EQUAL(renewed.funded_terms.back().initial_onboarding, renewal_quote->storage_escrow);
        const auto bytes = cybou::SerializeCybouState(state);
        BOOST_REQUIRE(bytes);
        const auto restored = cybou::DeserializeCybouState(*bytes);
        BOOST_REQUIRE(restored);
        BOOST_CHECK(restored->leases.at(*publication_id) == renewed);
        BOOST_CHECK(cybou::SerializeCybouState(*restored) == bytes);
        BOOST_CHECK(cybou::CybouStateHash(*restored) == cybou::CybouStateHash(state));
        auto corrupt = *restored;
        ++corrupt.leases.at(*publication_id).funded_terms.front().replica_share;
        BOOST_CHECK(cybou::ValidateCybouState(corrupt) == cybou::StateValidationError::INVALID_STORAGE_LEASE);
        const auto corrupt_bytes = cybou::SerializeCybouState(corrupt, false);
        BOOST_REQUIRE(corrupt_bytes);
        BOOST_CHECK(!cybou::DeserializeCybouState(*corrupt_bytes));
        corrupt = *restored;
        corrupt.leases.at(*publication_id).funded_terms.back().funding_operation_id = initial_term.funding_operation_id;
        BOOST_CHECK(cybou::ValidateCybouState(corrupt) == cybou::StateValidationError::INVALID_STORAGE_LEASE);
        corrupt = *restored;
        ++corrupt.leases.at(*publication_id).funded_terms.back().first_period;
        BOOST_CHECK(cybou::ValidateCybouState(corrupt) == cybou::StateValidationError::INVALID_STORAGE_LEASE);
        auto truncated = *bytes;
        truncated.pop_back();
        BOOST_CHECK(!cybou::DeserializeCybouState(truncated));
        auto missing_terms = *bytes;
        missing_terms.resize(missing_terms.size() - 4 - 2 * 112);
        BOOST_CHECK(!cybou::DeserializeCybouState(missing_terms));
        auto oversized_count = *bytes;
        const auto count_offset = oversized_count.size() - 4 - 2 * 112;
        oversized_count[count_offset] = 3;
        BOOST_CHECK(!cybou::DeserializeCybouState(oversized_count));
        BOOST_CHECK_THROW(cybou::FundStorageLease(state, *publication_id, account, units, params.storage_replica_target,
            30, quote->storage_escrow, params, initial_term.funding_operation_id), std::invalid_argument);
        BOOST_CHECK(cybou::SerializeCybouState(state) == bytes);
    }
}
BOOST_AUTO_TEST_SUITE_END()
