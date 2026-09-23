// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>
#include <cybou/bft_engine.h>
#include <cybou/block.h>
#include <cybou/network_definition.h>
#include <cybou/signing.h>
#include <cybou/state_store.h>
#include <cybou/validator.h>
#include <dbwrapper.h>
#include <test/util/setup_common.h>
#include <tinyformat.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <vector>

namespace {

struct MockValidatorNode {
    uint256 validator_id;
    std::array<unsigned char, 32> seed{};
    uint256 consensus_pubkey;

    static MockValidatorNode Create(uint8_t index)
    {
        MockValidatorNode node;
        node.seed.fill(0);
        node.seed[0] = index;
        node.consensus_pubkey = cybou::DeriveEd25519PublicKey(node.seed).value();
        node.validator_id = uint256::FromUserHex(strprintf("%02x", 0xa0 + index)).value();
        return node;
    }

    cybou::BftCommitVoteV1 SignCommit(
        const uint256& network_id,
        const uint256& block_id,
        uint64_t height,
        const uint256& val_set_commitment) const
    {
        const uint256 digest = cybou::ComputeBftCommitDigest(network_id, block_id, height, val_set_commitment);
        cybou::BftCommitVoteV1 vote;
        vote.validator_id = validator_id;
        vote.signature = *cybou::SignValidatorVote(seed, digest);
        return vote;
    }
};

CDBWrapper MemoryDb()
{
    return CDBWrapper{{
        .path = "cybou-bft-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_bft_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(validator_set_invariants_and_serialization)
{
    std::vector<MockValidatorNode> nodes;
    nodes.reserve(4);
    for (uint8_t i = 1; i <= 4; ++i) {
        nodes.push_back(MockValidatorNode::Create(i));
    }

    cybou::ValidatorSetV1 val_set;
    for (const auto& node : nodes) {
        val_set.validators.push_back(cybou::ValidatorV1{
            .validator_id = node.validator_id,
            .consensus_public_key = node.consensus_pubkey,
            .weight = 1,
        });
    }

    BOOST_CHECK_EQUAL(val_set.Size(), 4);
    BOOST_CHECK_EQUAL(val_set.TotalWeight(), 4);
    BOOST_CHECK_EQUAL(val_set.FaultTolerance(), 1); // f = (4 - 1) / 3 = 1
    BOOST_CHECK_EQUAL(val_set.QuorumThreshold(), 3); // 2f + 1 = 3
    BOOST_CHECK(cybou::ValidateValidatorSet(val_set) == cybou::ValidatorSetValidationError::NONE);

    // Serialization & Deserialization
    const auto serialized = cybou::SerializeValidatorSet(val_set);
    const auto deserialized = cybou::DeserializeValidatorSet(serialized);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK(*deserialized == val_set);

    const uint256 commitment = cybou::ComputeValidatorSetCommitment(val_set);
    BOOST_CHECK(!commitment.IsNull());

    // Invariant violations:
    // 1. Less than MIN_VALIDATORS (0 validators)
    cybou::ValidatorSetV1 too_small = val_set;
    too_small.validators.clear();
    BOOST_CHECK(cybou::ValidateValidatorSet(too_small) == cybou::ValidatorSetValidationError::INVALID_VALIDATOR_COUNT);

    // 2. Weight != 1 (Hard rule: equal validator weight = 1)
    cybou::ValidatorSetV1 unequal_weight = val_set;
    unequal_weight.validators[0].weight = 2;
    BOOST_CHECK(cybou::ValidateValidatorSet(unequal_weight) == cybou::ValidatorSetValidationError::INVALID_WEIGHT);

    // 3. Null validator_id
    cybou::ValidatorSetV1 null_id = val_set;
    null_id.validators[0].validator_id = uint256{};
    BOOST_CHECK(cybou::ValidateValidatorSet(null_id) == cybou::ValidatorSetValidationError::NULL_VALIDATOR_ID);

    // 4. Duplicate validator_id
    cybou::ValidatorSetV1 dup_id = val_set;
    dup_id.validators[1].validator_id = dup_id.validators[0].validator_id;
    BOOST_CHECK(cybou::ValidateValidatorSet(dup_id) == cybou::ValidatorSetValidationError::DUPLICATE_VALIDATOR_ID);

    // 5. Duplicate consensus key
    cybou::ValidatorSetV1 dup_key = val_set;
    dup_key.validators[1].consensus_public_key = dup_key.validators[0].consensus_public_key;
    BOOST_CHECK(cybou::ValidateValidatorSet(dup_key) == cybou::ValidatorSetValidationError::DUPLICATE_CONSENSUS_KEY);
}

BOOST_AUTO_TEST_CASE(bft_finality_certificate_quorum_and_verification)
{
    const uint256 network_id{uint256::FromUserHex("42").value()};
    const uint256 block_1_id{uint256::FromUserHex("b1").value()};
    const uint64_t block_1_height{1};

    std::vector<MockValidatorNode> nodes;
    for (uint8_t i = 1; i <= 4; ++i) {
        nodes.push_back(MockValidatorNode::Create(i));
    }

    cybou::ValidatorSetV1 val_set;
    for (const auto& node : nodes) {
        val_set.validators.push_back(cybou::ValidatorV1{
            .validator_id = node.validator_id,
            .consensus_public_key = node.consensus_pubkey,
            .weight = 1,
        });
    }

    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    // 1. Unanimous 4/4 finality certificate
    cybou::BftFinalityCertificateV1 cert_4_of_4;
    cert_4_of_4.network_id = network_id;
    cert_4_of_4.block_id = block_1_id;
    cert_4_of_4.height = block_1_height;
    cert_4_of_4.validator_set_commitment = val_set_commitment;

    for (const auto& node : nodes) {
        cert_4_of_4.commit_votes.push_back(
            node.SignCommit(network_id, block_1_id, block_1_height, val_set_commitment));
    }

    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_4_of_4, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // Serialization & Deserialization
    const auto cert_bytes = cybou::SerializeFinalityCertificate(cert_4_of_4);
    const auto decoded_cert = cybou::DeserializeFinalityCertificate(cert_bytes);
    BOOST_REQUIRE(decoded_cert.has_value());
    BOOST_CHECK(*decoded_cert == cert_4_of_4);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(*decoded_cert, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // 2. 3/4 supermajority (1 faulty/byzantine node fails to vote)
    cybou::BftFinalityCertificateV1 cert_3_of_4 = cert_4_of_4;
    cert_3_of_4.commit_votes.pop_back();
    BOOST_CHECK_EQUAL(cert_3_of_4.commit_votes.size(), 3);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_3_of_4, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // 3. 2/4 votes (< quorum of 3)
    cybou::BftFinalityCertificateV1 cert_2_of_4 = cert_3_of_4;
    cert_2_of_4.commit_votes.pop_back();
    BOOST_CHECK_EQUAL(cert_2_of_4.commit_votes.size(), 2);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_2_of_4, val_set, network_id) ==
                cybou::FinalityVerificationError::INSUFFICIENT_VOTES);

    // 4. Duplicate vote attempt to forge quorum (validator 0 votes twice)
    cybou::BftFinalityCertificateV1 cert_dup_vote = cert_2_of_4;
    cert_dup_vote.commit_votes.push_back(cert_dup_vote.commit_votes[0]);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_dup_vote, val_set, network_id) ==
                cybou::FinalityVerificationError::DUPLICATE_VOTE);

    // 5. Unknown validator vote
    cybou::BftFinalityCertificateV1 cert_unknown = cert_3_of_4;
    cert_unknown.commit_votes[0].validator_id = uint256::FromUserHex("99").value();
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_unknown, val_set, network_id) ==
                cybou::FinalityVerificationError::UNKNOWN_VALIDATOR);

    // 6. Invalid / forged signature
    cybou::BftFinalityCertificateV1 cert_bad_sig = cert_3_of_4;
    cert_bad_sig.commit_votes[0].signature[5] ^= 0xFF;
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_bad_sig, val_set, network_id) ==
                cybou::FinalityVerificationError::INVALID_SIGNATURE);

    // 7. Wrong network ID
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_4_of_4, val_set, uint256::FromUserHex("9999").value()) ==
                cybou::FinalityVerificationError::NETWORK_MISMATCH);

    // 8. Mismatched validator set commitment
    cybou::BftFinalityCertificateV1 cert_bad_val_commit = cert_4_of_4;
    cert_bad_val_commit.validator_set_commitment = uint256::FromUserHex("feed").value();
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_bad_val_commit, val_set, network_id) ==
                cybou::FinalityVerificationError::VALIDATOR_SET_MISMATCH);
}

