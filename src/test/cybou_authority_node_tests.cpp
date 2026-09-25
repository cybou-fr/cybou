// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>
#include <cybou/bft.h>
#include <test/cybou_service_test_fixture.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(cybou_authority_node_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(producer_requires_matching_hybrid_validator_key)
{
    CybouServiceTestFixture fixture;
    auto wrong_seed = fixture.validator_seed;
    wrong_seed[0] ^= 0xff;
    cybou::CybouAuthorityNode wrong{fixture.runtime->GetStore(), wrong_seed};
    BOOST_CHECK(wrong.ProduceNextBlock().error == cybou::AuthorityProductionError::VALIDATOR_KEY_MISMATCH);
    BOOST_CHECK_EQUAL(fixture.runtime->GetFinalizedHeight().value_or(99), 0);

    cybou::CybouAuthorityNode producer{fixture.runtime->GetStore(), fixture.validator_seed};
    const auto first = producer.ProduceNextBlock();
    BOOST_REQUIRE(first);
    BOOST_REQUIRE(first.finalized_block);
    BOOST_CHECK_EQUAL(first.finalized_block->block.height, 1);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(first.finalized_block->certificate,
        fixture.genesis.validator_set, fixture.runtime->GetNetworkId()) ==
        cybou::FinalityVerificationError::NONE);
    BOOST_CHECK(fixture.runtime->GetBlockAtHeight(1) == first.finalized_block);
    const auto second = producer.ProduceNextBlock();
    BOOST_REQUIRE(second);
    BOOST_CHECK_EQUAL(fixture.runtime->GetFinalizedHeight().value_or(0), 2);
}

BOOST_AUTO_TEST_CASE(producer_rejects_finalized_operation_replay)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("authority-account.cybou");
    const auto block = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(block);
    BOOST_REQUIRE_EQUAL(block->block.operations.size(), 1U);
    cybou::CybouAuthorityNode producer{fixture.runtime->GetStore(), fixture.validator_seed};
    const auto& finalized_op = block->block.operations.front();
    const auto finalized_id = cybou::ComputeOperationId(finalized_op);
    BOOST_REQUIRE(finalized_id);
    BOOST_CHECK(fixture.runtime->GetStore().HasIndexedFinalizedOperation(*finalized_id));
    BOOST_CHECK(producer.SubmitOperationWithStatus(finalized_op) == cybou::OperationSubmitStatus::ALREADY_FINALIZED);
    auto conflicting_op = finalized_op;
    std::get<cybou::AccountCreateOp>(conflicting_op).work.nonce ^= 1;
    const auto conflicting_id = cybou::ComputeOperationId(conflicting_op);
    BOOST_REQUIRE(conflicting_id);
    BOOST_CHECK(!fixture.runtime->GetStore().HasIndexedFinalizedOperation(*conflicting_id));
    BOOST_CHECK(producer.SubmitOperationWithStatus(conflicting_op) == cybou::OperationSubmitStatus::REJECTED);
    BOOST_CHECK_EQUAL(producer.PendingCount(), 0U);
    const auto empty = producer.ProduceNextBlock();
    BOOST_REQUIRE(empty);
    BOOST_CHECK_EQUAL(empty.finalized_block->block.operations.size(), 0U);
}

