// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <chainparamsbase.h>
#include <clientversion.h>
#include <consensus/consensus.h>
#include <kernel/chainparams.h>
#include <pow.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_chainparams_tests)

BOOST_AUTO_TEST_CASE(dev_network_is_isolated)
{
    const auto params = CChainParams::Main();

    BOOST_CHECK_EQUAL(params->GetDefaultPort(), 29444);
    BOOST_CHECK_EQUAL(params->GetNetworkDisplayName(), "CYBOU-DEV");
    BOOST_CHECK_EQUAL(CreateBaseChainParams(ChainType::MAIN)->RPCPort(), 29443);
    BOOST_CHECK(params->DNSSeeds().empty());
    BOOST_CHECK(params->FixedSeeds().empty());
    BOOST_CHECK_EQUAL(params->Bech32HRP(), "cyb");
    BOOST_CHECK(params->GetAvailableSnapshotHeights().empty());

    const MessageStartChars expected_magic{0xac, 0x9e, 0x78, 0x64};
    BOOST_CHECK(params->MessageStart() == expected_magic);
    BOOST_CHECK_EQUAL(
        params->GetConsensus().hashGenesisBlock.GetHex(),
        "189801dbef446c3d45b89b5a0e0a30e8b50da8447c4cee454dc8e93f96515327");
    BOOST_CHECK_EQUAL(
        params->GenesisBlock().hashMerkleRoot.GetHex(),
        "a1cc066afc59f4a9b219a94f5e185772260ac1af43a4508bfc06acc61da91cb2");
}

BOOST_AUTO_TEST_CASE(product_identity_is_cybou)
{
    BOOST_CHECK_EQUAL(CLIENT_VERSION, 1);
    BOOST_CHECK_EQUAL(FormatFullVersion(), "v0.0.1");
    BOOST_CHECK_EQUAL(UA_NAME, "CYBOU");
    BOOST_CHECK_EQUAL(FormatSubVersion(UA_NAME, CLIENT_VERSION, {}), "/CYBOU:0.0.1/");
}

BOOST_AUTO_TEST_CASE(fixed_difficulty_rejects_every_nbits_change)
{
    const auto params{CChainParams::Main()};
    const auto& consensus{params->GetConsensus()};
    const uint32_t bits{params->GenesisBlock().nBits};

    BOOST_REQUIRE(consensus.fPowNoRetargeting);
    BOOST_CHECK(PermittedDifficultyTransition(consensus, consensus.DifficultyAdjustmentInterval(), bits, bits));
    BOOST_CHECK(!PermittedDifficultyTransition(consensus, consensus.DifficultyAdjustmentInterval(), bits, bits - 1));
    BOOST_CHECK(!PermittedDifficultyTransition(consensus, consensus.DifficultyAdjustmentInterval() + 1, bits, bits + 1));
}

BOOST_FIXTURE_TEST_CASE(deterministic_100_block_fixture, TestChain100Setup)
{
    LOCK(::cs_main);
    BOOST_REQUIRE_EQUAL(m_node.chainman->ActiveChain().Height(), COINBASE_MATURITY);
    BOOST_CHECK_EQUAL(
        m_node.chainman->ActiveChain().Tip()->GetBlockHash().GetHex(),
        "3bc6d2c27c8d18621daf8adf75568adf0cdcca846f712b0da42c82970e875fa5");
}

BOOST_AUTO_TEST_SUITE_END()
