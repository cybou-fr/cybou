// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/inbound_server.h>
#include <cybou/p2p/session.h>
#include <cybou/p2p/peer_manager.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/asio.hpp>
#include <boost/test/unit_test.hpp>

#include <atomic>
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

namespace {

void CheckSocketFinalityAfterValidatorRestart(bool after_precommit)
{
    using namespace std::chrono_literals;
    using boost::asio::ip::tcp;
    std::array<std::array<unsigned char, 32>, 4> seeds{};
    std::array<cybou::IdentityHybridPublicKey, 4> keys{};
    for (size_t i = 0; i < 4; ++i) {
        seeds[i][0] = static_cast<unsigned char>(0xb1 + i);
        const auto key = cybou::GenerateValidatorKeyPair(seeds[i]);
        BOOST_REQUIRE(key);
        keys[i] = key->public_key;
    }
    const auto genesis = cybou::CreateDevGenesisState(std::span<const cybou::IdentityHybridPublicKey>{keys});
    BOOST_REQUIRE(genesis);
    const auto definition = cybou::CreateDevNetworkDefinition(*genesis);
    const auto base = std::filesystem::temp_directory_path() /
        ("cybou-bft-restart-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(base);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{base};
    std::array<size_t, 4> seed_for_validator{};
    std::array<std::unique_ptr<cybou::CybouNodeRuntime>, 4> nodes;
    auto make_node = [&](size_t index, bool wipe) {
        cybou::NodeRuntimeConfig config{
            .network_definition = definition,
            .data_dir = base / ("node-" + std::to_string(index)),
            .validator_private_key = seeds[seed_for_validator[index]],
            .memory_only = false,
            .wipe_data = wipe,
        };
        return std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
    };
    for (size_t i = 0; i < 4; ++i) {
        size_t key_index = 0;
        while (key_index < 4 && genesis->validator_set.validators[i].consensus_public_key != keys[key_index])
            ++key_index;
        BOOST_REQUIRE(key_index < 4);
        seed_for_validator[i] = key_index;
        nodes[i] = make_node(i, true);
        BOOST_REQUIRE(nodes[i]->InitializeGenesis(*genesis));
    }

    boost::asio::io_context io;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    std::array<std::unique_ptr<cybou::p2p::InboundPeerServer>, 4> servers;
    std::array<std::unique_ptr<cybou::p2p::PeerManager>, 4> peers;
    std::atomic_bool stop{false};
    std::vector<std::jthread> listeners;
    struct StopGuard { std::atomic_bool& flag; ~StopGuard() { flag = true; } } guard{stop};
    for (size_t i = 1; i < 4; ++i) {
        servers[i] = std::make_unique<cybou::p2p::InboundPeerServer>(*nodes[i], io, tcp::endpoint{loopback, 0});
        peers[i] = std::make_unique<cybou::p2p::PeerManager>(*nodes[i]);
        listeners.emplace_back([&, i] { servers[i]->Run(stop); });
    }
    for (size_t i = 1; i < 4; ++i) {
        for (size_t j = 1; j < 4; ++j) {
            if (i != j) BOOST_REQUIRE(peers[i]->Connect("127.0.0.1", servers[j]->Port()));
        }
    }

    const auto leader = cybou::BftLeaderIndex(1, 0, 4);
    BOOST_REQUIRE(leader != 0);
    const auto proposal = nodes[leader]->ProposeConsensusBlock(0);
    BOOST_REQUIRE(proposal);
    std::array<cybou::BftPrevoteMsg, 4> prevotes;
    for (size_t i = 0; i < 4; ++i) {
        const auto vote = nodes[i]->ReceiveConsensusProposal(*proposal);
        BOOST_REQUIRE(vote);
        prevotes[i] = *vote;
    }
    if (after_precommit) {
        std::optional<cybou::BftPrecommitMsg> signed_precommit;
        for (size_t i = 1; i < 4; ++i) {
            if (const auto vote = nodes[0]->ReceiveConsensusPrevote(prevotes[i])) signed_precommit = vote;
        }
        BOOST_REQUIRE(signed_precommit);
        BOOST_REQUIRE(signed_precommit->block_id);
    }
    nodes[0].reset();
    nodes[0] = make_node(0, false);
    BOOST_REQUIRE(nodes[0]->InitializeGenesis(*genesis));
    BOOST_CHECK(!nodes[0]->ReceiveConsensusProposal(*proposal));
    BOOST_CHECK_EQUAL(nodes[0]->GetFinalizedHeight().value_or(99), 0U);

    bool finalized{false};
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (std::chrono::steady_clock::now() < deadline) {
        for (size_t i = 1; i < 4; ++i) nodes[i]->DrainConsensusMessages(*peers[i]);
        finalized = true;
        for (size_t i = 1; i < 4; ++i) {
            if (nodes[i]->GetFinalizedHeight().value_or(0) != 1) finalized = false;
        }
        if (finalized) break;
        std::this_thread::sleep_for(10ms);
    }
    BOOST_REQUIRE(finalized);
    cybou::p2p::PeerManager catchup{*nodes[0]};
    BOOST_REQUIRE(catchup.Connect("127.0.0.1", servers[1]->Port()));
    const auto sync = catchup.SyncFromPeer("127.0.0.1", servers[1]->Port(), 1);
    BOOST_CHECK_EQUAL(sync.blocks_applied, 1U);
    BOOST_CHECK_EQUAL(nodes[0]->GetFinalizedHeight().value_or(0), 1U);
    BOOST_CHECK(nodes[0]->ProposeConsensusBlock(2).has_value());
}

} // namespace

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
            .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 1000 + i}));
        clients.push_back(std::move(client));
    }
    BOOST_CHECK(clients.back()->Ping(77));
    BOOST_CHECK(clients.front()->Ping(78));

    tcp::socket excess_socket{io};
    excess_socket.connect(tcp::endpoint{loopback, server.Port()});
    cybou::p2p::PeerSession excess{std::move(excess_socket)};
    BOOST_CHECK(!excess.Handshake({.network_id = network, .finalized_height = 0,
        .finalized_tip = fixture.definition.genesis_block_id, .capabilities = 0, .nonce = 2000}));
}

