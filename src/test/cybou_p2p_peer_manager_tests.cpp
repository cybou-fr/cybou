// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/hex.h>
#include <cybou/p2p/peer_manager.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_test_setup.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(cybou_p2p_peer_manager_tests, CybouTestSetup)

BOOST_AUTO_TEST_CASE(removed_storage_wire_ids_are_rejected)
{
    for (uint8_t type = 21; type <= 28; ++type) {
        BOOST_CHECK(!cybou::p2p::EncodeFrame({static_cast<cybou::p2p::MessageType>(type), {}}));
        const std::array<unsigned char, 10> encoded{
            'C', 'Y', 'P', '2', cybou::p2p::WIRE_VERSION, type, 0, 0, 0, 0};
        BOOST_CHECK(!cybou::p2p::DecodeFrame(encoded));
    }
}

BOOST_AUTO_TEST_CASE(manager_tracks_two_live_peers_and_drops_closed_sockets)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second_acceptor{io, tcp::endpoint{loopback, 0}};
    std::array<bool, 2> served{false, false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        first_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served[0] = session.Handshake({.network_id = network, .finalized_height = 12, .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 101}) &&
            session.AnswerPing();
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served[1] = session.Handshake({.network_id = network, .finalized_height = 13, .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 102}) &&
            session.AnswerPing();
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto first_port = first_acceptor.local_endpoint().port();
    const auto second_port = second_acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, first_port));
    BOOST_REQUIRE(manager.Connect(address, second_port));
    BOOST_CHECK(!manager.Connect(address, first_port));
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 2U);
    const auto peers = manager.Peers();
    BOOST_REQUIRE_EQUAL(peers.size(), 2U);
    BOOST_CHECK(peers[0].hello.network_id == network);
    BOOST_CHECK_EQUAL(manager.PingAll(), 2U);
    first_server.join();
    second_server.join();
    BOOST_CHECK(served[0] && served[1]);
    BOOST_CHECK_EQUAL(manager.PingAll(), 0U);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_pings_only_the_requested_peer_budget_and_rotates)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second_acceptor{io, tcp::endpoint{loopback, 0}};
    std::array<bool, 2> served{false, false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        first_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served[0] = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 1801}) &&
            session.AnswerPing();
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served[1] = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 1802}) &&
            session.AnswerPing();
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    BOOST_REQUIRE(manager.Connect(address, first_acceptor.local_endpoint().port()));
    BOOST_REQUIRE(manager.Connect(address, second_acceptor.local_endpoint().port()));
    BOOST_CHECK_EQUAL(manager.PingSome(1), 1U);
    BOOST_CHECK_EQUAL(manager.PingSome(1), 1U);
    first_server.join();
    second_server.join();
    BOOST_CHECK(served[0] && served[1]);
}

