// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_service.h>

#include <cybou/node_runtime.h>
#include <cybou/p2p/inbound_server.h>
#include <cybou/p2p/session.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/publication_service.h>
#include <test/cybou_publication_builder.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_storage_test_network.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <map>
#include <memory>
#include <set>
#include <thread>

namespace {

struct PublishedContent {
    cybou::Hash256 operation_id;
    std::vector<cybou::ChunkId> leaves;
};

/** Stages a multi-chunk tree, publishes it with a self capsule and optionally finalizes it. */
PublishedContent Publish(CybouServiceTestFixture& fixture, cybou::CybouIdentityService& identity,
    cybou::PrivateApplicationStore& application_db, bool finalize)
{
    cybou::KVStore proof_db{cybou::KVStoreOptions{.memory_only = true}};
    cybou::test::PublicationBuilder stager{fixture.runtime->GetChunkBlobStore(),
        std::span<const unsigned char, 32>{fixture.runtime->GetNetworkBinding().begin(), 32}};
    std::size_t remaining{600 * 1024};
    const auto source = [&remaining](std::span<unsigned char> out) -> std::optional<std::size_t> {
        const auto n = std::min(out.size(), remaining);
        for (std::size_t i{0}; i < n; ++i) out[i] = static_cast<unsigned char>((remaining - i) * 31);
        remaining -= n;
        return n;
    };
    const std::vector<unsigned char> metadata{2, 3, 0, 0};
    const auto main = stager.StageTree(source, metadata);
    BOOST_REQUIRE(main);
    const auto prepared = stager.Finish(*main);
    BOOST_REQUIRE(prepared);
    PublishedContent content;
    for (std::uint32_t i{0}; i < prepared->chunk_count; ++i) {
        const auto leaf = stager.GetLeafId(i);
        BOOST_REQUIRE(leaf);
        content.leaves.push_back(*leaf);
    }
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity.GetKeyStore());
    cybou::PublicationService publication{*fixture.runtime, identity.GetKeyStore(), application_db, coordinator};
    const auto submitted = publication.SubmitPrepared("storage-job", *prepared);
    BOOST_REQUIRE(!submitted.operation_id.IsNull());
    content.operation_id = submitted.operation_id;
    if (finalize) BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    return content;
}

int ReplicaCount(const ProviderNetwork& network, const cybou::ChunkId& id)
{
    int count{0};
    for (const auto& endpoint : network.Endpoints()) count += network.Holds(endpoint, id) ? 1 : 0;
    return count;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_storage_service_tests)

BOOST_AUTO_TEST_CASE(zero_quota_rejects_storage_but_preserves_ping_and_block_sync)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("quota-owner.cybou");
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "quota-application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    std::vector<cybou::AuthorizedChunk> leaves;
    for (const auto& id : content.leaves) leaves.push_back({id});
    const auto commitment = cybou::BuildChunkAuthorizationTree(leaves);
    BOOST_REQUIRE(commitment);
    const auto publication = fixture.runtime->FindFinalizedRootPublication(content.operation_id);
    BOOST_REQUIRE(publication);
    BOOST_REQUIRE(cybou::VerifyChunkAuthorizationProof(*publication, content.leaves.front(), commitment->Proof(0)));
    cybou::CybouNodeRuntime node{{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "quota-node", .memory_only = true,
        .wipe_data = true, .storage_capacity_bytes = 0,
        .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0}};
    BOOST_REQUIRE(node.InitializeGenesis(fixture.genesis));
    for (uint64_t h = 1; h <= fixture.runtime->GetFinalizedHeight().value(); ++h)
        BOOST_REQUIRE(node.CommitBlock(*fixture.runtime->GetBlockAtHeight(h)));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    cybou::p2p::InboundPeerServer server{node, io, tcp::endpoint{loopback, 0}};
    std::atomic_bool stopping{false};
    std::jthread listener{[&] { server.Run(stopping); }};
    struct StopListener {
        std::atomic_bool& stopping;
        std::jthread& listener;
        ~StopListener() { stopping = true; if (listener.joinable()) listener.join(); }
    } stop_listener{stopping, listener};
    tcp::socket socket{io};
    socket.connect({loopback, server.Port()});
    cybou::p2p::PeerSession client{std::move(socket), cybou::p2p::TransportRole::CLIENT};
    BOOST_REQUIRE(client.Handshake({.network_binding = fixture.runtime->GetNetworkBinding(),
        .finalized_height = fixture.runtime->GetFinalizedHeight().value(),
        .finalized_tip = fixture.runtime->GetFinalizedTip().value(), .nonce = 55001}));
    BOOST_CHECK(!client.PeerStorageId());
    BOOST_REQUIRE(client.ProveStorageIdentity());
    const auto bytes = fixture.runtime->GetChunkBlobStore().Get(content.leaves.front());
    BOOST_REQUIRE(bytes);
    const auto admission = client.PutAuthorizedChunk(content.operation_id, content.leaves.front(), *bytes, commitment->Proof(0));
    BOOST_REQUIRE(admission);
    BOOST_CHECK(admission->status == cybou::ChunkAdmissionStatus::CAPACITY_EXCEEDED);
    BOOST_CHECK(client.Ping(55002));
    const auto block = client.RequestBlock(1);
    BOOST_CHECK(block.status == cybou::p2p::BlockRequestStatus::OK);
    BOOST_CHECK_EQUAL(node.GetDiagnostics().storage_used, 0U);
}