BOOST_AUTO_TEST_CASE(operation_pool_bounds_peer_admission_and_revalidates)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("pool-account.cybou");
    const auto finalized = fixture.runtime->GetBlockAtHeight(1);
    BOOST_REQUIRE(finalized);
    BOOST_REQUIRE_EQUAL(finalized->block.operations.size(), 1U);
    const auto& operation = finalized->block.operations.front();
    const auto encoded = cybou::SerializeProtocolOperation(operation);
    BOOST_REQUIRE(encoded);

    cybou::NodeRuntimeConfig config{
        .network_definition = fixture.definition,
        .data_dir = fixture.directory / "pool-observer",
        .validator_private_key = fixture.validator_seed,
        .memory_only = true,
        .wipe_data = true,
    };
    cybou::CybouNodeRuntime observer{std::move(config)};
    BOOST_REQUIRE(observer.InitializeGenesis(fixture.genesis));
    cybou::OperationPool too_small{observer.GetStore(),
        {.max_count = 1, .max_bytes = encoded->size() - 1}};
    BOOST_CHECK(too_small.Admit(operation) == cybou::PoolAdmission::REJECTED);
    cybou::OperationPool peer_limited{observer.GetStore(),
        {.max_count = 1, .max_bytes = encoded->size(), .max_peer_count = 0}};
    BOOST_CHECK(peer_limited.Admit(operation, std::string{"peer-a"}) == cybou::PoolAdmission::REJECTED);
    BOOST_CHECK(peer_limited.Admit(operation) == cybou::PoolAdmission::ACCEPTED);
    BOOST_CHECK(peer_limited.Admit(operation, std::string{"peer-a"}) == cybou::PoolAdmission::ALREADY_PENDING);
    BOOST_CHECK_EQUAL(peer_limited.Size(), 1U);
    BOOST_CHECK_EQUAL(peer_limited.Bytes(), encoded->size());
    BOOST_CHECK(observer.SubmitPeerOperation(operation, "peer-b").status == cybou::OperationSubmitStatus::ACCEPTED);
    BOOST_CHECK(observer.SubmitOperation(operation).status == cybou::OperationSubmitStatus::ALREADY_PENDING);
    BOOST_REQUIRE(observer.CommitBlock(*finalized));
    peer_limited.Revalidate();
    BOOST_CHECK_EQUAL(peer_limited.Size(), 0U);
    BOOST_CHECK_EQUAL(peer_limited.Bytes(), 0U);
    BOOST_CHECK(peer_limited.Admit(operation) == cybou::PoolAdmission::ALREADY_FINALIZED);
    BOOST_CHECK(observer.SubmitOperation(operation).status == cybou::OperationSubmitStatus::ALREADY_FINALIZED);
    const auto next = observer.ProduceBlock();
    BOOST_REQUIRE(next);
    BOOST_CHECK(next->block.operations.empty());
}

