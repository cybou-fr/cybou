// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_attestation.h>
#include <cybou/storage_assignment_observer.h>
#include <cybou/storage_assignment_observation_store.h>
#include <cybou/storage_assignment_payout.h>
#include <cybou/block_executor.h>
#include <cybou/protocol_limits.h>
#include <cybou/binary_codec.h>
#include <cybou/crypto/sha256.h>
#include <cybou/chunk_authorization.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <functional>
#include <future>

namespace {
cybou::Hash256 CounterHash(uint64_t value)
{
    cybou::Hash256 id;
    for (unsigned i = 0; i < 8; ++i) id.begin()[i] = static_cast<unsigned char>(value >> (i * 8));
    return id;
}
struct Signer final : cybou::PoaSigner {
    std::array<unsigned char, 32> seed;
    mutable unsigned calls{0};
    bool fail{false};
    std::function<bool()> frozen;
    explicit Signer(std::array<unsigned char, 32> secret) : seed{secret} {}
    std::optional<cybou::IdentityHybridPublicKey> PublicKey() const override
    { return cybou::DeriveIdentityPublicKey(seed, cybou::IdentityKeyPurpose::POA_FINALIZER); }
    std::optional<cybou::IdentityHybridSignature> Sign(std::span<const unsigned char> message) const override
    {
        ++calls;
        if (fail || (frozen && !frozen())) return std::nullopt;
        return cybou::SignIdentityMessage(seed, cybou::IdentityKeyPurpose::POA_FINALIZER, message);
    }
};
struct Fixture {
    CybouServiceTestFixture service;
    std::unique_ptr<cybou::CybouIdentityService> payer = service.CreateIdentity("attest-payer.cybou");
    std::unique_ptr<cybou::CybouIdentityService> provider = service.CreateIdentity("attest-provider.cybou");
    std::unique_ptr<cybou::PrivateApplicationStore> db;
    cybou::IdentityRegistry registry;
    cybou::StorageAssignmentPlan plan;
    std::vector<cybou::StorageAssignmentBindingProof> proofs;
    Signer signer{service.validator_seed};
    std::vector<unsigned char> ciphertext{1, 2, 3, 4};
    Fixture()
    {
        db = std::make_unique<cybou::PrivateApplicationStore>(payer->GetKeyStore(), service.directory / "attest");
        auto state = service.genesis;
        const auto height = service.runtime->GetFinalizedHeight().value();
        for (std::uint64_t h{1}; h <= height; ++h) {
            const auto block = service.runtime->GetBlockAtHeight(h);
            if (!block) throw std::runtime_error{"missing fixture block"};
            auto executed = cybou::ExecuteBlockOperations(state, block->block.operations,
                service.runtime->GetNetworkBinding(), h, service.definition.GetProtocolParameters(),
                &service.definition.GetPoaPublicKey());
            if (!executed) throw std::runtime_error{"fixture replay failed"};
            state = std::move(*executed.state);
        }
        registry = state.identities;
        service.runtime->SetIdentitySigner(std::make_shared<cybou::CybouKeyStoreIdentitySigner>(provider->GetKeyStore()));
        const auto binding = service.runtime->LocalStoragePayoutBinding();
        const auto storage = service.runtime->LocalStorageId();
        if (!binding || !storage) throw std::runtime_error{"missing fixture binding"};
        proofs.push_back({*storage, *binding});
        cybou::StorageAssignmentContext context;
        std::copy_n(service.runtime->GetNetworkBinding().begin(), 32, context.network_binding.begin());
        std::copy_n(service.runtime->GetFinalizedTip()->begin(), 32, context.finalized_seed.begin());
        std::copy_n(payer->GetKeyStore().GetAccountId()->Value().begin(), 32, context.payer.begin());
        context.publication.fill(8); context.chunk = cybou::ComputeChunkId(ciphertext);
        context.term_start = 0; context.term_end = 30; context.replicas = 1;
        cybou::StorageAssignmentProvider candidate{.storage_id = *storage};
        std::copy_n(binding->payout_account.Value().begin(), 32, candidate.payout_account.begin());
        const auto prepared = cybou::PrepareStorageAssignment(context, {candidate});
        if (!prepared) throw std::runtime_error{"fixture assignment failed"};
        plan = *prepared;
        signer.frozen = [&] { return cybou::LoadStorageAssignment(*db, plan.context) == plan; };
    }
    auto RegistrySnapshots() const { return std::array{cybou::StorageAssignmentRegistrySnapshot{*service.runtime->GetFinalizedTip(), &registry}}; }
    auto Attest() { return cybou::AttestStorageAssignment(*db, service.definition, plan, registry,
        *service.runtime->GetFinalizedTip(), proofs, signer); }
};
struct ObservedTransport final : cybou::StorageTransport {
    std::vector<unsigned char> bytes{1, 2, 3, 4};
    unsigned gets{0}, audits{0};
    bool unavailable{false}, audit_unavailable{false}, missing{false}, incorrect{false};
    std::optional<cybou::StorageAuditChallenge> last;
    std::vector<cybou::StorageEndpoint> Providers() override { return {}; }
    std::optional<cybou::ChunkAdmissionResult> Put(const cybou::StorageEndpoint&, const cybou::Hash256&,
        const cybou::ChunkId&, std::span<const unsigned char>, const cybou::ChunkAuthorizationProof&) override
    { return std::nullopt; }
    std::optional<cybou::ChunkAuthorizationProof> GetProof(const cybou::StorageEndpoint&,
        const cybou::Hash256&, const cybou::ChunkId&) override { return std::nullopt; }
    std::optional<std::vector<unsigned char>> Get(const cybou::StorageEndpoint&, const cybou::ChunkId&) override
    { ++gets; return unavailable ? std::nullopt : std::optional{bytes}; }
    std::optional<cybou::StorageAuditAnswer> Audit(const cybou::StorageEndpoint&,
        const cybou::StorageAuditChallenge& challenge) override
    {
        ++audits; last = challenge;
        if (unavailable || audit_unavailable) return std::nullopt;
        return cybou::StorageAuditAnswer{!missing, incorrect ? cybou::Hash256{1} :
            *cybou::ComputeStorageAuditResponse(bytes, challenge.byte_offset, challenge.nonce)};
    }
};
}
BOOST_AUTO_TEST_SUITE(cybou_storage_assignment_attestation_tests)

