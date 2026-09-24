// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>
#include <cybou/bft_engine.h>
#include <cybou/block.h>
#include <cybou/block_executor.h>
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
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

struct MockValidatorNode {
    uint256 validator_id;
    std::array<unsigned char, 32> seed{};
    cybou::IdentityHybridPublicKey consensus_pubkey;

    static MockValidatorNode Create(uint8_t index)
    {
        MockValidatorNode node;
        node.seed.fill(0);
        node.seed[0] = index;
        const auto keypair = cybou::GenerateValidatorKeyPair(node.seed).value();
        node.consensus_pubkey = keypair.public_key;
        node.validator_id = cybou::ComputeValidatorId(keypair.public_key);
        return node;
    }

    cybou::BftCommitVote SignCommit(
        const uint256& network_id,
        const uint256& block_id,
        uint64_t height,
        const uint256& val_set_commitment,
        uint32_t round = 0) const
    {
        const uint256 digest = cybou::ComputeBftCommitDigest(network_id, block_id, height, round, val_set_commitment);
        cybou::BftCommitVote vote;
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

BOOST_AUTO_TEST_CASE(bft_signing_journal_blocks_restart_equivocation)
{
    const auto dir = std::filesystem::temp_directory_path() / "cybou-bft-signing-test";
    std::filesystem::create_directories(dir);
    const auto journal = dir / "validator-signing.journal";
    std::filesystem::remove(journal);
    std::filesystem::remove(journal.string() + ".tmp");

    std::array<unsigned char, 32> seed{};
    seed[0] = 0x7a;
    const auto key = *cybou::GenerateValidatorKeyPair(seed);
    const cybou::ValidatorSet set{
        .version = cybou::VALIDATOR_SET_VERSION,
        .validators = {{.validator_id = cybou::ComputeValidatorId(key.public_key),
                        .consensus_public_key = key.public_key, .weight = 1}},
    };
    const auto network = uint256::FromUserHex("b00f").value();
    const auto execute = [](const std::vector<cybou::ProtocolOperation>&, uint64_t) -> std::optional<uint256> {
        return uint256::ONE;
    };
    {
        cybou::BftValidatorNode first{0, seed, network, set, execute, journal};
        first.SetHeight(1, uint256::ONE, set);
        const auto proposal = first.StartRound(0, {});
        BOOST_REQUIRE(proposal);
        const auto prevote = first.ReceiveProposal(*proposal);
        BOOST_REQUIRE(prevote);
        BOOST_REQUIRE(first.ReceivePrevote(*prevote));
    }
    {
        cybou::BftValidatorNode restarted{0, seed, network, set, execute, journal};
        restarted.SetHeight(1, uint256::ONE, set);
        BOOST_CHECK(!restarted.StartRound(0, {}));
        BOOST_CHECK(!restarted.StartRound(1, {}));
        restarted.SetHeight(2, uint256::ONE, set);
        const auto proposal = restarted.StartRound(0, {});
        BOOST_REQUIRE(proposal);
        const auto prevote = restarted.ReceiveProposal(*proposal);
        BOOST_REQUIRE(prevote);
        const auto precommit = restarted.ReceivePrevote(*prevote);
        BOOST_REQUIRE(precommit);
        BOOST_CHECK(precommit->block_id.has_value());
    }
    {
        std::ofstream damaged(journal, std::ios::binary | std::ios::trunc);
        damaged << "broken";
    }
    cybou::BftValidatorNode corrupt{0, seed, network, set, execute, journal};
    corrupt.SetHeight(3, uint256::ONE, set);
    BOOST_CHECK(!corrupt.StartRound(0, {}));
    std::filesystem::remove(journal);
    std::filesystem::remove_all(dir);
}

BOOST_AUTO_TEST_CASE(bft_uses_distinct_validator_id_and_reexecutes_before_vote)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 0x42;
    const auto keypair = *cybou::GenerateValidatorKeyPair(seed);
    const auto pub = keypair.public_key;
    const auto id = cybou::ComputeValidatorId(pub);
    const auto network = uint256::FromUserHex("cd").value();
    const auto root = uint256::FromUserHex("ef").value();
    const cybou::ValidatorSet set{
        .version = cybou::VALIDATOR_SET_VERSION,
        .validators = {{.validator_id = id, .consensus_public_key = pub, .weight = 1}},
    };
    const auto execute = [root](const std::vector<cybou::ProtocolOperation>& ops, uint64_t) -> std::optional<uint256> {
        return ops.empty() ? std::optional<uint256>{root} : std::nullopt;
    };
    cybou::BftValidatorNode node{0, seed, network, set, execute};
    node.SetHeight(1, uint256::ONE, set);
    BOOST_CHECK(node.GetValidatorId() == id);
    auto proposal = node.StartRound(0, {});
    BOOST_REQUIRE(proposal.has_value());
    BOOST_CHECK(proposal->proposer_id == id);
    auto valid_prevote = node.ReceiveProposal(*proposal);
    BOOST_REQUIRE(valid_prevote.has_value());
    BOOST_CHECK(valid_prevote->block_id == cybou::ComputeBlockId(proposal->block));

    cybou::BftValidatorNode rejecting{0, seed, network, set, execute};
    rejecting.SetHeight(1, uint256::ONE, set);
    rejecting.StartRound(0, {});
    auto forged = *proposal;
    forged.block.resulting_state_root = uint256::ONE;
    const auto forged_id = cybou::ComputeBlockId(forged.block);
    const auto digest = cybou::ComputeProposalDigest(network, 1, 0, id, forged_id);
    forged.signature = *cybou::SignValidatorVote(seed, digest);
    const auto nil_prevote = rejecting.ReceiveProposal(forged);
    BOOST_REQUIRE(nil_prevote.has_value());
    BOOST_CHECK(!nil_prevote->block_id.has_value());
    const auto invalid_commit_digest = cybou::ComputeBftCommitDigest(
        network, forged_id, 1, 0, cybou::ComputeValidatorSetCommitment(set));
    BOOST_CHECK(!rejecting.ReceivePrecommit(cybou::BftPrecommitMsg{
        .network_id = network, .height = 1, .round = 0, .validator_id = id,
        .block_id = forged_id, .signature = *cybou::SignValidatorVote(seed, invalid_commit_digest),
    }));
    BOOST_CHECK(rejecting.GetStep() != cybou::BftStep::FINALIZED);

    // A quorum for another block must never lock or finalize the local proposal.
    const auto other = uint256::FromUserHex("1234").value();
    const auto other_digest = cybou::ComputePrevoteDigest(network, 1, 0, id, other);
    cybou::BftPrevoteMsg equivocated{
        .network_id = network, .height = 1, .round = 0, .validator_id = id,
        .block_id = other, .signature = *cybou::SignValidatorVote(seed, other_digest),
    };
    auto precommit = node.ReceivePrevote(equivocated);
    BOOST_CHECK(!precommit.has_value());
    const auto commit_digest = cybou::ComputeBftCommitDigest(
        network, other, 1, 0, cybou::ComputeValidatorSetCommitment(set));
    const cybou::BftPrecommitMsg false_commit{
        .network_id = network, .height = 1, .round = 0, .validator_id = id,
        .block_id = other, .signature = *cybou::SignValidatorVote(seed, commit_digest),
    };
    BOOST_CHECK(!node.ReceivePrecommit(false_commit));
    BOOST_CHECK(node.GetStep() != cybou::BftStep::FINALIZED);

    auto next_set = set;
    next_set.validators[0].validator_id = uint256::FromUserHex("ac").value();
    std::array<unsigned char, 32> other_seed{};
    other_seed[0] = 0x43;
    const auto other_keypair = *cybou::GenerateValidatorKeyPair(other_seed);
    next_set.validators.insert(next_set.validators.begin(), cybou::Validator{
        .validator_id = cybou::ComputeValidatorId(other_keypair.public_key),
        .consensus_public_key = other_keypair.public_key,
        .weight = 1,
    });
    node.SetHeight(2, cybou::ComputeBlockId(proposal->block), next_set);
    BOOST_CHECK_EQUAL(node.GetNodeIndex(), 1U);
    BOOST_CHECK(node.GetValidatorId() == next_set.validators[1].validator_id);
    BOOST_CHECK(node.StartRound(1, {}).has_value());
}

BOOST_AUTO_TEST_CASE(bft_executor_rejects_invalid_operations_before_proposal)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 0x33;
    const auto keypair = *cybou::GenerateValidatorKeyPair(seed);
    const auto pub = keypair.public_key;
    const auto val_id = cybou::ComputeValidatorId(pub);
    const cybou::ValidatorSet set{
        .version = cybou::VALIDATOR_SET_VERSION,
        .validators = {{.validator_id = val_id, .consensus_public_key = pub, .weight = 1}},
    };
    cybou::CybouState parent;
    parent.validator_set = set;
    const cybou::CybouProtocolParameters params{};
    const auto network = uint256::FromUserHex("ab01").value();
    auto execute = [&parent, &params, &network](
        const std::vector<cybou::ProtocolOperation>& ops, uint64_t height) -> std::optional<uint256> {
        const auto result = cybou::ExecuteBlockOperations(parent, ops, network, height, params);
        return result ? std::optional<uint256>{result.state_root} : std::nullopt;
    };
    cybou::BftValidatorNode node{0, seed, network, set, execute};
    node.SetHeight(1, uint256::ONE, set);
    BOOST_CHECK(node.StartRound(0, {}).has_value());
    const cybou::AuthorizedPayment invalid_payment{
        .authorization = {
            .account_id = cybou::AccountId{uint256::FromUserHex("02").value()},
            .device_id = cybou::IdentityKeyId{1},
            .nonce = 0,
            .activation_nonce = 0,
            .kind = cybou::DeviceOperationKind::PAYMENT,
            .payload_commitment = cybou::IdentityKeyId{1},
            .signature = {},
        },
        .payment = {
            .recipient = cybou::AccountId{uint256::ONE},
            .amount = 1,
        },
    };
    // A bare authorized operation with no sender account cannot execute.
    BOOST_CHECK(!node.StartRound(0, {cybou::ProtocolOperation{invalid_payment}}).has_value());
}