BOOST_AUTO_TEST_CASE(manager_refuses_wrong_network_peer)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto wrong_network = cybou::ParseUint256UserHex("02");
    BOOST_REQUIRE(wrong_network);
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        session.Handshake({.network_id = *wrong_network, .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 103});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::WRONG_NETWORK);
    server.join();
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(proof_request_is_bound_to_the_discovered_provider_id)
{
    CybouServiceTestFixture fixture;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    boost::asio::io_context io;
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    auto make_provider = [&](const std::string& name) {
        cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
            .data_dir = fixture.directory / name, .memory_only = true, .wipe_data = true,
            .storage_enabled = true, .storage_capacity_bytes = 64ULL << 20};
        auto runtime = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
        if (!runtime->InitializeGenesis(fixture.genesis)) throw std::runtime_error{"provider genesis failed"};
        return runtime;
    };
    auto first = make_provider("proof-provider-1");
    auto second = make_provider("proof-provider-2");
    const auto first_id = first->LocalProviderId();
    const auto second_id = second->LocalProviderId();
    BOOST_REQUIRE(first_id && second_id);
    BOOST_REQUIRE(*first_id != *second_id);
    const auto network = fixture.runtime->GetNetworkId();
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    std::atomic_bool first_handshake{false};
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        first_handshake = session.Handshake({.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_STORAGE | cybou::p2p::CAP_STORAGE_PROOFS,
            .nonce = 0x3101}, [provider = first.get()](auto message) {
                return provider->SignProviderProof(message);
            });
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(address, port));
    first_server.join();
    BOOST_REQUIRE(first_handshake.load());
    const auto discovered = manager.StoragePeers();
    BOOST_REQUIRE_EQUAL(discovered.size(), 1U);
    BOOST_CHECK(discovered.front().provider_id == first_id);
    manager.DisconnectAll();

    std::atomic_bool second_handshake{false};
    std::atomic_bool request_served{false};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        second_handshake = session.Handshake({.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_STORAGE | cybou::p2p::CAP_STORAGE_PROOFS,
            .nonce = 0x3102}, [provider = second.get()](auto message) {
                return provider->SignProviderProof(message);
            });
        if (second_handshake) request_served = session.ServeNext(*second);
    }};
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto current = manager.StoragePeers();
    BOOST_REQUIRE_EQUAL(current.size(), 1U);
    BOOST_CHECK(current.front().provider_id == second_id);
    cybou::ChunkId chunk{};
    chunk[0] = 1;
    const auto operation = uint256::ONE;
    BOOST_CHECK(!manager.GetChunkAuthorizationProof(address, port, *first_id, operation, chunk));
    manager.DisconnectAll();
    second_server.join();
    BOOST_CHECK(second_handshake.load());
    BOOST_CHECK(!request_served.load());
}

BOOST_AUTO_TEST_CASE(explicit_validator_peer_evicts_discovered_peer_at_capacity)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    const auto network = fixture.runtime->GetNetworkId();
    std::vector<std::unique_ptr<tcp::acceptor>> acceptors;
    std::vector<std::unique_ptr<tcp::acceptor>> replacement_acceptors;
    std::vector<std::jthread> replacement_servers;
    std::vector<uint16_t> ports;
    std::array<std::atomic_bool, cybou::p2p::MAX_OUTBOUND_PEERS> handshakes{};
    for (size_t i = 0; i < handshakes.size(); ++i) {
        acceptors.push_back(std::make_unique<tcp::acceptor>(io, tcp::endpoint{loopback, 0}));
        ports.push_back(acceptors.back()->local_endpoint().port());
        auto* listener = acceptors.back().get();
        replacement_servers.emplace_back([&, i, listener] {
            tcp::socket socket{io};
            listener->accept(socket);
            cybou::p2p::PeerSession session{std::move(socket)};
            handshakes[i] = session.Handshake({.network_id = network,
                .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
                .capabilities = 0, .nonce = 200 + i});
        });
    }
    replacement_acceptors.push_back(std::make_unique<tcp::acceptor>(io, tcp::endpoint{loopback, 0}));
    const auto replacement_port = replacement_acceptors.back()->local_endpoint().port();

    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    for (size_t i = 0; i < cybou::p2p::MAX_OUTBOUND_PEERS; ++i) {
        BOOST_REQUIRE(manager.Connect(address, ports[i]));
    }
    BOOST_CHECK(!manager.Connect(address, ports.back())); // discovered peers cannot displace connections
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), cybou::p2p::MAX_OUTBOUND_PEERS);
    manager.SetExplicitEndpoints({{address, replacement_port}});
    replacement_servers.emplace_back([&] {
        tcp::socket socket{io};
        replacement_acceptors.back()->accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        handshakes.back() = session.Handshake({.network_id = network,
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = 0, .nonce = 300});
    });
    BOOST_REQUIRE(manager.Connect(address, replacement_port)); // explicit validator replaces a discovered peer
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), cybou::p2p::MAX_OUTBOUND_PEERS);
    const auto peers = manager.Peers();
    BOOST_REQUIRE_EQUAL(peers.size(), cybou::p2p::MAX_OUTBOUND_PEERS);
    BOOST_CHECK(std::any_of(peers.begin(), peers.end(), [&](const auto& peer) {
        return peer.address == address && peer.port == replacement_port;
    }));
    manager.DisconnectAll();
    for (auto& server : replacement_servers) server.join();
    BOOST_CHECK(std::all_of(handshakes.begin(), handshakes.end(), [](const auto& ok) { return ok.load(); }));
}

