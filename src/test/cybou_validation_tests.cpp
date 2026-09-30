// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#include <cybou/authority.h>
#include <cybou/validation_service.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <thread>
#include <set>
#include <cybou/p2p/session.h>
#include <cybou/chunk_authorization.h>
#include <cybou/secret_file.h>
#include <boost/asio.hpp>
namespace {
using namespace cybou;
FinalizedValidationSnapshot Snapshot(CybouNodeRuntime& runtime) {
    std::optional<FinalizedValidationSnapshot> result;
    if (!runtime.ReadFinalizedValidationSnapshot([&](const auto& snapshot) { result = snapshot; })) throw std::runtime_error{"snapshot unavailable"};
    return *result;
}
AuthorizedNodeBinding Binding(CybouIdentityService& owner, const FinalizedValidationSnapshot& snapshot,
    std::span<const unsigned char, 32> secret, bool revoke = false, uint64_t offset = 0, bool storage = false) {
    auto& keys = owner.GetKeyStore(); const auto account = *keys.GetAccountId();
    const auto* identity = snapshot.state.identities.Find(account);
    AuthorizedNodeBinding op;
    op.binding = {*DeriveIdentityPublicKey(secret, IdentityKeyPurpose::VALIDATION_NODE), revoke};
    if (storage) op.binding.provider_key = *DeriveIdentityPublicKey(secret, IdentityKeyPurpose::STORAGE_PROVIDER);
    op.authorization = {account, identity->nonce + offset, identity->key_epoch, IdentityOperationKind::NODE_BINDING,
        *ComputeNodeBindingCommitment(op.binding), {}};
    const auto digest = *ComputeIdentityOperationDigest(snapshot.base.network_id, op.authorization);
    op.authorization.signature = *keys.SignAuthorization(digest);
    op.node_proof = *SignIdentityMessage(secret, IdentityKeyPurpose::VALIDATION_NODE, digest);
    if (storage) op.provider_proof = *SignIdentityMessage(secret, IdentityKeyPurpose::STORAGE_PROVIDER, digest);
    return op;
}
struct BoundFixture : CybouServiceTestFixture {
    std::unique_ptr<CybouIdentityService> owner{CreateIdentity("validation.vault")};
    std::array<unsigned char, 32> secret{};
    IdentityHybridPublicKey key;
    explicit BoundFixture(uint64_t epoch_blocks = DEFAULT_EPOCH_BLOCKS, bool storage = false) : CybouServiceTestFixture(0x72, epoch_blocks) {
        secret[0] = 0x13; key = *DeriveIdentityPublicKey(secret, IdentityKeyPurpose::VALIDATION_NODE);
        auto op = Binding(*owner, Snapshot(*runtime), secret, false, 0, storage);
        if (!runtime->SubmitOperation(op) || !runtime->ProduceBlock()) throw std::runtime_error{"binding failed"};
    }
};
} // namespace
BOOST_AUTO_TEST_SUITE(cybou_validation_tests)
BOOST_AUTO_TEST_CASE(secret_files_are_exclusive_private_bounded_and_not_replaceable) {
    BoundFixture net;const auto path=net.directory/"private.seed";
    BOOST_REQUIRE(CreateSecretFile(path,net.secret));
    BOOST_CHECK(!CreateSecretFile(path,net.secret));
    const auto read=ReadSecretFile(path,32);BOOST_REQUIRE(read);
    BOOST_CHECK_EQUAL_COLLECTIONS(read->begin(),read->end(),net.secret.begin(),net.secret.end());
    BOOST_CHECK(!ReadSecretFile(path,31));
    std::error_code ec;const auto link=net.directory/"linked.seed";
    std::filesystem::create_symlink(path,link,ec);
    if(!ec)BOOST_CHECK(!ReadSecretFile(link,32));
}
BOOST_AUTO_TEST_CASE(owner_can_revoke_stolen_node_without_service_key_proofs) {
    BoundFixture net;
    const auto snapshot = Snapshot(*net.runtime);
    const auto account = *net.owner->GetAccountId();
    const auto* identity = snapshot.state.identities.Find(account);
    AuthorizedNodeBinding revoke;
    revoke.binding.revoke = true;
    revoke.binding.revoke_node_id = *ValidationNodeId(net.key);
    revoke.authorization = {account, identity->nonce, identity->key_epoch, IdentityOperationKind::NODE_BINDING,
        *ComputeNodeBindingCommitment(revoke.binding), {}};
    revoke.authorization.signature = *net.owner->GetKeyStore().SignAuthorization(
        *ComputeIdentityOperationDigest(snapshot.base.network_id, revoke.authorization));
    const auto encoded = SerializeProtocolOperation(revoke);
    BOOST_REQUIRE(encoded);
    BOOST_CHECK_EQUAL(encoded->size(), 2 + IDENTITY_OPERATION_AUTH_SIZE + 34);
    BOOST_CHECK(DeserializeProtocolOperation(*encoded) == std::optional<ProtocolOperation>{revoke});
    const auto result = ExecuteBlockOperations(snapshot.state, {revoke}, snapshot.base.network_id,
        snapshot.base.height + 1, snapshot.parameters, snapshot.base.block_id);
    BOOST_REQUIRE(result);
    BOOST_CHECK(!result.state->bound_nodes.contains(revoke.binding.revoke_node_id));
}
BOOST_AUTO_TEST_CASE(ingress_budget_counts_rejected_work_before_verification_and_bounds_bursts) {
    p2p::IngressBudget budget; const auto now=std::chrono::steady_clock::now();
    for(unsigned i=0;i<8;++i) BOOST_CHECK(budget.Admit("198.51.100.1",p2p::IngressBudget::Work::OPERATION,100,now));
    BOOST_CHECK(!budget.Admit("198.51.100.1",p2p::IngressBudget::Work::OPERATION,100,now));
    BOOST_CHECK(budget.Admit("198.51.100.2",p2p::IngressBudget::Work::OPERATION,100,now));
    BOOST_CHECK(budget.Admit("198.51.100.1",p2p::IngressBudget::Work::OPERATION,100,now+std::chrono::seconds{1}));
    BOOST_CHECK(!budget.Admit("198.51.100.3",p2p::IngressBudget::Work::OPERATION,(1U<<20)+1,now));
    for(unsigned i=0;i<4;++i) BOOST_CHECK(budget.Admit("198.51.100.1",p2p::IngressBudget::Work::CONNECTION,0,now));
    BOOST_CHECK(!budget.Admit("198.51.100.1",p2p::IngressBudget::Work::CONNECTION,0,now));
    BOOST_CHECK(budget.Admit("198.51.100.1",p2p::IngressBudget::Work::CONNECTION,0,now+std::chrono::seconds{60}));
}
BOOST_AUTO_TEST_CASE(possession_proofs_match_native_blake3_for_unbalanced_trees_and_last_byte_lengths) {
    for (size_t size : {size_t{1089}, size_t{2048}, size_t{2049}, size_t{4096}, size_t{5121}, size_t{65536}, ENCRYPTED_CHUNK_MAX_STORED_BYTES}) {
        std::vector<unsigned char> bytes(size);
        for (size_t i{0}; i < size; ++i) bytes[i] = static_cast<unsigned char>(i * 37 + i / 17);
        const auto id = ComputeChunkId(bytes);
        const auto leaves = (size + 1023) / 1024;
        for (uint32_t leaf : {0U, static_cast<uint32_t>(leaves / 2), static_cast<uint32_t>(leaves - 1)}) {
            const auto proof = BuildChunkPossessionProof(bytes, leaf); BOOST_REQUIRE(proof);
            BOOST_CHECK(VerifyChunkPossessionProof(id, *proof));
            const auto encoded = SerializeChunkPossessionProof(*proof); BOOST_REQUIRE(encoded);
            BOOST_CHECK(DeserializeChunkPossessionProof(*encoded) == proof);
            auto wrong = *proof; wrong.leaf_bytes[0] ^= 1; BOOST_CHECK(!VerifyChunkPossessionProof(id, wrong));
            if (leaf == leaves - 1) { wrong = *proof; ++wrong.stored_bytes; BOOST_CHECK(!VerifyChunkPossessionProof(id, wrong)); }
            if (!proof->siblings.empty()) { wrong = *proof; wrong.siblings[0][0] ^= 1; BOOST_CHECK(!VerifyChunkPossessionProof(id, wrong)); }
        }
    }
}
BOOST_AUTO_TEST_CASE(possession_proofs_match_native_blake3_at_block_boundaries_and_random_inputs) {
    std::set<size_t> sizes;
    for (size_t boundary = 1024; boundary <= 8192; boundary += 1024) {
        for (const auto delta : {-65, -64, -1, 0, 1, 63, 64, 65}) {
            const auto size = static_cast<int64_t>(boundary) + delta;
            if (size >= static_cast<int64_t>(ENCRYPTED_CHUNK_MIN_STORED_BYTES)) sizes.insert(static_cast<size_t>(size));
        }
    }
    uint32_t random = 0x6d2b79f5;
    const auto next = [&] { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    for (unsigned sample = 0; sample < 64; ++sample) sizes.insert(1089 + next() % 30000);
    for (const auto size : sizes) {
        std::vector<unsigned char> bytes(size);
        for (auto& byte : bytes) byte = static_cast<unsigned char>(next());
        const auto expected = ComputeChunkId(bytes); // vetted native BLAKE3 implementation
        const auto leaves = (size + 1023) / 1024;
        const bool boundary_case = size <= 8192;
        const auto check_leaf = [&](uint32_t leaf) {
            const auto proof = BuildChunkPossessionProof(bytes, leaf);
            BOOST_REQUIRE(proof);
            BOOST_CHECK(VerifyChunkPossessionProof(expected, *proof));
            auto encoded = SerializeChunkPossessionProof(*proof);
            BOOST_REQUIRE(encoded);
            BOOST_CHECK(DeserializeChunkPossessionProof(*encoded) == proof);
            encoded->pop_back();
            BOOST_CHECK(!DeserializeChunkPossessionProof(*encoded));
            auto extra = *proof;
            extra.siblings.push_back({});
            BOOST_CHECK(!VerifyChunkPossessionProof(expected, extra));
        };
        if (boundary_case) {
            for (uint32_t leaf = 0; leaf < leaves; ++leaf) check_leaf(leaf);
        } else {
            check_leaf(0);
            check_leaf(static_cast<uint32_t>(next() % leaves));
            check_leaf(static_cast<uint32_t>(leaves - 1));
        }
    }
}
BOOST_AUTO_TEST_CASE(canonical_service_evidence_earns_byte_epoch_work_and_never_rewards_timeouts) {
    BoundFixture net{32, true}; const auto account = *net.owner->GetAccountId();
    std::vector<unsigned char> bytes(5001, 0x42); const auto chunk = ComputeChunkId(bytes);
    RootPublication publication{chunk, ChunkAuthorizationLeafHash(chunk), 1, {RootRecipientCapsule{}}};
    AuthorizedRootPublication op;
    op.publication = publication;
    auto snapshot = Snapshot(*net.runtime); const auto* identity = snapshot.state.identities.Find(account);
    op.authorization = {account, identity->nonce, identity->key_epoch, IdentityOperationKind::ROOT_PUBLICATION,
        *ComputeRootPublicationPayloadCommitment(publication), {}};
    op.authorization.signature = *net.owner->GetKeyStore().SignAuthorization(*ComputeIdentityOperationDigest(snapshot.base.network_id, op.authorization));
    BOOST_REQUIRE(net.runtime->SubmitOperation(op)); BOOST_REQUIRE(net.runtime->ProduceBlock());
    const auto publication_id = *ComputeOperationId(op);
    const auto send = [&](ServiceEvidenceKind kind, bool corrupt = false) {
        const auto s = Snapshot(*net.runtime);
        ServiceEvidence evidence;
        evidence.account_id = account; evidence.node_id = *ValidationNodeId(net.key);
        evidence.base_height = s.base.height; evidence.base_block_id = s.base.block_id; evidence.kind = kind;
        evidence.publication_id = publication_id; evidence.chunk_id = chunk;
        const auto leaf = kind == ServiceEvidenceKind::STORAGE_COMMIT ? static_cast<uint32_t>((bytes.size() + 1023) / 1024 - 1)
            : StorageChallengeLeaf(s.base.network_id, s.base.block_id, account, chunk, bytes.size());
        evidence.possession = BuildChunkPossessionProof(bytes, leaf);
        if (corrupt) evidence.possession->leaf_bytes[0] ^= 1;
        evidence.signature = *SignIdentityMessage(net.secret, IdentityKeyPurpose::VALIDATION_NODE, *ServiceEvidenceDigest(s.base.network_id, evidence));
        const auto encoded = SerializeServiceEvidence(evidence); BOOST_REQUIRE(encoded);
        BOOST_CHECK(DeserializeServiceEvidence(*encoded) == evidence);
        BOOST_REQUIRE(net.runtime->SubmitOperation(evidence)); BOOST_REQUIRE(net.runtime->ProduceBlock());
    };
    send(ServiceEvidenceKind::STORAGE_COMMIT);
    while (net.runtime->GetFinalizedHeight().value_or(0) < 32) BOOST_REQUIRE(net.runtime->ProduceBlock());
    for (uint64_t height{32}; height < 64; height += 2) {
        while (net.runtime->GetFinalizedHeight().value_or(0) < height) BOOST_REQUIRE(net.runtime->ProduceBlock());
        send(ServiceEvidenceKind::STORAGE_RESPONSE);
    }
    BOOST_REQUIRE(net.runtime->ProduceBlock()); // completed epoch 1
    auto owner = net.runtime->GetAccountState(account); BOOST_REQUIRE(owner);
    BOOST_CHECK_EQUAL(owner->authority.liveness, 0U); // settled after the last possible epoch-1 response
    BOOST_CHECK_EQUAL(owner->authority.storage_remainder, bytes.size());
    send(ServiceEvidenceKind::STORAGE_RESPONSE, true);
    owner = net.runtime->GetAccountState(account); BOOST_REQUIRE(owner);
    BOOST_CHECK_EQUAL(owner->authority.liveness, 1U);
    BOOST_CHECK_EQUAL(owner->authority.penalty_debt, FALSE_STORAGE_CLAIM_PENALTY);
    while (net.runtime->GetFinalizedHeight().value_or(0) < 128) BOOST_REQUIRE(net.runtime->ProduceBlock());
    owner = net.runtime->GetAccountState(account); BOOST_REQUIRE(owner);
    BOOST_CHECK_EQUAL(owner->authority.liveness, 1U);
    BOOST_CHECK_EQUAL(owner->authority.storage_remainder, bytes.size());
    BOOST_CHECK_EQUAL(owner->authority.penalty_debt, FALSE_STORAGE_CLAIM_PENALTY);
    const auto state = *net.runtime->GetStore().LoadState().state;
    const auto encoded = SerializeCybouState(state); BOOST_REQUIRE(encoded);
    const auto decoded = DeserializeCybouState(*encoded); BOOST_REQUIRE(decoded);
    BOOST_CHECK(decoded->storage_pledges == state.storage_pledges);
}
BOOST_AUTO_TEST_CASE(real_tcp_validation_never_submits_to_poa) {
    BoundFixture net; net.runtime->ConfigureValidation(net.secret);
    const auto snapshot = Snapshot(*net.runtime); std::array<unsigned char, 32> other{}; other[0] = 0x53;
    const auto operation = Binding(*net.owner, snapshot, other);
    boost::asio::io_context io; using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, {boost::asio::ip::address_v4::loopback(), 0}};
    std::jthread server{[&] {
        tcp::socket socket{io}; acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        if (session.Handshake({.network_id = snapshot.base.network_id, .finalized_height = snapshot.base.height,
            .finalized_tip = snapshot.base.block_id, .capabilities = cybou::p2p::CAP_VALIDATION, .nonce = 70001})) session.ServeNext(*net.runtime);
    }};
    tcp::socket socket{io}; socket.connect(acceptor.local_endpoint()); cybou::p2p::PeerSession session{std::move(socket)};
    BOOST_REQUIRE(session.Handshake({.network_id = snapshot.base.network_id, .finalized_height = snapshot.base.height,
        .finalized_tip = snapshot.base.block_id, .nonce = 70002}));
    const auto response = session.RequestValidation(operation); BOOST_REQUIRE(response); BOOST_REQUIRE(response->attestation);
    BOOST_CHECK(VerifyValidationAttestation(*response->attestation, operation, snapshot, net.key));
    BOOST_CHECK(net.runtime->GetOperationStatus(*ComputeOperationId(operation)).kind == OperationStatusKind::UNKNOWN);
}
BOOST_AUTO_TEST_CASE(canonical_resources_aggregate_reservations_and_never_refund_epoch_bandwidth) {
    BoundFixture net; auto snapshot=Snapshot(*net.runtime); const auto account=*net.owner->GetAccountId();
    auto& owner=snapshot.state.accounts.at(account); owner.bandwidth_reserved=0;
    auto params=snapshot.parameters; params.authority.storage_base=100; params.authority.storage_per_tier=0;
    params.authority.bandwidth_base=100; params.authority.bandwidth_per_tier=0;
    const auto* identity=snapshot.state.identities.Find(account);
    const auto reserve=[&](uint64_t nonce,uint64_t bytes,ResourceDomain domain,unsigned tag) {
        uint256 use; use.begin()[0]=tag;
        AuthorizedResourceReservation op{{account,nonce,identity->key_epoch,IdentityOperationKind::RESOURCE_RESERVATION,{},{}},{{{domain,bytes,use}}}};
        op.authorization.payload_commitment=*ResourceReservationCommitment(op.reservation);
        op.authorization.signature=*net.owner->GetKeyStore().SignAuthorization(*ComputeIdentityOperationDigest(snapshot.base.network_id,op.authorization));
        return op;
    };
    const auto first=reserve(identity->nonce,60,ResourceDomain::STORAGE,1);
    const auto second=reserve(identity->nonce+1,41,ResourceDomain::STORAGE,2);
    auto result=ExecuteBlockOperations(snapshot.state,{first,second},snapshot.base.network_id,snapshot.base.height+1,params,snapshot.base.block_id);
    BOOST_CHECK(!result); BOOST_CHECK_EQUAL(snapshot.state.resources.size(),0U);
    const auto bandwidth=reserve(identity->nonce+1,80,ResourceDomain::BANDWIDTH,3);
    result=ExecuteBlockOperations(snapshot.state,{first,bandwidth},snapshot.base.network_id,snapshot.base.height+1,params,snapshot.base.block_id); BOOST_REQUIRE(result);
    BOOST_CHECK_EQUAL(result.state->resources.size(),2U); BOOST_CHECK_EQUAL(result.state->accounts.at(account).bandwidth_reserved,80U);
    const auto encoded=SerializeCybouState(*result.state); BOOST_REQUIRE(encoded);
    const auto restored=DeserializeCybouState(*encoded); BOOST_REQUIRE(restored); BOOST_CHECK(restored->resources==result.state->resources);
    AuthorizedResourceRelease release{{account,identity->nonce+2,identity->key_epoch,IdentityOperationKind::RESOURCE_RELEASE,{},{}},{{ResourceGrantId(*ComputeOperationId(bandwidth),0)}}};
    release.authorization.payload_commitment=*ResourceReleaseCommitment(release.release);
    release.authorization.signature=*net.owner->GetKeyStore().SignAuthorization(*ComputeIdentityOperationDigest(snapshot.base.network_id,release.authorization));
    auto released=ExecuteBlockOperations(*result.state,{release},snapshot.base.network_id,snapshot.base.height+2,params,snapshot.base.block_id); BOOST_REQUIRE(released);
    BOOST_CHECK_EQUAL(released.state->accounts.at(account).bandwidth_reserved,80U);
    const auto overflow=reserve(identity->nonce+3,21,ResourceDomain::BANDWIDTH,4);
    BOOST_CHECK(!ExecuteBlockOperations(*released.state,{overflow},snapshot.base.network_id,snapshot.base.height+3,params,snapshot.base.block_id));
    const auto next_epoch=ExecuteBlockOperations(*released.state,{},snapshot.base.network_id,params.epoch_blocks,params,snapshot.base.block_id); BOOST_REQUIRE(next_epoch);
    BOOST_CHECK_EQUAL(next_epoch.state->accounts.at(account).bandwidth_reserved,0U);
    BOOST_CHECK_EQUAL(next_epoch.state->resources.size(),1U);
}
BOOST_AUTO_TEST_CASE(finalized_resource_ticket_is_provider_bound_exact_and_durably_one_use) {
    BoundFixture net; const auto account=*net.owner->GetAccountId();
    NodeRuntimeConfig provider_config{.network_definition=net.definition,.data_dir=net.directory/"ticket-provider",
        .storage_enabled=true,.storage_capacity_bytes=1ULL<<20};
    auto provider=std::make_unique<CybouNodeRuntime>(std::move(provider_config));
    BOOST_REQUIRE(provider->InitializeGenesis(net.genesis));const auto provider_id=provider->LocalProviderId();BOOST_REQUIRE(provider_id);
    std::vector<unsigned char> bytes(5001,0x42);const auto chunk=ComputeChunkId(bytes);
    AuthorizedRootPublication publication;
    publication.publication={chunk,ChunkAuthorizationLeafHash(chunk),1,{RootRecipientCapsule{}}};
    auto snapshot=Snapshot(*net.runtime);const auto* identity=snapshot.state.identities.Find(account);
    publication.authorization={account,identity->nonce,identity->key_epoch,IdentityOperationKind::ROOT_PUBLICATION,
        *ComputeRootPublicationPayloadCommitment(publication.publication),{}};
    publication.authorization.signature=*net.owner->GetKeyStore().SignAuthorization(*ComputeIdentityOperationDigest(snapshot.base.network_id,publication.authorization));
    BOOST_REQUIRE(net.runtime->SubmitOperation(publication));BOOST_REQUIRE(net.runtime->ProduceBlock());
    snapshot=Snapshot(*net.runtime);identity=snapshot.state.identities.Find(account);
    ResourceTicket ticket;ticket.publication_id=*ComputeOperationId(publication);ticket.request_nonce=uint256::ONE;
    AuthorizedResourceReservation reservation;
    reservation.reservation.grants={
        {ResourceDomain::STORAGE,bytes.size(),ResourceUseCommitment(snapshot.base.network_id,account,*provider_id,ResourceUse::STORE,ticket.publication_id,chunk,bytes.size())},
        {ResourceDomain::BANDWIDTH,bytes.size(),ResourceUseCommitment(snapshot.base.network_id,account,*provider_id,ResourceUse::PUT,ticket.publication_id,chunk,bytes.size(),ticket.request_nonce)}};
    reservation.authorization={account,identity->nonce,identity->key_epoch,IdentityOperationKind::RESOURCE_RESERVATION,*ResourceReservationCommitment(reservation.reservation),{}};
    reservation.authorization.signature=*net.owner->GetKeyStore().SignAuthorization(*ComputeIdentityOperationDigest(snapshot.base.network_id,reservation.authorization));
    BOOST_REQUIRE(net.runtime->SubmitOperation(reservation));BOOST_REQUIRE(net.runtime->ProduceBlock());
    const auto id=*ComputeOperationId(reservation);ticket.storage_grant=ResourceGrantId(id,0);ticket.bandwidth_grant=ResourceGrantId(id,1);
    BOOST_CHECK(DeserializeResourceTicket(SerializeResourceTicket(ticket))==std::optional{ticket});
    const auto sync=[&](CybouNodeRuntime& receiver) {
        for(uint64_t h=receiver.GetFinalizedHeight().value_or(0)+1;h<=net.runtime->GetFinalizedHeight().value_or(0);++h)
            BOOST_REQUIRE(receiver.CommitBlock(*net.runtime->GetBlockAtHeight(h)));
    };
    sync(*provider);
    BOOST_CHECK(!provider->ConsumeResourceTicket(ticket,chunk,bytes.size()+1,ResourceUse::PUT));
    BOOST_CHECK(!provider->ConsumeResourceTicket(ticket,chunk,bytes.size(),ResourceUse::GET));
    auto tampered=ticket;tampered.request_nonce.begin()[0]^=2;
    BOOST_CHECK(!provider->ConsumeResourceTicket(tampered,chunk,bytes.size(),ResourceUse::PUT));
    BOOST_CHECK(provider->ConsumeResourceTicket(ticket,chunk,bytes.size(),ResourceUse::PUT));
    BOOST_CHECK(!provider->ConsumeResourceTicket(ticket,chunk,bytes.size(),ResourceUse::PUT));
    provider.reset();
    NodeRuntimeConfig restarted{.network_definition=net.definition,.data_dir=net.directory/"ticket-provider",
        .storage_enabled=true,.storage_capacity_bytes=1ULL<<20};
    provider=std::make_unique<CybouNodeRuntime>(std::move(restarted));BOOST_REQUIRE(provider->InitializeGenesis(net.genesis));
    BOOST_CHECK(!provider->ConsumeResourceTicket(ticket,chunk,bytes.size(),ResourceUse::PUT));
    NodeRuntimeConfig foreign{.network_definition=net.definition,.data_dir=net.directory/"foreign-provider",
        .storage_enabled=true,.storage_capacity_bytes=1ULL<<20};
    CybouNodeRuntime other{std::move(foreign)};BOOST_REQUIRE(other.InitializeGenesis(net.genesis));sync(other);
    BOOST_CHECK(!other.ConsumeResourceTicket(ticket,chunk,bytes.size(),ResourceUse::PUT));
}
BOOST_AUTO_TEST_CASE(validation_contribution_requires_exact_coinclusion_and_false_claim_retains_debt) {
    BoundFixture net;
    auto other = net.CreateIdentity("other-validation.vault");
    auto snapshot = Snapshot(*net.runtime);
    std::array<unsigned char, 32> second{}; second[0] = 0x63;
    const auto subject = Binding(*other, snapshot, second);
    const auto receipt_for = [&](const ProtocolOperation& target) {
        ServiceEvidence receipt;
        receipt.account_id = *net.owner->GetAccountId(); receipt.node_id = *ValidationNodeId(net.key);
        receipt.base_height = snapshot.base.height; receipt.base_block_id = snapshot.base.block_id;
        receipt.base_state_root = snapshot.base.state_root; receipt.kind = ServiceEvidenceKind::VALIDATION_RECEIPT;
        receipt.publication_id = *ComputeOperationId(target); receipt.validated_operation = *SerializeProtocolOperation(target);
        receipt.signature = *SignIdentityMessage(net.secret, IdentityKeyPurpose::VALIDATION_NODE,
            *ServiceEvidenceDigest(snapshot.base.network_id, receipt));
        return receipt;
    };
    const auto receipt = receipt_for(subject);
    BOOST_CHECK(DeserializeProtocolOperation(*SerializeProtocolOperation(receipt)) == std::optional<ProtocolOperation>{receipt});
    const auto execute = [&](std::vector<ProtocolOperation> ops) {
        return ExecuteBlockOperations(snapshot.state, ops, snapshot.base.network_id, snapshot.base.height + 1,
            snapshot.parameters, snapshot.base.block_id);
    };
    const auto included = execute({subject, receipt}); BOOST_REQUIRE(included);
    const auto account = *net.owner->GetAccountId();
    BOOST_CHECK_EQUAL(included.state->accounts.at(account).authority.validation, 1U);
    const auto absent = execute({receipt}); BOOST_REQUIRE(absent);
    BOOST_CHECK_EQUAL(absent.state->accounts.at(account).authority.validation, 0U);
    const auto duplicate = execute({subject, receipt, receipt}); BOOST_CHECK(!duplicate);
    const auto self = Binding(*net.owner, snapshot, second);
    const auto self_result = execute({self, receipt_for(self)}); BOOST_REQUIRE(self_result);
    BOOST_CHECK_EQUAL(self_result.state->accounts.at(account).authority.validation, 0U);
    auto invalid = subject; ++invalid.authorization.nonce;
    const auto invalid_result = execute({receipt_for(invalid)}); BOOST_REQUIRE(invalid_result);
    BOOST_CHECK_EQUAL(invalid_result.state->accounts.at(account).authority.penalty_debt, FALSE_STORAGE_CLAIM_PENALTY);
    const auto recursive = receipt_for(receipt); BOOST_CHECK(!execute({recursive}));
}
BOOST_AUTO_TEST_CASE(bound_service_checks_exact_finalized_nonce_and_conflicts_survive_restart) {
    BoundFixture net; auto snapshot = Snapshot(*net.runtime);
    std::array<unsigned char, 32> other{}; other[0] = 0x21;
    auto op = Binding(*net.owner, snapshot, other);
    auto speculative = Binding(*net.owner, snapshot, other, false, 1);
    BOOST_CHECK(ValidateAgainstFinalizedBase(op, snapshot) == ValidationCheck::VALID);
    BOOST_CHECK(ValidateAgainstFinalizedBase(speculative, snapshot) == ValidationCheck::INVALID);
    KVStore journal{{.path = net.directory / "validation-journal"}};
    ValidationState state{journal, snapshot.base.network_id, *ValidationNodeId(net.key)};
    ValidationService service{state, net.key,
        [&](auto digest) { return SignIdentityMessage(net.secret, IdentityKeyPurpose::VALIDATION_NODE, digest); },
        [&](const auto& callback) { return net.runtime->ReadFinalizedValidationSnapshot(callback); }};
    const auto result = service.CheckReserveAndAttest(op);
    BOOST_REQUIRE(result.attestation);
    BOOST_CHECK(VerifyValidationAttestation(*result.attestation, op, snapshot, net.key));
    const auto encoded = SerializeValidationAttestation(*result.attestation);
    BOOST_REQUIRE(encoded);
    BOOST_CHECK(DeserializeValidationAttestation(*encoded) == result.attestation);
    auto padded = *encoded; padded.push_back(0); BOOST_CHECK(!DeserializeValidationAttestation(padded));
    BOOST_CHECK(service.CheckReserveAndAttest(op).attestation.has_value());
    other[0] = 0x22; const auto conflict = Binding(*net.owner, snapshot, other);
    BOOST_CHECK(service.CheckReserveAndAttest(conflict).check == ValidationCheck::CONFLICT);
    ValidationState restarted{journal, snapshot.base.network_id, *ValidationNodeId(net.key)};
    BOOST_CHECK(restarted.Reserve(snapshot.base, conflict.authorization.account_id, conflict.authorization.nonce,
        *ComputeOperationId(conflict)) == ValidationCheck::CONFLICT);
    auto forged = *result.attestation; forged.signature.ml_dsa[0] ^= 1;
    BOOST_CHECK(!VerifyValidationAttestation(forged, op, snapshot, net.key));
    forged = *result.attestation; forged.base.network_id = uint256::ONE;
    BOOST_CHECK(!VerifyValidationAttestation(forged, op, snapshot, net.key));
    BOOST_REQUIRE(net.runtime->ProduceBlock());
    BOOST_CHECK(!VerifyValidationAttestation(*result.attestation, op, Snapshot(*net.runtime), net.key));
    // Checking and signing never admitted the operation to the finalizer.
    BOOST_CHECK(!net.runtime->FindFinalizedOperation(*ComputeOperationId(op)).height);
    journal.Write(std::string{"validation-state"}, std::vector<unsigned char>{1, 2, 3}, true);
    ValidationState corrupt{journal, snapshot.base.network_id, *ValidationNodeId(net.key)};
    BOOST_CHECK(corrupt.Reserve(snapshot.base, op.authorization.account_id, op.authorization.nonce,
        *ComputeOperationId(op)) == ValidationCheck::UNAVAILABLE);
}
BOOST_AUTO_TEST_CASE(local_policy_is_off_by_default_and_counts_accounts_not_nodes) {
    BoundFixture net; const auto snapshot = Snapshot(*net.runtime);
    std::array<unsigned char, 32> second{}; second[0] = 0x31;
    const auto op = Binding(*net.owner, snapshot, second);
    KVStore journal{{.memory_only = true}};
    ValidationState state{journal, snapshot.base.network_id, *ValidationNodeId(net.key)};
    ValidationService service{state, net.key,
        [&](auto digest) { return SignIdentityMessage(net.secret, IdentityKeyPurpose::VALIDATION_NODE, digest); },
        [&](const auto& callback) { return net.runtime->ReadFinalizedValidationSnapshot(callback); }};
    const auto result = service.CheckReserveAndAttest(op); BOOST_REQUIRE(result.attestation);
    const std::array values{*result.attestation, *result.attestation};
    ValidationTrustPolicy policy;
    policy.nodes.emplace(result.attestation->node_id, ValidationTrustPolicy::TrustedNode{*net.owner->GetAccountId(), net.key});
    BOOST_CHECK(!HasLocalValidation(op, snapshot, values, policy));
    policy.required_accounts = 1; BOOST_CHECK(HasLocalValidation(op, snapshot, values, policy));
    policy.required_accounts = 2; BOOST_CHECK(!HasLocalValidation(op, snapshot, values, policy));
    policy.required_accounts = 1; policy.nodes.begin()->second.account = AccountId{uint256::ONE};
    BOOST_CHECK(!HasLocalValidation(op, snapshot, values, policy));
}
BOOST_AUTO_TEST_CASE(concurrent_conflicts_never_both_reserve_and_base_rollback_is_rejected) {
    KVStore db{{.memory_only = true}};
    ValidationBase base{uint256::ONE, 10, uint256::ONE, uint256::ONE};
    ValidationState state{db, uint256::ONE, uint256::ONE};
    const AccountId account{uint256::ONE};
    ValidationCheck first, second; uint256 other; other.begin()[0] = 2;
    std::thread a{[&] { first = state.Reserve(base, account, 1, uint256::ONE); }};
    std::thread b{[&] { second = state.Reserve(base, account, 1, other); }};
    a.join(); b.join();
    BOOST_CHECK((first == ValidationCheck::VALID && second == ValidationCheck::CONFLICT) ||
        (second == ValidationCheck::VALID && first == ValidationCheck::CONFLICT));
    auto rollback = base; --rollback.height;
    BOOST_CHECK(state.Reserve(rollback, account, 1, other) == ValidationCheck::BASE_CHANGED);
}
BOOST_AUTO_TEST_CASE(protocol_budget_is_canonical_and_cannot_be_raised_inside_its_block) {
    BoundFixture net; auto snapshot = Snapshot(*net.runtime); const auto account = *net.owner->GetAccountId();
    auto& original = snapshot.state.accounts.at(account);
    snapshot.state.onboarding_pool -= 1000; original.balance += 1000;
    auto params = snapshot.parameters; params.authority.protocol_base = 2; params.authority.protocol_per_tier = 0;
    original.authority.protocol_used = 0;
    std::vector<ProtocolOperation> operations;
    const auto* record = snapshot.state.identities.Find(account);
    for (uint64_t i{0}; i < 3; ++i) {
        AuthorizedSystemLock lock;
        lock.lock.amount = 1;
        lock.authorization = {account, record->nonce + i, record->key_epoch, IdentityOperationKind::SYSTEM_LOCK,
            *ComputeSystemLockPayloadCommitment(lock.lock), {}};
        lock.authorization.signature = *net.owner->GetKeyStore().SignAuthorization(*ComputeIdentityOperationDigest(snapshot.base.network_id, lock.authorization));
        operations.emplace_back(lock);
    }
    const auto fail = ExecuteBlockOperations(snapshot.state, operations, snapshot.base.network_id, snapshot.base.height + 1, params);
    BOOST_CHECK(fail.error == BlockExecutionError::AUTHORITY_BUDGET_EXHAUSTED);
    BOOST_CHECK_EQUAL(snapshot.state.accounts.at(account).authority.protocol_used, 0U);
    operations.pop_back();
    const auto pass = ExecuteBlockOperations(snapshot.state, operations, snapshot.base.network_id, snapshot.base.height + 1, params);
    BOOST_REQUIRE(pass);
    BOOST_CHECK_EQUAL(pass.state->accounts.at(account).authority.system_contribution, 2U);
    BOOST_CHECK_EQUAL(pass.state->accounts.at(account).authority.protocol_used, 2U);
}
BOOST_AUTO_TEST_SUITE_END()
