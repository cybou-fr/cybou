// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/node_service.h>
#include <cybou/p2p/session.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_node_service_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(observer_network_worker_continues_after_peer_protocol_error)
{
    CybouServiceTestFixture local;
    CybouServiceTestFixture foreign{0x41};
    BOOST_REQUIRE(foreign.runtime->ProduceBlock());

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    std::atomic_bool peer_served{false};
    std::jthread peer{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        const auto foreign_block = foreign.runtime->GetBlockAtHeight(1);
        if (foreign_block && session.Handshake({
                .network_id = local.runtime->GetNetworkId(),
                .finalized_height = 1,
                .finalized_tip = cybou::ComputeBlockId(foreign_block->block),
                .capabilities = cybou::p2p::CAP_SERVE_BLOCKS,
                .nonce = 29001}) && session.ServeNext(*foreign.runtime)) {
            peer_served.store(true);
        }
    }};

    cybou::CybouNodeService service{{
        .runtime = cybou::NodeRuntimeConfig{
            .network_definition = local.definition,
            .data_dir = local.directory / "service-observer",
            .p2p_endpoint = std::make_pair(loopback.to_string(), acceptor.local_endpoint().port()),
            .memory_only = true,
            .wipe_data = true,
        },
        .genesis = local.genesis,
    }};
    service.Start();

    std::mutex mutex;
    std::condition_variable changed;
    size_t callbacks{0};
    bool first_was_protocol_error{false};
    service.StartNetwork(
        {loopback.to_string(), acceptor.local_endpoint().port()},
        {.sync_interval = std::chrono::milliseconds{25}, .sync_batch_size = 1, .listen_endpoint = std::nullopt},
        [&](const cybou::SyncPeerResult& result, const cybou::NodeRuntimeStatus&, size_t) {
            {
                std::lock_guard lock{mutex};
                // The runtime isolates the peer's protocol error and exposes
                // the failed sync as retryable CONNECTION_FAILED to the worker.
                if (callbacks == 0) first_was_protocol_error = result.status == cybou::SyncPeerStatus::CONNECTION_FAILED;
                ++callbacks;
            }
            changed.notify_all();
            return true;
        });

    {
        std::unique_lock lock{mutex};
        BOOST_REQUIRE(changed.wait_for(lock, std::chrono::seconds{5}, [&] { return callbacks >= 2; }));
    }
    service.StopNetwork();
    peer.join();

    BOOST_CHECK(peer_served.load());
    BOOST_CHECK(first_was_protocol_error);
    BOOST_CHECK_GE(callbacks, 2U);
    BOOST_CHECK_EQUAL(service.Runtime().GetFinalizedHeight().value_or(99), 0U);
}

BOOST_AUTO_TEST_SUITE_END()