BOOST_AUTO_TEST_CASE(canonical_prepare_activate_retains_verified_bindings_and_allocation)
{
    using namespace cybou;
    Fixture f;
    auto provider2 = f.service.CreateIdentity("canonical-provider2.cybou");
    CybouServiceTestFixture other_host;
    other_host.runtime->DisablePoaSigner();
    other_host.runtime->SetIdentitySigner(std::make_shared<CybouKeyStoreIdentitySigner>(provider2->GetKeyStore()));
    const auto binding2 = other_host.runtime->LocalStoragePayoutBinding(); BOOST_REQUIRE(binding2);
    const auto storage2 = other_host.runtime->LocalStorageId(); BOOST_REQUIRE(storage2);
    auto state = *f.service.runtime->GetStore().GetStateSnapshot().state;
    const auto network = f.service.runtime->GetNetworkBinding();
    const auto& params = f.service.definition.GetProtocolParameters();
    const auto& poa = f.service.definition.GetPoaPublicKey();
    const auto payer = *f.payer->GetKeyStore().GetAccountId();
    const Hash256 publication{91};
    const std::array leaves{AuthorizedChunk{ComputeChunkId(f.ciphertext)}, AuthorizedChunk{ComputeChunkId(std::array<unsigned char,1>{9})}};
    const auto tree = BuildChunkAuthorizationTree(leaves); BOOST_REQUIRE(tree);
    BOOST_REQUIRE(RecordPublication(state, publication, payer, tree->root, 2, 1));
    const auto escrow = ComputeStorageLeaseEscrow(params, 2, 2, 30); BOOST_REQUIRE(escrow);
    FundStorageLease(state, publication, payer, 2, 2, 30, *escrow, params, publication);
    BOOST_REQUIRE(ValidateCybouState(state) == StateValidationError::NONE);
    const auto sign = [&](StorageSettlement op) {
        const auto digest = ComputeStorageSettlementDigest(network, op); BOOST_REQUIRE(digest);
        const auto signature = SignIdentityMessage(f.service.validator_seed, IdentityKeyPurpose::POA_FINALIZER, *digest);
        BOOST_REQUIRE(signature); op.poa_signature = *signature; return op;
    };
    StorageSettlement prepare{.period_start_utc = 100000};
    prepare.action = StorageSettlementAction::PREPARE; prepare.funded_term_id = publication;
    prepare.assignment_epoch = 1; prepare.eligible.push_back({f.proofs[0].storage_id, f.proofs[0].binding});
    prepare.eligible.push_back({*storage2, *binding2});
    std::sort(prepare.eligible.begin(), prepare.eligible.end(), [](const auto& a, const auto& b) { return a.storage_id < b.storage_id; });
    prepare = sign(prepare);
    const auto bytes = SerializeProtocolOperation(ProtocolOperation{prepare}); BOOST_REQUIRE(bytes);
    BOOST_CHECK_EQUAL(bytes->size(), 3435U + 2U * 6380U);
    BOOST_REQUIRE(DeserializeProtocolOperation(*bytes));
    BOOST_CHECK(std::get<StorageSettlement>(*DeserializeProtocolOperation(*bytes)) == prepare);
    const auto original_root = CybouStateHash(state); BOOST_REQUIRE(original_root);
    auto bad = prepare; bad.poa_signature.ed25519[0] ^= 1;
    BOOST_CHECK(ApplyStorageSettlement(bad, network, params, poa, state, 10, Hash256{2}) == StorageSettlementError::INVALID_SIGNATURE);
    BOOST_CHECK(CybouStateHash(state) == original_root);
    bad = prepare; bad.eligible[0].binding.authorization.ed25519[0] ^= 1; bad = sign(bad);
    BOOST_CHECK(ApplyStorageSettlement(bad, network, params, poa, state, 10, Hash256{2}) == StorageSettlementError::INVALID_ASSIGNMENT);
    BOOST_CHECK(CybouStateHash(state) == original_root);
    BlockExecutor first(state, network, 10, params, &poa, Hash256{2});
    BlockExecutor second(state, network, 10, params, &poa, Hash256{2});
    BOOST_REQUIRE(first.ApplyOperation(ProtocolOperation{prepare}).IsOk());
    BOOST_REQUIRE(second.ApplyOperation(ProtocolOperation{prepare}).IsOk());
    const auto one = first.Finalize(), two = second.Finalize(); BOOST_REQUIRE(one); BOOST_REQUIRE(two);
    BOOST_CHECK(one.state_root == two.state_root); state = *one.state;
    BOOST_CHECK_EQUAL(state.settlement.next_period, 0U);
    BOOST_CHECK_EQUAL(state.settlement.next_period_start_utc, 100000U);
    const auto term = state.leases.at(publication).funded_terms.front();
    BOOST_REQUIRE_EQUAL(term.declarations.size(), 1U);
    BOOST_CHECK_EQUAL(term.declarations[0].eligible[0].accepted_height, 10U);
    BOOST_CHECK_EQUAL(term.next_assignment_epoch, 2U);
    const auto prepared_root = CybouStateHash(state);
    BOOST_CHECK(ApplyStorageSettlement(prepare, network, params, poa, state, 10, Hash256{2}) == StorageSettlementError::INVALID_ASSIGNMENT);
    BOOST_CHECK(CybouStateHash(state) == prepared_root);
    StorageSettlement activate{.period_start_utc = 100000}; activate.action = StorageSettlementAction::ACTIVATE;
    activate.preparation_id = *ComputeOperationId(ProtocolOperation{prepare});
    for (const auto& leaf : leaves) activate.manifest.push_back(leaf.id);
    activate = sign(activate);
    BOOST_CHECK_EQUAL(SerializeProtocolOperation(ProtocolOperation{activate})->size(), 3427U + 64U);
    for (const auto height : {11U,13U}) {
        BOOST_CHECK(ApplyStorageSettlement(activate, network, params, poa, state, height, Hash256{3}) == StorageSettlementError::WRONG_ACTIVATION_HEIGHT);
        BOOST_CHECK(CybouStateHash(state) == prepared_root);
    }
    BOOST_CHECK(ApplyStorageSettlement(activate, network, params, poa, state, 12, {}) == StorageSettlementError::WRONG_ACTIVATION_HEIGHT);
    bad = activate; std::reverse(bad.manifest.begin(), bad.manifest.end()); bad = sign(bad);
    BOOST_CHECK(ApplyStorageSettlement(bad, network, params, poa, state, 12, Hash256{3}) == StorageSettlementError::INVALID_ASSIGNMENT);
    BOOST_CHECK(CybouStateHash(state) == prepared_root);
    // Accepted bindings are historical facts; ACTIVATE does not need fresh signatures.
    BOOST_REQUIRE(ApplyStorageSettlement(activate, network, params, poa, state, 12, Hash256{3}) == StorageSettlementError::NONE);
    const auto& active_term = state.leases.at(publication).funded_terms.front();
    BOOST_REQUIRE_EQUAL(active_term.assignments.size(), 1U);
    BOOST_CHECK(active_term.assignments[0].seed == Hash256{3});
    std::array<uint32_t,2> units{};
    for (const auto& allocation : active_term.assignments[0].allocations) units[allocation.slot] += allocation.units;
    BOOST_CHECK_EQUAL(units[0], 2U); BOOST_CHECK_EQUAL(units[1], 2U);
    BOOST_CHECK_EQUAL(TotalCybou(state), TotalCybou(*one.state));
    const auto encoded = SerializeCybouState(state); BOOST_REQUIRE(encoded);
    const auto reopened = DeserializeCybouState(*encoded); BOOST_REQUIRE(reopened);
    BOOST_CHECK(CybouStateHash(*reopened) == CybouStateHash(state));
    BOOST_CHECK(reopened->leases.at(publication).funded_terms == state.leases.at(publication).funded_terms);
    auto corrupt = state; ++corrupt.leases.at(publication).funded_terms[0].assignments[0].allocations[0].units;
    BOOST_CHECK(ValidateCybouState(corrupt) == StateValidationError::INVALID_STORAGE_LEASE);
    corrupt = state; corrupt.leases.at(publication).funded_terms[0].declarations[0].eligible.push_back(term.declarations[0].eligible[0]);
    BOOST_CHECK(ValidateCybouState(corrupt) == StateValidationError::INVALID_STORAGE_LEASE);
    StorageSettlement pay{.period_start_utc = 100000}; pay.period_end_utc = 186400; pay.evidence_root = Hash256{5}; pay = sign(pay);
    BOOST_CHECK(ApplyStorageSettlement(pay, network, params, poa, state) == StorageSettlementError::NONE);
    BOOST_CHECK_EQUAL(state.settlement.next_period, 1U);
}