BOOST_AUTO_TEST_CASE(manager_refuses_hello_tip_conflicting_with_known_block)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool handshake_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = uint256::ONE,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 111});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    server.join();
    BOOST_CHECK(handshake_ok);
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::HANDSHAKE_FAILED);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);

    tcp::acceptor matching_acceptor{io, tcp::endpoint{loopback, 0}};
    bool matching_handshake_ok{false};
    std::jthread matching_server{[&] {
        tcp::socket socket{io};
        matching_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        matching_handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = *fixture.runtime->GetFinalizedTip(),
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 112});
    }};
    BOOST_CHECK(manager.Connect(loopback.to_string(), matching_acceptor.local_endpoint().port()));
    matching_server.join();
    BOOST_CHECK(matching_handshake_ok);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 1U);
}

BOOST_AUTO_TEST_CASE(manager_refuses_wrong_genesis_tip)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto wrong_tip = fixture.definition.genesis_block_id == uint256::ONE ?
        *cybou::ParseUint256UserHex("02") : uint256::ONE;
    bool handshake_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        handshake_ok = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = wrong_tip,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 115});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    server.join();
    BOOST_CHECK(handshake_ok);
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::HANDSHAKE_FAILED);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_reports_unavailable_endpoint)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto port = acceptor.local_endpoint().port();
    acceptor.close();
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), port));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::UNAVAILABLE);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(storage_placeme_peer_that_closes_without_hello)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        socket.close();
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::UNAVAILABLE);
    server.join();
}

BOOST_AUTO_TEST_CASE(manager_rejects_peer_without_block_service)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 107});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 1);
    server.join();
    BOOST_CHECK(result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_distinguishes_malformed_block_response_from_disconnect)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    const auto network = fixture.runtime->GetNetworkId();
    for (const bool malformed : {true, false}) {
        tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
        bool served{false};
        std::jthread server{[&] {
            tcp::socket socket{io};
            acceptor.accept(socket);
            cybou::p2p::PeerSession session{std::move(socket)};
            served = session.Handshake({.network_id = network, .finalized_height = 1,
                .finalized_tip = fixture.definition.genesis_block_id, .capabilities = cybou::p2p::CAP_SERVE_BLOCKS,
                .nonce = malformed ? 108ULL : 109ULL});
            std::array<unsigned char, 18> request{};
            size_t received{0};
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (served && received < request.size() && std::chrono::steady_clock::now() < deadline) {
                boost::system::error_code ec;
                const auto count = session.Socket().read_some(
                    boost::asio::buffer(request.data() + received, request.size() - received), ec);
                if (ec == boost::asio::error::would_block || ec == boost::asio::error::try_again) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                if (ec) { served = false; break; }
                received += count;
            }
            served = served && received == request.size();
            if (served && malformed) {
                const auto frame = cybou::p2p::EncodeFrame({cybou::p2p::MessageType::BLOCK_META,
                    {0xff, 0xff, 0xff, 0x7f}});
                if (frame) {
                    boost::system::error_code ec;
                    boost::asio::write(session.Socket(), boost::asio::buffer(*frame), ec);
                    served = !ec;
                }
            }
        }};
        cybou::p2p::PeerManager manager{*fixture.runtime};
        const auto address = loopback.to_string();
        const auto port = acceptor.local_endpoint().port();
        BOOST_REQUIRE(manager.Connect(address, port));
        const auto result = manager.SyncFromPeer(address, port, 1);
        server.join();
        BOOST_CHECK(served);
        BOOST_CHECK(result.status == (malformed ? cybou::SyncPeerStatus::PROTOCOL_ERROR :
            cybou::SyncPeerStatus::CONNECTION_FAILED));
        BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
    }
}