BOOST_AUTO_TEST_CASE(nothing_leaves_the_node_before_finality)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture};
    network.Sync();
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, /*finalize=*/false);
    BOOST_REQUIRE_GT(content.leaves.size(), 1U);
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    const auto result = storage.Secure(content.operation_id, content.leaves);
    BOOST_CHECK(result.state == cybou::DurabilityState::SECURING);
    BOOST_CHECK_EQUAL(network.puts, 0);
    BOOST_CHECK(!storage.GetDurability(content.operation_id));
}

BOOST_AUTO_TEST_CASE(beta_target_places_two_distinct_remote_replicas)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};

    // A wrong chunk list is refused outright.
    auto wrong = content.leaves;
    std::swap(wrong.front(), wrong.back());
    BOOST_CHECK(storage.Secure(content.operation_id, wrong).state == cybou::DurabilityState::NEEDS_ATTENTION);
    BOOST_CHECK_EQUAL(network.puts, 0);

    const auto result = storage.Secure(content.operation_id, content.leaves);
    BOOST_CHECK(result.state == cybou::DurabilityState::PROTECTED);
    BOOST_CHECK_EQUAL(result.chunks_at_target, content.leaves.size());
    BOOST_CHECK_EQUAL(result.min_replicas, 2U);
    BOOST_CHECK_EQUAL(result.ProgressPercent(storage.RemoteReplicaTarget()), 100);
    // Exactly the remote target on distinct providers; the local copy is extra.
    for (const auto& leaf : content.leaves) {
        BOOST_CHECK_EQUAL(ReplicaCount(network, leaf), 2);
        BOOST_CHECK(fixture.runtime->GetChunkBlobStore().Has(leaf));
    }
    // Idempotent: no further PUTs once protected, and the state survives reopen.
    const int puts = network.puts;
    BOOST_CHECK(storage.Secure(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);
    BOOST_CHECK_EQUAL(network.puts, puts);
    cybou::StorageService reopened{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    const auto durability = reopened.GetDurability(content.operation_id);
    BOOST_REQUIRE(durability);
    BOOST_CHECK(durability->state == cybou::DurabilityState::PROTECTED);
}

BOOST_AUTO_TEST_CASE(too_few_or_lagging_providers_keep_content_securing)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    const auto endpoints = network.Endpoints();
    // One provider is not synced yet (rejects NOT_FINALIZED), one is offline.
    network.lagging.insert(endpoints[0]);
    network.offline.insert(endpoints[1]);
    network.Sync();
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    auto result = storage.Secure(content.operation_id, content.leaves);
    BOOST_CHECK(result.state == cybou::DurabilityState::SECURING);
    BOOST_CHECK_EQUAL(result.min_replicas, 1U);
    BOOST_CHECK(!result.error.empty());
    // When the network recovers, Resume completes placement.
    network.lagging.clear();
    network.offline.clear();
    network.Sync();
    result = storage.Resume(content.operation_id);
    BOOST_CHECK(result.state == cybou::DurabilityState::PROTECTED);
}

