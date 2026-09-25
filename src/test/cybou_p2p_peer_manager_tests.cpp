// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_manager.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
#include <chrono>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_p2p_peer_manager_tests, BasicTestingSetup)

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
        served[0] = session.Handshake({.network_id = network, .finalized_height = 12, .finalized_tip = {}, .capabilities = 0, .nonce = 101}) &&
            session.AnswerPing();
    }};
    std::jthread second_server{[&] {
        tcp::socket socket{io};
        second_acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        served[1] = session.Handshake({.network_id = network, .finalized_height = 13, .finalized_tip = {}, .capabilities = 0, .nonce = 102}) &&
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

BOOST_AUTO_TEST_CASE(manager_refuses_wrong_network_peer)
{
    CybouServiceTestFixture fixture;
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    tcp::acceptor acceptor{io, tcp::endpoint{loopback, 0}};
    const auto wrong_network = uint256::FromUserHex("02");
    BOOST_REQUIRE(wrong_network);
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession session{std::move(socket)};
        session.Handshake({.network_id = *wrong_network, .finalized_height = 0, .finalized_tip = {}, .capabilities = 0, .nonce = 103});
    }};
    cybou::p2p::PeerManager manager{*fixture.runtime};
    BOOST_CHECK(!manager.Connect(loopback.to_string(), acceptor.local_endpoint().port()));
    BOOST_CHECK(manager.LastConnectStatus() == cybou::p2p::PeerConnectStatus::HANDSHAKE_FAILED);
    server.join();
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

BOOST_AUTO_TEST_CASE(manager_retries_peer_that_closes_without_hello)
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
            .finalized_tip = {}, .capabilities = 0, .nonce = 107});
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
                .finalized_tip = {}, .capabilities = cybou::p2p::CAP_SERVE_BLOCKS,
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
            .capabilities = cybou::p2p::CAP_SERVE_BLOCKS, .nonce = 104}) &&
            session.ServeNext(*fixture.runtime) && session.ServeNext(*fixture.runtime);
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
        .data_dir = fixture.directory / "other-producer", .validator_private_key = fixture.validator_seed,
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
            .finalized_tip = {}, .capabilities = cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 105}) &&
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

BOOST_AUTO_TEST_CASE(runtime_routes_submission_and_verified_sync_over_configured_peer)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("runtime-route.cybou");
    const auto source = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(source);
    const auto operation = source->block.operations.front();

    cybou::NodeRuntimeConfig producer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "route-producer", .validator_private_key = fixture.validator_seed,
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
            .finalized_tip = {}, .capabilities = cybou::p2p::CAP_SERVE_BLOCKS |
                cybou::p2p::CAP_ACCEPT_OPERATIONS, .nonce = 106}) &&
            session.ServeNext(producer) && producer.ProduceBlock().has_value() &&
            session.ServeNext(producer);
    }};
    cybou::NodeRuntimeConfig observer_config{.network_definition = fixture.definition,
        .data_dir = fixture.directory / "route-observer",
        .p2p_endpoint = std::make_pair(loopback.to_string(), acceptor.local_endpoint().port()),
        .memory_only = true, .wipe_data = true};
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(observer.HasSubmitEndpoint());
    const auto submitted = observer.SubmitOperation(operation);
    BOOST_CHECK(submitted.status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0U);
    const auto synced = observer.SyncFromConfiguredPeer(1);
    server.join();
    BOOST_CHECK(served);
    BOOST_CHECK_EQUAL(synced.blocks_applied, 1U);
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 1U);
    BOOST_CHECK(observer.GetFinalizedTip() == producer.GetFinalizedTip());
}

BOOST_AUTO_TEST_SUITE_END()
