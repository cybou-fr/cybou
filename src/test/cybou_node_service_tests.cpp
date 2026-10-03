// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/node_service.h>
#include <cybou/p2p/session.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_test_setup.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_node_service_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(desktop_finalizer_worker_produces_blocks_and_stops_cleanly)
{
    CybouServiceTestFixture local;
    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_definition = local.definition,
            .data_dir = local.directory / "desktop-finalizer-service",
            .poa_finalizer_recovery_entropy = local.validator_seed,
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestLabAdmissionPolicy(),
        },
        .genesis = local.genesis,
    }};
    service.Start();
    service.StartBlockProduction(10);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (service.Runtime().GetFinalizedHeight().value_or(0) < 2 &&
        std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
    BOOST_REQUIRE_GE(service.Runtime().GetFinalizedHeight().value_or(0), 2U);
    service.Runtime().DisablePoaSigner();
    service.StopBlockProduction();
    const auto stopped_height = service.Runtime().GetFinalizedHeight().value_or(0);
    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(0), stopped_height);
}

BOOST_AUTO_TEST_CASE(full_node_network_service_starts_without_an_initial_peer)
{
    CybouServiceTestFixture local;
    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_definition = local.definition,
            .data_dir = local.directory / "peerless-observer-service",
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestLabAdmissionPolicy(),
        },
        .genesis = local.genesis,
    }};
    service.Start();

    std::mutex mutex;
    std::condition_variable changed;
    bool callback_seen{false};
    bool reports_waiting_for_peer{false};
    service.StartNetwork({.sync_interval = std::chrono::milliseconds{20}, .sync_batch_size = 1},
        [&](const cybou::SyncPeerResult& result, const cybou::NodeRuntimeStatus& status, size_t peers) {
            std::lock_guard lock{mutex};
            callback_seen = true;
            reports_waiting_for_peer = status.is_initialized && peers == 0 &&
                result.status == cybou::SyncPeerStatus::CONNECTION_FAILED;
            changed.notify_all();
            return true;
        });

    {
        std::unique_lock lock{mutex};
        BOOST_REQUIRE(changed.wait_for(lock, std::chrono::seconds{5}, [&] { return callback_seen; }));
    }
    service.StopNetwork();
    BOOST_CHECK(reports_waiting_for_peer);
}

BOOST_AUTO_TEST_CASE(configured_peer_is_not_eclipsed_by_newer_stale_hello)
{
    CybouServiceTestFixture primary;
    CybouServiceTestFixture secondary;
    CybouServiceTestFixture configured_source;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second{io, tcp::endpoint{loopback, 0}};
    std::jthread first_server{[&] {
        tcp::socket socket{io}; first.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        if (session.Handshake({.network_binding=primary.runtime->GetNetworkBinding(),
                .finalized_height=0,.finalized_tip=primary.definition.genesis_block_id,
                .nonce=30001})) {
            while (session.ServeNext(*configured_source.runtime)) {}
        }
    }};
    std::optional<std::jthread> second_server;
    auto observer=std::make_unique<cybou::CybouNodeRuntime>(cybou::NodeRuntimeConfig{
        .network_definition=primary.definition,.data_dir=primary.directory/"route-observer",
        .configured_peers={{std::make_pair(loopback.to_string(),first.local_endpoint().port())}},
        .memory_only=true,.wipe_data=true,
        .peer_admission_policy = TestLabAdmissionPolicy()});
    BOOST_REQUIRE(observer->InitializeGenesis(primary.genesis));
    BOOST_CHECK(observer->SyncFromConfiguredPeer(2).status==cybou::SyncPeerStatus::UP_TO_DATE);
    // The original route advances, but its stored HELLO remains at height zero.
    const auto block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(block);
    BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*block)));
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*block)));
    const auto second_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(second_block);
    BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*second_block)));
    second_server.emplace([&] {
        tcp::socket socket{io}; second.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        if (session.Handshake({.network_binding=secondary.runtime->GetNetworkBinding(),
                .finalized_height=1,.finalized_tip=cybou::ComputeBlockId(block->block),
                .nonce=30002})) {
            while (session.ServeNext(*secondary.runtime)) {}
        }
    });
    observer->SetConfiguredPeerEndpoints({{loopback.to_string(),second.local_endpoint().port()}});
    observer->SyncFromConfiguredPeer(2);
    // Choosing the later, higher HELLO would return only its one stale block.
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99),2U);
    // A configured peer that stops advancing must not prevent failover.
    if (secondary.runtime->GetFinalizedHeight().value_or(0) < 2)
        BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*second_block)));
    const auto third_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(third_block);
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*third_block)));
    observer->SyncFromConfiguredPeer(2);
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99),3U);
    // Partial progress from the preferred route must not starve a fresher one.
    if (configured_source.runtime->GetFinalizedHeight().value_or(0) < 3)
        BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*third_block)));
    const auto fourth_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(fourth_block);
    BOOST_REQUIRE(static_cast<bool>(configured_source.runtime->CommitBlock(*fourth_block)));
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*fourth_block)));
    const auto fifth_block=primary.runtime->ProduceBlock();
    BOOST_REQUIRE(fifth_block);
    BOOST_REQUIRE(static_cast<bool>(secondary.runtime->CommitBlock(*fifth_block)));
    const auto progress=observer->SyncFromConfiguredPeer(2);
    BOOST_CHECK_EQUAL(progress.blocks_applied,2U);
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99),5U);
    observer.reset();
    first_server.join(); second_server->join();
}