BOOST_AUTO_TEST_CASE(storage_loss_and_corruption_are_repaired)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture, 4};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    BOOST_REQUIRE(storage.Secure(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);

    // One provider dies after ACK; another starts returning corrupt bytes.
    const auto endpoints = network.Endpoints();
    cybou::StorageEndpoint dead, bad;
    for (const auto& endpoint : endpoints) {
        if (!network.Holds(endpoint, content.leaves.front())) continue;
        if (dead.port == 0) dead = endpoint;
        else if (bad.port == 0) bad = endpoint;
    }
    BOOST_REQUIRE(dead.port != 0 && bad.port != 0);
    network.offline.insert(dead);
    network.corrupt.insert(bad);
    // Repair must work even after the local encrypted cache was evicted.
    for (const auto& leaf : content.leaves) {
        BOOST_REQUIRE(fixture.runtime->GetChunkBlobStore().Remove(leaf));
    }
    const auto repaired = storage.Audit(content.operation_id);
    // Leaf 0 lost both known copies; the remaining providers may hold other leaves only.
    for (std::size_t i{0}; i < content.leaves.size(); ++i) {
        int healthy{0};
        for (const auto& endpoint : endpoints) {
            if (endpoint == dead || endpoint == bad) continue;
            healthy += network.Holds(endpoint, content.leaves[i]) ? 1 : 0;
        }
        if (repaired.state == cybou::DurabilityState::PROTECTED) BOOST_CHECK_GE(healthy, 2);
    }
    BOOST_CHECK(repaired.state != cybou::DurabilityState::NEEDS_ATTENTION);
    // With the corrupt provider fixed, any remaining gaps close.
    network.corrupt.clear();
    const auto healed = storage.Audit(content.operation_id);
    BOOST_CHECK(healed.state == cybou::DurabilityState::PROTECTED);
}

BOOST_AUTO_TEST_CASE(audits_record_evidence_and_drop_a_provider_with_wrong_answers)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture, 3};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    BOOST_REQUIRE(storage.Secure(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);

    // Each counted replica came with one verified receipt.
    std::uint64_t receipts{0};
    for (const auto& [_, evidence] : storage.ProviderEvidence()) receipts += evidence.receipts;
    BOOST_CHECK_EQUAL(receipts, content.leaves.size() * cybou::BETA_REMOTE_REPLICA_TARGET);

    cybou::StorageEndpoint bad;
    for (const auto& endpoint : network.Endpoints()) {
        if (network.Holds(endpoint, content.leaves.front())) { bad = endpoint; break; }
    }
    BOOST_REQUIRE(bad.port != 0);
    network.corrupt.insert(bad);
    for (int i{0}; i < 8; ++i) BOOST_REQUIRE(storage.AuditNextPlacement(content.leaves.size()));
    // Mostly cheap random-offset audits; a wrong answer is a failure whatever path caught it.
    BOOST_CHECK_GT(network.audits, 0);
    const auto evidence = storage.ProviderEvidence();
    BOOST_REQUIRE(evidence.contains(bad.storage_id));
    BOOST_CHECK_GT(evidence.at(bad.storage_id).failures, 0U);
    for (const auto& [storage_id, entry] : evidence) {
        if (storage_id != bad.storage_id) BOOST_CHECK_GT(entry.successes, 0U);
    }
    const auto placement = storage.DescribePlacement(content.operation_id);
    BOOST_REQUIRE(placement);
    for (const auto& replicas : placement->replicas) {
        for (const auto& replica : replicas) BOOST_CHECK(!cybou::SameProvider(replica, bad));
    }
}