BOOST_AUTO_TEST_CASE(full_nodes_finalize_prepare_then_later_seed_then_activation)
{
    using namespace cybou;
    Fixture f;
    auto provider2 = f.service.CreateIdentity("finalized-provider2.cybou");
    CybouServiceTestFixture verifier;
    verifier.runtime->DisablePoaSigner();
    verifier.runtime->SetIdentitySigner(std::make_shared<CybouKeyStoreIdentitySigner>(provider2->GetKeyStore()));
    const auto second_binding = verifier.runtime->LocalStoragePayoutBinding(); BOOST_REQUIRE(second_binding);
    const auto second_storage = verifier.runtime->LocalStorageId(); BOOST_REQUIRE(second_storage);
    const auto network = f.service.runtime->GetNetworkBinding();
    const auto payer = *f.payer->GetKeyStore().GetAccountId();
    const auto snapshot = f.service.runtime->GetStore().GetStateSnapshot(); BOOST_REQUIRE(snapshot);
    const auto* identity = snapshot.state->identities.Find(payer); BOOST_REQUIRE(identity);
    const auto chunk = ComputeChunkId(f.ciphertext);
    const std::array leaves{AuthorizedChunk{chunk}};
    const auto tree = BuildChunkAuthorizationTree(leaves); BOOST_REQUIRE(tree);
    RootPublication publication;
    publication.root_chunk_id = chunk; publication.chunk_authorization_root = tree->root;
    publication.chunk_count = 1; publication.lease_periods = 30;
    RootRecipientCapsule capsule; capsule.encapsulation.fill(0x53); capsule.wrapped_content_key.fill(0x64);
    publication.recipient_capsules.push_back(capsule);
    IdentityOperationAuthorization auth{.account_id = payer, .nonce = identity->nonce, .key_epoch = identity->key_epoch,
        .kind = IdentityOperationKind::ROOT_PUBLICATION, .payload_commitment = *ComputeRootPublicationPayloadCommitment(publication)};
    CybouKeyStoreIdentitySigner author{f.payer->GetKeyStore()};
    const auto signature = author.SignAuthorization(*ComputeIdentityOperationDigest(network, auth)); BOOST_REQUIRE(signature);
    auth.signature = *signature;
    const ProtocolOperation root{AuthorizedRootPublication{auth, publication}};
    const auto publication_id = ComputeOperationId(root); BOOST_REQUIRE(publication_id);
    BOOST_REQUIRE(f.service.runtime->SubmitOperation(root)); BOOST_REQUIRE(f.service.runtime->ProduceBlock());
    StorageSettlement prepare{.period_start_utc = 100000}; prepare.action = StorageSettlementAction::PREPARE;
    prepare.funded_term_id = *publication_id; prepare.assignment_epoch = 1;
    prepare.eligible = {{f.proofs[0].storage_id, f.proofs[0].binding}, {*second_storage, *second_binding}};
    std::sort(prepare.eligible.begin(), prepare.eligible.end(), [](const auto& a, const auto& b) { return a.storage_id < b.storage_id; });
    RuntimeStorageTransport transport{*f.service.runtime};
    auto journal = std::make_unique<StorageService>(*f.service.runtime, transport, *f.db, 2);
    const auto unsigned_root = f.service.runtime->GetStateRoot();
    const auto prepared_submit = journal->SubmitSettlement(prepare); BOOST_REQUIRE(prepared_submit);
    const auto signed_prepare = journal->PreparedSettlement(prepare); BOOST_REQUIRE(signed_prepare);
    auto conflicting_prepare = prepare; conflicting_prepare.period_start_utc += 1;
    BOOST_CHECK_THROW(journal->SubmitSettlement(conflicting_prepare), std::runtime_error);
    journal.reset(); f.db.reset();
    f.db = std::make_unique<PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    journal = std::make_unique<StorageService>(*f.service.runtime, transport, *f.db, 2);
    BOOST_CHECK(journal->PreparedSettlement(prepare) == signed_prepare);
    BOOST_CHECK(journal->SubmitSettlement(prepare).op_id == prepared_submit.op_id);
    BOOST_CHECK(f.service.runtime->GetStateRoot() == unsigned_root); // Signing does not finalize.
    BOOST_REQUIRE(f.service.runtime->SubmitOperation(ProtocolOperation{*signed_prepare}));
    BOOST_CHECK(f.service.runtime->GetStateRoot() == unsigned_root); // Candidate is volatile.
    BOOST_REQUIRE(f.service.runtime->ProduceBlock());
    const auto prepare_height = *f.service.runtime->GetFinalizedHeight();
    StorageSettlement activate{.period_start_utc = 100000}; activate.action = StorageSettlementAction::ACTIVATE;
    activate.preparation_id = *ComputeOperationId(ProtocolOperation{*signed_prepare}); activate.manifest = {chunk};
    BOOST_CHECK(!f.service.runtime->SignStorageSettlement(activate)); // h+1 is too early.
    const auto rotation_entropy = GenerateRecoveryEntropy(); BOOST_REQUIRE(rotation_entropy);
    const auto rotating = provider2->RotateIdentitySync(EncodeRecoveryWords(*rotation_entropy), "correct horse battery staple");
    BOOST_REQUIRE(rotating.phase == IdentityOperationPhase::ACCEPTED);
    BOOST_REQUIRE(f.service.runtime->ProduceBlock());
    BOOST_REQUIRE(provider2->ResumeIdentityRotationSync("correct horse battery staple").phase == IdentityOperationPhase::FINALIZED);
    const auto seed = *f.service.runtime->GetFinalizedTip();
    BOOST_CHECK_EQUAL(*f.service.runtime->GetFinalizedHeight(), prepare_height + 1);
    const auto activated_submit = journal->SubmitSettlement(activate); BOOST_REQUIRE(activated_submit);
    const auto signed_activate = journal->PreparedSettlement(activate); BOOST_REQUIRE(signed_activate);
    BOOST_CHECK(journal->PreparedSettlement(prepare) == signed_prepare);
    auto conflicting_activate = activate; conflicting_activate.period_start_utc += 1;
    BOOST_CHECK_THROW(journal->SubmitSettlement(conflicting_activate), std::runtime_error);
    BOOST_REQUIRE(f.service.runtime->ProduceBlock());
    for (uint64_t h{1}; h <= *f.service.runtime->GetFinalizedHeight(); ++h)
        BOOST_REQUIRE(verifier.runtime->CommitBlock(*f.service.runtime->GetBlockAtHeight(h)));
    BOOST_CHECK(f.service.runtime->GetStateRoot() == verifier.runtime->GetStateRoot());
    const auto accepted = verifier.runtime->GetStore().GetStateSnapshot(); BOOST_REQUIRE(accepted);
    const auto& term = accepted.state->leases.at(*publication_id).funded_terms.front();
    BOOST_REQUIRE_EQUAL(term.declarations.size(), 1U); BOOST_REQUIRE_EQUAL(term.assignments.size(), 1U);
    BOOST_CHECK(term.assignments[0].seed == seed);
    const auto* rotated = accepted.state->identities.Find(second_binding->payout_account); BOOST_REQUIRE(rotated);
    BOOST_CHECK_EQUAL(rotated->key_epoch, 1U);
    BOOST_CHECK(!VerifyIdentityMessage(rotated->authorization_key, second_binding->authorization,
        StoragePayoutBindingDigest(network, *second_storage, second_binding->payout_account)));
    const auto frozen = std::find_if(term.declarations[0].eligible.begin(), term.declarations[0].eligible.end(),
        [&](const auto& e) { return e.storage_id == *second_storage; });
    BOOST_REQUIRE(frozen != term.declarations[0].eligible.end()); BOOST_CHECK_EQUAL(frozen->key_epoch, 0U);
    BOOST_CHECK_EQUAL(term.declarations[0].height, prepare_height);
    BOOST_CHECK_EQUAL(accepted.state->settlement.next_period, 0U);
    BOOST_CHECK(!f.service.runtime->SignStorageSettlement(activate)); // Cannot replay in h+3.
    const auto exact = SerializeCybouState(*accepted.state); BOOST_REQUIRE(exact);
    const auto reopened = DeserializeCybouState(*exact); BOOST_REQUIRE(reopened);
    BOOST_CHECK(CybouStateHash(*reopened) == f.service.runtime->GetStateRoot());

    // Canonical observations use the accepted activation, not f.Attest() or a
    // historical caller registry. The transport is an isolated exact-byte fixture.
    ObservedTransport canonical_transport;
    for (const auto& allocation : term.assignments[0].allocations) {
        auto& provider_node = allocation.storage_id == f.proofs[0].storage_id ?
            *f.service.runtime : *verifier.runtime;
        const auto receipt = provider_node.SignStorageProof(StorageReceiptMessage(network, *publication_id, chunk, 4));
        BOOST_REQUIRE(receipt);
        StorageEndpoint endpoint{allocation.storage_id, "fixture", 29461};
        for (auto* node : {f.service.runtime.get(), verifier.runtime.get()}) {
            const auto observed = ObserveCanonicalStorageReplica(*node, canonical_transport, endpoint,
                *publication_id, *ComputeOperationId(ProtocolOperation{*signed_activate}), chunk,
                allocation.slot, *receipt, 4, f.ciphertext);
            BOOST_REQUIRE(observed); BOOST_CHECK(observed->kind == StorageAssignmentObservationKind::FULL_GET);
            BOOST_CHECK(observed->retrieved_bytes == f.ciphertext);
            const auto audited = ObserveCanonicalStorageReplica(*node, canonical_transport, endpoint,
                *publication_id, *ComputeOperationId(ProtocolOperation{*signed_activate}), chunk,
                allocation.slot, *receipt, 4, f.ciphertext, false);
            BOOST_REQUIRE(audited); BOOST_CHECK(audited->kind == StorageAssignmentObservationKind::OFFSET_AUDIT);
        }
        const auto gets = canonical_transport.gets, audits = canonical_transport.audits;
        BOOST_CHECK(!ObserveCanonicalStorageReplica(*f.service.runtime, canonical_transport, endpoint,
            CounterHash(123), *ComputeOperationId(ProtocolOperation{*signed_activate}), chunk, allocation.slot, *receipt, 4));
        BOOST_CHECK(!ObserveCanonicalStorageReplica(*f.service.runtime, canonical_transport, endpoint,
            *publication_id, *ComputeOperationId(ProtocolOperation{*signed_prepare}), chunk, allocation.slot, *receipt, 4));
        BOOST_CHECK(!ObserveCanonicalStorageReplica(*f.service.runtime, canonical_transport, endpoint,
            *publication_id, *ComputeOperationId(ProtocolOperation{*signed_activate}), chunk, 2, *receipt, 4));
        auto bad_receipt = *receipt; bad_receipt.back() ^= 1;
        BOOST_CHECK(!ObserveCanonicalStorageReplica(*f.service.runtime, canonical_transport, endpoint,
            *publication_id, *ComputeOperationId(ProtocolOperation{*signed_activate}), chunk, allocation.slot, bad_receipt, 4));
        BOOST_CHECK_EQUAL(canonical_transport.gets, gets); BOOST_CHECK_EQUAL(canonical_transport.audits, audits);
        canonical_transport.missing = true;
        BOOST_CHECK(!ObserveCanonicalStorageReplica(*f.service.runtime, canonical_transport, endpoint,
            *publication_id, *ComputeOperationId(ProtocolOperation{*signed_activate}), chunk, allocation.slot,
            *receipt, 4, f.ciphertext, false));
        BOOST_CHECK_EQUAL(canonical_transport.gets, gets); // Negative audit is never hidden by GET.
        canonical_transport.missing = false;
    }

    // Real PAY finality on both nodes; tiny terms accumulate service before
    // whole CYBOU entitlement. This tests PoA-attested totals, not raw audits.
    const auto activation_id = *ComputeOperationId(ProtocolOperation{*signed_activate});
    const auto total = TotalCybou(*accepted.state);
    std::map<AccountId, uint64_t> provider_before;
    for (const auto& allocation : term.assignments[0].allocations)
        provider_before[allocation.payout_account] = accepted.state->accounts.at(allocation.payout_account).onboarding_system_balance;
    std::optional<ProtocolOperation> first_pay;
    for (uint64_t period = 0; period < 30; ++period) {
        StorageSettlement pay{.period = period, .period_start_utc = 100000 + period * 86400};
        pay.period_end_utc = pay.period_start_utc + 86400;
        // Synthetic references exercise journal retention; this is not collector provenance.
        const std::array references{CounterHash(period + 100)};
        BinaryWriter reference_bytes; reference_bytes.U32(1); reference_bytes.Fixed({references[0].begin(), 32});
        const auto encoded_references = reference_bytes.Take();
        BOOST_REQUIRE(crypto::ComputeSha256({std::span<const unsigned char>{encoded_references}}, pay.evidence_root.begin()));
        pay.activation_witnesses = {activation_id};
        for (const auto& allocation : term.assignments[0].allocations)
            pay.entries.push_back({*publication_id, allocation.payout_account, period == 29 ? 1U : 0U,
                allocation.slot, allocation.storage_id, (period + 1) * 86400});
        std::sort(pay.entries.begin(), pay.entries.end(), [](const auto& a, const auto& b) {
            return std::tie(a.funding_operation_id,a.slot,a.storage_id,a.payout_account) <
                std::tie(b.funding_operation_id,b.slot,b.storage_id,b.payout_account); });
        const auto before = f.service.runtime->GetStateRoot();
        auto bad = pay; bad.entries[0].amount += 1;
        BOOST_CHECK(!f.service.runtime->SignStorageSettlement(bad));
        bad = pay; bad.entries[0].verified_unit_seconds += 1;
        BOOST_CHECK(!f.service.runtime->SignStorageSettlement(bad));
        bad = pay; bad.activation_witnesses.clear();
        BOOST_CHECK(!f.service.runtime->SignStorageSettlement(bad));
        bad = pay; bad.activation_witnesses.push_back(CounterHash(999));
        std::sort(bad.activation_witnesses.begin(),bad.activation_witnesses.end());
        BOOST_CHECK(!f.service.runtime->SignStorageSettlement(bad));
        BOOST_CHECK(!journal->SubmitSettlement(pay)); // Fresh nonempty PAY requires references.
        BOOST_CHECK(!journal->PreparedSettlement(pay));
        const auto pay_submit = journal->SubmitSettlement(pay, references); BOOST_REQUIRE(pay_submit);
        const auto signed_pay = journal->PreparedSettlement(pay); BOOST_REQUIRE(signed_pay);
        BOOST_CHECK(journal->SubmitSettlement(pay).op_id == pay_submit.op_id);
        auto conflicting_pay = pay; conflicting_pay.evidence_root = CounterHash(123456);
        BOOST_CHECK_THROW(journal->SubmitSettlement(conflicting_pay), std::runtime_error);
        BOOST_CHECK(f.service.runtime->GetStateRoot() == before);
        const ProtocolOperation operation{*signed_pay}; if (!first_pay) first_pay = operation;
        BOOST_REQUIRE(f.service.runtime->SubmitOperation(operation));
        BOOST_CHECK(f.service.runtime->GetStateRoot() == before);
        const auto block = f.service.runtime->ProduceBlock(); BOOST_REQUIRE(block);
        BOOST_REQUIRE(verifier.runtime->CommitBlock(*block));
        BOOST_CHECK(f.service.runtime->GetStateRoot() == verifier.runtime->GetStateRoot());
        const auto paid = verifier.runtime->GetStore().GetStateSnapshot(); BOOST_REQUIRE(paid);
        BOOST_CHECK_EQUAL(TotalCybou(*paid.state), total);
        const auto& retained = paid.state->leases.at(*publication_id).funded_terms.front();
        BOOST_REQUIRE_EQUAL(retained.service_payments.size(), 2U);
        BOOST_CHECK_EQUAL(retained.paid_onboarding, period == 29 ? 2U : 0U);
        BOOST_CHECK_EQUAL(retained.service_payments[0].verified_unit_seconds, (period + 1) * 86400);
        const auto serialized = SerializeCybouState(*paid.state); BOOST_REQUIRE(serialized);
        const auto restored = DeserializeCybouState(*serialized); BOOST_REQUIRE(restored);
        BOOST_CHECK(CybouStateHash(*restored) == verifier.runtime->GetStateRoot());
        BOOST_CHECK(!f.service.runtime->SignStorageSettlement(*signed_pay));
    }
    const auto final = verifier.runtime->GetStore().GetStateSnapshot(); BOOST_REQUIRE(final);
    const auto& closed = final.state->leases.at(*publication_id);
    BOOST_CHECK_EQUAL(closed.escrow_onboarding + closed.escrow_locked, 0U);
    for (const auto& [account, balance] : provider_before)
        BOOST_CHECK_EQUAL(final.state->accounts.at(account).onboarding_system_balance, balance + 1);
    const auto final_root = f.service.runtime->GetStateRoot();
    journal.reset(); f.db.reset();
    f.db = std::make_unique<PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    journal = std::make_unique<StorageService>(*f.service.runtime, transport, *f.db, 2);
    BOOST_CHECK(journal->SubmitSettlement(prepare).status == OperationSubmitStatus::ALREADY_FINALIZED);
    BOOST_CHECK(journal->SubmitSettlement(activate).status == OperationSubmitStatus::ALREADY_FINALIZED);
    BOOST_CHECK(journal->SubmitSettlement(std::get<StorageSettlement>(*first_pay)).status == OperationSubmitStatus::ALREADY_FINALIZED);
    BOOST_CHECK(journal->PreparedSettlement(prepare) == signed_prepare);
    BOOST_CHECK(journal->PreparedSettlement(activate) == signed_activate);
    BOOST_CHECK(f.service.runtime->SubmitOperation(*first_pay).status == OperationSubmitStatus::ALREADY_FINALIZED);
    BOOST_CHECK(f.service.runtime->GetStateRoot() == final_root);
}

