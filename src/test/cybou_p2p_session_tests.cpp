// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/session.h>
#include <cybou/bft_engine.h>
#include <cybou/block.h>
#include <cybou/validator.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <thread>

BOOST_FIXTURE_TEST_SUITE(cybou_p2p_session_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(frame_rejects_bad_size_and_version)
{
    const cybou::p2p::Frame frame{cybou::p2p::MessageType::PING, {1, 2, 3}};
    const auto bytes = cybou::p2p::EncodeFrame(frame);
    BOOST_REQUIRE(bytes);
    BOOST_CHECK(cybou::p2p::DecodeFrame(*bytes)->payload == frame.payload);
    auto bad = *bytes;
    bad[4] = 99;
    BOOST_CHECK(!cybou::p2p::DecodeFrame(bad));
    bad = *bytes;
    bad[6] = 0xff;
    bad[7] = 0xff;
    BOOST_CHECK(!cybou::p2p::DecodeFrame(bad));
    BOOST_CHECK(!cybou::p2p::EncodeFrame({cybou::p2p::MessageType::PING,
        std::vector<unsigned char>(cybou::p2p::MAX_FRAME_PAYLOAD + 1)}));
    const cybou::p2p::Frame inventory{cybou::p2p::MessageType::OP_INV,
        std::vector<unsigned char>(32, 1)};
    const auto encoded_inventory = cybou::p2p::EncodeFrame(inventory);
    BOOST_REQUIRE(encoded_inventory);
    BOOST_CHECK(cybou::p2p::DecodeFrame(*encoded_inventory)->payload == inventory.payload);
    const cybou::p2p::Frame block_inventory{cybou::p2p::MessageType::BLOCK_INV,
        std::vector<unsigned char>(41, 1)};
    const auto encoded_blocks = cybou::p2p::EncodeFrame(block_inventory);
    BOOST_REQUIRE(encoded_blocks);
    BOOST_CHECK(cybou::p2p::DecodeFrame(*encoded_blocks)->payload == block_inventory.payload);
}

BOOST_AUTO_TEST_CASE(hello_rejects_missing_finalized_tip)
{
    cybou::p2p::Hello hello{.network_id = uint256::ONE, .finalized_height = 0,
        .finalized_tip = uint256::ONE, .capabilities = 0, .nonce = 1};
    const auto encoded = cybou::p2p::EncodeHello(hello);
    BOOST_CHECK(cybou::p2p::DecodeHello(encoded));
    auto missing_tip = encoded;
    std::fill(missing_tip.begin() + 40, missing_tip.begin() + 72, 0);
    BOOST_CHECK(!cybou::p2p::DecodeHello(missing_tip));
}

BOOST_AUTO_TEST_CASE(loopback_session_checks_network_and_stays_open_for_ping)
{
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, tcp::endpoint{boost::asio::ip::address_v4::loopback(), 0}};
    const auto endpoint = acceptor.local_endpoint();
    uint256 network = uint256::ONE;
    cybou::p2p::Hello server_hello{.network_id = network, .finalized_height = 8,
        .finalized_tip = uint256::ONE, .capabilities = 3, .nonce = 11};
    cybou::p2p::Hello client_hello{.network_id = network, .finalized_height = 7,
        .finalized_tip = uint256::ONE, .capabilities = 1, .nonce = 22};
    bool server_ok{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession peer{std::move(socket)};
        server_ok = peer.Handshake(server_hello) && peer.Peer()->finalized_height == 7 && peer.AnswerPing();
    }};
    tcp::socket socket{io};
    socket.connect(endpoint);
    cybou::p2p::PeerSession peer{std::move(socket)};
    BOOST_REQUIRE(peer.Handshake(client_hello));
    BOOST_REQUIRE(peer.Peer());
    BOOST_CHECK_EQUAL(peer.Peer()->finalized_height, 8);
    BOOST_CHECK(peer.Ping(42));
    server.join();
    BOOST_CHECK(server_ok);
}

BOOST_AUTO_TEST_CASE(loopback_session_rejects_network_mismatch)
{
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, tcp::endpoint{boost::asio::ip::address_v4::loopback(), 0}};
    bool server_accepted{true};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession peer{std::move(socket)};
        server_accepted = peer.Handshake({.network_id = uint256::ONE,
            .finalized_height = 0, .finalized_tip = uint256::ONE, .capabilities = 0, .nonce = 31});
    }};
    tcp::socket socket{io};
    socket.connect(acceptor.local_endpoint());
    cybou::p2p::PeerSession peer{std::move(socket)};
    const auto other_network = uint256::FromUserHex("02");
    BOOST_REQUIRE(other_network);
    BOOST_CHECK(!peer.Handshake({.network_id = *other_network,
        .finalized_height = 0, .finalized_tip = uint256::ONE, .capabilities = 0, .nonce = 32}));
    server.join();
    BOOST_CHECK(!server_accepted);
}

BOOST_AUTO_TEST_CASE(client_respects_advertised_block_capability)
{
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, tcp::endpoint{boost::asio::ip::address_v4::loopback(), 0}};
    bool answered{false};
    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession peer{std::move(socket)};
        answered = peer.Handshake({.network_id = uint256::ONE, .finalized_height = 0,
            .finalized_tip = uint256::ONE, .capabilities = 0, .nonce = 41}) && peer.AnswerPing();
    }};
    tcp::socket socket{io};
    socket.connect(acceptor.local_endpoint());
    cybou::p2p::PeerSession peer{std::move(socket)};
    BOOST_REQUIRE(peer.Handshake({.network_id = uint256::ONE, .finalized_height = 0,
        .finalized_tip = uint256::ONE, .capabilities = 0, .nonce = 42}));
    BOOST_CHECK(peer.RequestBlock(1).status == cybou::p2p::BlockRequestStatus::INVALID_REQUEST);
    BOOST_CHECK(peer.Ping(43));
    server.join();
    BOOST_CHECK(answered);
}