BOOST_AUTO_TEST_CASE(shadow_accounting_credits_verified_intervals_and_survives_restart)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture, 2};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    std::map<std::array<unsigned char, 32>, cybou::StorageProviderEvidence> before;
    {
        cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
        BOOST_REQUIRE(storage.Secure(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);
        // Rent estimate: one billing unit per chunk at two replicas (DEC-279).
        BOOST_CHECK(storage.EstimatedDailyRent() == cybou::StorageRentPerDay(content.leaves.size(), 2));
        // A receipt alone earns nothing: credit needs a later successful check.
        for (const auto& [_, evidence] : storage.ProviderEvidence()) BOOST_CHECK_EQUAL(evidence.verified_unit_seconds, 0U);
        std::this_thread::sleep_for(std::chrono::milliseconds{1100});
        BOOST_REQUIRE(storage.AuditNextPlacement(content.leaves.size()));
        before = storage.ProviderEvidence();
        BOOST_REQUIRE_EQUAL(before.size(), 2U);
        for (const auto& [_, evidence] : before) {
            BOOST_CHECK_GE(evidence.verified_unit_seconds, content.leaves.size());
            BOOST_CHECK(evidence.shadow_reward.cybou > 0 || evidence.shadow_reward.remainder > 0);
        }
    }
    // Evidence is persisted in the encrypted Application DB and reloads after restart.
    cybou::StorageService restarted{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    const auto after = restarted.ProviderEvidence();
    BOOST_REQUIRE_EQUAL(after.size(), before.size());
    for (const auto& [storage_id, evidence] : before) {
        BOOST_REQUIRE(after.contains(storage_id));
        BOOST_CHECK_EQUAL(after.at(storage_id).verified_unit_seconds, evidence.verified_unit_seconds);
        BOOST_CHECK_EQUAL(after.at(storage_id).successes, evidence.successes);
        BOOST_CHECK_EQUAL(after.at(storage_id).receipts, evidence.receipts);
        BOOST_CHECK_EQUAL(after.at(storage_id).shadow_reward.remainder, evidence.shadow_reward.remainder);
    }
}

BOOST_AUTO_TEST_CASE(surviving_remote_copy_repairs_every_chunk_without_local_cache)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("repair-owner.cybou");
    ProviderNetwork network{fixture, 4};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    BOOST_REQUIRE(storage.Secure(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);
    const auto endpoints = network.Endpoints();
    network.offline.insert(endpoints.front());
    std::map<cybou::ChunkId, std::vector<unsigned char>> expected;
    for (const auto& leaf : content.leaves) {
        const auto bytes = fixture.runtime->GetChunkBlobStore().Get(leaf);
        BOOST_REQUIRE(bytes);
        expected.emplace(leaf, *bytes);
        BOOST_REQUIRE(fixture.runtime->GetChunkBlobStore().Remove(leaf));
    }
    const auto height = fixture.runtime->GetFinalizedHeight();
    const auto repaired = storage.Audit(content.operation_id);
    BOOST_REQUIRE(repaired.state == cybou::DurabilityState::PROTECTED);
    for (const auto& [leaf, bytes] : expected) {
        unsigned healthy{0};
        for (const auto& endpoint : endpoints) {
            const auto remote = network.Get(endpoint, leaf);
            if (remote && *remote == bytes) ++healthy;
        }
        BOOST_CHECK_GE(healthy, 2U);
        const auto fetched = storage.Fetch(leaf);
        BOOST_REQUIRE(fetched);
        BOOST_CHECK(*fetched == bytes);
    }
    BOOST_CHECK(fixture.runtime->GetFinalizedHeight() == height);
}

BOOST_AUTO_TEST_CASE(all_remote_unavailable_downgrades_and_fresh_placement_rebuild_recovers)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("rebuild-owner.cybou");
    ProviderNetwork network{fixture};
    const auto app_path = fixture.directory / "application";
    PublishedContent content;
    std::map<cybou::ChunkId, std::vector<unsigned char>> expected;
    {
        cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), app_path};
        content = Publish(fixture, *identity, application_db, true);
        network.Sync();
        cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
        BOOST_REQUIRE(storage.Secure(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);
        for (const auto& leaf : content.leaves) {
            const auto bytes = fixture.runtime->GetChunkBlobStore().Get(leaf);
            BOOST_REQUIRE(bytes);
            expected.emplace(leaf, *bytes);
            BOOST_REQUIRE(fixture.runtime->GetChunkBlobStore().Remove(leaf));
        }
        network.SetAllOffline(true);
        const auto unavailable = storage.Audit(content.operation_id);
        BOOST_CHECK(unavailable.state != cybou::DurabilityState::PROTECTED);
        BOOST_CHECK_EQUAL(unavailable.min_replicas, 0U);
        for (const auto& leaf : content.leaves) BOOST_CHECK(!storage.Fetch(leaf));
    }
    std::filesystem::remove_all(app_path);
    cybou::PrivateApplicationStore fresh{identity->GetKeyStore(), app_path};
    cybou::StorageService storage{*fixture.runtime, network, fresh, cybou::BETA_REMOTE_REPLICA_TARGET};
    BOOST_CHECK(!storage.GetDurability(content.operation_id));
    BOOST_CHECK(storage.Rebuild(content.operation_id, content.leaves).state != cybou::DurabilityState::PROTECTED);
    const auto height = fixture.runtime->GetFinalizedHeight();
    network.SetAllOffline(false);
    BOOST_REQUIRE(storage.Rebuild(content.operation_id, content.leaves).state == cybou::DurabilityState::PROTECTED);
    for (const auto& [leaf, bytes] : expected) {
        const auto fetched = storage.Fetch(leaf);
        BOOST_REQUIRE(fetched);
        BOOST_CHECK(*fetched == bytes);
    }
    BOOST_CHECK(fixture.runtime->GetFinalizedHeight() == height);
}