BOOST_AUTO_TEST_CASE(validator_set_invariants_and_serialization)
{
    std::vector<MockValidatorNode> nodes;
    nodes.reserve(4);
    for (uint8_t i = 1; i <= 4; ++i) {
        nodes.push_back(MockValidatorNode::Create(i));
    }

    cybou::ValidatorSet val_set;
    for (const auto& node : nodes) {
        val_set.validators.push_back(cybou::Validator{
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
    cybou::ValidatorSet too_small = val_set;
    too_small.validators.clear();
    BOOST_CHECK(cybou::ValidateValidatorSet(too_small) == cybou::ValidatorSetValidationError::INVALID_VALIDATOR_COUNT);

    // 2. Weight != 1 (Hard rule: equal validator weight = 1)
    cybou::ValidatorSet unequal_weight = val_set;
    unequal_weight.validators[0].weight = 2;
    BOOST_CHECK(cybou::ValidateValidatorSet(unequal_weight) == cybou::ValidatorSetValidationError::INVALID_WEIGHT);

    // 3. Null validator_id
    cybou::ValidatorSet null_id = val_set;
    null_id.validators[0].validator_id = uint256{};
    BOOST_CHECK(cybou::ValidateValidatorSet(null_id) == cybou::ValidatorSetValidationError::NULL_VALIDATOR_ID);

    // 4. Duplicate validator_id
    cybou::ValidatorSet dup_id = val_set;
    dup_id.validators[1] = dup_id.validators[0];
    BOOST_CHECK(cybou::ValidateValidatorSet(dup_id) == cybou::ValidatorSetValidationError::DUPLICATE_VALIDATOR_ID);

    // 5. Duplicate consensus key
    cybou::ValidatorSet dup_key = val_set;
    dup_key.validators[1].consensus_public_key.ed25519 = dup_key.validators[0].consensus_public_key.ed25519;
    dup_key.validators[1].validator_id = uint256{*cybou::ComputeValidatorKeyId(dup_key.validators[1].consensus_public_key)};
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

    cybou::ValidatorSet val_set;
    for (const auto& node : nodes) {
        val_set.validators.push_back(cybou::Validator{
            .validator_id = node.validator_id,
            .consensus_public_key = node.consensus_pubkey,
            .weight = 1,
        });
    }

    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    // 1. Unanimous 4/4 finality certificate
    cybou::BftFinalityCertificate cert_4_of_4;
    cert_4_of_4.network_id = network_id;
    cert_4_of_4.block_id = block_1_id;
    cert_4_of_4.height = block_1_height;
    cert_4_of_4.round = 0;
    cert_4_of_4.validator_set_commitment = val_set_commitment;

    for (const auto& node : nodes) {
        cert_4_of_4.commit_votes.push_back(
            node.SignCommit(network_id, block_1_id, block_1_height, val_set_commitment));
    }

    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_4_of_4, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // Serialization & Deserialization
    const auto cert_bytes = cybou::SerializeFinalityCertificate(cert_4_of_4);
    BOOST_REQUIRE(cert_bytes.has_value());
    const auto decoded_cert = cybou::DeserializeFinalityCertificate(*cert_bytes);
    BOOST_REQUIRE(decoded_cert.has_value());
    BOOST_CHECK(*decoded_cert == cert_4_of_4);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(*decoded_cert, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // 2. 3/4 supermajority (1 faulty/byzantine node fails to vote)
    cybou::BftFinalityCertificate cert_3_of_4 = cert_4_of_4;
    cert_3_of_4.commit_votes.pop_back();
    BOOST_CHECK_EQUAL(cert_3_of_4.commit_votes.size(), 3);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_3_of_4, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // 3. 2/4 votes (< quorum of 3)
    cybou::BftFinalityCertificate cert_2_of_4 = cert_3_of_4;
    cert_2_of_4.commit_votes.pop_back();
    BOOST_CHECK_EQUAL(cert_2_of_4.commit_votes.size(), 2);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_2_of_4, val_set, network_id) ==
                cybou::FinalityVerificationError::INSUFFICIENT_VOTES);

    // 4. Duplicate vote attempt to forge quorum (validator 0 votes twice)
    cybou::BftFinalityCertificate cert_dup_vote = cert_2_of_4;
    cert_dup_vote.commit_votes.push_back(cert_dup_vote.commit_votes[0]);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_dup_vote, val_set, network_id) ==
                cybou::FinalityVerificationError::DUPLICATE_VOTE);

    // 5. Unknown validator vote
    cybou::BftFinalityCertificate cert_unknown = cert_3_of_4;
    cert_unknown.commit_votes[0].validator_id = uint256::FromUserHex("99").value();
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_unknown, val_set, network_id) ==
                cybou::FinalityVerificationError::UNKNOWN_VALIDATOR);

    // 6. Invalid / forged signature
    cybou::BftFinalityCertificate cert_bad_sig = cert_3_of_4;
    cert_bad_sig.commit_votes[0].signature.ed25519[5] ^= 0xFF;
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_bad_sig, val_set, network_id) ==
                cybou::FinalityVerificationError::INVALID_SIGNATURE);

    // 7. Wrong network ID
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_4_of_4, val_set, uint256::FromUserHex("9999").value()) ==
                cybou::FinalityVerificationError::NETWORK_MISMATCH);

    // 8. Mismatched validator set commitment
    cybou::BftFinalityCertificate cert_bad_val_commit = cert_4_of_4;
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

    cybou::ValidatorSet val_set;
    for (const auto& node : nodes) {
        val_set.validators.push_back(cybou::Validator{
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
        .identities{},
        .validator_set = val_set,
        .names{},
    };

    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
        .epoch_blocks = 10,
    };

    const uint256 genesis_block_id{uint256::FromUserHex("1000").value()};

    const cybou::CybouNetworkDefinition definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = genesis_block_id,
        .genesis_state_root = *cybou::CybouStateHash(genesis_state),
        .protocol_parameters = params,
        .initial_validator_set_commitment = val_set_commitment,
        .operator_authority = std::nullopt,
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
    std::vector<cybou::ProtocolOperation> block_1_ops;
    cybou::CybouBlock block_1{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = genesis_block_id,
        .height = 1,
        .operations = block_1_ops,
        .resulting_state_root = *cybou::CybouStateHash(genesis_state),
    };
    const uint256 block_1_id = cybou::ComputeBlockId(block_1);

    cybou::BftFinalityCertificate cert_1;
    cert_1.network_id = network_id;
    cert_1.block_id = block_1_id;
    cert_1.height = 1;
    cert_1.round = 0;
    cert_1.validator_set_commitment = val_set_commitment;

    // Validators 1, 2, 3 vote commit
    for (size_t i = 0; i < 3; ++i) {
        cert_1.commit_votes.push_back(
            nodes[i].SignCommit(network_id, block_1_id, 1, val_set_commitment));
    }

    // Verify certificate before state commit
    BOOST_REQUIRE(cybou::VerifyFinalityCertificate(cert_1, val_set, network_id) ==
                  cybou::FinalityVerificationError::NONE);

    cybou::FinalizedBlock finalized_1{
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
    std::vector<cybou::ProtocolOperation> block_2_ops;
    cybou::CybouBlock block_2{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = block_1_id,
        .height = 2,
        .operations = block_2_ops,
        .resulting_state_root = *cybou::CybouStateHash(genesis_state),
    };
    const uint256 block_2_id = cybou::ComputeBlockId(block_2);

    cybou::BftFinalityCertificate cert_2;
    cert_2.network_id = network_id;
    cert_2.block_id = block_2_id;
    cert_2.height = 2;
    cert_2.round = 0;
    cert_2.validator_set_commitment = val_set_commitment;

    for (const auto& node : nodes) {
        cert_2.commit_votes.push_back(
            node.SignCommit(network_id, block_2_id, 2, val_set_commitment));
    }

    BOOST_REQUIRE(cybou::VerifyFinalityCertificate(cert_2, val_set, network_id) ==
                  cybou::FinalityVerificationError::NONE);

    cybou::FinalizedBlock finalized_2{
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
        sim.Node(i).SetHeight(2, cybou::ComputeBlockId(sim.Node(i).GetLatestFinalizedBlock()->block), sim.GetValidatorSet());
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
        sim.Node(i).SetHeight(3, cybou::ComputeBlockId(sim.Node(0).GetLatestFinalizedBlock()->block), sim.GetValidatorSet());
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
    // Signed round-0 messages cannot be replaced after the partition heals.
    BOOST_CHECK(sim.StepRound(3, 1, {}, state_root));
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
        cybou::ValidatorSet set;
        set.version = cybou::VALIDATOR_SET_VERSION;
        for (size_t i = 1; i <= tc.n; ++i) {
            auto node = MockValidatorNode::Create(static_cast<uint8_t>(i));
            set.validators.push_back(cybou::Validator{
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
    cybou::ValidatorSet val_set;
    val_set.validators.push_back(cybou::Validator{
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
        .identities{},
        .validator_set = val_set,
        .names{},
    };

    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 128,
        .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
        .epoch_blocks = 10,
    };

    const uint256 genesis_block_id{uint256::FromUserHex("1001").value()};

    const cybou::CybouNetworkDefinition definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = genesis_block_id,
        .genesis_state_root = *cybou::CybouStateHash(genesis_state),
        .protocol_parameters = params,
        .initial_validator_set_commitment = val_set_commitment,
        .operator_authority = std::nullopt,
    };
    const uint256 network_id = cybou::NetworkId(definition);

    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(genesis_state));

    // Height 1 block proposal
    cybou::CybouBlock block_1{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = genesis_block_id,
        .height = 1,
        .operations = {},
        .resulting_state_root = *cybou::CybouStateHash(genesis_state),
    };
    const uint256 block_1_id = cybou::ComputeBlockId(block_1);

    // 1/1 Finality Certificate
    cybou::BftFinalityCertificate cert_1{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_1_id,
        .height = 1,
        .round = 0,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {
            node.SignCommit(network_id, block_1_id, 1, val_set_commitment),
        },
    };

    // Verify 1/1 certificate
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_1, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);

    // Empty votes fails
    cybou::BftFinalityCertificate empty_cert = cert_1;
    empty_cert.commit_votes.clear();
    BOOST_CHECK(cybou::VerifyFinalityCertificate(empty_cert, val_set, network_id) ==
                cybou::FinalityVerificationError::INSUFFICIENT_VOTES);

    // Commit to StateStore
    cybou::FinalizedBlock finalized_1{
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
    sim.Node(0).SetHeight(2, cybou::ComputeBlockId(sim.Node(0).GetLatestFinalizedBlock()->block), sim.GetValidatorSet());
    BOOST_CHECK(sim.StepRound(2, 0, {}, state_root));
    BOOST_CHECK(sim.Node(0).GetStep() == cybou::BftStep::FINALIZED);
    BOOST_REQUIRE(sim.Node(0).GetLatestFinalizedBlock().has_value());
    BOOST_CHECK_EQUAL(sim.Node(0).GetLatestFinalizedBlock()->block.height, 2);
}

BOOST_AUTO_TEST_CASE(bft_round_bound_signatures_and_replay_rejection)
{
    const uint256 network_id = uint256::FromUserHex("cafe").value();
    const auto node = MockValidatorNode::Create(0);
    cybou::ValidatorSet val_set;
    val_set.validators.push_back(cybou::Validator{
        .validator_id = node.validator_id,
        .consensus_public_key = node.consensus_pubkey,
        .weight = 1,
    });
    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);
    const uint256 block_id = uint256::FromUserHex("beef").value();

    // 1. Signature produced in round 0 vs round 1
    const uint256 digest_round_0 = cybou::ComputeBftCommitDigest(
        network_id, block_id, 1, 0, val_set_commitment);
    const auto sig_round_0 = *cybou::SignValidatorVote(node.seed, digest_round_0);

    const uint256 digest_round_1 = cybou::ComputeBftCommitDigest(
        network_id, block_id, 1, 1, val_set_commitment);
    const auto sig_round_1 = *cybou::SignValidatorVote(node.seed, digest_round_1);

    // Digests and signatures must be distinct across rounds
    BOOST_CHECK(digest_round_0 != digest_round_1);
    BOOST_CHECK(sig_round_0 != sig_round_1);

    // 2. Precommit with round 0 signature replayed as round 1 message must fail
    cybou::BftValidatorNode validator{
        0, node.seed, network_id, val_set,
        [](const std::vector<cybou::ProtocolOperation>&, uint64_t) {
            return uint256::FromUserHex("2222");
        },
    };
    validator.SetHeight(1, uint256::ZERO, val_set);
    validator.StartRound(1, {});

    const cybou::BftPrecommitMsg replayed_precommit{
        .network_id = network_id,
        .height = 1,
        .round = 1,
        .validator_id = node.validator_id,
        .block_id = block_id,
        .signature = sig_round_0,
    };
    BOOST_CHECK(!validator.ReceivePrecommit(replayed_precommit));

    // 3. Certificate round matching: cert for round 1 rejects round 0 vote
    cybou::BftFinalityCertificate cert_round_1{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = network_id,
        .block_id = block_id,
        .height = 1,
        .round = 1,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {
            cybou::BftCommitVote{
                .validator_id = node.validator_id,
                .signature = sig_round_0,
            },
        },
    };
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_round_1, val_set, network_id) ==
                cybou::FinalityVerificationError::INVALID_SIGNATURE);

    // Correct vote for round 1 succeeds
    cert_round_1.commit_votes[0].signature = sig_round_1;
    BOOST_CHECK(cybou::VerifyFinalityCertificate(cert_round_1, val_set, network_id) ==
                cybou::FinalityVerificationError::NONE);
}