BOOST_AUTO_TEST_CASE(listener_closes_peer_with_conflicting_finalized_tip)
{
    CybouServiceTestFixture fixture;
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    boost::asio::io_context io;
    using boost::asio::ip::tcp;
    const auto loopback = boost::asio::ip::address_v4::loopback();
    cybou::p2p::InboundPeerServer server{*fixture.runtime, io, tcp::endpoint{loopback, 0}};
    std::atomic_bool stop{false};
    std::jthread listener{[&] { server.Run(stop); }};
    struct StopGuard { std::atomic_bool& stop; ~StopGuard() { stop = true; } } guard{stop};
    const auto network = fixture.runtime->GetNetworkId();
    const auto wrong_tip = fixture.definition.genesis_block_id == uint256::ONE ?
        *uint256::FromUserHex("02") : uint256::ONE;

    tcp::socket wrong_socket{io};
    wrong_socket.connect(tcp::endpoint{loopback, server.Port()});
    cybou::p2p::PeerSession wrong{std::move(wrong_socket)};
    BOOST_REQUIRE(wrong.Handshake({.network_id = network, .finalized_height = 0,
        .finalized_tip = wrong_tip, .capabilities = 0, .nonce = 3000}));
    BOOST_CHECK(!wrong.Ping(1));

    tcp::socket conflicting_socket{io};
    conflicting_socket.connect(tcp::endpoint{loopback, server.Port()});
    cybou::p2p::PeerSession conflicting{std::move(conflicting_socket)};
    BOOST_REQUIRE(conflicting.Handshake({.network_id = network, .finalized_height = 1,
        .finalized_tip = wrong_tip, .capabilities = 0, .nonce = 3001}));
    BOOST_CHECK(!conflicting.Ping(2));

    tcp::socket good_socket{io};
    good_socket.connect(tcp::endpoint{loopback, server.Port()});
    cybou::p2p::PeerSession good{std::move(good_socket)};
    BOOST_REQUIRE(good.Handshake({.network_id = network, .finalized_height = 1,
        .finalized_tip = *fixture.runtime->GetFinalizedTip(), .capabilities = 0, .nonce = 3002}));
    BOOST_CHECK(good.Ping(3));
}