BOOST_AUTO_TEST_CASE(bft_consensus_round_to_state_store_execution)
{
    // Spin up 4 validators
    std::vector<MockValidatorNode> nodes;
    for (uint8_t i = 1; i <= 4; ++i) {
        nodes.push_back(MockValidatorNode::Create(i));
    }

    cybou::ValidatorSetV1 val_set;
    for (const auto& node : nodes) {
        val_set.validators.push_back(cybou::ValidatorV1{
            .validator_id = node.validator_id,
            .consensus_public_key = node.consensus_pubkey,
            .weight = 1,
        });
    }
    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    const cybou::CybouState genesis_state{
        .onboarding_pool = 100000,
        .security_reward_pool = 5000,
        .pending_fee_pool = 0,
        .accounts{},
    };

    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
        .epoch_blocks = 10,
    };

    const uint256 genesis_block_id{uint256::FromUserHex("1000").value()};

    const cybou::CybouNetworkDefinitionV1 definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = genesis_block_id,
        .genesis_state_root = cybou::CybouStateHash(genesis_state),
        .protocol_parameters = params,
        .initial_validator_set_commitment = val_set_commitment,
    };
    const uint256 network_id = cybou::NetworkId(definition);

    // Initialize StateStore
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(genesis_state));

    auto head = store.GetFinalizedHead();
    BOOST_REQUIRE(head.has_value());
    BOOST_CHECK_EQUAL(head->height, 0);
    BOOST_CHECK(head->block_id == genesis_block_id);

    // Round 1: Height 1 block proposal & BFT consensus with 3/4 votes
    std::vector<cybou::ProtocolOperationV1> block_1_ops;
    cybou::CybouBlockV1 block_1{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = genesis_block_id,
        .height = 1,
        .operations = block_1_ops,
        .resulting_state_root = cybou::CybouStateHash(genesis_state),
    };
    const uint256 block_1_id = cybou::ComputeBlockId(block_1);

    cybou::BftFinalityCertificateV1 cert_1;
    cert_1.network_id = network_id;
    cert_1.block_id = block_1_id;
    cert_1.height = 1;
    cert_1.validator_set_commitment = val_set_commitment;

    // Validators 1, 2, 3 vote commit
    for (size_t i = 0; i < 3; ++i) {
        cert_1.commit_votes.push_back(
            nodes[i].SignCommit(network_id, block_1_id, 1, val_set_commitment));
    }

    // Verify certificate before state commit
    BOOST_REQUIRE(cybou::VerifyFinalityCertificate(cert_1, val_set, network_id) ==
                  cybou::FinalityVerificationError::NONE);

    cybou::FinalizedBlockV1 finalized_1{
        .block = block_1,
        .certificate = cert_1,
    };

    // StateStore commit at finalized height 1
    BOOST_REQUIRE(store.CommitFinalizedBlock(finalized_1, val_set));
    head = store.GetFinalizedHead();
    BOOST_REQUIRE(head.has_value());
    BOOST_CHECK_EQUAL(head->height, 1);
    BOOST_CHECK(head->block_id == block_1_id);

    // Verify block was persisted and can be retrieved
    const auto loaded_b1 = store.GetBlock(block_1_id);
    BOOST_REQUIRE(loaded_b1.has_value());
    BOOST_CHECK(loaded_b1->block == block_1);
    BOOST_CHECK(loaded_b1->certificate == cert_1);

    // Round 2: Height 2 block proposal & BFT consensus with 4/4 unanimous votes
    std::vector<cybou::ProtocolOperationV1> block_2_ops;
    cybou::CybouBlockV1 block_2{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = block_1_id,
        .height = 2,
        .operations = block_2_ops,
        .resulting_state_root = cybou::CybouStateHash(genesis_state),
    };
    const uint256 block_2_id = cybou::ComputeBlockId(block_2);

    cybou::BftFinalityCertificateV1 cert_2;
    cert_2.network_id = network_id;
    cert_2.block_id = block_2_id;
    cert_2.height = 2;
    cert_2.validator_set_commitment = val_set_commitment;

    for (const auto& node : nodes) {
        cert_2.commit_votes.push_back(
            node.SignCommit(network_id, block_2_id, 2, val_set_commitment));
    }

    BOOST_REQUIRE(cybou::VerifyFinalityCertificate(cert_2, val_set, network_id) ==
                  cybou::FinalityVerificationError::NONE);

    cybou::FinalizedBlockV1 finalized_2{
        .block = block_2,
        .certificate = cert_2,
    };

    BOOST_REQUIRE(store.CommitFinalizedBlock(finalized_2, val_set));
    head = store.GetFinalizedHead();
    BOOST_REQUIRE(head.has_value());
    BOOST_CHECK_EQUAL(head->height, 2);
    BOOST_CHECK(head->block_id == block_2_id);
}