BOOST_AUTO_TEST_CASE(bft_adversarial_split_prevotes_round_recovery)
{
    // N=4 network: Quorum = 3.
    // In round 0, 2 nodes prevote block A, 2 nodes prevote block B.
    // Neither block achieves 3/4 prevote quorum.
    // Validators timeout on prevote and issue nil precommits.
    // Advance to round 1: Round 1 proposer proposes block A.
    // All nodes prevote block A, reach quorum, lock block A in round 1,
    // issue round 1 precommits, and achieve finality at round 1.
    const uint256 network_id = uint256::FromUserHex("cafe").value();
    std::vector<MockValidatorNode> mock_nodes;
    cybou::ValidatorSet val_set;
    for (uint8_t i = 0; i < 4; ++i) {
        mock_nodes.push_back(MockValidatorNode::Create(i));
        val_set.validators.push_back(cybou::Validator{
            .validator_id = mock_nodes.back().validator_id,
            .consensus_public_key = mock_nodes.back().consensus_pubkey,
            .weight = 1,
        });
    }

    std::vector<std::unique_ptr<cybou::BftValidatorNode>> nodes;
    for (size_t i = 0; i < 4; ++i) {
        nodes.push_back(std::make_unique<cybou::BftValidatorNode>(
            i, mock_nodes[i].seed, network_id, val_set,
            [](const std::vector<cybou::ProtocolOperation>&, uint64_t) {
                return uint256::FromUserHex("1111");
            }
        ));
        nodes.back()->SetHeight(1, uint256::ZERO, val_set);
    }

    // Start round 0: Leader 1 % 4 = 1.
    const size_t leader_r0 = cybou::BftLeaderIndex(1, 0, 4);
    BOOST_CHECK_EQUAL(leader_r0, 1);
    std::optional<cybou::BftProposalMsg> proposal_r0;
    for (size_t i = 0; i < 4; ++i) {
        auto prop = nodes[i]->StartRound(0, {});
        if (i == leader_r0) proposal_r0 = prop;
    }

    BOOST_REQUIRE(proposal_r0.has_value());
    const uint256 block_a_id = cybou::ComputeBlockId(proposal_r0->block);

    // Nodes 0 and 1 accept proposal A
    const auto pv0 = nodes[0]->ReceiveProposal(*proposal_r0);
    const auto pv1 = nodes[1]->ReceiveProposal(*proposal_r0);
    BOOST_REQUIRE(pv0 && pv0->block_id == block_a_id);
    BOOST_REQUIRE(pv1 && pv1->block_id == block_a_id);

    // Byzantine / split behavior: nodes 2 and 3 prevote alternative block B
    const uint256 block_b_id = uint256::FromUserHex("bbbb").value();
    const uint256 pv2_digest = cybou::ComputePrevoteDigest(network_id, 1, 0, mock_nodes[2].validator_id, block_b_id);
    const cybou::BftPrevoteMsg pv2{
        .network_id = network_id, .height = 1, .round = 0,
        .validator_id = mock_nodes[2].validator_id, .block_id = block_b_id,
        .signature = *cybou::SignValidatorVote(mock_nodes[2].seed, pv2_digest),
    };
    const uint256 pv3_digest = cybou::ComputePrevoteDigest(network_id, 1, 0, mock_nodes[3].validator_id, block_b_id);
    const cybou::BftPrevoteMsg pv3{
        .network_id = network_id, .height = 1, .round = 0,
        .validator_id = mock_nodes[3].validator_id, .block_id = block_b_id,
        .signature = *cybou::SignValidatorVote(mock_nodes[3].seed, pv3_digest),
    };

    // Distribute prevotes to Node 0: receives 2 for A, 2 for B
    nodes[0]->ReceivePrevote(*pv0);
    nodes[0]->ReceivePrevote(*pv1);
    nodes[0]->ReceivePrevote(pv2);
    nodes[0]->ReceivePrevote(pv3);

    // Neither block reached 3/4 quorum: Node 0 cannot finalize in round 0
    BOOST_CHECK(nodes[0]->GetStep() != cybou::BftStep::FINALIZED);
    BOOST_CHECK(!nodes[0]->GetLatestFinalizedBlock().has_value());

    // Advance to round 1: Leader (1 + 1) % 4 = 2.
    const size_t leader_r1 = cybou::BftLeaderIndex(1, 1, 4);
    BOOST_CHECK_EQUAL(leader_r1, 2);
    std::optional<cybou::BftProposalMsg> proposal_r1;
    for (size_t i = 0; i < 4; ++i) {
        auto prop = nodes[i]->StartRound(1, {});
        if (i == leader_r1) proposal_r1 = prop;
    }

    BOOST_REQUIRE(proposal_r1.has_value());
    const uint256 block_r1_id = cybou::ComputeBlockId(proposal_r1->block);

    // All 4 nodes receive proposal_r1 and prevote it
    std::vector<cybou::BftPrevoteMsg> prevotes_r1;
    for (size_t i = 0; i < 4; ++i) {
        auto pv = nodes[i]->ReceiveProposal(*proposal_r1);
        BOOST_REQUIRE(pv.has_value() && pv->block_id == block_r1_id);
        prevotes_r1.push_back(*pv);
    }

    // Exchange round 1 prevotes; all nodes lock and emit round 1 precommits
    std::vector<cybou::BftPrecommitMsg> precommits_r1;
    for (size_t i = 0; i < 4; ++i) {
        std::optional<cybou::BftPrecommitMsg> pc;
        for (const auto& pv : prevotes_r1) {
            auto res = nodes[i]->ReceivePrevote(pv);
            if (res) pc = res;
        }
        BOOST_REQUIRE(pc.has_value() && pc->block_id == block_r1_id);
        BOOST_CHECK_EQUAL(pc->round, 1);
        precommits_r1.push_back(*pc);
    }

    // Exchange round 1 precommits; all nodes reach finality in round 1
    for (size_t i = 0; i < 4; ++i) {
        for (const auto& pc : precommits_r1) {
            nodes[i]->ReceivePrecommit(pc);
        }
        BOOST_CHECK(nodes[i]->GetStep() == cybou::BftStep::FINALIZED);
        BOOST_REQUIRE(nodes[i]->GetLatestFinalizedBlock().has_value());
        const auto& cert = nodes[i]->GetLatestFinalizedBlock()->certificate;
        BOOST_CHECK_EQUAL(cert.round, 1);
        BOOST_CHECK_EQUAL(cert.height, 1);
        BOOST_CHECK(cert.block_id == block_r1_id);
        BOOST_CHECK(cybou::VerifyFinalityCertificate(cert, val_set, network_id) ==
                    cybou::FinalityVerificationError::NONE);
    }
}