BOOST_AUTO_TEST_CASE(manager_retries_peer_that_cannot_serve_announced_height)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    for (const bool promises_block : {false, true}) {
        tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
        bool served{false};
        std::jthread server{[&] {
            tcp::socket socket{io};
            acceptor.accept(socket);
            cybou::p2p::PeerSession session{std::move(socket)};
            served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
                .finalized_height = promises_block ? 1ULL : 0ULL, .finalized_tip = fixture.definition.genesis_block_id,
                .capabilities = cybou::p2p::CAP_SERVE_BLOCKS,
                .nonce = promises_block ? 113ULL : 114ULL}) &&
                session.ServeNext(*fixture.runtime);
        }};
        cybou::p2p::PeerManager manager{*fixture.runtime};
        const auto address = loopback.to_string();
        const auto port = acceptor.local_endpoint().port();
        BOOST_REQUIRE(manager.Connect(address, port));
        const auto result = manager.SyncFromPeer(address, port, 1);
        server.join();
        BOOST_CHECK(served);
        BOOST_CHECK(result.status == (promises_block ? cybou::SyncPeerStatus::CONNECTION_FAILED :
            cybou::SyncPeerStatus::UP_TO_DATE));
        BOOST_CHECK_EQUAL(manager.ConnectedCount(), promises_block ? 0U : 1U);
    }
}

BOOST_AUTO_TEST_CASE(manager_syncs_two_verified_blocks_on_one_session)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "observer", .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served = session.Handshake({.network_id = network, .finalized_height = 2,
            .finalized_tip = fixture.runtime->GetFinalizedTip().value(),
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_BLOCK_INVENTORY, .nonce = 104}) &&
            session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime) &&
            session.ServeNext(*fixture.runtime);
    }};
    cybou::p2p::PeerManager manager{observer};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 2);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(result.blocks_applied, 2U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 2U);
    BOOST_CHECK(observer.GetFinalizedTip() == fixture.runtime->GetFinalizedTip());
}

BOOST_AUTO_TEST_CASE(manager_fans_out_finalized_block_without_duplicate_payload)
{
    CybouServiceTestFixture fixture;
    const auto produced = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(produced);
    const auto block_id = cybou::ComputeBlockId(produced->block);
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "block-gossip-observer", .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_BLOCK_ANNOUNCEMENTS, .nonce = 121}) &&
            session.ServeNext(observer);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK_EQUAL(manager.FanoutRecentBlocks(), 1U);
    BOOST_CHECK_EQUAL(manager.FanoutRecentBlocks(), 0U);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 1U);
    BOOST_CHECK(observer.GetFinalizedTip().value_or(uint256{}) == block_id);
}

BOOST_AUTO_TEST_CASE(manager_rejects_block_that_disagrees_with_inventory)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "inventory-observer", .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        if (!session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = fixture.runtime->GetFinalizedTip().value(),
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_BLOCK_INVENTORY,
            .nonce = 115})) return;
        std::array<unsigned char, 19> request{};
        boost::system::error_code error;
        size_t received{0};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (received < request.size() && std::chrono::steady_clock::now() < deadline) {
            const auto count = session.Socket().read_some(
                boost::asio::buffer(request.data() + received, request.size() - received), error);
            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again) {
                error.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (error) return;
            received += count;
        }
        if (received != request.size()) return;
        std::vector<unsigned char> inventory{1, 1, 0, 0, 0, 0, 0, 0, 0};
        inventory.insert(inventory.end(), uint256::ONE.begin(), uint256::ONE.end());
        const auto frame = cybou::p2p::EncodeFrame({cybou::p2p::MessageType::BLOCK_INV, inventory});
        if (!frame) return;
        size_t written{0};
        while (written < frame->size() && std::chrono::steady_clock::now() < deadline) {
            const auto count = session.Socket().write_some(
                boost::asio::buffer(frame->data() + written, frame->size() - written), error);
            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again) {
                error.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            if (error) return;
            written += count;
        }
        served = written == frame->size() && session.ServeNext(*fixture.runtime);
    }};
    cybou::p2p::PeerManager manager{observer};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR);
    BOOST_CHECK_EQUAL(result.blocks_applied, 0U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
}

