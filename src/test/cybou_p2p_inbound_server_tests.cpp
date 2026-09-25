// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/inbound_server.h>
#include <cybou/p2p/session.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(cybou_p2p_inbound_server_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(idle_peer_does_not_block_other_peers_and_limit_is_enforced)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    cybou::p2p::InboundPeerServer server{*fixture.runtime, io, tcp::endpoint{loopback, 0}};
    std::atomic_bool stop{false};
    std::jthread listener{[&] { server.Run(stop); }};
    struct StopGuard { std::atomic_bool& stop; ~StopGuard() { stop = true; } } guard{stop};

    std::vector<std::unique_ptr<cybou::p2p::PeerSession>> clients;
    const auto network = fixture.runtime->GetNetworkId();
    for (size_t i = 0; i < cybou::p2p::MAX_INBOUND_PEERS; ++i) {
        tcp::socket socket{io};
        socket.connect(tcp::endpoint{loopback, server.Port()});
        auto client = std::make_unique<cybou::p2p::PeerSession>(std::move(socket));
        BOOST_REQUIRE(client->Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = {}, .capabilities = 0, .nonce = 1000 + i}));
        clients.push_back(std::move(client));
    }
    BOOST_CHECK(clients.back()->Ping(77));
    BOOST_CHECK(clients.front()->Ping(78));

    tcp::socket excess_socket{io};
    excess_socket.connect(tcp::endpoint{loopback, server.Port()});
    cybou::p2p::PeerSession excess{std::move(excess_socket)};
    BOOST_CHECK(!excess.Handshake({.network_id = network, .finalized_height = 0,
        .finalized_tip = {}, .capabilities = 0, .nonce = 2000}));
}

BOOST_AUTO_TEST_SUITE_END()