BOOST_AUTO_TEST_CASE(bft_state_machine_simulator_consensus_and_fault_tolerance)
{
    const uint256 network_id{uint256::FromUserHex("99").value()};
    cybou::BftSimulator sim{network_id};
    const uint256 state_root{uint256::FromUserHex("1111").value()};

    // Scenario 1: Unanimous consensus (all 4 nodes online)
    BOOST_CHECK(sim.StepRound(1, 0, {}, state_root));
    for (size_t i = 0; i < 4; ++i) {
        BOOST_CHECK(sim.Node(i).GetStep() == cybou::BftStep::FINALIZED);
        BOOST_REQUIRE(sim.Node(i).GetLatestFinalizedBlock().has_value());
    }

    // Advance to height 2
    for (size_t i = 0; i < 4; ++i) {
        sim.Node(i).SetHeight(2, cybou::ComputeBlockId(sim.Node(i).GetLatestFinalizedBlock()->block));
    }

    // Scenario 2: 1 node crash (f=1 fault tolerance with N=4, quorum=3)
    sim.SetNodeOnline(3, false); // node 3 crashed
    BOOST_CHECK(sim.StepRound(2, 0, {}, state_root));
    for (size_t i = 0; i < 3; ++i) {
        BOOST_CHECK(sim.Node(i).GetStep() == cybou::BftStep::FINALIZED);
    }
    // Node 3 was offline, didn't finalize
    BOOST_CHECK(sim.Node(3).GetStep() != cybou::BftStep::FINALIZED);

    // Restore node 3
    sim.SetNodeOnline(3, true);

    // Advance to height 3
    for (size_t i = 0; i < 4; ++i) {
        sim.Node(i).SetHeight(3, cybou::ComputeBlockId(sim.Node(0).GetLatestFinalizedBlock()->block));
    }

    // Scenario 3: 2 vs 2 network partition ({0, 1} vs {2, 3})
    // Neither partition can reach quorum (3 votes needed), block cannot finalize!
    sim.SetPartition({0, 1}, {2, 3});
    BOOST_CHECK(!sim.StepRound(3, 0, {}, state_root));
    for (size_t i = 0; i < 4; ++i) {
        BOOST_CHECK(sim.Node(i).GetStep() != cybou::BftStep::FINALIZED);
    }

    // Scenario 4: Heal network partition
    sim.ClearPartition();
    BOOST_CHECK(sim.StepRound(3, 0, {}, state_root));
    for (size_t i = 0; i < 4; ++i) {
        BOOST_CHECK(sim.Node(i).GetStep() == cybou::BftStep::FINALIZED);
    }
}