BOOST_AUTO_TEST_CASE(manager_rejects_block_conflicting_with_announced_finalized_tip)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "tip-observer", .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 1, .finalized_tip = uint256::ONE,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 110}) &&
            session.ServeNext(*fixture.runtime);
    }};
    cybou::p2p::PeerManager manager{observer};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto result = manager.SyncFromPeer(address, port, 1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR);
    BOOST_CHECK_EQUAL(result.blocks_applied, 0U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
}

BOOST_AUTO_TEST_CASE(manager_submits_canonical_operation_with_separate_acknowledgment)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("p2p-account.cybou");
    const auto source_block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source_block);
    BOOST_REQUIRE_EQUAL(source_block->block.operations.size(), 1U);
    const auto operation = source_block->block.operations.front();
    const auto op_id = cybou::ComputeOperationId(operation);
    BOOST_REQUIRE(op_id);

    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "other-producer", .poa_finalizer_recovery_entropy = fixture.validator_seed,
        .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime receiver{std::move(config)};
    BOOST_REQUIRE(receiver.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    bool served{false};
    const auto network = fixture.runtime->GetNetworkId();
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 105}) &&
            session.ServeNext(receiver) && session.ServeNext(receiver);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto port = acceptor.local_endpoint().port();
    BOOST_REQUIRE(manager.Connect(address, port));
    const auto submitted = manager.SubmitOperation(address, port, operation);
    const auto repeated = manager.SubmitOperation(address, port, operation);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK(submitted.status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK(submitted.op_id == *op_id);
    BOOST_CHECK(repeated.status == cybou::OperationSubmitStatus::ALREADY_PENDING);
    BOOST_CHECK(repeated.op_id == *op_id);
    BOOST_CHECK_EQUAL(receiver.GetFinalizedHeight().value_or(99), 0U);
    const auto finalized = receiver.ProduceBlock();
    BOOST_REQUIRE(finalized);
    BOOST_CHECK_EQUAL(finalized->block.operations.size(), 1U);
    BOOST_CHECK(finalized->block.operations.front() == operation);
}

BOOST_AUTO_TEST_CASE(manager_submits_to_next_peer_when_first_cannot_accept_operations)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("p2p-failover.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();
    const auto op_id = cybou::ComputeOperationId(operation);
    BOOST_REQUIRE(op_id);
    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "failover-producer",
        .poa_finalizer_recovery_entropy = fixture.validator_seed, .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime receiver{std::move(config)};
    BOOST_REQUIRE(receiver.InitializeGenesis(fixture.genesis));

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor first{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor second{io, tcp::endpoint{loopback, 0}};
    bool first_handshake{false};
    bool second_served{false};
    std::jthread first_server{[&] {
        tcp::socket socket{io};
        first.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        first_handshake = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = 0, .nonce = 116});
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        second_served = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 117}) &&
            session.ServeNext(receiver);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto result = manager.SubmitOperationToAny({{address, first.local_endpoint().port()},
        {address, second.local_endpoint().port()}}, operation);
    first_server.join();
    second_server.join();
    BOOST_CHECK(first_handshake);
    BOOST_CHECK(second_served);
    BOOST_REQUIRE(result.acknowledgment);
    BOOST_CHECK(result.acknowledgment->status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK(result.acknowledgment->op_id == *op_id);
    BOOST_CHECK(!result.delivery_uncertain);
    BOOST_REQUIRE(result.endpoint);
    BOOST_CHECK_EQUAL(result.endpoint->second, second.local_endpoint().port());
    BOOST_CHECK_EQUAL(receiver.GetFinalizedHeight().value_or(99), 0U);
    const auto finalized = receiver.ProduceBlock();
    BOOST_REQUIRE(finalized);
    BOOST_CHECK_EQUAL(finalized->block.operations.size(), 1U);
    BOOST_CHECK(finalized->block.operations.front() == operation);
}