BOOST_AUTO_TEST_CASE(atomic_assignment_wire_limits_and_exact_consumption)
{
    using namespace cybou;
    Fixture f;
    StorageSettlement prepare{.period_start_utc = 1}; prepare.action = StorageSettlementAction::PREPARE;
    prepare.funded_term_id = Hash256{1}; prepare.assignment_epoch = 1;
    prepare.poa_signature.ml_dsa.resize(3309);
    for (unsigned i{1}; i <= 20; ++i) {
        StorageSettlementBinding e{.binding = f.proofs[0].binding}; e.storage_id[0] = i;
        prepare.eligible.push_back(e);
    }
    auto encoded = SerializeProtocolOperation(ProtocolOperation{prepare}); BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), 131035U); BOOST_REQUIRE(DeserializeProtocolOperation(*encoded));
    auto body = *SerializeStorageSettlement(prepare); body.push_back(0);
    BOOST_CHECK(!DeserializeStorageSettlement(body));
    body = *SerializeStorageSettlement(prepare); body.resize(body.size() - 1);
    BOOST_CHECK(!DeserializeStorageSettlement(body));
    body = *SerializeStorageSettlement(prepare); body[16] = 4;
    BOOST_CHECK(!DeserializeStorageSettlement(body));
    body = *SerializeStorageSettlement(prepare); std::fill_n(body.begin() + 57, 4, 255);
    BOOST_CHECK(!DeserializeStorageSettlement(body));
    auto extra = prepare.eligible.back(); extra.storage_id[0] = 21; prepare.eligible.push_back(extra);
    BOOST_CHECK(!SerializeStorageSettlement(prepare));
    StorageSettlement activate{.period_start_utc = 1}; activate.action = StorageSettlementAction::ACTIVATE;
    activate.preparation_id = Hash256{2}; activate.poa_signature.ml_dsa.resize(3309);
    for (unsigned i{1}; i <= 3988; ++i) {
        ChunkId chunk{}; chunk[0] = i >> 8; chunk[1] = i; activate.manifest.push_back(chunk);
    }
    encoded = SerializeProtocolOperation(ProtocolOperation{activate}); BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), 131043U); BOOST_REQUIRE(DeserializeProtocolOperation(*encoded));
    ChunkId extra_chunk{}; extra_chunk[0] = 255; activate.manifest.push_back(extra_chunk);
    BOOST_CHECK(!SerializeStorageSettlement(activate));
    activate.manifest.pop_back(); activate.manifest.back() = activate.manifest.front();
    BOOST_CHECK(!SerializeStorageSettlement(activate));
    StorageSettlement pay{.period_start_utc = 1}; pay.period_end_utc = 86401; pay.evidence_root = Hash256{3};
    pay.poa_signature.ml_dsa.resize(3309);
    for (uint64_t i = 1; i <= 1024; ++i)
        pay.entries.push_back({CounterHash(i), f.proofs[0].binding.payout_account, 0, 0, f.proofs[0].storage_id, 1});
    std::sort(pay.entries.begin(), pay.entries.end(), [](const auto& a,const auto& b) { return a.funding_operation_id < b.funding_operation_id; });
    for (uint64_t i = 1; i <= 372; ++i) pay.activation_witnesses.push_back(CounterHash(i));
    std::sort(pay.activation_witnesses.begin(),pay.activation_witnesses.end());
    encoded = SerializeProtocolOperation(ProtocolOperation{pay}); BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), 131055U); BOOST_CHECK(DeserializeProtocolOperation(*encoded));
    pay.activation_witnesses.push_back(CounterHash(373)); std::sort(pay.activation_witnesses.begin(),pay.activation_witnesses.end());
    BOOST_CHECK(!SerializeStorageSettlement(pay));
    pay.activation_witnesses.pop_back();
    body = *encoded; body.push_back(0); BOOST_CHECK(!DeserializeProtocolOperation(body));
    body = *encoded; std::fill_n(body.begin() + 58,4,255); BOOST_CHECK(!DeserializeProtocolOperation(body));
}

BOOST_AUTO_TEST_CASE(bindings_require_both_keys_and_the_matching_snapshot)
{
    Fixture f;
    const auto verify = [&](const auto& proofs) { return cybou::VerifyStorageAssignmentBindings(
        f.plan, f.registry, *f.service.runtime->GetFinalizedTip(), proofs); };
    BOOST_REQUIRE(verify(f.proofs));
    auto bad = f.proofs; bad[0].binding.authorization.ed25519[0] ^= 1;
    BOOST_CHECK(!verify(bad));
    bad = f.proofs; bad[0].binding.storage_proof[0] ^= 1;
    BOOST_CHECK(!verify(bad));
    bad = f.proofs; bad[0].storage_id[0] ^= 1;
    BOOST_CHECK(!verify(bad));
    bad = f.proofs; bad[0].binding.payout_account = *f.payer->GetKeyStore().GetAccountId();
    BOOST_CHECK(!verify(bad));
    BOOST_CHECK(!cybou::VerifyStorageAssignmentBindings(f.plan, {}, *f.service.runtime->GetFinalizedTip(), f.proofs));
    BOOST_CHECK(!cybou::VerifyStorageAssignmentBindings(f.plan, f.registry, cybou::Hash256{1}, f.proofs));
    BOOST_CHECK(!verify(std::vector<cybou::StorageAssignmentBindingProof>{}));
    bad = f.proofs; bad.push_back(bad.front());
    BOOST_CHECK(!verify(bad));
}

BOOST_AUTO_TEST_CASE(portable_attestation_codec_is_exact_bounded_and_requires_verification)
{
    Fixture f;
    const auto attested = f.Attest(); BOOST_REQUIRE(attested);
    const auto encoded = cybou::EncodeStorageAssignmentAttestation(*attested); BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), 3562U + 64U * f.plan.eligible.size());
    const auto decoded = cybou::DecodeStorageAssignmentAttestation(*encoded); BOOST_REQUIRE(decoded);
    BOOST_CHECK(*decoded == *attested);
    BOOST_CHECK(cybou::VerifyStorageAssignmentAttestation(f.service.definition, *decoded));
    BOOST_CHECK(cybou::VerifyStorageAssignmentBindings(decoded->plan, f.registry,
        *f.service.runtime->GetFinalizedTip(), f.proofs));
    BOOST_CHECK(cybou::EncodeStorageAssignmentAttestation(*decoded) == encoded);
    for (const auto size : {0U, 184U, 188U, 189U, 3561U, 3625U}) {
        BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(std::span{*encoded}.first(size)));
    }
    auto bad = *encoded; bad.push_back(0);
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(bad));
    bad = *encoded;
    for (unsigned i{0}; i < 4; ++i) bad[185 + i] = 255;
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(bad));
    bad = *encoded; std::fill_n(bad.begin() + 185, 4, 0);
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(bad));
    // Structurally valid bytes never stand in for a signature check.
    bad = *encoded; bad.back() ^= 1;
    const auto tampered = cybou::DecodeStorageAssignmentAttestation(bad); BOOST_REQUIRE(tampered);
    BOOST_CHECK(!cybou::VerifyStorageAssignmentAttestation(f.service.definition, *tampered));
    auto invalid = *attested; invalid.signature.ml_dsa.pop_back();
    BOOST_CHECK(!cybou::EncodeStorageAssignmentAttestation(invalid));
    invalid = *attested; invalid.plan.commitment[0] ^= 1;
    BOOST_CHECK(!cybou::EncodeStorageAssignmentAttestation(invalid));
    std::vector<unsigned char> oversized(cybou::MAX_OPERATION_PAYLOAD_BYTES + 1);
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(oversized));
}

BOOST_AUTO_TEST_CASE(portable_attestation_rejects_duplicate_unsorted_and_oversized_eligible_sets)
{
    Fixture f;
    std::vector<cybou::StorageAssignmentProvider> eligible;
    for (unsigned i{1}; i <= cybou::MAX_STORAGE_ASSIGNMENT_CANDIDATES; ++i) {
        cybou::StorageAssignmentProvider p;
        p.storage_id[0] = static_cast<unsigned char>(i >> 8);
        p.storage_id[1] = static_cast<unsigned char>(i);
        p.payout_account = p.storage_id;
        eligible.push_back(p);
    }
    const auto plan = cybou::PrepareStorageAssignment(f.plan.context, eligible); BOOST_REQUIRE(plan);
    cybou::AttestedStorageAssignment attested{*plan, {.ml_dsa = std::vector<unsigned char>(3309)}};
    const auto encoded = cybou::EncodeStorageAssignmentAttestation(attested); BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), 69098U);
    const auto decoded = cybou::DecodeStorageAssignmentAttestation(*encoded); BOOST_REQUIRE(decoded);
    BOOST_CHECK(*decoded == attested);
    auto bad = *encoded;
    std::copy_n(bad.begin() + 189, 64, bad.begin() + 253);
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(bad));
    bad = *encoded;
    std::swap_ranges(bad.begin() + 189, bad.begin() + 253, bad.begin() + 253);
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(bad));
    bad = *encoded; bad[185] = 1; bad[186] = 4; // 1025, reject before allocation.
    BOOST_CHECK(!cybou::DecodeStorageAssignmentAttestation(bad));
    attested.plan.eligible.push_back(eligible.back());
    BOOST_CHECK(!cybou::EncodeStorageAssignmentAttestation(attested));
}

