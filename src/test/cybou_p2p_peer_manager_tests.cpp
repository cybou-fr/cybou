// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_manager.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <array>
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
    server.join();
    BOOST_CHECK_EQUAL(manager.ConnectedCount(), 0U);
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
            .finalized_tip = fixture.runtime->GetFinalizedTip().value(), .capabilities = 0, .nonce = 104}) &&
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
            .finalized_tip = {}, .capabilities = 0, .nonce = 105}) &&
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

BOOST_AUTO_TEST_SUITE_END()