BOOST_AUTO_TEST_CASE(consensus_message_serialization_and_p2p_exchange)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 7;
    const auto keypair = cybou::GenerateValidatorKeyPair(seed).value();
    const auto val_id = cybou::ComputeValidatorId(keypair.public_key);
    const uint256 network = uint256::ONE;

    // 1. Prevote serialization
    const uint256 pv_digest = cybou::ComputePrevoteDigest(network, 10, 2, val_id, uint256::ONE);
    cybou::BftPrevoteMsg pv{
        .network_id = network,
        .height = 10,
        .round = 2,
        .validator_id = val_id,
        .block_id = uint256::ONE,
        .signature = *cybou::SignValidatorVote(seed, pv_digest),
    };
    const auto enc_pv = cybou::SerializeBftPrevoteMsg(pv);
    BOOST_REQUIRE(enc_pv.has_value());
    const auto dec_pv = cybou::DeserializeBftPrevoteMsg(*enc_pv);
    BOOST_REQUIRE(dec_pv.has_value());
    BOOST_CHECK(*dec_pv == pv);

    // Prevote nil serialization
    const uint256 pv_nil_digest = cybou::ComputePrevoteDigest(network, 10, 2, val_id, std::nullopt);
    cybou::BftPrevoteMsg pv_nil{
        .network_id = network,
        .height = 10,
        .round = 2,
        .validator_id = val_id,
        .block_id = std::nullopt,
        .signature = *cybou::SignValidatorVote(seed, pv_nil_digest),
    };
    const auto enc_pv_nil = cybou::SerializeBftPrevoteMsg(pv_nil);
    BOOST_REQUIRE(enc_pv_nil.has_value());
    const auto dec_pv_nil = cybou::DeserializeBftPrevoteMsg(*enc_pv_nil);
    BOOST_REQUIRE(dec_pv_nil.has_value());
    BOOST_CHECK(*dec_pv_nil == pv_nil);

    // 2. Precommit serialization
    cybou::BftPrecommitMsg pc{
        .network_id = network,
        .height = 10,
        .round = 2,
        .validator_id = val_id,
        .block_id = uint256::ONE,
        .signature = *cybou::SignValidatorVote(seed, pv_digest),
    };
    const auto enc_pc = cybou::SerializeBftPrecommitMsg(pc);
    BOOST_REQUIRE(enc_pc.has_value());
    const auto dec_pc = cybou::DeserializeBftPrecommitMsg(*enc_pc);
    BOOST_REQUIRE(dec_pc.has_value());
    BOOST_CHECK(*dec_pc == pc);

    // 3. Proposal serialization
    cybou::CybouBlock block;
    block.height = 10;
    block.parent_block_id = uint256::ONE;
    block.resulting_state_root = uint256::ONE;
    const uint256 block_id = cybou::ComputeBlockId(block);
    const uint256 prop_digest = cybou::ComputeProposalDigest(network, 10, 2, val_id, block_id);
    cybou::BftProposalMsg prop{
        .network_id = network,
        .height = 10,
        .round = 2,
        .proposer_id = val_id,
        .block = block,
        .signature = *cybou::SignValidatorVote(seed, prop_digest),
    };
    const auto enc_prop = cybou::SerializeBftProposalMsg(prop);
    BOOST_REQUIRE(enc_prop.has_value());
    const auto dec_prop = cybou::DeserializeBftProposalMsg(*enc_prop);
    BOOST_REQUIRE(dec_prop.has_value());
    BOOST_CHECK(*dec_prop == prop);

    // 4. Over-the-wire P2P exchange over bidirectional PeerSession
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    tcp::acceptor acceptor{io, tcp::endpoint{boost::asio::ip::address_v4::loopback(), 0}};
    bool server_received_all{false};

    std::jthread server{[&] {
        tcp::socket socket{io};
        acceptor.accept(socket);
        cybou::p2p::PeerSession peer{std::move(socket)};
        BOOST_REQUIRE(peer.Handshake({.network_id = network, .finalized_height = 0,
            .finalized_tip = uint256::ONE, .capabilities = cybou::p2p::CAP_CONSENSUS, .nonce = 1001}));
        const auto r_prop = peer.ReadProposal();
        const auto r_pv = peer.ReadPrevote();
        const auto r_pc = peer.ReadPrecommit();
        server_received_all = r_prop && *r_prop == prop &&
                              r_pv && *r_pv == pv &&
                              r_pc && *r_pc == pc;
    }};

    tcp::socket socket{io};
    socket.connect(acceptor.local_endpoint());
    cybou::p2p::PeerSession client{std::move(socket)};
    BOOST_REQUIRE(client.Handshake({.network_id = network, .finalized_height = 0,
        .finalized_tip = uint256::ONE, .capabilities = cybou::p2p::CAP_CONSENSUS, .nonce = 1002}));
    BOOST_CHECK(client.SendProposal(prop));
    BOOST_CHECK(client.SendPrevote(pv));
    BOOST_CHECK(client.SendPrecommit(pc));

    server.join();
    BOOST_CHECK(server_received_all);
}

BOOST_AUTO_TEST_SUITE_END()