BOOST_AUTO_TEST_CASE(full_node_network_worker_recovers_after_peer_protocol_error)
{
    CybouServiceTestFixture local;
    CybouServiceTestFixture foreign{0x41};
    BOOST_REQUIRE(local.runtime->ProduceBlock());
    BOOST_REQUIRE(foreign.runtime->ProduceBlock());

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor recovery_acceptor{io, tcp::endpoint{loopback, 0}};
    std::atomic_bool peer_served_bad_block{false};
    std::atomic_bool peer_served_valid_block{false};
    std::jthread bad_peer{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const auto foreign_block = foreign.runtime->GetBlockAtHeight(1);
        if (foreign_block && session.Handshake({
                .network_binding = local.runtime->GetNetworkBinding(),
                .finalized_height = 1,
                .finalized_tip = cybou::ComputeBlockId(foreign_block->block),
                .nonce = 29001}) && session.ServeNext(*foreign.runtime)) {
            peer_served_bad_block.store(true);
        }
    }};
    std::jthread recovery_peer{[&] {
        tcp::socket socket{io};
        recovery_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket), cybou::p2p::TransportRole::SERVER};
        const auto valid_block = local.runtime->GetBlockAtHeight(1);
        if (valid_block && session.Handshake({
                .network_binding = local.runtime->GetNetworkBinding(),
                .finalized_height = 1,
                .finalized_tip = cybou::ComputeBlockId(valid_block->block),
                .nonce = 29002}) && session.ServeNext(*local.runtime) && session.ServeNext(*local.runtime)) {
            peer_served_valid_block.store(true);
        }
    }};

    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_definition = local.definition,
            .data_dir = local.directory / "service-observer",
            .configured_peers = {{std::make_pair(loopback.to_string(), acceptor.local_endpoint().port())}},
            .memory_only = true,
            .wipe_data = true,
            .peer_admission_policy = TestLabAdmissionPolicy(),
        },
        .genesis = local.genesis,
    }};
    service.Start();
    service.Runtime().SetConfiguredPeerEndpoints({
        {loopback.to_string(), acceptor.local_endpoint().port()},
        {loopback.to_string(), recovery_acceptor.local_endpoint().port()},
    });

    std::mutex mutex;
    std::condition_variable changed;
    size_t callbacks{0};
    bool malformed_peer_result_was_retryable{false};
    bool recovered{false};
    service.StartNetwork(
        {.sync_interval = std::chrono::milliseconds{25}, .sync_batch_size = 1, .listen_endpoint = std::nullopt},
        [&](const cybou::SyncPeerResult& result, const cybou::NodeRuntimeStatus& runtime_status, size_t) {
            {
                std::lock_guard lock{mutex};
                // The runtime isolates the peer's protocol error and exposes
                // the failed sync as retryable CONNECTION_FAILED to the worker.
                if (callbacks == 0) malformed_peer_result_was_retryable =
                    result.status == cybou::SyncPeerStatus::CONNECTION_FAILED;
                ++callbacks;
                recovered = recovered || runtime_status.finalized_height == 1;
            }
            changed.notify_all();
            return true;
        });

    {
        std::unique_lock lock{mutex};
        BOOST_REQUIRE(changed.wait_for(lock, std::chrono::seconds{10}, [&] { return callbacks >= 2 && recovered; }));
    }
    service.StopNetwork();
    bad_peer.join();
    recovery_peer.join();

    BOOST_CHECK(peer_served_bad_block.load());
    BOOST_CHECK(peer_served_valid_block.load());
    BOOST_CHECK(malformed_peer_result_was_retryable);
    BOOST_CHECK_GE(callbacks, 2U);
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(99), 1U);
}

BOOST_AUTO_TEST_SUITE_END()