BOOST_AUTO_TEST_CASE(bft_prevote_liveness_handles_interleaved_nil_and_delivers_block_quorum)
{
    // N=4, Q=3. Test that interleaved nil prevote does NOT cause premature nil precommit.
    const uint256 network_id{uint256::FromUserHex("42").value()};
    const uint256 root{uint256::FromUserHex("99").value()};
    std::vector<MockValidatorNode> mocks;
    cybou::ValidatorSet val_set{.version = cybou::VALIDATOR_SET_VERSION, .validators = {}};
    for (uint8_t i = 0; i < 4; ++i) {
        auto m = MockValidatorNode::Create(i);
        mocks.push_back(m);
        val_set.validators.push_back({
            .validator_id = m.validator_id,
            .consensus_public_key = m.consensus_pubkey,
            .weight = 1,
        });
    }

    const auto execute = [root](const std::vector<cybou::ProtocolOperation>& ops, uint64_t) -> std::optional<uint256> {
        return ops.empty() ? std::optional<uint256>{root} : std::nullopt;
    };

    // Node 0 under test
    cybou::BftValidatorNode node0{0, mocks[0].seed, network_id, val_set, execute};
    node0.SetHeight(1, uint256::ONE, val_set);

    const size_t leader_idx = cybou::BftLeaderIndex(1, 0, 4);
    cybou::BftValidatorNode leader{leader_idx, mocks[leader_idx].seed, network_id, val_set, execute};
    leader.SetHeight(1, uint256::ONE, val_set);
    auto proposal = leader.StartRound(0, {});
    BOOST_REQUIRE(proposal.has_value());
    const uint256 block_id = cybou::ComputeBlockId(proposal->block);

    // Node 0 receives proposal and emits its own prevote
    auto pv0 = node0.ReceiveProposal(*proposal);
    BOOST_REQUIRE(pv0.has_value() && pv0->block_id == block_id);

    // Node 1 prevotes Block
    const uint256 pv_digest = cybou::ComputePrevoteDigest(network_id, 1, 0, mocks[1].validator_id, block_id);
    cybou::BftPrevoteMsg pv1{
        .network_id = network_id,
        .height = 1,
        .round = 0,
        .validator_id = mocks[1].validator_id,
        .block_id = block_id,
        .signature = *cybou::SignValidatorVote(mocks[1].seed, pv_digest),
    };

    // Node 2 prevotes Nil (delayed or network partitioned)
    const uint256 nil_digest = cybou::ComputePrevoteDigest(network_id, 1, 0, mocks[2].validator_id, std::nullopt);
    cybou::BftPrevoteMsg pv2_nil{
        .network_id = network_id,
        .height = 1,
        .round = 0,
        .validator_id = mocks[2].validator_id,
        .block_id = std::nullopt,
        .signature = *cybou::SignValidatorVote(mocks[2].seed, nil_digest),
    };

    // Node 3 prevotes Block
    const uint256 pv3_digest = cybou::ComputePrevoteDigest(network_id, 1, 0, mocks[3].validator_id, block_id);
    cybou::BftPrevoteMsg pv3{
        .network_id = network_id,
        .height = 1,
        .round = 0,
        .validator_id = mocks[3].validator_id,
        .block_id = block_id,
        .signature = *cybou::SignValidatorVote(mocks[3].seed, pv3_digest),
    };

    // Deliver pv1 to node 0: now 2 votes for Block (node0 + node1). No quorum yet.
    auto res1 = node0.ReceivePrevote(pv1);
    BOOST_CHECK(!res1.has_value());

    // Deliver pv2_nil to node 0: now 3 prevotes received (node0=Block, node1=Block, node2=Nil).
    // Total prevotes = 3 >= quorum (3), BUT Block only has 2 votes and Nil only has 1.
    // Node 0 MUST NOT precommit nil here! It must wait for the 4th vote.
    auto res2 = node0.ReceivePrevote(pv2_nil);
    BOOST_CHECK(!res2.has_value());
    BOOST_CHECK(node0.GetStep() != cybou::BftStep::PRECOMMIT);

    // Deliver pv3 to node 0: now 3 votes for Block (node0 + node1 + node3) >= quorum (3).
    // Node 0 MUST lock and produce a precommit for Block!
    auto res3 = node0.ReceivePrevote(pv3);
    BOOST_REQUIRE(res3.has_value());
    BOOST_CHECK(res3->block_id == block_id);
    BOOST_CHECK(node0.GetStep() == cybou::BftStep::PRECOMMIT);
}

