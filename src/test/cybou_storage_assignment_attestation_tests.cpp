// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_attestation.h>
#include <cybou/storage_assignment_observer.h>
#include <cybou/storage_assignment_observation_store.h>
#include <cybou/storage_assignment_payout.h>
#include <cybou/block_executor.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <functional>
#include <future>

namespace {
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
BOOST_AUTO_TEST_SUITE_END()