BOOST_AUTO_TEST_CASE(attestation_is_self_verified_durable_and_reused_without_resigning)
{
    Fixture f;
    const auto signed_plan = f.Attest();
    BOOST_REQUIRE(signed_plan);
    BOOST_REQUIRE(cybou::VerifyStorageAssignmentAttestation(f.service.definition, *signed_plan));
    BOOST_CHECK_EQUAL(f.signer.calls, 1U);
    f.signer.fail = true;
    BOOST_CHECK(f.Attest() == signed_plan);
    BOOST_CHECK_EQUAL(f.signer.calls, 1U);
    f.db.reset();
    f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    BOOST_CHECK(cybou::LoadStorageAssignmentAttestation(*f.db, f.service.definition, f.plan.context) == signed_plan);
    const auto retained = cybou::LoadStorageAssignmentBindings(*f.db, f.service.definition,
        f.plan.context, f.registry, *f.service.runtime->GetFinalizedTip());
    BOOST_REQUIRE(retained);
    BOOST_REQUIRE_EQUAL(retained->size(), 1U);
    BOOST_CHECK(retained->front().binding == f.proofs.front().binding);
    BOOST_CHECK(!cybou::LoadStorageAssignmentBindings(*f.db, f.service.definition, f.plan.context,
        {}, *f.service.runtime->GetFinalizedTip()));
    auto tampered = *signed_plan; tampered.signature.ed25519[0] ^= 1;
    BOOST_CHECK(!cybou::VerifyStorageAssignmentAttestation(f.service.definition, tampered));
    tampered = *signed_plan; ++tampered.plan.context.epoch;
    BOOST_CHECK(!cybou::VerifyStorageAssignmentAttestation(f.service.definition, tampered));
    tampered = *signed_plan; tampered.plan.context.chunk[0] ^= 1;
    BOOST_CHECK(!cybou::VerifyStorageAssignmentAttestation(f.service.definition, tampered));
    f.payer->GetKeyStore().Clear();
    BOOST_CHECK(!f.Attest());
    BOOST_CHECK_EQUAL(f.signer.calls, 1U);
}

BOOST_AUTO_TEST_CASE(receipt_is_bound_to_assigned_key_chunk_publication_and_size)
{
    Fixture f;
    const auto signed_plan = f.Attest();
    BOOST_REQUIRE(signed_plan);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, f.ciphertext.size()));
    BOOST_REQUIRE(receipt);
    BOOST_CHECK(cybou::VerifyAssignedStorageReceipt(f.service.definition, *signed_plan, 0, *receipt, 4));
    BOOST_CHECK(!cybou::VerifyAssignedStorageReceipt(f.service.definition, *signed_plan, 0, *receipt, 5));
    BOOST_CHECK(!cybou::VerifyAssignedStorageReceipt(f.service.definition, *signed_plan, 1, *receipt, 4));
    auto bad = *receipt; bad.back() ^= 1;
    BOOST_CHECK(!cybou::VerifyAssignedStorageReceipt(f.service.definition, *signed_plan, 0, bad, 4));
    const auto other = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), cybou::Hash256{1}, f.plan.context.chunk, 4));
    BOOST_REQUIRE(other);
    BOOST_CHECK(!cybou::VerifyAssignedStorageReceipt(f.service.definition, *signed_plan, 0, *other, 4));
    cybou::CybouNodeRuntime foreign{{.network_genesis = f.service.definition,
        .data_dir = f.service.directory / "foreign-storage", .memory_only = true, .wipe_data = true}};
    BOOST_REQUIRE(foreign.InitializeGenesis(f.service.genesis));
    const auto foreign_receipt = foreign.SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4));
    BOOST_REQUIRE(foreign_receipt);
    BOOST_CHECK(!cybou::VerifyAssignedStorageReceipt(f.service.definition, *signed_plan, 0, *foreign_receipt, 4));
}

BOOST_AUTO_TEST_CASE(wrong_signer_and_corrupt_journal_fail_without_replacement)
{
    Fixture f;
    {
        cybou::PrivateApplicationStore::Batch enclosing{*f.db};
        BOOST_CHECK(!f.Attest());
        BOOST_CHECK_EQUAL(f.signer.calls, 0U);
        BOOST_CHECK(!cybou::LoadStorageAssignment(*f.db, f.plan.context));
    }
    auto wrong_seed = f.service.validator_seed; wrong_seed[0] ^= 1;
    Signer wrong{wrong_seed};
    BOOST_CHECK(!cybou::AttestStorageAssignment(*f.db, f.service.definition, f.plan, f.registry,
        *f.service.runtime->GetFinalizedTip(), f.proofs, wrong));
    BOOST_CHECK_EQUAL(wrong.calls, 0U);
    f.signer.fail = true;
    BOOST_CHECK(!f.Attest());
    BOOST_CHECK(cybou::LoadStorageAssignment(*f.db, f.plan.context) == f.plan);
    f.signer.fail = false;
    BOOST_REQUIRE(f.Attest());
    BOOST_CHECK_EQUAL(f.signer.calls, 2U);
    const auto key = "storage/assignment-attestation/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.plan.commitment}}.GetHex();
    const auto binding_key = key + "/binding/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.proofs[0].storage_id}}.GetHex();
    BOOST_REQUIRE(f.db->Put(binding_key, std::vector<unsigned char>{1, 2, 3}));
    BOOST_CHECK(!cybou::LoadStorageAssignmentBindings(*f.db, f.service.definition, f.plan.context,
        f.registry, *f.service.runtime->GetFinalizedTip()));
    BOOST_CHECK(!f.Attest());
    BOOST_CHECK_EQUAL(f.signer.calls, 2U);
    BOOST_REQUIRE(f.db->Put(binding_key, cybou::EncodeStoragePayoutBinding(f.proofs[0].binding)));
    BOOST_REQUIRE(f.db->Put(key, std::vector<unsigned char>{1, 2, 3}));
    BOOST_CHECK(!cybou::LoadStorageAssignmentAttestation(*f.db, f.service.definition, f.plan.context));
    BOOST_CHECK(!f.Attest());
    BOOST_CHECK_EQUAL(f.signer.calls, 2U);
}
BOOST_AUTO_TEST_CASE(assigned_observation_checks_scope_before_transport_and_exact_get)
{
    Fixture f;
    const auto signed_plan = f.Attest(); BOOST_REQUIRE(signed_plan);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4));
    BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    const auto observe = [&](const auto& target, const auto& plan, const auto& proof, unsigned size) {
        return cybou::ObserveAssignedStorageReplica(transport, target, f.service.definition, plan, 0, proof, size);
    };
    auto foreign = endpoint; foreign.storage_id[0] ^= 1;
    BOOST_CHECK(!observe(foreign, *signed_plan, *receipt, 4));
    auto bad_plan = *signed_plan; bad_plan.signature.ed25519[0] ^= 1;
    BOOST_CHECK(!observe(endpoint, bad_plan, *receipt, 4));
    auto bad_receipt = *receipt; bad_receipt.back() ^= 1;
    BOOST_CHECK(!observe(endpoint, *signed_plan, bad_receipt, 4));
    BOOST_CHECK(!observe(endpoint, *signed_plan, *receipt, 5));
    BOOST_CHECK_EQUAL(transport.gets, 0U); BOOST_CHECK_EQUAL(transport.audits, 0U);
    const auto success = observe(endpoint, *signed_plan, *receipt, 4); BOOST_REQUIRE(success);
    BOOST_CHECK(success->kind == cybou::StorageAssignmentObservationKind::FULL_GET);
    BOOST_CHECK(success->retrieved_bytes == f.ciphertext);
    BOOST_CHECK(success->receipt == *receipt);
    BOOST_CHECK(success->storage_id == endpoint.storage_id);
    BOOST_CHECK(success->assignment_commitment == f.plan.commitment);
    BOOST_CHECK(!success->challenge && !success->answer);
    transport.bytes[0] ^= 1;
    BOOST_CHECK(!observe(endpoint, *signed_plan, *receipt, 4));
    transport.bytes = {1, 2, 3};
    BOOST_CHECK(!observe(endpoint, *signed_plan, *receipt, 4));
    transport.unavailable = true;
    BOOST_CHECK(!observe(endpoint, *signed_plan, *receipt, 4));
}

BOOST_AUTO_TEST_CASE(assigned_audit_uses_fresh_challenge_and_preserves_negative_answers)
{
    Fixture f;
    const auto signed_plan = f.Attest(); BOOST_REQUIRE(signed_plan);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4));
    BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    const auto observe = [&](bool full = false) { return cybou::ObserveAssignedStorageReplica(
        transport, endpoint, f.service.definition, *signed_plan, 0, *receipt, 4, f.ciphertext, full); };
    const auto first = observe(); BOOST_REQUIRE(first);
    BOOST_CHECK(first->kind == cybou::StorageAssignmentObservationKind::OFFSET_AUDIT);
    BOOST_REQUIRE(first->challenge); BOOST_REQUIRE(first->answer);
    BOOST_CHECK(first->retrieved_bytes.empty());
    const auto second = observe(); BOOST_REQUIRE(second); BOOST_REQUIRE(second->challenge);
    BOOST_CHECK(first->challenge->nonce != second->challenge->nonce);
    BOOST_CHECK_EQUAL(transport.gets, 0U);
    transport.missing = true; BOOST_CHECK(!observe());
    transport.missing = false; transport.incorrect = true; BOOST_CHECK(!observe());
    BOOST_CHECK_EQUAL(transport.gets, 0U);
    transport.incorrect = false;
    const auto full = observe(true); BOOST_REQUIRE(full);
    BOOST_CHECK(full->kind == cybou::StorageAssignmentObservationKind::FULL_GET);
    transport.audit_unavailable = true;
    const auto fallback = observe(); BOOST_REQUIRE(fallback);
    BOOST_CHECK(fallback->kind == cybou::StorageAssignmentObservationKind::FULL_GET);
    BOOST_CHECK(!fallback->challenge && !fallback->answer);
    transport.unavailable = true; BOOST_CHECK(!observe());
    BOOST_CHECK_EQUAL(transport.gets, 3U);
    auto corrupt_local = f.ciphertext; corrupt_local[0] ^= 1;
    const auto requests = transport.audits + transport.gets;
    BOOST_CHECK(!cybou::ObserveAssignedStorageReplica(transport, endpoint, f.service.definition,
        *signed_plan, 0, *receipt, 4, corrupt_local));
    BOOST_CHECK_EQUAL(transport.audits + transport.gets, requests);
}
BOOST_AUTO_TEST_CASE(observation_journal_retains_raw_audit_and_rechecks_after_reopen)
{
    Fixture f;
    const auto assignment = f.Attest(); BOOST_REQUIRE(assignment);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4));
    BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    const auto record = cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
        f.service.definition, *assignment, 0, *receipt, 4, 100, 100, f.ciphertext);
    BOOST_REQUIRE(record); BOOST_REQUIRE(record->observation.challenge); BOOST_REQUIRE(record->observation.answer);
    const auto original_nonce = record->observation.challenge->nonce;
    BOOST_CHECK(!cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
        f.service.definition, *assignment, 0, *receipt, 4, 100, 100, f.ciphertext));
    f.db.reset();
    f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    const auto loaded = cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 100, f.ciphertext);
    BOOST_REQUIRE(loaded); BOOST_REQUIRE(loaded->observation.challenge); BOOST_REQUIRE(loaded->observation.answer);
    BOOST_CHECK(loaded->observation.challenge->nonce == original_nonce);
    BOOST_CHECK(loaded->observation.answer->response_hash == record->observation.answer->response_hash);
    BOOST_CHECK(loaded->observation.receipt == *receipt);
    BOOST_CHECK_EQUAL(loaded->observed_at_utc, 100U);
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 101, f.ciphertext));
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 1, 100, f.ciphertext));
    auto wrong = f.ciphertext; wrong[0] ^= 1;
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 100, wrong));
    const auto prefix = "storage/assignment-observations/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.plan.commitment}}.GetHex() + "/0";
    auto raw = f.db->Get(prefix + "/100"); BOOST_REQUIRE(raw);
    raw->back() ^= 1; BOOST_REQUIRE(f.db->Put(prefix + "/100", *raw));
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 100, f.ciphertext));
    BOOST_CHECK(!cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
        f.service.definition, *assignment, 0, *receipt, 4, 100, 100, f.ciphertext));
    BOOST_CHECK(f.db->Get(prefix + "/100") == raw);
}