BOOST_AUTO_TEST_CASE(rebuild_preserves_verified_progress_across_rate_limits_and_service_restarts)
{
    CybouServiceTestFixture fixture;
    auto identity=fixture.CreateIdentity("rebuild-progress.cybou");
    ProviderNetwork network{fixture, 1};
    cybou::PrivateApplicationStore staging{identity->GetKeyStore(), fixture.directory/"staging"};
    const auto content=Publish(fixture, *identity, staging, true);
    BOOST_REQUIRE_GT(content.leaves.size(), 1U);
    network.Sync();
    cybou::StorageService initial{*fixture.runtime, network, staging, 1};
    BOOST_REQUIRE(initial.Secure(content.operation_id, content.leaves).state==cybou::DurabilityState::PROTECTED);
    const auto recovered_path=fixture.directory/"recovered";
    for (std::size_t round{0}; round<content.leaves.size(); ++round) {
        network.proof_budget=1;
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), recovered_path};
        cybou::StorageService resumed{*fixture.runtime, network, db, 1};
        BOOST_CHECK(!resumed.GetDurability(content.operation_id));
        const auto result=resumed.Rebuild(content.operation_id, content.leaves);
        if (round+1<content.leaves.size()) {
            BOOST_CHECK(result.state==cybou::DurabilityState::SECURING);
            BOOST_CHECK(!resumed.GetDurability(content.operation_id));
        } else BOOST_CHECK(result.state==cybou::DurabilityState::PROTECTED);
    }
}

BOOST_AUTO_TEST_CASE(fetch_uses_local_cache_then_verified_remote_copy)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture};
    const auto app_path = fixture.directory / "application";
    std::vector<cybou::ChunkId> leaves;
    {
        cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), app_path};
        const auto content = Publish(fixture, *identity, application_db, true);
        leaves = content.leaves;
        network.Sync();
        cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
        BOOST_REQUIRE(storage.Secure(content.operation_id, leaves).state == cybou::DurabilityState::PROTECTED);
    }
    const auto original = fixture.runtime->GetChunkBlobStore().Get(leaves.front());
    BOOST_REQUIRE(original);
    // Lost placement cache and evicted local copy: retrieval needs only the ChunkID.
    std::filesystem::remove_all(app_path);
    BOOST_REQUIRE(fixture.runtime->GetChunkBlobStore().Remove(leaves.front()));
    cybou::PrivateApplicationStore fresh{identity->GetKeyStore(), app_path};
    cybou::StorageService storage{*fixture.runtime, network, fresh, cybou::BETA_REMOTE_REPLICA_TARGET};
    // A corrupt provider is skipped in favour of a valid copy.
    network.corrupt.insert(network.Endpoints().front());
    const auto fetched = storage.Fetch(leaves.front());
    BOOST_REQUIRE(fetched);
    BOOST_CHECK(*fetched == *original);
    BOOST_CHECK(fixture.runtime->GetChunkBlobStore().Has(leaves.front()));
    // Nothing anywhere: unavailable, not an error that blocks callers.
    cybou::ChunkId unknown{};
    unknown[0] = 0x42;
    BOOST_CHECK(!storage.Fetch(unknown));
}