BOOST_AUTO_TEST_CASE(consensus_mode_quorum_and_fault_tolerance_scaling)
{
    // Test N = 1 to 7 according to the consensus specification:
    // quorum = floor(2*N/3) + 1
    // f = (N - 1) / 3
    struct TestCase {
        size_t n;
        size_t expected_quorum;
        size_t expected_f;
        cybou::ConsensusMode expected_mode;
    };

    const std::vector<TestCase> cases = {
        {1, 1, 0, cybou::ConsensusMode::AUTHORITY},    // 1/1 quorum, f = 0
        {2, 2, 0, cybou::ConsensusMode::INTEGRATION},  // 2/2 quorum, f = 0
        {3, 3, 0, cybou::ConsensusMode::INTEGRATION},  // 3/3 quorum, f = 0
        {4, 3, 1, cybou::ConsensusMode::BFT},          // 3/4 quorum, f = 1
        {5, 4, 1, cybou::ConsensusMode::BFT},          // 4/5 quorum, f = 1
        {6, 5, 1, cybou::ConsensusMode::BFT},          // 5/6 quorum, f = 1
        {7, 5, 2, cybou::ConsensusMode::BFT},          // 5/7 quorum, f = 2
    };

    for (const auto& tc : cases) {
        cybou::ValidatorSetV1 set;
        set.version = cybou::VALIDATOR_SET_VERSION;
        for (size_t i = 1; i <= tc.n; ++i) {
            auto node = MockValidatorNode::Create(static_cast<uint8_t>(i));
            set.validators.push_back(cybou::ValidatorV1{
                .validator_id = node.validator_id,
                .consensus_public_key = node.consensus_pubkey,
                .weight = 1,
            });
        }

        BOOST_CHECK_EQUAL(set.Size(), tc.n);
        BOOST_CHECK_EQUAL(set.QuorumThreshold(), tc.expected_quorum);
        BOOST_CHECK_EQUAL(set.FaultTolerance(), tc.expected_f);
        BOOST_CHECK(set.Mode() == tc.expected_mode);
        BOOST_CHECK(cybou::ValidateValidatorSet(set) == cybou::ValidatorSetValidationError::NONE);
    }
}

