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
        .peer_admission_policy = TestLabAdmissionPolicy()}};
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

BOOST_AUTO_TEST_CASE(provider_loss_and_corruption_are_repaired)
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

BOOST_AUTO_TEST_CASE(runtime_transport_places_and_fetches_over_cyp2)
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
            .peer_admission_policy = TestLabAdmissionPolicy()};
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
        // A Full Node whose runtime reaches providers only through CYP2.
        cybou::NodeRuntimeConfig client_config{.network_genesis = fixture.definition,
            .data_dir = fixture.directory / "socket-client", .configured_peers = {{endpoints.front()}},
            .memory_only = true, .wipe_data = true, .peer_admission_policy = TestLabAdmissionPolicy()};
        cybou::CybouNodeRuntime client{std::move(client_config)};
        BOOST_REQUIRE(client.InitializeGenesis(fixture.genesis));
        client.SetConfiguredPeerEndpoints(endpoints);
        for (int i{0}; i < 6 && client.StorageEndpoints().size() < 2; ++i) client.SyncFromConfiguredPeer(10);
        BOOST_REQUIRE_EQUAL(client.StorageEndpoints().size(), 2U);
        // Each storage peer is known by the ProviderID it proved on demand.
        std::set<std::array<unsigned char, 32>> proven;
        for (const auto& peer : client.StorageEndpoints()) proven.insert(peer.provider_id);
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
                proof = transport.GetProof({peer.provider_id, peer.address, peer.port}, content.operation_id, leaf);
                if (proof) break;
            }
            BOOST_REQUIRE(proof);
            BOOST_CHECK(cybou::VerifyChunkAuthorizationProof(*publication, leaf, *proof));
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

BOOST_AUTO_TEST_CASE(one_provider_key_is_one_replica_whatever_its_endpoints)
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

BOOST_AUTO_TEST_CASE(provider_proof_binds_key_session_and_network)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{.network_genesis = fixture.definition,
        .data_dir = fixture.directory / "proof-provider", .memory_only = true, .wipe_data = true,
        .storage_capacity_bytes = 1ULL << 20};
    cybou::CybouNodeRuntime provider{std::move(config)};
    BOOST_REQUIRE(provider.InitializeGenesis(fixture.genesis));
    const auto network_binding = provider.GetNetworkBinding();
    cybou::p2p::Hello signer{.network_binding = network_binding, .nonce = 11};
    cybou::p2p::Hello verifier{.network_binding = network_binding, .nonce = 22};
    std::array<unsigned char, 32> exporter{};
    exporter[0] = 1;
    const auto message = cybou::p2p::StorageProofMessage(signer, verifier, exporter);
    auto proof = provider.SignProviderProof(message);
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

BOOST_AUTO_TEST_SUITE_END()