BOOST_AUTO_TEST_CASE(runtime_transport_places_and_fetches_over_p2p)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    const auto height = fixture.runtime->GetFinalizedHeight().value_or(0);

    // Two real storage providers listening on loopback, synced to finality.
    boost::asio::io_context io;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    std::vector<std::unique_ptr<cybou::CybouNodeRuntime>> providers;
    std::vector<std::unique_ptr<cybou::p2p::InboundPeerServer>> servers;
    std::atomic_bool stopping{false};
    std::vector<std::jthread> listeners;
    // Fatal assertions must stop listeners before jthread joins during unwind.
    struct StopListeners {
        std::atomic_bool& stopping;
        std::vector<std::jthread>& listeners;
        ~StopListeners() { stopping = true; listeners.clear(); }
    } stop_listeners{stopping, listeners};
    std::vector<std::pair<std::string, uint16_t>> endpoints;
    for (int i{0}; i < 2; ++i) {
        cybou::NodeRuntimeConfig config{.network_genesis = fixture.definition,
            .data_dir = fixture.directory / ("socket-provider-" + std::to_string(i)),
            .memory_only = true, .wipe_data = true, .storage_capacity_bytes = 64ULL << 20,
            .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0};
        auto provider = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
        BOOST_REQUIRE(provider->InitializeGenesis(fixture.genesis));
        for (std::uint64_t h{1}; h <= height; ++h) BOOST_REQUIRE(provider->CommitBlock(*fixture.runtime->GetBlockAtHeight(h)));
        servers.push_back(std::make_unique<cybou::p2p::InboundPeerServer>(*provider, io,
            boost::asio::ip::tcp::endpoint{loopback, 0}));
        endpoints.emplace_back(loopback.to_string(), servers.back()->Port());
        providers.push_back(std::move(provider));
    }
    for (auto& server : servers) listeners.emplace_back([&stopping, s = server.get()] { s->Run(stopping); });
    {
        // A Full Node whose runtime reaches providers only through CYBOU P2P.
        cybou::NodeRuntimeConfig client_config{.network_genesis = fixture.definition,
            .data_dir = fixture.directory / "socket-client", .configured_peers = {{endpoints.front()}},
            .memory_only = true, .wipe_data = true, .peer_admission_policy = TestPeerAdmissionPolicy(), .operation_work_bits = 0};
        cybou::CybouNodeRuntime client{std::move(client_config)};
        BOOST_REQUIRE(client.InitializeGenesis(fixture.genesis));
        client.SetConfiguredPeerEndpoints(endpoints);
        for (int i{0}; i < 6 && client.StorageEndpoints().size() < 2; ++i) client.SyncFromConfiguredPeer(10);
        BOOST_REQUIRE_EQUAL(client.StorageEndpoints().size(), 2U);
        // Each storage peer is known by the ProviderID it proved on demand.
        std::set<std::array<unsigned char, 32>> proven;
        for (const auto& peer : client.StorageEndpoints()) proven.insert(peer.storage_id);
        BOOST_CHECK(proven == (std::set{*providers[0]->LocalStorageId(), *providers[1]->LocalStorageId()}));

        cybou::RuntimeStorageTransport transport{client};
        cybou::StorageService storage{*fixture.runtime, transport, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
        const auto result = storage.Secure(content.operation_id, content.leaves);
        BOOST_CHECK(result.state == cybou::DurabilityState::PROTECTED);
        const auto publication = fixture.runtime->FindFinalizedRootPublication(content.operation_id);
        BOOST_REQUIRE(publication);
        for (const auto& leaf : content.leaves) {
            BOOST_CHECK(providers[0]->HasFinalizedChunk(leaf));
            BOOST_CHECK(providers[1]->HasFinalizedChunk(leaf));
            std::optional<cybou::ChunkAuthorizationProof> proof;
            for (const auto& peer : client.StorageEndpoints()) {
                proof = transport.GetProof({peer.storage_id, peer.address, peer.port}, content.operation_id, leaf);
                if (proof) break;
            }
            BOOST_REQUIRE(proof);
            BOOST_CHECK(cybou::VerifyChunkAuthorizationProof(*publication, leaf, *proof));
        }
        // Random-offset audit over CYBOU P2P answers only from exact admitted bytes.
        const auto local = fixture.runtime->GetChunkBlobStore().Get(content.leaves.front());
        BOOST_REQUIRE(local);
        for (const auto& peer : client.StorageEndpoints()) {
            const cybou::StorageEndpoint provider{peer.storage_id, peer.address, peer.port};
            cybou::StorageAuditChallenge challenge{.chunk_id = content.leaves.front(), .byte_offset = local->size() / 2};
            challenge.nonce.fill(0x42);
            const auto answer = transport.Audit(provider, challenge);
            BOOST_REQUIRE(answer);
            BOOST_CHECK(answer->held);
            BOOST_CHECK(answer->response_hash ==
                *cybou::ComputeStorageAuditResponse(*local, challenge.byte_offset, challenge.nonce));
            challenge.chunk_id.fill(0x99);
            const auto missing = transport.Audit(provider, challenge);
            BOOST_REQUIRE(missing);
            BOOST_CHECK(!missing->held);
        }
        // Every counted replica retains a receipt signed by its own StorageId.
        for (const auto& provider : providers) {
            const auto storage_id = *provider->LocalStorageId();
            const auto receipt = application_db.Get("storage/receipt/" + content.operation_id.GetHex() + '/' +
                cybou::Hash256{std::span<const unsigned char, 32>{content.leaves.front()}}.GetHex() + '/' +
                cybou::Hash256{std::span<const unsigned char, 32>{storage_id}}.GetHex());
            BOOST_REQUIRE(receipt);
            const auto size = static_cast<std::uint32_t>(local->size());
            BOOST_CHECK(cybou::VerifyStorageReceipt(*receipt, fixture.runtime->GetNetworkBinding(),
                content.operation_id, content.leaves.front(), size) == storage_id);
            BOOST_CHECK(!cybou::VerifyStorageReceipt(*receipt, fixture.runtime->GetNetworkBinding(),
                content.operation_id, content.leaves.front(), size + 1));
        }
        const auto original = fixture.runtime->GetChunkBlobStore().Get(content.leaves.back());
        BOOST_REQUIRE(original);
        BOOST_REQUIRE(fixture.runtime->GetChunkBlobStore().Remove(content.leaves.back()));
        const auto fetched = storage.Fetch(content.leaves.back());
        BOOST_REQUIRE(fetched);
        BOOST_CHECK(*fetched == *original);
    }
    stopping = true;
    listeners.clear();
}