BOOST_AUTO_TEST_CASE(observation_journal_full_get_is_atomic_idempotent_and_does_not_duplicate_body)
{
    Fixture f;
    const auto assignment = f.Attest(); BOOST_REQUIRE(assignment);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4));
    BOOST_REQUIRE(receipt);
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    const auto collect = [&] {
        ObservedTransport transport;
        return cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
            f.service.definition, *assignment, 0, *receipt, 4, 200, 200, {}, true);
    };
    auto first = std::async(std::launch::async, collect);
    auto second = std::async(std::launch::async, collect);
    BOOST_REQUIRE(first.get()); BOOST_REQUIRE(second.get());
    f.db.reset();
    f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    const auto loaded = cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 200, f.ciphertext);
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(loaded->observation.kind == cybou::StorageAssignmentObservationKind::FULL_GET);
    BOOST_CHECK(loaded->observation.retrieved_bytes.empty());
    BOOST_CHECK(!loaded->observation.challenge && !loaded->observation.answer);
    BOOST_REQUIRE(collect());
    const auto prefix = "storage/assignment-observations/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.plan.commitment}}.GetHex() + "/0";
    const auto index = f.db->Get(prefix + "/index"); BOOST_REQUIRE(index);
    BOOST_CHECK_EQUAL(index->size(), 12U);
    BOOST_CHECK_EQUAL((*index)[0], 1U);
    BOOST_REQUIRE(f.db->Erase(prefix + "/200"));
    BOOST_CHECK(!collect()); // Missing indexed record is corruption, not a new observation.
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 200, f.ciphertext));
}

BOOST_AUTO_TEST_CASE(observation_journal_rejects_invalid_time_nested_locked_and_corrupt_index)
{
    Fixture f;
    const auto assignment = f.Attest(); BOOST_REQUIRE(assignment);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4));
    BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    const auto collect = [&](std::uint64_t time, std::uint64_t through) {
        return cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
            f.service.definition, *assignment, 0, *receipt, 4, time, through, {}, true);
    };
    BOOST_CHECK(!collect(0, 10)); BOOST_CHECK(!collect(11, 10));
    {
        cybou::PrivateApplicationStore::Batch enclosing{*f.db};
        BOOST_CHECK(!collect(10, 10));
    }
    BOOST_CHECK_EQUAL(transport.gets, 0U);
    const auto prefix = "storage/assignment-observations/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.plan.commitment}}.GetHex() + "/0";
    BOOST_REQUIRE(f.db->Put(prefix + "/index", std::vector<unsigned char>{1, 16, 0, 0})); // count 4097
    BOOST_CHECK(!collect(10, 10));
    BOOST_CHECK_EQUAL(transport.gets, 0U);
    BOOST_REQUIRE(f.db->Erase(prefix + "/index"));
    transport.unavailable = true; BOOST_CHECK(!collect(10, 10));
    BOOST_CHECK(!f.db->Has(prefix + "/index"));
    transport.unavailable = false; BOOST_REQUIRE(collect(10, 10));
    f.payer->GetKeyStore().Clear();
    BOOST_CHECK(!collect(11, 11));
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 10, f.ciphertext));
}
BOOST_AUTO_TEST_CASE(service_observations_feed_cumulative_quote_without_first_check_or_early_payment)
{
    Fixture f;
    const auto assignment = f.Attest(); BOOST_REQUIRE(assignment);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4)); BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    cybou::StorageAssignmentEvidenceScope scope{f.plan, 0, 1000, 86400};
    const auto observe = [&](std::uint64_t time) {
        return cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
            f.service.definition, *assignment, 0, *receipt, 4, time, time, f.ciphertext, false, &scope);
    };
    BOOST_REQUIRE(observe(1000));
    BOOST_CHECK_EQUAL(transport.gets, 1U); // Admission starts with an exact GET.
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 0), 0U);
    BOOST_REQUIRE(observe(44200));
    BOOST_CHECK_EQUAL(transport.audits, 1U);
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 0), 43200U);
    const auto requests = transport.gets + transport.audits;
    BOOST_REQUIRE(observe(44200)); // Exact durable retry is read-only.
    BOOST_CHECK_EQUAL(transport.gets + transport.audits, requests);
    const std::array scopes{scope}; const std::array<cybou::ChunkId, 1> chunks{f.plan.context.chunk};
    const auto quote = [&](std::uint64_t period) {
        return cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition,
            scopes, chunks, 5, period, {}, f.RegistrySnapshots());
    };
    BOOST_REQUIRE(observe(87400));
    const auto day = quote(0); BOOST_REQUIRE(day);
    BOOST_CHECK_EQUAL(day->verified_unit_seconds, 86400U);
    BOOST_CHECK_EQUAL(day->payout, 0U);
    for (std::uint64_t half_day{3}; half_day <= 60; ++half_day) {
        if (half_day == 17) {
            f.db.reset();
            f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
        }
        BOOST_REQUIRE(observe(1000 + half_day * cybou::STORAGE_ASSIGNMENT_CHECK_INTERVAL_SECONDS));
    }
    const auto complete = quote(29); BOOST_REQUIRE(complete);
    BOOST_CHECK_EQUAL(complete->verified_unit_seconds, 30U * 86400U);
    BOOST_CHECK_EQUAL(complete->payout, 1U);
    BOOST_CHECK_EQUAL(transport.gets, 8U); // First success, then 8,16,...,56.
    BOOST_CHECK_EQUAL(transport.audits, 53U);
    BOOST_CHECK_EQUAL(quote(0)->payout, 0U); // No future-service import into day one.
}

BOOST_AUTO_TEST_CASE(service_failures_gaps_and_crash_intents_break_credit_without_resetting_get_deadline)
{
    Fixture f;
    const auto assignment = f.Attest(); BOOST_REQUIRE(assignment);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4)); BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    cybou::StorageAssignmentEvidenceScope scope{f.plan, 0, 1000, 86400};
    const auto observe = [&](std::uint64_t time) {
        return cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
            f.service.definition, *assignment, 0, *receipt, 4, time, time, f.ciphertext, false, &scope);
    };
    for (unsigned i{0}; i < 7; ++i) BOOST_REQUIRE(observe(1000 + i * 100));
    BOOST_CHECK_EQUAL(transport.gets, 1U);
    transport.unavailable = true;
    BOOST_CHECK(!observe(1700));
    BOOST_CHECK(!observe(1800));
    BOOST_CHECK_EQUAL(transport.gets, 3U); // Failed eighth-success GET remains due.
    f.db.reset();
    f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    transport.unavailable = false;
    BOOST_REQUIRE(observe(1900));
    BOOST_CHECK_EQUAL(transport.gets, 4U);
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 0), 600U);
    BOOST_REQUIRE(observe(2000));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 0), 700U);
    BOOST_REQUIRE(observe(2000 + 86400)); // Exactly 24 hours is payable and split by period.
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 1), 87100U);
    BOOST_REQUIRE(observe(2001 + 2 * 86400)); // More than 24 hours pays none of the gap.
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 2), 87100U);
    const auto key = "storage/assignment-observations/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.plan.commitment}}.GetHex() + "/0/service-checkpoint";
    const auto checkpoint = f.db->Get(key); BOOST_REQUIRE(checkpoint);
    auto interrupted = *checkpoint;
    std::fill_n(interrupted.begin() + 24, 8, 0); // Durable pre-I/O intent has no successful predecessor.
    interrupted.back() = 1;
    BOOST_REQUIRE(f.db->Put(key, interrupted));
    f.db.reset();
    f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    BOOST_REQUIRE(observe(2101 + 2 * 86400));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 2), 87100U);
    BOOST_REQUIRE(observe(2201 + 2 * 86400));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 2), 87200U);
    auto replacement_context = f.plan.context; ++replacement_context.epoch;
    const auto replacement_plan = cybou::PrepareStorageAssignment(replacement_context, f.plan.eligible);
    BOOST_REQUIRE(replacement_plan);
    f.plan = *replacement_plan;
    const auto replacement = f.Attest(); BOOST_REQUIRE(replacement);
    cybou::StorageAssignmentEvidenceScope replacement_scope{f.plan, 0, 1000, 86400};
    const auto gets_before_replacement = transport.gets;
    BOOST_REQUIRE(cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
        f.service.definition, *replacement, 0, *receipt, 4, 2251 + 2 * 86400, 2251 + 2 * 86400,
        f.ciphertext, false, &replacement_scope));
    BOOST_CHECK_EQUAL(transport.gets, gets_before_replacement + 1);
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, replacement_scope, 2), 0U);
    BOOST_REQUIRE(f.db->Erase(key)); // Missing checkpoint with retained observations is corruption.
    const auto requests = transport.gets + transport.audits;
    BOOST_CHECK(!observe(2301 + 2 * 86400));
    BOOST_CHECK_EQUAL(transport.gets + transport.audits, requests);
}