BOOST_AUTO_TEST_CASE(manager_distinguishes_rejection_from_missing_operation_acknowledgment)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("p2p-ack-status.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();

    cybou::NodeRuntimeConfig config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "rejecting-observer", .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime rejecting{std::move(config)};
    BOOST_REQUIRE(rejecting.InitializeGenesis(fixture.genesis));
    tcp::acceptor reject_acceptor{io, tcp::endpoint{loopback, 0}};
    bool rejected_response_sent{false};
    std::jthread reject_server{[&] {
        tcp::socket socket{io};
        reject_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        rejected_response_sent = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 118}) &&
            session.ServeNext(rejecting);
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    const auto address = loopback.to_string();
    const auto rejected = manager.SubmitOperationToAny({{address, reject_acceptor.local_endpoint().port()}}, operation);
    reject_server.join();
    BOOST_CHECK(rejected_response_sent);
    BOOST_REQUIRE(rejected.acknowledgment);
    BOOST_CHECK(rejected.acknowledgment->status == cybou::OperationSubmitStatus::REJECTED);
    BOOST_CHECK(rejected.endpoint);
    BOOST_CHECK(!rejected.delivery_uncertain);

    tcp::acceptor dropped_acceptor{io, tcp::endpoint{loopback, 0}};
    bool dropped_handshake{false};
    std::jthread dropped_server{[&] {
        tcp::socket socket{io};
        dropped_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        dropped_handshake = session.Handshake({.network_id = fixture.runtime->GetNetworkId(),
            .finalized_height = 0, .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 119});
    }};
    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "unconfirmed-observer",
        .p2p_endpoint = std::make_pair(address, dropped_acceptor.local_endpoint().port()),
        .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto unconfirmed = observer.SubmitOperation(operation);
    dropped_server.join();
    BOOST_CHECK(dropped_handshake);
    BOOST_CHECK(unconfirmed.status == cybou::OperationSubmitStatus::REJECTED);
    BOOST_CHECK(unconfirmed.delivery_uncertain);
}