BOOST_AUTO_TEST_CASE(authority_mode_n1_consensus_and_state_store)
{
    // N = 1 Authority Mode: Single validator produces blocks and finality certificates
    const auto node = MockValidatorNode::Create(1);
    cybou::ValidatorSetV1 val_set;
    val_set.validators.push_back(cybou::ValidatorV1{
        .validator_id = node.validator_id,
        .consensus_public_key = node.consensus_pubkey,
        .weight = 1,
    });

    BOOST_CHECK_EQUAL(val_set.Size(), 1);
    BOOST_CHECK_EQUAL(val_set.QuorumThreshold(), 1);
    BOOST_CHECK_EQUAL(val_set.FaultTolerance(), 0);
    BOOST_CHECK(val_set.Mode() == cybou::ConsensusMode::AUTHORITY);
    BOOST_CHECK(cybou::ValidateValidatorSet(val_set) == cybou::ValidatorSetValidationError::NONE);

    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    const cybou::CybouState genesis_state{
        .onboarding_pool = 100000,
        .security_reward_pool = 5000,
        .pending_fee_pool = 0,
        .accounts{},
    };

    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
        .epoch_blocks = 10,
    };

    const uint256 genesis_block_id{uint256::FromUserHex("1001").value()};

    const cybou::CybouNetworkDefinitionV1 definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = genesis_block_id,
        .genesis_state_root = cybou::CybouStateHash(genesis_state),
        .protocol_parameters = params,
        .initial_validator_set_commitment = val_set_commitment,
    };
    const uint256 network_id = cybou::NetworkId(definition);

    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(genesis_state));

    // Height 1 block proposal
    cybou::CybouBlockV1 block_1{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = genesis_block_id,
        .height = 1,
        .operations = {},
        .resulting_state_root = cybou::CybouStateHash(genesis_state),
    };
    const uint256 block_1_id = cybou::ComputeBlockId(block_1);

    // 1/1 Finality Certificate
    cybou::BftFinalityCertificateV1 cert_1{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_1_id,
        .height = 1,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {
            node.SignCommit(network_id, block_1_id, 1, val_set_commitment),
        },
    };

    // Verify 1/1 certificate
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_1, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // Empty votes fails
    cybou::BftFinalityCertificateV1 empty_cert = cert_1;
    empty_cert.commit_votes.clear();
    BOOST_CHECK(cybou::VerifyFinalityCertificate(empty_cert, val_set, network_id) ==
                cybou::FinalityVerificationError::INSUFFICIENT_VOTES);

    // Commit to StateStore
    cybou::FinalizedBlockV1 finalized_1{
        .block = block_1,
        .certificate = cert_1,
    };
    BOOST_REQUIRE(store.CommitFinalizedBlock(finalized_1, val_set));

    auto head = store.GetFinalizedHead();
    BOOST_REQUIRE(head.has_value());
    BOOST_CHECK_EQUAL(head->height, 1);
    BOOST_CHECK(head->block_id == block_1_id);
}

BOOST_AUTO_TEST_CASE(bft_simulator_authority_mode_n1)
{
    const uint256 network_id{uint256::FromUserHex("88").value()};
    cybou::BftSimulator sim{network_id, 1}; // N = 1
    BOOST_CHECK_EQUAL(sim.NodeCount(), 1);
    BOOST_CHECK_EQUAL(sim.Quorum(), 1);
    BOOST_CHECK(sim.GetValidatorSet().Mode() == cybou::ConsensusMode::AUTHORITY);

    const uint256 state_root{uint256::FromUserHex("2222").value()};

    // Step height 1
    BOOST_CHECK(sim.StepRound(1, 0, {}, state_root));
    BOOST_CHECK(sim.Node(0).GetStep() == cybou::BftStep::FINALIZED);
    BOOST_REQUIRE(sim.Node(0).GetLatestFinalizedBlock().has_value());
    BOOST_CHECK_EQUAL(sim.Node(0).GetLatestFinalizedBlock()->certificate.commit_votes.size(), 1);

    // Step height 2
    sim.Node(0).SetHeight(2, cybou::ComputeBlockId(sim.Node(0).GetLatestFinalizedBlock()->block));
    BOOST_CHECK(sim.StepRound(2, 0, {}, state_root));
    BOOST_CHECK(sim.Node(0).GetStep() == cybou::BftStep::FINALIZED);
    BOOST_REQUIRE(sim.Node(0).GetLatestFinalizedBlock().has_value());
    BOOST_CHECK_EQUAL(sim.Node(0).GetLatestFinalizedBlock()->block.height, 2);
}

BOOST_AUTO_TEST_SUITE_END()