BOOST_AUTO_TEST_CASE(bft_prevote_all_voted_without_quorum_precommits_nil)
{
    // N=4, Q=3. If all 4 validators have voted and no block reached quorum, precommit nil.
    const uint256 network_id{uint256::FromUserHex("42").value()};
    const uint256 root{uint256::FromUserHex("99").value()};
    std::vector<MockValidatorNode> mocks;
    cybou::ValidatorSet val_set{.version = cybou::VALIDATOR_SET_VERSION, .validators = {}};
    for (uint8_t i = 0; i < 4; ++i) {
        auto m = MockValidatorNode::Create(i);
        mocks.push_back(m);
        val_set.validators.push_back({
            .validator_id = m.validator_id,
            .consensus_public_key = m.consensus_pubkey,
            .weight = 1,
        });
    }

    const auto execute = [root](const std::vector<cybou::ProtocolOperation>& ops, uint64_t) -> std::optional<uint256> {
        return ops.empty() ? std::optional<uint256>{root} : std::nullopt;
    };

    cybou::BftValidatorNode node0{0, mocks[0].seed, network_id, val_set, execute};
    node0.SetHeight(1, uint256::ONE, val_set);

    const size_t leader_idx = cybou::BftLeaderIndex(1, 0, 4);
    cybou::BftValidatorNode leader{leader_idx, mocks[leader_idx].seed, network_id, val_set, execute};
    leader.SetHeight(1, uint256::ONE, val_set);
    auto proposal = leader.StartRound(0, {});
    BOOST_REQUIRE(proposal.has_value());
    const uint256 block_id = cybou::ComputeBlockId(proposal->block);

    node0.ReceiveProposal(*proposal);

    const uint256 pv_digest = cybou::ComputePrevoteDigest(network_id, 1, 0, mocks[1].validator_id, block_id);
    cybou::BftPrevoteMsg pv1{
        .network_id = network_id,
        .height = 1,
        .round = 0,
        .validator_id = mocks[1].validator_id,
        .block_id = block_id,
        .signature = *cybou::SignValidatorVote(mocks[1].seed, pv_digest),
    };

    // Node 2 and Node 3 vote Nil (so total for Block is only 2: node0 + node1)
    const uint256 nil_digest2 = cybou::ComputePrevoteDigest(network_id, 1, 0, mocks[2].validator_id, std::nullopt);
    cybou::BftPrevoteMsg pv2_nil{
        .network_id = network_id,
        .height = 1,
        .round = 0,
        .validator_id = mocks[2].validator_id,
        .block_id = std::nullopt,
        .signature = *cybou::SignValidatorVote(mocks[2].seed, nil_digest2),
    };

    const uint256 nil_digest3 = cybou::ComputePrevoteDigest(network_id, 1, 0, mocks[3].validator_id, std::nullopt);
    cybou::BftPrevoteMsg pv3_nil{
        .network_id = network_id,
        .height = 1,
        .round = 0,
        .validator_id = mocks[3].validator_id,
        .block_id = std::nullopt,
        .signature = *cybou::SignValidatorVote(mocks[3].seed, nil_digest3),
    };

    BOOST_CHECK(!node0.ReceivePrevote(pv1).has_value());
    BOOST_CHECK(!node0.ReceivePrevote(pv2_nil).has_value());

    // When 4th vote arrives (all 4 voted, no block quorum reached):
    auto res = node0.ReceivePrevote(pv3_nil);
    BOOST_REQUIRE(res.has_value());
    BOOST_CHECK(!res->block_id.has_value()); // nil precommit!
    BOOST_CHECK(node0.GetStep() == cybou::BftStep::PRECOMMIT);
}

BOOST_AUTO_TEST_SUITE_END()