BOOST_AUTO_TEST_CASE(socket_consensus_rotates_past_offline_leader)
{
    using namespace std::chrono_literals;
    using boost::asio::ip::tcp;
    std::array<std::array<unsigned char, 32>, 4> seeds{};
    std::array<cybou::IdentityHybridPublicKey, 4> keys{};
    for (size_t i = 0; i < 4; ++i) {
        seeds[i][0] = static_cast<unsigned char>(0xa1 + i);
        const auto key = cybou::GenerateValidatorKeyPair(seeds[i]);
        BOOST_REQUIRE(key);
        keys[i] = key->public_key;
    }
    const auto genesis = cybou::CreateDevGenesisState(std::span<const cybou::IdentityHybridPublicKey>{keys});
    BOOST_REQUIRE(genesis);
    const auto definition = cybou::CreateDevNetworkDefinition(*genesis);
    const auto offline = cybou::BftLeaderIndex(1, 0, 4);
    std::array<std::unique_ptr<cybou::CybouNodeRuntime>, 4> nodes;
    for (size_t i = 0; i < 4; ++i) {
        if (i == offline) continue;
        size_t key_index = 0;
        while (key_index < 4 && genesis->validator_set.validators[i].consensus_public_key != keys[key_index]) ++key_index;
        BOOST_REQUIRE(key_index < 4);
        cybou::NodeRuntimeConfig config{
            .network_definition = definition,
            .data_dir = std::filesystem::temp_directory_path() / ("cybou-socket-bft-" + std::to_string(i)),
            .validator_private_key = seeds[key_index],
            .memory_only = true,
            .wipe_data = true,
        };
        nodes[i] = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
        BOOST_REQUIRE(nodes[i]->InitializeGenesis(*genesis));
    }
    boost::asio::io_context io;
    std::array<std::unique_ptr<cybou::p2p::InboundPeerServer>, 4> servers;
    std::array<std::unique_ptr<cybou::p2p::PeerManager>, 4> peers;
    std::atomic_bool stop{false};
    std::vector<std::jthread> listeners;
    struct StopGuard { std::atomic_bool& flag; ~StopGuard() { flag = true; } } guard{stop};
    const auto loopback = boost::asio::ip::address_v4::loopback();
    for (size_t i = 0; i < 4; ++i) {
        if (!nodes[i]) continue;
        servers[i] = std::make_unique<cybou::p2p::InboundPeerServer>(*nodes[i], io, tcp::endpoint{loopback, 0});
        peers[i] = std::make_unique<cybou::p2p::PeerManager>(*nodes[i]);
        listeners.emplace_back([&, i] { servers[i]->Run(stop); });
    }
    for (size_t i = 0; i < 4; ++i) {
        if (!nodes[i]) continue;
        for (size_t j = 0; j < 4; ++j) {
            if (i != j && nodes[j]) BOOST_REQUIRE(peers[i]->Connect("127.0.0.1", servers[j]->Port()));
        }
    }
    // Every live validator begins round 0, then its local timeout advances it.
    for (auto& node : nodes) if (node) node->TickConsensus(1ms);
    for (int phase = 0; phase < 3; ++phase) {
        std::this_thread::sleep_for(3ms);
        for (auto& node : nodes) if (node) node->TickConsensus(1ms);
    }

    const auto deadline = std::chrono::steady_clock::now() + 2s;
    bool finalized{false};
    while (std::chrono::steady_clock::now() < deadline) {
        for (size_t i = 0; i < 4; ++i) if (nodes[i]) nodes[i]->DrainConsensusMessages(*peers[i]);
        finalized = true;
        for (const auto& node : nodes) if (node && node->GetFinalizedHeight().value_or(0) != 1) finalized = false;
        if (finalized) break;
        std::this_thread::sleep_for(10ms);
    }
    BOOST_CHECK(finalized);
    for (const auto& node : nodes) {
        if (!node) continue;
        const auto block = node->GetBlockAtHeight(1);
        BOOST_REQUIRE(block);
        BOOST_CHECK_EQUAL(block->certificate.round, 1U);
        BOOST_CHECK_EQUAL(block->certificate.commit_votes.size(), 3U);
    }

    // Rejoin the missing validator from the verified block, reconnect a
    // surviving outbound peer, and finalize the next height through sockets.
    const auto first_block = nodes[(offline + 1) % 4]->GetBlockAtHeight(1);
    BOOST_REQUIRE(first_block);
    size_t recovered_key = 0;
    while (recovered_key < 4 &&
        genesis->validator_set.validators[offline].consensus_public_key != keys[recovered_key]) ++recovered_key;
    BOOST_REQUIRE(recovered_key < 4);
    cybou::NodeRuntimeConfig recovered_config{
        .network_definition = definition,
        .data_dir = std::filesystem::temp_directory_path() / "cybou-socket-bft-recovered",
        .validator_private_key = seeds[recovered_key],
        .memory_only = true,
        .wipe_data = true,
    };
    nodes[offline] = std::make_unique<cybou::CybouNodeRuntime>(std::move(recovered_config));
    BOOST_REQUIRE(nodes[offline]->InitializeGenesis(*genesis));
    BOOST_REQUIRE(nodes[offline]->CommitBlock(*first_block));
    servers[offline] = std::make_unique<cybou::p2p::InboundPeerServer>(
        *nodes[offline], io, tcp::endpoint{loopback, 0});
    peers[offline] = std::make_unique<cybou::p2p::PeerManager>(*nodes[offline]);
    listeners.emplace_back([&, offline] { servers[offline]->Run(stop); });
    const auto reconnecting = (offline + 1) % 4;
    peers[reconnecting]->DisconnectAll();
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            if (i == j) continue;
            const auto connected = peers[i]->Peers();
            const bool missing = std::none_of(connected.begin(), connected.end(),
                [&](const auto& peer) { return peer.port == servers[j]->Port(); });
            if (missing) BOOST_REQUIRE(peers[i]->Connect("127.0.0.1", servers[j]->Port()));
        }
    }
    for (auto& node : nodes) node->TickConsensus(1s);
    finalized = false;
    const auto second_deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < second_deadline) {
        for (size_t i = 0; i < 4; ++i) nodes[i]->DrainConsensusMessages(*peers[i]);
        finalized = true;
        for (const auto& node : nodes) if (node->GetFinalizedHeight().value_or(0) != 2) finalized = false;
        if (finalized) break;
        std::this_thread::sleep_for(10ms);
    }
    BOOST_CHECK(finalized);

    // Validator 0 equivocates on height 3. Duplicate and conflicting signed
    // prevotes travel over CYP2; the other three still form a clean quorum.
    const size_t faulty = 0;
    size_t faulty_key = 0;
    while (faulty_key < 4 &&
        genesis->validator_set.validators[faulty].consensus_public_key != keys[faulty_key]) ++faulty_key;
    BOOST_REQUIRE(faulty_key < 4);
    for (size_t i = 0; i < 4; ++i) {
        if (i == faulty) continue;
        peers[i]->DisconnectAll();
        for (size_t j = 0; j < 4; ++j) {
            if (j != i && j != faulty)
                BOOST_REQUIRE(peers[i]->Connect("127.0.0.1", servers[j]->Port()));
        }
    }
    for (size_t i = 0; i < 4; ++i) if (i != faulty) nodes[i]->TickConsensus(1s);
    const auto sign_faulty_vote = [&](const uint256& block_id) {
        const auto validator_id = genesis->validator_set.validators[faulty].validator_id;
        const auto digest = cybou::ComputePrevoteDigest(
            nodes[faulty]->GetNetworkId(), 3, 0, validator_id, block_id);
        return cybou::BftPrevoteMsg{
            .network_id = nodes[faulty]->GetNetworkId(),
            .height = 3,
            .round = 0,
            .validator_id = validator_id,
            .block_id = block_id,
            .signature = *cybou::SignValidatorVote(seeds[faulty_key], digest),
        };
    };
    const auto first_vote = sign_faulty_vote(uint256::ONE);
    const auto conflicting_vote = sign_faulty_vote(*uint256::FromUserHex("02"));
    for (size_t i = 0; i < 4; ++i) {
        if (i == faulty) continue;
        tcp::socket socket{io};
        socket.connect(tcp::endpoint{loopback, servers[i]->Port()});
        cybou::p2p::PeerSession adversary{std::move(socket)};
        const auto status = nodes[faulty]->GetStatus();
        BOOST_REQUIRE(adversary.Handshake({.network_id = status.network_id,
            .finalized_height = status.finalized_height, .finalized_tip = status.finalized_tip,
            .capabilities = cybou::p2p::CAP_CONSENSUS, .nonce = 5000 + i}));
        BOOST_REQUIRE(adversary.SendPrevote(first_vote));
        BOOST_REQUIRE(adversary.SendPrevote(first_vote));
        BOOST_REQUIRE(adversary.SendPrevote(conflicting_vote));
        BOOST_REQUIRE(adversary.Ping(6000 + i));
    }
    finalized = false;
    const auto third_deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < third_deadline) {
        for (size_t i = 0; i < 4; ++i) if (i != faulty) nodes[i]->DrainConsensusMessages(*peers[i]);
        finalized = true;
        for (size_t i = 0; i < 4; ++i) {
            if (i != faulty && nodes[i]->GetFinalizedHeight().value_or(0) != 3) finalized = false;
        }
        if (finalized) break;
        std::this_thread::sleep_for(10ms);
    }
    BOOST_CHECK(finalized);
    BOOST_CHECK_EQUAL(nodes[faulty]->GetFinalizedHeight().value_or(0), 2U);
    for (size_t i = 0; i < 4; ++i) {
        if (i == faulty) continue;
        const auto block = nodes[i]->GetBlockAtHeight(3);
        BOOST_REQUIRE(block);
        BOOST_CHECK_EQUAL(block->certificate.commit_votes.size(), 3U);
    }
}

BOOST_AUTO_TEST_CASE(socket_finality_after_prevote_restart)
{
    CheckSocketFinalityAfterValidatorRestart(false);
}

BOOST_AUTO_TEST_CASE(socket_finality_after_precommit_restart)
{
    CheckSocketFinalityAfterValidatorRestart(true);
}

BOOST_AUTO_TEST_SUITE_END()