BOOST_AUTO_TEST_CASE(development_target_is_one_remote_replica)
{
    BOOST_CHECK_EQUAL(cybou::DEVELOPMENT_REMOTE_REPLICA_TARGET, 1);
    BOOST_CHECK_EQUAL(cybou::BETA_REMOTE_REPLICA_TARGET, 2);
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    // Only one provider is reachable: enough for the development target.
    network.SetAllOffline(true);
    network.offline.erase(network.Endpoints().front());
    cybou::StorageService storage{*fixture.runtime, network, application_db};
    BOOST_CHECK_EQUAL(storage.RemoteReplicaTarget(), 1);
    const auto result = storage.Secure(content.operation_id, content.leaves);
    BOOST_CHECK(result.state == cybou::DurabilityState::PROTECTED);
    BOOST_CHECK_EQUAL(result.min_replicas, 1U);
}

BOOST_AUTO_TEST_CASE(one_storage_key_is_one_replica_whatever_its_endpoints)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture, 2};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();
    // Provider A also answers on a second address; provider B is offline.
    const auto endpoints = network.Endpoints();
    const auto alias = network.AddAlias(endpoints[0], "10.9.9.9");
    network.offline.insert(endpoints[1]);
    cybou::StorageService storage{*fixture.runtime, network, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};
    auto result = storage.Secure(content.operation_id, content.leaves);
    BOOST_CHECK(result.state == cybou::DurabilityState::SECURING);
    BOOST_CHECK_EQUAL(result.min_replicas, 1U);
    const auto placement = storage.DescribePlacement(content.operation_id);
    BOOST_REQUIRE(placement);
    for (const auto& replicas : placement->replicas) BOOST_CHECK_EQUAL(replicas.size(), 1U);
    // A second, distinct provider key completes the Beta target.
    network.offline.erase(endpoints[1]);
    result = storage.Secure(content.operation_id, content.leaves);
    BOOST_CHECK(result.state == cybou::DurabilityState::PROTECTED);
    BOOST_CHECK_EQUAL(result.min_replicas, 2U);
    (void)alias;
}