BOOST_AUTO_TEST_CASE(runtime_routes_submission_and_verified_sync_over_configured_peer)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("runtime-route.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();

    cybou::NodeRuntimeConfig producer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "route-producer", .poa_finalizer_recovery_entropy = fixture.validator_seed,
        .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime producer{std::move(producer_config)};
    BOOST_REQUIRE(producer.InitializeGenesis(fixture.genesis));
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = producer.GetNetworkId();
    bool served{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = cybou::p2p::CAP_SERVE_BLOCKS |
                cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 106}) &&
            session.ServeNext(producer) && producer.ProduceBlock().has_value() &&
            session.ServeNext(producer) && session.ServeNext(producer);
    }};
    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "route-observer",
        .p2p_endpoint = std::make_pair(loopback.to_string(), acceptor.local_endpoint().port()),
        .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(observer.CanSubmitOperations());
    const auto submitted = observer.SubmitOperation(operation);
    BOOST_CHECK(submitted.status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
    const auto synced = observer.SyncFromConfiguredPeer(1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(synced.blocks_applied, 1U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 1U);
    BOOST_CHECK(observer.GetFinalizedTip() == producer.GetFinalizedTip());
    BOOST_CHECK_EQUAL(observer.ConnectedPeerCount(), 1U);
}

BOOST_AUTO_TEST_CASE(runtime_discovers_and_syncs_from_a_second_peer)
{
    CybouServiceTestFixture fixture;
    const auto block = fixture.runtime->ProduceBlock();
    BOOST_REQUIRE(block);

    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor seed_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor source_acceptor{io, tcp::endpoint{loopback, 0}};
    tcp::acceptor* seed_listener{&seed_acceptor};
    tcp::acceptor* source_listener{&source_acceptor};
    if (seed_listener->local_endpoint().port() > source_listener->local_endpoint().port()) {
        std::swap(seed_listener, source_listener);
    }
    const auto network = fixture.runtime->GetNetworkId();
    const auto seed_endpoint = std::make_pair(loopback.to_string(), seed_listener->local_endpoint().port());
    const auto source_endpoint = std::make_pair(loopback.to_string(), source_listener->local_endpoint().port());

    cybou::NodeRuntimeConfig seed_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "multi-peer-seed", .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime seed{std::move(seed_config)};
    BOOST_REQUIRE(seed.InitializeGenesis(fixture.genesis));
    seed.SetExplicitPeerEndpoints({source_endpoint});

    std::atomic_bool seed_served{false};
    std::atomic_bool source_served{false};
    std::jthread seed_server{[&] {
        tcp::socket socket{io};
        seed_listener->accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        const bool handshake = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS | cybou::p2p::CAP_PEER_DISCOVERY, .nonce = 7811});
        seed_served = handshake && session.ServeNext(seed) && session.ServeNext(seed) &&
            session.ServeNext(seed) && session.ServeNext(seed);
    }};
    std::jthread source_server{[&] {
        tcp::socket socket{io};
        source_listener->accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        // Both peers advertise the same stale HELLO snapshot. The source's
        // canonical store is already ahead, so stopping at the first
        // UP_TO_DATE peer would mask its new block.
        const bool handshake = session.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 7812});
        source_served = handshake && session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime);
    }};

    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "multi-peer-observer", .p2p_endpoint = seed_endpoint,
        .memory_only = true, .wipe_data = true};
    auto observer = std::make_unique<cybou::CybouNodeRuntime>(std::move(observer_config));
    BOOST_REQUIRE(observer->InitializeGenesis(fixture.genesis));

    const auto first_sync = observer->SyncFromConfiguredPeer(1);
    BOOST_CHECK(first_sync.IsConnected());
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(99), 0U);
    const auto second_sync = observer->SyncFromConfiguredPeer(1);
    BOOST_CHECK(second_sync.IsConnected());
    BOOST_CHECK_EQUAL(second_sync.blocks_applied, 1U);
    BOOST_CHECK_EQUAL(observer->GetFinalizedHeight().value_or(0), 1U);
    BOOST_CHECK_EQUAL(observer->ConnectedPeerCount(), 2U);

    seed_server.join();
    source_server.join();
    BOOST_CHECK(seed_served.load());
    BOOST_CHECK(source_served.load());
}

BOOST_AUTO_TEST_CASE(manager_discovers_peers_from_connected_peer)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto network = fixture.runtime->GetNetworkId();

    const std::vector<std::pair<std::string, uint16_t>> seed_peers{
        {"192.168.1.50", 29460},
        {"192.168.1.51", 29460},
    };

    // Remote node with CAP_PEER_DISCOVERY
    cybou::NodeRuntimeConfig remote_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "remote-discovery-node",
        .p2p_endpoint = std::make_pair("192.168.1.49", uint16_t{29460}),
        .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime remote_runtime{std::move(remote_config)};
    BOOST_REQUIRE(remote_runtime.InitializeGenesis(fixture.genesis));
    remote_runtime.AddDiscoveredPeerEndpoints(seed_peers);

    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        BOOST_REQUIRE(session.Handshake({
            .network_id = network, .finalized_height = 0,
            .finalized_tip = fixture.definition.genesis_block_id,
            .capabilities = cybou::p2p::CAP_PEER_DISCOVERY,
            .nonce = 7771,
        }));
        session.ServeNext(remote_runtime);
    }};

    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_REQUIRE(manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 1U);

    // Initial known endpoints on local runtime should be empty
    BOOST_CHECK_EQUAL(fixture.runtime->GetPeerEndpointsForGossip().size(), 0U);

    // DiscoverPeers queries remote peer and populates local runtime
    BOOST_CHECK_EQUAL(manager.DiscoverPeers(0), 0U);
    const size_t added = manager.DiscoverPeers(1);
    server.join();

    BOOST_CHECK_GE(added, 2U);
    const auto known = manager.KnownEndpoints();
    BOOST_CHECK_GE(known.size(), 2U);
}

BOOST_AUTO_TEST_SUITE_END()
