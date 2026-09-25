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

BOOST_AUTO_TEST_SUITE_END()