BOOST_AUTO_TEST_CASE(storage_proof_binds_key_session_and_network)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "proof-provider", .memory_only = true, .wipe_data = true,
        .storage_capacity_bytes = 1ULL << 20, .operation_work_bits = 0};
    cybou::CybouNodeRuntime provider{std::move(config)};
    BOOST_REQUIRE(provider.InitializeGenesis(fixture.genesis));
    const auto network_binding = provider.GetNetworkBinding();
    cybou::p2p::Hello signer{.network_binding = network_binding, .nonce = 11};
    cybou::p2p::Hello verifier{.network_binding = network_binding, .nonce = 22};
    std::array<unsigned char, 32> exporter{};
    exporter[0] = 1;
    const auto message = cybou::p2p::StorageProofMessage(signer, verifier, exporter);
    auto proof = provider.SignStorageProof(message);
    BOOST_REQUIRE(proof);
    BOOST_CHECK(cybou::p2p::VerifyStorageProof(*proof, message) == provider.LocalStorageId());
    // Replayed into another session or network, or tampered with, it proves nothing.
    auto other_session = verifier;
    other_session.nonce = 23;
    BOOST_CHECK(!cybou::p2p::VerifyStorageProof(*proof, cybou::p2p::StorageProofMessage(signer, other_session, exporter)));
    auto other_network = signer;
    other_network.network_binding = cybou::Hash256{};
    BOOST_CHECK(!cybou::p2p::VerifyStorageProof(*proof, cybou::p2p::StorageProofMessage(other_network, verifier, exporter)));
    (*proof)[5] ^= 0x01;
    BOOST_CHECK(!cybou::p2p::VerifyStorageProof(*proof, message));
    // Even a zero-quota Full Node has its own storage identity.
    BOOST_CHECK(fixture.runtime->LocalStorageId());
}

BOOST_AUTO_TEST_CASE(concurrency_inspection_not_blocked_during_placement)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("storage-owner.cybou");
    ProviderNetwork network{fixture};
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    const auto content = Publish(fixture, *identity, application_db, true);
    network.Sync();

    class SlowTransport final : public cybou::StorageTransport {
    public:
        explicit SlowTransport(ProviderNetwork& base) : m_base{base} {}
        std::vector<cybou::StorageEndpoint> Providers() override { return m_base.Providers(); }
        std::optional<cybou::ChunkAdmissionResult> Put(const cybou::StorageEndpoint& provider,
            const cybou::Hash256& op_id, const cybou::ChunkId& chunk_id, std::span<const unsigned char> bytes,
            const cybou::ChunkAuthorizationProof& proof) override
        {
            in_put.store(true);
            std::unique_lock lock{cv_mutex};
            cv.wait_for(lock, std::chrono::milliseconds(2000), [&] { return inspect_done.load(); });
            return m_base.Put(provider, op_id, chunk_id, bytes, proof);
        }
        std::optional<std::vector<unsigned char>> Get(const cybou::StorageEndpoint& provider,
            const cybou::ChunkId& chunk_id) override { return m_base.Get(provider, chunk_id); }
        std::optional<cybou::ChunkAuthorizationProof> GetProof(const cybou::StorageEndpoint& provider,
            const cybou::Hash256& op_id, const cybou::ChunkId& chunk_id) override
        {
            return m_base.GetProof(provider, op_id, chunk_id);
        }

        ProviderNetwork& m_base;
        std::atomic_bool in_put{false};
        std::atomic_bool inspect_done{false};
        std::mutex cv_mutex;
        std::condition_variable cv;
    } slow_transport{network};

    cybou::StorageService storage{*fixture.runtime, slow_transport, application_db, cybou::BETA_REMOTE_REPLICA_TARGET};

    std::atomic_bool secure_done{false};
    std::jthread worker([&] {
        storage.Secure(content.operation_id, content.leaves);
        secure_done.store(true);
    });

    while (!slow_transport.in_put.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto start = std::chrono::steady_clock::now();
    const auto durability = storage.GetDurability(content.operation_id);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    BOOST_CHECK(durability.has_value());
    BOOST_CHECK_LT(elapsed, 500);

    const auto fetched = storage.Fetch(content.leaves.front());
    BOOST_CHECK(fetched.has_value());

    {
        std::lock_guard lock{slow_transport.cv_mutex};
        slow_transport.inspect_done.store(true);
    }
    slow_transport.cv.notify_all();
    worker.join();
    BOOST_CHECK(secure_done.load());
}

BOOST_AUTO_TEST_SUITE_END()
