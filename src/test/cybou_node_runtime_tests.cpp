// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(cybou_node_runtime_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(runtime_finalizes_account_and_observer_verifies_block)
{
    CybouServiceTestFixture fixture;
    const auto status = fixture.runtime->GetStatus();
    BOOST_CHECK(status.is_initialized);
    BOOST_CHECK(status.is_authority);
    BOOST_CHECK_EQUAL(status.validator_count, 1U);
    const auto alice = fixture.CreateIdentity("alice.cybou");
    const auto account = alice->GetAccountId();
    BOOST_REQUIRE(account);
    BOOST_CHECK(fixture.runtime->GetAccountState(*account));
    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);

    cybou::NodeRuntimeConfig observer_config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "observer",
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime observer{std::move(observer_config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    BOOST_CHECK(!observer.GetStatus().is_authority);
    BOOST_REQUIRE(observer.CommitBlock(*block));
    BOOST_CHECK(observer.GetAccountState(*account) == fixture.runtime->GetAccountState(*account));
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(0), 1);
    BOOST_REQUIRE_EQUAL(block->block.operations.size(), 1U);
    const auto op_id = cybou::ComputeOperationId(block->block.operations.front());
    BOOST_REQUIRE(op_id);
    const auto found = observer.FindFinalizedOperation(*op_id);
    BOOST_CHECK(found.status == cybou::FinalizedOperationLookupStatus::FOUND);
    BOOST_CHECK_EQUAL(found.height, 1U);
    BOOST_CHECK_EQUAL(found.operation_index, 0U);
    BOOST_CHECK(found.block_id == cybou::ComputeBlockId(block->block));
    const auto missing_id = *op_id == uint256::ONE ? *uint256::FromUserHex("02") : uint256::ONE;
    const auto missing = observer.FindFinalizedOperation(missing_id);
    BOOST_CHECK(missing.status == cybou::FinalizedOperationLookupStatus::NOT_FOUND);
    BOOST_CHECK_EQUAL(missing.scanned_height, 1U);
}

BOOST_AUTO_TEST_CASE(runtime_rejects_foreign_genesis_and_block)
{
    CybouServiceTestFixture fixture;
    std::array<unsigned char, 32> foreign_seed{};
    foreign_seed[0] = 0x41;
    const auto foreign_pair = cybou::GenerateValidatorKeyPair(foreign_seed);
    BOOST_REQUIRE(foreign_pair);
    const auto foreign_genesis = cybou::CreateDevGenesisState(foreign_pair->public_key);
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "foreign-observer",
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_CHECK(!observer.InitializeGenesis(foreign_genesis));
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    const auto foreign_definition = cybou::CreateDevNetworkDefinition(foreign_genesis);
    cybou::NodeRuntimeConfig foreign_config{
        .network_definition = foreign_definition,
        .data_dir = fixture.directory / "foreign-producer",
        .validator_private_key = foreign_seed,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime foreign{std::move(foreign_config)};
    BOOST_REQUIRE(foreign.InitializeGenesis(foreign_genesis));
    const auto block = foreign.ProduceBlock();
    BOOST_REQUIRE(block);
    BOOST_CHECK(!observer.CommitBlock(*block));
    BOOST_CHECK_EQUAL(observer.GetFinalizedHeight().value_or(99), 0);
}

BOOST_AUTO_TEST_CASE(runtime_explicit_peers_take_priority_over_discovered)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "peer-priority",
        .local_p2p_endpoint = std::make_pair("127.0.0.1", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    // Operator-approved validator endpoints.
    runtime.SetExplicitPeerEndpoints({{"10.0.0.10", 8333}, {"10.0.0.11", 8333}});
    // Malicious flood: lexicographically smaller addresses that would eclipse
    // the validator topology in a single sorted set, plus this node's own
    // listener, plus out-of-scope targets.
    std::vector<std::pair<std::string, uint16_t>> flood;
    for (int i = 1; i <= 40; ++i) {
        flood.emplace_back("1.1.1." + std::to_string(i), 7000);
    }
    flood.emplace_back("127.0.0.1", 29001); // own listener
    flood.emplace_back("127.0.0.1", 29002); // own address, other port
    flood.emplace_back("0.0.0.0", 8333);    // unspecified
    flood.emplace_back("224.0.0.1", 8333);  // multicast
    flood.emplace_back("169.254.1.1", 8333); // link-local
    runtime.AddDiscoveredPeerEndpoints(flood);

    const auto gossip = runtime.GetPeerEndpointsForGossip();
    // Capped at 32 targets: explicit peers first, discovered flood behind them.
    BOOST_REQUIRE_EQUAL(gossip.size(), 32U);
    // Explicit validator peers always come first, in front of any discovered
    // lexicographically-smaller hint.
    BOOST_CHECK_EQUAL(gossip[0].first, "10.0.0.10");
    BOOST_CHECK_EQUAL(gossip[1].first, "10.0.0.11");
    BOOST_CHECK_EQUAL(gossip[0].second, 8333);
    // Flood addresses are present but strictly behind the explicit peers.
    for (size_t i = 2; i < gossip.size(); ++i) {
        BOOST_CHECK(gossip[i].first.rfind("1.1.1.", 0) == 0);
    }
}

BOOST_AUTO_TEST_CASE(runtime_discovery_filters_self_and_out_of_scope_addresses)
{
    CybouServiceTestFixture fixture;
    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "peer-policy",
        .local_p2p_endpoint = std::make_pair("203.0.113.5", uint16_t{29001}),
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime runtime{std::move(config)};
    BOOST_REQUIRE(runtime.InitializeGenesis(fixture.genesis));

    // With a public listener, loopback/link-local/unspecified/multicast
    // discovered targets must not be dialed (SSRF-style pivot).
    runtime.AddDiscoveredPeerEndpoints({
        {"203.0.113.5", 29001},   // own listener: always rejected
        {"127.0.0.1", 8333},      // loopback rejected: listener is public
        {"::1", 8333},            // v6 loopback rejected
        {"169.254.10.20", 8333},  // v4 link-local rejected
        {"fe80::1", 8333},        // v6 link-local rejected
        {"0.0.0.0", 8333},        // unspecified rejected
        {"::", 8333},             // unspecified v6 rejected
        {"224.0.0.1", 8333},      // multicast rejected
        {"198.51.100.7", 8333},   // public: accepted
    });
    const auto gossip = runtime.GetPeerEndpointsForGossip();
    BOOST_REQUIRE_EQUAL(gossip.size(), 1U);
    BOOST_CHECK_EQUAL(gossip[0].first, "198.51.100.7");
}

BOOST_AUTO_TEST_SUITE_END()