BOOST_AUTO_TEST_CASE(n4_bft_distributed_consensus_with_fault_tolerance_and_catchup)
{
    // 1. Generate 4 distinct validator keypairs
    std::vector<std::array<unsigned char, 32>> seeds(4);
    std::vector<cybou::IdentityHybridPublicKey> pubkeys(4);
    for (size_t i = 0; i < 4; ++i) {
        seeds[i].fill(0);
        seeds[i][0] = static_cast<unsigned char>(0x91 + i * 7);
        const auto key = cybou::GenerateValidatorKeyPair(seeds[i]);
        BOOST_REQUIRE(key);
        pubkeys[i] = key->public_key;
    }

    const auto genesis_opt = cybou::CreateDevGenesisState(pubkeys);
    BOOST_REQUIRE(genesis_opt);
    const auto genesis = *genesis_opt;
    BOOST_REQUIRE_EQUAL(genesis.validator_set.validators.size(), 4U);
    BOOST_CHECK_EQUAL(genesis.validator_set.QuorumThreshold(), 3U);
    BOOST_CHECK_EQUAL(genesis.validator_set.FaultTolerance(), 1U);

    auto definition = cybou::CreateDevNetworkDefinition(genesis);
    definition.protocol_parameters.account_creation_work_bits = 0;

    const auto base_dir = std::filesystem::temp_directory_path() / "cybou-n4-bft-dist";
    std::filesystem::create_directories(base_dir);

    // 2. Start 4 nodes ordered by validator set index
    std::vector<std::unique_ptr<cybou::CybouNodeRuntime>> nodes(4);
    for (size_t v = 0; v < 4; ++v) {
        for (size_t i = 0; i < 4; ++i) {
            if (genesis.validator_set.validators[v].consensus_public_key == pubkeys[i]) {
                cybou::NodeRuntimeConfig config{
                    .network_definition = definition,
                    .data_dir = base_dir / ("node-" + std::to_string(v)),
                    .validator_private_key = seeds[i],
                    .memory_only = true,
                    .wipe_data = true,
                };
                auto rt = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
                BOOST_REQUIRE(rt->InitializeGenesis(genesis));
                nodes[v] = std::move(rt);
                break;
            }
        }
    }

    // 3. Height 1: Node 3 is OFFLINE (simulating 1 faulty node, f = 1)
    const std::vector<size_t> online = {0, 1, 2};
    const size_t leader_idx_1 = cybou::BftLeaderIndex(1, 0, 4);

    // Leader proposes block 1
    const auto proposal_1 = nodes[leader_idx_1]->ProposeConsensusBlock(0);
    BOOST_REQUIRE(proposal_1);
    BOOST_CHECK_EQUAL(proposal_1->height, 1U);

    // Online nodes receive proposal and generate prevotes
    std::vector<cybou::BftPrevoteMsg> prevotes_1;
    for (size_t idx : online) {
        const auto pv = nodes[idx]->ReceiveConsensusProposal(*proposal_1);
        BOOST_REQUIRE(pv);
        BOOST_CHECK(pv->block_id.has_value());
        prevotes_1.push_back(*pv);
    }
    BOOST_REQUIRE_EQUAL(prevotes_1.size(), 3U);

    // Online nodes exchange prevotes and generate precommits
    std::vector<cybou::BftPrecommitMsg> precommits_1;
    for (size_t idx : online) {
        std::optional<cybou::BftPrecommitMsg> pc;
        for (const auto& pv : prevotes_1) {
            auto res = nodes[idx]->ReceiveConsensusPrevote(pv);
            if (res) pc = res;
        }
        BOOST_REQUIRE(pc);
        BOOST_CHECK(pc->block_id.has_value());
        precommits_1.push_back(*pc);
    }
    BOOST_REQUIRE_EQUAL(precommits_1.size(), 3U);

    // Online nodes exchange precommits and finalize block 1
    for (size_t idx : online) {
        for (const auto& pc : precommits_1) {
            nodes[idx]->ReceiveConsensusPrecommit(pc);
        }
    }

    // Verify all 3 online nodes have finalized height 1
    for (size_t idx : online) {
        BOOST_CHECK_EQUAL(nodes[idx]->GetFinalizedHeight().value_or(0), 1U);
        const auto blk = nodes[idx]->GetBlockAtHeight(1);
        BOOST_REQUIRE(blk);
        BOOST_CHECK(cybou::VerifyFinalityCertificate(blk->certificate,
            genesis.validator_set, nodes[idx]->GetNetworkId()) == cybou::FinalityVerificationError::NONE);
        BOOST_CHECK_EQUAL(blk->certificate.commit_votes.size(), 3U); // 3 of 4 votes
    }

    // Node 3 is still at height 0 (offline)
    BOOST_CHECK_EQUAL(nodes[3]->GetFinalizedHeight().value_or(99), 0U);

    // 4. Node 3 recovers and catches up to height 1
    const auto block_1 = nodes[0]->GetBlockAtHeight(1);
    BOOST_REQUIRE(block_1);
    BOOST_CHECK(nodes[3]->CommitBlock(*block_1));
    BOOST_CHECK_EQUAL(nodes[3]->GetFinalizedHeight().value_or(0), 1U);

    // 5. Height 2: All 4 nodes are online!
    const size_t leader_idx_2 = cybou::BftLeaderIndex(2, 0, 4);
    const auto proposal_2 = nodes[leader_idx_2]->ProposeConsensusBlock(0);
    BOOST_REQUIRE(proposal_2);
    BOOST_CHECK_EQUAL(proposal_2->height, 2U);

    std::vector<cybou::BftPrevoteMsg> prevotes_2;
    for (size_t i = 0; i < 4; ++i) {
        const auto pv = nodes[i]->ReceiveConsensusProposal(*proposal_2);
        BOOST_REQUIRE(pv);
        BOOST_CHECK(pv->block_id.has_value());
        prevotes_2.push_back(*pv);
    }
    BOOST_REQUIRE_EQUAL(prevotes_2.size(), 4U);

    std::vector<cybou::BftPrecommitMsg> precommits_2;
    for (size_t i = 0; i < 4; ++i) {
        std::optional<cybou::BftPrecommitMsg> pc;
        for (const auto& pv : prevotes_2) {
            auto res = nodes[i]->ReceiveConsensusPrevote(pv);
            if (res) pc = res;
        }
        BOOST_REQUIRE(pc);
        BOOST_CHECK(pc->block_id.has_value());
        precommits_2.push_back(*pc);
    }
    BOOST_REQUIRE_EQUAL(precommits_2.size(), 4U);

    for (size_t i = 0; i < 4; ++i) {
        for (const auto& pc : precommits_2) {
            nodes[i]->ReceiveConsensusPrecommit(pc);
        }
    }

    // Verify all 4 nodes have finalized height 2
    for (size_t i = 0; i < 4; ++i) {
        BOOST_CHECK_EQUAL(nodes[i]->GetFinalizedHeight().value_or(0), 2U);
        const auto blk = nodes[i]->GetBlockAtHeight(2);
        BOOST_REQUIRE(blk);
        BOOST_CHECK(cybou::VerifyFinalityCertificate(blk->certificate,
            genesis.validator_set, nodes[i]->GetNetworkId()) == cybou::FinalityVerificationError::NONE);
    }
}

BOOST_AUTO_TEST_SUITE_END()