BOOST_AUTO_TEST_CASE(service_credit_and_observation_commit_roll_back_together_on_overlap)
{
    Fixture f;
    const auto assignment = f.Attest(); BOOST_REQUIRE(assignment);
    const auto publication = cybou::Hash256{std::span<const unsigned char, 32>{f.plan.context.publication}};
    const auto receipt = f.service.runtime->SignStorageProof(cybou::StorageReceiptMessage(
        f.service.runtime->GetNetworkBinding(), publication, f.plan.context.chunk, 4)); BOOST_REQUIRE(receipt);
    ObservedTransport transport;
    cybou::StorageEndpoint endpoint{.storage_id = f.plan.selected[0].storage_id};
    cybou::StorageAssignmentEvidenceScope scope{f.plan, 0, 1000, 86400};
    const auto observe = [&](std::uint64_t time) {
        return cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
            f.service.definition, *assignment, 0, *receipt, 4, time, time, f.ciphertext, false, &scope);
    };
    BOOST_REQUIRE(observe(1000));
    cybou::StorageAssignmentInterval conflict{0, 1000, 1100}; conflict.proof_commitment.fill(9);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scope, conflict, 1100) == cybou::StorageEvidenceAppendResult::ADDED);
    BOOST_CHECK(!observe(1200));
    BOOST_CHECK(!cybou::LoadAssignedStorageObservation(*f.db, f.service.definition, *assignment, 0, 1200, f.ciphertext));
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 0), 100U);
    BOOST_REQUIRE(observe(1300)); // Uncommitted prior result cannot become a successful predecessor.
    BOOST_CHECK_EQUAL(*cybou::StorageAssignmentVerifiedSeconds(*f.db, scope, 0), 100U);
    auto invalid_scope = scope; invalid_scope.term_start_utc = std::numeric_limits<std::uint64_t>::max();
    BOOST_CHECK(!cybou::ObserveAndStoreAssignedStorageReplica(*f.db, transport, endpoint,
        f.service.definition, *assignment, 0, *receipt, 4, 1400, 1400, f.ciphertext, false, &invalid_scope));
}

BOOST_AUTO_TEST_CASE(slot_payout_quote_uses_cumulative_evidence_finality_and_survives_reopen)
{
    Fixture f;
    const auto attested = f.Attest(); BOOST_REQUIRE(attested);
    cybou::StorageAssignmentEvidenceScope scope{f.plan, 0, 1000, 86400};
    std::array scopes{scope}; std::array<cybou::ChunkId, 1> chunks{f.plan.context.chunk};
    const auto quote = [&](std::uint64_t period, const std::vector<cybou::StorageAssignmentPaid>& paid = {}) {
        return cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, scopes, chunks, 5, period, paid, f.RegistrySnapshots());
    };
    auto empty = quote(0); BOOST_REQUIRE(empty); BOOST_CHECK_EQUAL(empty->payout, 0U);
    cybou::StorageAssignmentInterval first{0, 1000, 87400}; first.proof_commitment.fill(1);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scope, first, 87400) == cybou::StorageEvidenceAppendResult::ADDED);
    auto day = quote(0); BOOST_REQUIRE(day); BOOST_REQUIRE_EQUAL(day->entries.size(), 1U);
    BOOST_CHECK_EQUAL(day->budget.per_replica, 1U); BOOST_CHECK_EQUAL(day->payout, 0U);
    BOOST_CHECK_EQUAL(day->verified_unit_seconds, 86400U);
    BOOST_CHECK(!quote(0, {{f.plan.selected[0], 1}})); // Paid cannot exceed accrued floor.
    for (std::uint64_t period{1}; period < 30; ++period) {
        cybou::StorageAssignmentInterval interval{period, 1000 + period * 86400, 1000 + (period + 1) * 86400};
        interval.proof_commitment.fill(static_cast<unsigned char>(period + 1));
        BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scope, interval, interval.end_utc) == cybou::StorageEvidenceAppendResult::ADDED);
    }
    auto complete = quote(29); BOOST_REQUIRE(complete); BOOST_CHECK_EQUAL(complete->payout, 1U);
    BOOST_CHECK_EQUAL(complete->finalized_paid, 0U);
    BOOST_CHECK_EQUAL(quote(0)->payout, 0U); // Future evidence never pays an earlier period.
    BOOST_CHECK_EQUAL(quote(29)->payout, 1U); // Preparing twice never marks paid.
    f.db.reset();
    f.db = std::make_unique<cybou::PrivateApplicationStore>(f.payer->GetKeyStore(), f.service.directory / "attest");
    BOOST_CHECK_EQUAL(quote(29)->payout, 1U);
    const std::vector<cybou::StorageAssignmentPaid> paid{{f.plan.selected[0], 1}};
    auto finalized = quote(29, paid); BOOST_REQUIRE(finalized);
    BOOST_CHECK_EQUAL(finalized->payout, 0U); BOOST_CHECK_EQUAL(finalized->finalized_paid, 1U);
    BOOST_CHECK(!quote(29, {{f.plan.selected[0], 2}}));
    BOOST_CHECK(!quote(29, {{f.plan.selected[0], 0}, {f.plan.selected[0], 0}}));
    BOOST_CHECK(!quote(30));
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition,
        scopes, chunks, 5, 29, {}, {}));
    auto wrong_snapshot = f.RegistrySnapshots(); wrong_snapshot[0].block_id = cybou::Hash256{1};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition,
        scopes, chunks, 5, 29, {}, wrong_snapshot));
    cybou::IdentityRegistry wrong_registry;
    auto wrong_keys = f.RegistrySnapshots(); wrong_keys[0].registry = &wrong_registry;
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition,
        scopes, chunks, 5, 29, {}, wrong_keys));
    const auto key = "storage/assignment-attestation/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.plan.commitment}}.GetHex() + "/binding/" +
        cybou::Hash256{std::span<const unsigned char, 32>{f.proofs[0].storage_id}}.GetHex();
    BOOST_REQUIRE(f.db->Put(key, std::vector<unsigned char>{1, 2, 3}));
    BOOST_CHECK(!quote(29));
    BOOST_REQUIRE(f.db->Put(key, cybou::EncodeStoragePayoutBinding(f.proofs[0].binding)));
    BOOST_REQUIRE(quote(29));
    f.payer->GetKeyStore().Clear(); BOOST_CHECK(!quote(29));
}

BOOST_AUTO_TEST_CASE(slot_payout_quote_requires_complete_epoch_and_chunk_manifest)
{
    Fixture f;
    BOOST_REQUIRE(f.Attest());
    auto context = f.plan.context; ++context.epoch;
    const auto replacement = cybou::PrepareStorageAssignment(context, f.plan.eligible); BOOST_REQUIRE(replacement);
    BOOST_REQUIRE(cybou::AttestStorageAssignment(*f.db, f.service.definition, *replacement, f.registry,
        *f.service.runtime->GetFinalizedTip(), f.proofs, f.signer));
    cybou::StorageAssignmentEvidenceScope old_scope{f.plan, 0, 1000, 100}, new_scope{*replacement, 0, 1000, 100};
    cybou::StorageAssignmentInterval early{0, 1000, 1050}; early.proof_commitment.fill(1);
    auto late = early; late.start_utc = 1050; late.end_utc = 1100; late.proof_commitment.fill(2);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, old_scope, early, 1100) == cybou::StorageEvidenceAppendResult::ADDED);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, new_scope, late, 1100) == cybou::StorageEvidenceAppendResult::ADDED);
    std::array<cybou::ChunkId, 1> chunks{f.plan.context.chunk};
    std::array all{old_scope, new_scope}; std::array newest{new_scope};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, newest, chunks, 5, 0, {}, f.RegistrySnapshots()));
    const auto combined = cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, all, chunks, 5, 0, {}, f.RegistrySnapshots());
    BOOST_REQUIRE(combined); BOOST_REQUIRE_EQUAL(combined->entries.size(), 1U);
    BOOST_CHECK_EQUAL(combined->verified_unit_seconds, 100U);
    BOOST_CHECK_EQUAL(combined->entries[0].verified_unit_seconds, 100U); // Same provider across epochs retains one floor.
    std::array duplicated{old_scope, old_scope};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, duplicated, chunks, 5, 0, {}, f.RegistrySnapshots()));
    std::array duplicate_chunks{f.plan.context.chunk, f.plan.context.chunk};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, all, duplicate_chunks, 5, 0, {}, f.RegistrySnapshots()));
    auto unknown = f.plan.context.chunk; unknown[0] ^= 1;
    std::array incomplete{f.plan.context.chunk, unknown};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, all, incomplete, 5, 0, {}, f.RegistrySnapshots()));
    auto bad = all; ++bad[1].period_seconds;
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, bad, chunks, 5, 0, {}, f.RegistrySnapshots()));
    auto foreign = f.plan.selected[0]; foreign.storage_id[0] ^= 1;
    std::array paid{cybou::StorageAssignmentPaid{foreign, 1}};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, all, chunks, 5, 0, paid, f.RegistrySnapshots()));
}
BOOST_AUTO_TEST_CASE(slot_payout_provider_replacement_preserves_separate_floors_under_one_budget)
{
    Fixture f;
    auto context = f.plan.context; context.term_end = 1;
    const auto initial = cybou::PrepareStorageAssignment(context, f.plan.eligible); BOOST_REQUIRE(initial);
    f.plan = *initial; BOOST_REQUIRE(f.Attest());
    cybou::CybouNodeRuntime foreign{{.network_genesis = f.service.definition,
        .data_dir = f.service.directory / "replacement-storage", .memory_only = true, .wipe_data = true}};
    BOOST_REQUIRE(foreign.InitializeGenesis(f.service.genesis));
    foreign.SetIdentitySigner(std::make_shared<cybou::CybouKeyStoreIdentitySigner>(f.provider->GetKeyStore()));
    const auto binding = foreign.LocalStoragePayoutBinding(); const auto storage = foreign.LocalStorageId();
    BOOST_REQUIRE(binding); BOOST_REQUIRE(storage);
    cybou::StorageAssignmentProvider replacement_provider{.storage_id = *storage};
    std::copy_n(binding->payout_account.Value().begin(), 32, replacement_provider.payout_account.begin());
    ++context.epoch;
    const auto replacement = cybou::PrepareStorageAssignment(context, {replacement_provider}); BOOST_REQUIRE(replacement);
    std::array proofs{cybou::StorageAssignmentBindingProof{*storage, *binding}};
    BOOST_REQUIRE(cybou::AttestStorageAssignment(*f.db, f.service.definition, *replacement, f.registry,
        *f.service.runtime->GetFinalizedTip(), proofs, f.signer));
    std::array scopes{cybou::StorageAssignmentEvidenceScope{f.plan, 0, 1000, 86400},
        cybou::StorageAssignmentEvidenceScope{*replacement, 0, 1000, 86400}};
    cybou::StorageAssignmentInterval early{0, 1000, 44200}; early.proof_commitment.fill(1);
    auto late = early; late.start_utc = 44200; late.end_utc = 87400; late.proof_commitment.fill(2);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scopes[0], early, 87400) == cybou::StorageEvidenceAppendResult::ADDED);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scopes[1], late, 87400) == cybou::StorageEvidenceAppendResult::ADDED);
    std::array<cybou::ChunkId, 1> chunks{f.plan.context.chunk};
    const auto quote = cybou::PrepareStorageAssignmentSlotPayouts(*f.db, f.service.definition, scopes, chunks, 5, 0, {}, f.RegistrySnapshots());
    BOOST_REQUIRE(quote); BOOST_REQUIRE_EQUAL(quote->entries.size(), 2U);
    BOOST_CHECK_EQUAL(quote->budget.per_replica, 1U); BOOST_CHECK_EQUAL(quote->verified_unit_seconds, 86400U);
    BOOST_CHECK_EQUAL(quote->payout, 0U);
    for (const auto& entry : quote->entries) {
        BOOST_CHECK_EQUAL(entry.verified_unit_seconds, 43200U); BOOST_CHECK_EQUAL(entry.entitlement, 0U);
    }
    // Merging providers' fractions would incorrectly pay one CYBOU.
    BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(quote->budget, quote->verified_unit_seconds, 0), 1U);
}
BOOST_AUTO_TEST_CASE(slot_quote_multiple_chunks_share_one_budget_and_reject_staged_evidence)
{
    Fixture f;
    auto context = f.plan.context; context.term_end = 1;
    auto initial = cybou::PrepareStorageAssignment(context, f.plan.eligible); BOOST_REQUIRE(initial);
    f.plan = *initial; BOOST_REQUIRE(f.Attest());
    context.chunk = cybou::ComputeChunkId(std::vector<unsigned char>{4, 3, 2, 1});
    const auto other = cybou::PrepareStorageAssignment(context, f.plan.eligible); BOOST_REQUIRE(other);
    BOOST_REQUIRE(cybou::AttestStorageAssignment(*f.db, f.service.definition, *other, f.registry,
        *f.service.runtime->GetFinalizedTip(), f.proofs, f.signer));
    std::array scopes{cybou::StorageAssignmentEvidenceScope{f.plan, 0, 1000, 86400},
        cybou::StorageAssignmentEvidenceScope{*other, 0, 1000, 86400}};
    std::array chunks{f.plan.context.chunk, other->context.chunk};
    const auto quote = [&] { return cybou::PrepareStorageAssignmentSlotPayouts(*f.db,
        f.service.definition, scopes, chunks, 5, 0, {}, f.RegistrySnapshots()); };
    cybou::StorageAssignmentInterval interval{0, 1000, 87400}; interval.proof_commitment.fill(1);
    {
        cybou::PrivateApplicationStore::Batch pending{*f.db};
        BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scopes[0], interval, 87400) ==
            cybou::StorageEvidenceAppendResult::ADDED);
        BOOST_CHECK(!quote());
    } // Rollback: staged service never became durable.
    const auto empty = quote(); BOOST_REQUIRE(empty);
    BOOST_CHECK_EQUAL(empty->verified_unit_seconds, 0U);
    for (const auto& scope : scopes) {
        BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scope, interval, 87400) ==
            cybou::StorageEvidenceAppendResult::ADDED);
    }
    const auto complete = quote(); BOOST_REQUIRE(complete);
    BOOST_REQUIRE_EQUAL(complete->entries.size(), 1U);
    BOOST_CHECK_EQUAL(complete->verified_unit_seconds, 172800U);
    BOOST_CHECK_EQUAL(complete->budget.per_replica, 1U);
    BOOST_CHECK_EQUAL(complete->payout, 1U); // Not one ceil share per chunk.
}
BOOST_AUTO_TEST_CASE(term_quote_requires_all_slots_and_preserves_individual_provider_payments)
{
    Fixture f;
    auto second_identity = f.service.CreateIdentity("second-paid-provider.cybou");
    auto state = f.service.genesis;
    for (std::uint64_t h{1}; h <= *f.service.runtime->GetFinalizedHeight(); ++h) {
        const auto block = f.service.runtime->GetBlockAtHeight(h); BOOST_REQUIRE(block);
        const auto executed = cybou::ExecuteBlockOperations(state, block->block.operations,
            f.service.runtime->GetNetworkBinding(), h, f.service.definition.GetProtocolParameters(),
            &f.service.definition.GetPoaPublicKey());
        BOOST_REQUIRE(executed); state = *executed.state;
    }
    f.registry = state.identities;
    cybou::CybouNodeRuntime foreign{{.network_genesis = f.service.definition,
        .data_dir = f.service.directory / "second-paid-storage", .memory_only = true, .wipe_data = true}};
    BOOST_REQUIRE(foreign.InitializeGenesis(f.service.genesis));
    foreign.SetIdentitySigner(std::make_shared<cybou::CybouKeyStoreIdentitySigner>(second_identity->GetKeyStore()));
    const auto binding = foreign.LocalStoragePayoutBinding(); const auto storage = foreign.LocalStorageId();
    BOOST_REQUIRE(binding); BOOST_REQUIRE(storage);
    auto context = f.plan.context; context.replicas = 2; context.term_end = 1;
    std::copy_n(f.service.runtime->GetFinalizedTip()->begin(), 32, context.finalized_seed.begin());
    auto eligible = f.plan.eligible;
    cybou::StorageAssignmentProvider candidate{.storage_id = *storage};
    std::copy_n(binding->payout_account.Value().begin(), 32, candidate.payout_account.begin());
    eligible.push_back(candidate); f.proofs.push_back({*storage, *binding});
    const auto plan = cybou::PrepareStorageAssignment(context, eligible); BOOST_REQUIRE(plan);
    f.plan = *plan; BOOST_REQUIRE(f.Attest());
    std::array scopes{cybou::StorageAssignmentEvidenceScope{f.plan, 0, 1000, 86400},
        cybou::StorageAssignmentEvidenceScope{f.plan, 1, 1000, 86400}};
    std::array<cybou::ChunkId, 1> chunks{f.plan.context.chunk};
    const auto quote = [&](const std::vector<cybou::StorageAssignmentSlotPaid>& paid = {}) {
        return cybou::PrepareStorageAssignmentTermPayouts(*f.db, f.service.definition,
            scopes, chunks, 5, 0, paid, f.RegistrySnapshots());
    };
    std::array only_first{scopes[0]};
    BOOST_CHECK(!cybou::PrepareStorageAssignmentTermPayouts(*f.db, f.service.definition,
        only_first, chunks, 5, 0, {}, f.RegistrySnapshots()));
    cybou::StorageAssignmentInterval interval{0, 1000, 87400}; interval.proof_commitment.fill(1);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scopes[0], interval, 87400) == cybou::StorageEvidenceAppendResult::ADDED);
    auto partial = quote(); BOOST_REQUIRE(partial); BOOST_REQUIRE_EQUAL(partial->slots.size(), 2U);
    BOOST_CHECK_EQUAL(partial->budget.total, 2U);
    BOOST_CHECK_EQUAL(partial->payout, 1U); BOOST_CHECK_EQUAL(partial->slots[1].payout, 0U);
    BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scopes[1], interval, 87400) == cybou::StorageEvidenceAppendResult::ADDED);
    auto complete = quote(); BOOST_REQUIRE(complete); BOOST_CHECK_EQUAL(complete->payout, 2U);
    std::vector<cybou::StorageAssignmentSlotPaid> paid{{0, {f.plan.selected[0], 1}}};
    auto settled = quote(paid); BOOST_REQUIRE(settled);
    BOOST_CHECK_EQUAL(settled->finalized_paid, 1U); BOOST_CHECK_EQUAL(settled->payout, 1U);
    paid.push_back({1, {f.plan.selected[1], 1}});
    BOOST_CHECK_EQUAL(quote(paid)->payout, 0U);
    BOOST_CHECK_EQUAL(quote()->payout, 2U); // Preparation did not write paid history.
    paid.push_back({2, {f.plan.selected[0], 0}}); BOOST_CHECK(!quote(paid));
    BOOST_CHECK(!quote(std::vector<cybou::StorageAssignmentSlotPaid>(1025)));
    auto mixed = scopes; ++mixed[1].term_start_utc;
    BOOST_CHECK(!cybou::PrepareStorageAssignmentTermPayouts(*f.db, f.service.definition,
        mixed, chunks, 5, 0, {}, f.RegistrySnapshots()));
    {
        cybou::PrivateApplicationStore::Batch pending{*f.db}; BOOST_CHECK(!quote());
    }
    // Different epochs can individually have valid distinct pairs yet reuse the
    // same provider in overlapping slots. Full term preparation must reject it.
    auto separate = context; separate.publication[0] ^= 1;
    const auto first_plan = cybou::PrepareStorageAssignment(separate, eligible); BOOST_REQUIRE(first_plan);
    std::optional<cybou::StorageAssignmentPlan> switched;
    for (unsigned attempt{0}; attempt < 64; ++attempt) {
        ++separate.epoch;
        auto trial = cybou::PrepareStorageAssignment(separate, eligible); BOOST_REQUIRE(trial);
        if (trial->selected[1] == first_plan->selected[0]) { switched = trial; break; }
    }
    BOOST_REQUIRE(switched);
    for (const auto& assignment : {*first_plan, *switched}) {
        BOOST_REQUIRE(cybou::AttestStorageAssignment(*f.db, f.service.definition, assignment, f.registry,
            *f.service.runtime->GetFinalizedTip(), f.proofs, f.signer));
    }
    std::array overlaps{cybou::StorageAssignmentEvidenceScope{*first_plan, 0, 1000, 86400},
        cybou::StorageAssignmentEvidenceScope{*switched, 1, 1000, 86400}};
    for (const auto& scope : overlaps) {
        BOOST_REQUIRE(cybou::AppendStorageAssignmentEvidence(*f.db, scope, interval, 87400) == cybou::StorageEvidenceAppendResult::ADDED);
    }
    BOOST_CHECK(!cybou::PrepareStorageAssignmentTermPayouts(*f.db, f.service.definition,
        overlaps, chunks, 5, 0, {}, f.RegistrySnapshots()));
}
BOOST_AUTO_TEST_SUITE_END()
