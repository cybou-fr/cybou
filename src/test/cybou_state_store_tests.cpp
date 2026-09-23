// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/network_definition.h>
#include <cybou/state_store.h>
#include <cybou/validator.h>

#include <dbwrapper.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(cybou_state_store_tests, BasicTestingSetup)

namespace {

const cybou::AccountId ACCOUNT_ID{uint256::FromUserHex("0a").value()};
const cybou::AccountId ACCOUNT_ID_2{uint256::FromUserHex("0b").value()};

const std::array<unsigned char, 32> AUTH_PRIVKEY = []{
    std::array<unsigned char, 32> k{};
    k[0] = 0x42;
    return k;
}();
const uint256 AUTH_KEY = *cybou::DeriveEd25519PublicKey(AUTH_PRIVKEY);

const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};
const std::string HEAD_KEY{"cybou/head/v1"};

struct MockVal {
    std::array<unsigned char, 32> priv{};
    uint256 pub;
};

const std::vector<MockVal> TEST_VAL_NODES = []{
    std::vector<MockVal> nodes;
    for (uint8_t i = 1; i <= 4; ++i) {
        MockVal node;
        node.priv.fill(0);
        node.priv[0] = i;
        node.pub = *cybou::DeriveEd25519PublicKey(node.priv);
        nodes.push_back(node);
    }
    return nodes;
}();

const cybou::ValidatorSetV1 TEST_VALIDATOR_SET = []{
    cybou::ValidatorSetV1 val_set;
    for (const auto& node : TEST_VAL_NODES) {
        val_set.validators.push_back(cybou::ValidatorV1{
            .validator_id = node.pub,
            .consensus_public_key = node.pub,
            .weight = 1,
        });
    }
    return val_set;
}();

const cybou::CybouProtocolParameters PARAMS{
    .account_creation_work_bits = 0,
    .account_creation_epoch_lag = 1,
    .max_account_creates_per_block = 128,
    .onboarding_bonus = cybou::DEV_ONBOARDING_BONUS,
    .epoch_blocks = 10,
};

cybou::CybouState GenesisState()
{
    return cybou::CybouState{
        .onboarding_pool = 20000,
        .security_reward_pool = 1000,
        .pending_fee_pool = 500,
        .accounts{},
        .validator_set = TEST_VALIDATOR_SET,
    };
}

cybou::CybouNetworkDefinitionV1 TestNetworkDefinition()
{
    return cybou::CybouNetworkDefinitionV1{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = uint256::ONE,
        .genesis_state_root = cybou::CybouStateHash(GenesisState()),
        .protocol_parameters = PARAMS,
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(TEST_VALIDATOR_SET),
    };
}

cybou::AccountCreateOpV1 ValidOp(
    const cybou::AccountId& acc = ACCOUNT_ID,
    const std::array<unsigned char, 32>& priv = AUTH_PRIVKEY)
{
    const uint256 pub = *cybou::DeriveEd25519PublicKey(priv);
    cybou::AccountCreateOpV1 op{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = acc,
        .initial_authorization = cybou::AccountAuthorizationV1{.authorization_descriptor = pub},
        .creation_work{
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = cybou::NetworkId(TestNetworkDefinition()),
            .account_id = acc,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(cybou::AccountAuthorizationV1{.authorization_descriptor = pub}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    op.proof_of_possession = *cybou::SignUserMessage(
        priv, cybou::ComputeAccountPopDigest(op.creation_work.network_id, acc, pub));
    return op;
}

CDBWrapper MemoryDb()
{
    return CDBWrapper{{
        .path = "cybou-state-store-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
}

cybou::FinalizedBlockV1 MakeFinalizedBlock(
    const cybou::CybouStateStore& store,
    const std::vector<cybou::ProtocolOperationV1>& ops,
    const cybou::ValidatorSetV1& val_set = TEST_VALIDATOR_SET,
    const std::vector<MockVal>& vals = TEST_VAL_NODES,
    std::optional<uint256> override_parent = std::nullopt,
    std::optional<uint64_t> override_height = std::nullopt,
    std::optional<uint256> override_network_id = std::nullopt,
    const cybou::OperatorAuthoritySignatureVerifier* op_verifier = nullptr)
{
    const auto loaded = store.LoadState();
    const uint64_t height = override_height.value_or(store.GetFinalizedHeight().value_or(0) + 1);
    const uint256 parent = override_parent.value_or(store.GetFinalizedTip().value_or(uint256{}));
    const uint256 net_id = override_network_id.value_or(store.GetNetworkId());

    cybou::CybouState candidate = loaded && loaded.state ? *loaded.state : GenesisState();
    const cybou::ProtocolExecutionContextV1 ctx{
        .network_id = net_id,
        .block_height = height,
        .params = PARAMS,
        .operator_authority = store.GetOperatorAuthority() ? &*store.GetOperatorAuthority() : nullptr,
        .operator_verifier = op_verifier,
    };
    for (const auto& op : ops) {
        cybou::ApplyProtocolOperation(op, ctx, candidate);
    }
    if (candidate.pending_fee_pool > 0) {
        cybou::RoutePendingFees(candidate);
    }
    const uint256 state_root = cybou::CybouStateHash(candidate);

    cybou::CybouBlockV1 block{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = parent,
        .height = height,
        .operations = ops,
        .resulting_state_root = state_root,
    };
    const uint256 block_id = cybou::ComputeBlockId(block);

    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);
    const uint256 commit_digest = cybou::ComputeBftCommitDigest(
        net_id, block_id, height, 0, val_set_commitment);

    cybou::BftFinalityCertificateV1 cert{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = net_id,
        .block_id = block_id,
        .height = height,
        .round = 0,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {},
    };
    for (const auto& v : vals) {
        cert.commit_votes.push_back(cybou::BftCommitVoteV1{
            .validator_id = v.pub,
            .signature = *cybou::SignValidatorVote(v.priv, commit_digest),
        });
    }

    return cybou::FinalizedBlockV1{
        .block = std::move(block),
        .certificate = std::move(cert),
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(network_definition_roundtrip_rejects_truncation_and_trailing_data)
{
    auto definition = TestNetworkDefinition();
    auto bytes = cybou::SerializeNetworkDefinition(definition);
    const auto decoded = cybou::DeserializeNetworkDefinition(bytes);
    BOOST_REQUIRE(decoded);
    BOOST_CHECK(cybou::SerializeNetworkDefinition(*decoded) == bytes);
    for (size_t size = 0; size < bytes.size(); ++size) {
        BOOST_CHECK(!cybou::DeserializeNetworkDefinition(std::span{bytes}.first(size)));
    }
    bytes.push_back(0);
    BOOST_CHECK(!cybou::DeserializeNetworkDefinition(bytes));

    definition.operator_authority = cybou::OperatorAuthorityKeySet{
        .keyset_id = uint256::ONE,
        .ed25519_public_key = {1},
        .mldsa65_public_key = {1},
        .active_from_epoch = 0,
        .retired_from_epoch = std::nullopt,
    };
    const auto authority_bytes = cybou::SerializeNetworkDefinition(definition);
    const auto decoded_authority = cybou::DeserializeNetworkDefinition(authority_bytes);
    BOOST_REQUIRE(decoded_authority);
    BOOST_CHECK(cybou::SerializeNetworkDefinition(*decoded_authority) == authority_bytes);
    BOOST_CHECK(!cybou::DeserializeNetworkDefinition(std::span{authority_bytes}.first(authority_bytes.size() - 1)));

    // Invalid authority validations
    auto bad_auth_def = definition;
    bad_auth_def.operator_authority->keyset_id = uint256{};
    BOOST_CHECK(cybou::ValidateNetworkDefinition(bad_auth_def) ==
                cybou::NetworkDefinitionError::NULL_OPERATOR_AUTHORITY_KEYSET_ID);

    bad_auth_def = definition;
    bad_auth_def.operator_authority->ed25519_public_key.fill(0);
    BOOST_CHECK(cybou::ValidateNetworkDefinition(bad_auth_def) ==
                cybou::NetworkDefinitionError::NULL_OPERATOR_AUTHORITY_KEY);

    bad_auth_def = definition;
    bad_auth_def.operator_authority->mldsa65_public_key.fill(0);
    BOOST_CHECK(cybou::ValidateNetworkDefinition(bad_auth_def) ==
                cybou::NetworkDefinitionError::NULL_OPERATOR_AUTHORITY_KEY);

    bad_auth_def = definition;
    bad_auth_def.operator_authority->active_from_epoch = 1;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(bad_auth_def) ==
                cybou::NetworkDefinitionError::INVALID_OPERATOR_AUTHORITY_EPOCH);

    bad_auth_def = definition;
    bad_auth_def.operator_authority->retired_from_epoch = 10;
    BOOST_CHECK(cybou::ValidateNetworkDefinition(bad_auth_def) ==
                cybou::NetworkDefinitionError::INVALID_OPERATOR_AUTHORITY_EPOCH);
}

BOOST_AUTO_TEST_CASE(genesis_initializes_once_and_loads)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::NOT_FOUND);
    BOOST_CHECK(!store.GetStateRoot().has_value());
    BOOST_CHECK(!store.GetFinalizedHead().has_value());

    const auto genesis{GenesisState()};
    BOOST_CHECK(store.InitializeGenesis(genesis));
    BOOST_CHECK(!store.InitializeGenesis(genesis));

    const auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK(*loaded.state == genesis);
    BOOST_CHECK_EQUAL(loaded.state->onboarding_pool, 20000);
    BOOST_CHECK_EQUAL(loaded.state->security_reward_pool, 1000);
    BOOST_CHECK_EQUAL(loaded.state->pending_fee_pool, 500);

    const auto root{store.GetStateRoot()};
    BOOST_REQUIRE(root.has_value());
    BOOST_CHECK(*root == cybou::CybouStateHash(genesis));

    const auto head{store.GetFinalizedHead()};
    BOOST_REQUIRE(head.has_value());
    BOOST_CHECK(head->block_id == definition.genesis_block_id);
    BOOST_CHECK_EQUAL(head->height, 0);
    BOOST_CHECK(*store.GetFinalizedTip() == definition.genesis_block_id);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 0);
}

BOOST_AUTO_TEST_CASE(candidate_root_uses_canonical_state_and_height)
{
    auto db = MemoryDb();
    cybou::CybouStateStore store{db, TestNetworkDefinition()};
    BOOST_CHECK(!store.ComputeCandidateStateRoot({}, 1).has_value());
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    const auto operation = ValidOp();
    const auto expected = MakeFinalizedBlock(store, {operation});
    const auto root = store.ComputeCandidateStateRoot({operation}, 1);
    BOOST_REQUIRE(root.has_value());
    BOOST_CHECK(*root == expected.block.resulting_state_root);
    BOOST_CHECK(!store.ComputeCandidateStateRoot({operation}, 2).has_value());

    BOOST_REQUIRE(store.CommitFinalizedBlock(expected));
    BOOST_CHECK(!store.ComputeCandidateStateRoot({operation}, 1).has_value());
    BOOST_CHECK(!store.ComputeCandidateStateRoot({operation}, 2).has_value()); // Duplicate account.
}

BOOST_AUTO_TEST_CASE(genesis_rejects_invalid_definition_or_state_mismatch)
{
    auto db{MemoryDb()};
    auto bad_definition{TestNetworkDefinition()};
    bad_definition.genesis_state_root = uint256::FromUserHex("99").value();
    cybou::CybouStateStore store{db, bad_definition};
    BOOST_CHECK(store.InitializeGenesis(GenesisState()).error ==
        cybou::GenesisInitError::GENESIS_STATE_MISMATCH);

    bad_definition.protocol_version = 0;
    cybou::CybouStateStore invalid_store{db, bad_definition};
    BOOST_CHECK(invalid_store.InitializeGenesis(GenesisState()).error ==
        cybou::GenesisInitError::INVALID_NETWORK_DEFINITION);
}

BOOST_AUTO_TEST_CASE(state_store_detects_tampering_and_corruption)
{
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};
        const auto genesis{GenesisState()};

        // Missing hash: corrupt.
        BOOST_REQUIRE(store.InitializeGenesis(genesis));
        db.Erase(HASH_KEY);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};
        const auto genesis{GenesisState()};

        // Missing head: corrupt.
        BOOST_REQUIRE(store.InitializeGenesis(genesis));
        db.Erase(HEAD_KEY);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
    {
        auto db{MemoryDb()};
        cybou::CybouStateStore store{db, TestNetworkDefinition()};
        const auto genesis{GenesisState()};

        // Bit-flip in the stored state breaks the hash check.
        BOOST_REQUIRE(store.InitializeGenesis(genesis));
        BOOST_REQUIRE(store.LoadState());
        auto corrupt_bytes{cybou::SerializeCybouState(genesis)};
        corrupt_bytes.back() ^= 1;
        db.Write(STATE_KEY, corrupt_bytes);
        BOOST_CHECK(store.LoadState().error == cybou::StateLoadError::CORRUPT);
    }
}

BOOST_AUTO_TEST_CASE(state_store_rejects_reopen_with_different_network_definition)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore original{db, definition};
    BOOST_REQUIRE(original.InitializeGenesis(GenesisState()));

    cybou::CybouStateStore compatible{db, definition};
    BOOST_REQUIRE(compatible.LoadState());
    BOOST_CHECK(*compatible.GetStoredNetworkId() == compatible.GetNetworkId());

    auto incompatible_definition{definition};
    ++incompatible_definition.protocol_parameters.onboarding_bonus;
    cybou::CybouStateStore incompatible{db, incompatible_definition};
    BOOST_CHECK(incompatible.LoadState().error == cybou::StateLoadError::NETWORK_MISMATCH);

    auto mail_policy_definition{definition};
    ++mail_policy_definition.protocol_parameters.new_account_mail_limit_per_epoch;
    BOOST_CHECK(cybou::NetworkId(mail_policy_definition) != original.GetNetworkId());
    cybou::CybouStateStore mail_policy_store{db, mail_policy_definition};
    BOOST_CHECK(mail_policy_store.LoadState().error == cybou::StateLoadError::NETWORK_MISMATCH);

    auto authority_definition{definition};
    authority_definition.operator_authority = cybou::OperatorAuthorityKeySet{
        .keyset_id = uint256::ONE,
        .ed25519_public_key = {1},
        .mldsa65_public_key = {1},
        .active_from_epoch = 0,
        .retired_from_epoch = std::nullopt,
    };
    BOOST_CHECK(cybou::NetworkId(authority_definition) != original.GetNetworkId());
    cybou::CybouStateStore authority_store{db, authority_definition};
    BOOST_CHECK(authority_store.LoadState().error == cybou::StateLoadError::NETWORK_MISMATCH);

    const auto fb = MakeFinalizedBlock(original, {});
    BOOST_CHECK(incompatible.CommitFinalizedBlock(fb, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::NETWORK_MISMATCH);
    BOOST_REQUIRE(original.GetFinalizedTip().has_value());
    BOOST_CHECK(*original.GetFinalizedTip() == definition.genesis_block_id);
    BOOST_CHECK_EQUAL(*original.GetFinalizedHeight(), 0);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_is_tip_ordered_and_atomic)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    // Block 1 with wrong parent
    auto bad_parent_fb = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)}, TEST_VALIDATOR_SET, TEST_VAL_NODES,
        uint256::FromUserHex("99").value());
    BOOST_CHECK(store.CommitFinalizedBlock(bad_parent_fb, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);

    // Block 1 with correct parent
    auto fb1 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);

    // Block 2 with wrong parent
    auto bad_parent_fb2 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID_2)}, TEST_VALIDATOR_SET, TEST_VAL_NODES,
        definition.genesis_block_id);
    BOOST_CHECK(store.CommitFinalizedBlock(bad_parent_fb2, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);

    // Block 2 with correct parent
    auto fb2 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID_2)});
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);

    const auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK_EQUAL(loaded.state->onboarding_pool, 8125); // 20000 - 12000 + 125 fee routing
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID_2).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_ID_2).creation_epoch, 0);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_replay_of_applied_block)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_CHECK(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::BLOCK_ALREADY_APPLIED);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_state_root_mismatch)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    // Tamper with state root inside the block
    fb1.block.resulting_state_root = uint256::FromUserHex("beef").value();
    // Update certificate to match the new block ID
    const uint256 new_bid = cybou::ComputeBlockId(fb1.block);
    fb1.certificate.block_id = new_bid;
    const uint256 digest = cybou::ComputeBftCommitDigest(
        store.GetNetworkId(), new_bid, 1, 0, cybou::ComputeValidatorSetCommitment(TEST_VALIDATOR_SET));
    for (size_t i = 0; i < TEST_VAL_NODES.size(); ++i) {
        fb1.certificate.commit_votes[i].signature = *cybou::SignValidatorVote(TEST_VAL_NODES[i].priv, digest);
    }

    BOOST_CHECK(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::STATE_ROOT_MISMATCH);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_invalid_certificate)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    // Drop votes to below quorum (< 3)
    fb1.certificate.commit_votes.pop_back();
    fb1.certificate.commit_votes.pop_back();
    BOOST_CHECK(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::INVALID_CERTIFICATE);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_invalid_operation_without_mutation)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    const auto snapshot{store.LoadState()};
    BOOST_REQUIRE(snapshot);

    // Duplicate account creation in block 2
    auto fb2 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    const auto result{store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::INVALID_OPERATION);
    BOOST_CHECK(result.op_result.account_create_result.error == cybou::AccountCreateError::ACCOUNT_ALREADY_EXISTS);
    BOOST_REQUIRE(store.LoadState());
    BOOST_CHECK(*store.LoadState().state == *snapshot.state);
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_too_many_account_creates)
{
    auto db{MemoryDb()};
    cybou::CybouProtocolParameters strict_params{PARAMS};
    strict_params.max_account_creates_per_block = 1;
    auto strict_network{TestNetworkDefinition()};
    strict_network.protocol_parameters = strict_params;
    cybou::CybouStateStore store{db, strict_network};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    std::vector<cybou::ProtocolOperationV1> ops{
        ValidOp(ACCOUNT_ID),
        ValidOp(ACCOUNT_ID_2),
    };
    auto fb = MakeFinalizedBlock(store, ops);

    const auto result{store.CommitFinalizedBlock(fb, TEST_VALIDATOR_SET)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::TOO_MANY_ACCOUNT_CREATES);
    BOOST_CHECK(store.LoadState().error != cybou::StateLoadError::CORRUPT);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_requires_initialized_state)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, TestNetworkDefinition()};
    auto fb = MakeFinalizedBlock(store, {ValidOp()});
    BOOST_CHECK(store.CommitFinalizedBlock(fb, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::STATE_NOT_INITIALIZED);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_advances_height_monotonically)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID)});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);

    auto fb2 = MakeFinalizedBlock(store, {ValidOp(ACCOUNT_ID_2)});
    const uint256 block2_id = cybou::ComputeBlockId(fb2.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);
    BOOST_CHECK(*store.GetFinalizedTip() == block2_id);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_height_overflow)
{
    auto db{MemoryDb()};
    cybou::CybouStateStore store{db, TestNetworkDefinition()};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));
    const cybou::FinalizedHeadV1 max_head{
        .block_id = uint256::FromUserHex("51").value(),
        .height = std::numeric_limits<uint64_t>::max(),
    };
    db.Write(HEAD_KEY, max_head);

    auto fb = MakeFinalizedBlock(store, {}, TEST_VALIDATOR_SET, TEST_VAL_NODES, max_head.block_id, max_head.height + 1);
    BOOST_CHECK(store.CommitFinalizedBlock(fb, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::INVALID_HEIGHT);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_executes_authorized_payment)
{
    std::array<unsigned char, 32> priv1{};
    priv1.fill(0x55);
    const auto pub1{cybou::DeriveEd25519PublicKey(priv1)};
    BOOST_REQUIRE(pub1.has_value());

    std::array<unsigned char, 32> priv2{};
    priv2.fill(0x66);
    const auto pub2{cybou::DeriveEd25519PublicKey(priv2)};
    BOOST_REQUIRE(pub2.has_value());

    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    const auto net_id{cybou::NetworkId(definition)};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    // Block 1: Onboard Account 1 and Account 2
    auto op1{ValidOp(ACCOUNT_ID, priv1)};
    auto op2{ValidOp(ACCOUNT_ID_2, priv2)};

    auto fb1 = MakeFinalizedBlock(store, {op1, op2});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);

    // In state, fund Account 1 balance
    auto current_state{*store.LoadState().state};
    current_state.accounts.at(ACCOUNT_ID).balance = 5000;
    // Update store state for test
    db.Write(STATE_KEY, cybou::SerializeCybouState(current_state));
    db.Write(HASH_KEY, cybou::CybouStateHash(current_state));

    // Block 2: Authorized payment from Account 1 -> Account 2
    const cybou::PaymentOpV1 payment{
        .version = cybou::PAYMENT_OP_VERSION,
        .recipient = ACCOUNT_ID_2,
        .amount = 1500,
    };
    const uint256 digest{cybou::ComputeUserOperationDigest(net_id, ACCOUNT_ID, 0, payment)};
    const auto sig{cybou::SignUserMessage(priv1, std::span<const unsigned char>{digest.begin(), digest.size()})};
    BOOST_REQUIRE(sig.has_value());

    const cybou::AuthorizedOperationV1 auth_payment{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = ACCOUNT_ID,
        .nonce = 0,
        .payload = payment,
        .signature = *sig,
    };

    auto fb2 = MakeFinalizedBlock(store, {auth_payment});
    const uint256 block2_id = cybou::ComputeBlockId(fb2.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET));

    const auto final_state{store.LoadState()};
    BOOST_REQUIRE(final_state);
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID).balance, 3500); // 5000 - 1500
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID).system_balance, 5999); // 6000 - 1
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID).next_nonce, 1);
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_ID_2).balance, 1500);
    BOOST_CHECK_EQUAL(final_state.state->pending_fee_pool, 1); // 1 fee in pool (remainder of 4-chunk routing)
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);
    BOOST_CHECK(*store.GetFinalizedTip() == block2_id);

    // Verify GetBlock retrieves persisted finalized block
    const auto loaded_fb2 = store.GetBlock(block2_id);
    BOOST_REQUIRE(loaded_fb2.has_value());
    BOOST_CHECK(loaded_fb2->block == fb2.block);
    BOOST_CHECK(loaded_fb2->certificate == fb2.certificate);
}

namespace {

class MockStoreAuthorityVerifier final : public cybou::OperatorAuthoritySignatureVerifier
{
public:
    bool Verify(
        const cybou::OperatorAuthorityKeySet& keyset,
        const cybou::SignatureBundleV1& bundle,
        std::span<const unsigned char> message) const override
    {
        return cybou::IsPresent(bundle) && bundle.authority_keyset_id == keyset.keyset_id;
    }
};

cybou::SignatureBundleV1 CreateStoreMockSignatureBundle(const uint256& keyset_id)
{
    cybou::SignatureBundleV1 bundle;
    bundle.suite_id = cybou::SignatureSuiteId::HYBRID_ED25519_MLDSA65_V1;
    bundle.authority_keyset_id = keyset_id;
    bundle.classical_signature.fill(0x11);
    bundle.pq_signature.fill(0x22);
    return bundle;
}

} // namespace

BOOST_AUTO_TEST_CASE(store_genesis_validator_set_validation_and_access)
{
    auto db = MemoryDb();
    auto definition = TestNetworkDefinition();
    cybou::CybouStateStore store{db, definition};

    // 1. Rejects genesis with mismatched validator set commitment
    auto bad_genesis = GenesisState();
    bad_genesis.validator_set.validators.pop_back(); // 3 instead of 4
    // State hash mismatch occurs first if genesis_state_root was derived from original
    const auto bad_res = store.InitializeGenesis(bad_genesis);
    BOOST_CHECK(!bad_res);

    // 2. Initialize with valid genesis state matching network definition
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    // 3. GetValidatorSet returns canonical validator set
    const auto val_set = store.GetValidatorSet();
    BOOST_REQUIRE(val_set.has_value());
    BOOST_CHECK_EQUAL(val_set->Size(), 4);
    BOOST_CHECK_EQUAL(val_set->TotalWeight(), 4);
    BOOST_CHECK(*val_set == TEST_VALIDATOR_SET);

    // 4. Reject block commit with mismatched validator_set parameter
    auto bad_val_set = TEST_VALIDATOR_SET;
    bad_val_set.validators.pop_back();
    auto fb = MakeFinalizedBlock(store, {ValidOp()});
    const auto mismatch_res = store.CommitFinalizedBlock(fb, bad_val_set);
    BOOST_CHECK(mismatch_res.error == cybou::BlockTransitionError::VALIDATOR_SET_MISMATCH);
}

BOOST_AUTO_TEST_CASE(store_commits_block_with_validator_admission_and_removal)
{
    auto db = MemoryDb();
    auto definition = TestNetworkDefinition();

    const uint256 keyset_id{uint256::FromUserHex("aa").value()};
    const cybou::OperatorAuthorityKeySet authority{
        .keyset_id = keyset_id,
        .ed25519_public_key = {1},
        .mldsa65_public_key = {1},
        .active_from_epoch = 0,
        .retired_from_epoch = std::nullopt,
    };
    auto verifier = std::make_shared<MockStoreAuthorityVerifier>();

    definition.operator_authority = authority;
    cybou::CybouStateStore store{db, definition, verifier};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    // Create 5th validator node
    MockVal node5;
    node5.priv.fill(0);
    node5.priv[0] = 5;
    node5.pub = *cybou::DeriveEd25519PublicKey(node5.priv);

    const cybou::ValidatorAdmissionOpV1 admission_op{
        .version = cybou::VALIDATOR_ADMISSION_OP_VERSION,
        .validator_id = node5.pub,
        .consensus_public_key = node5.pub,
        .activation_epoch = 0,
        .operator_signature = CreateStoreMockSignatureBundle(keyset_id),
    };

    // Block 1: admit 5th validator, signed by current 4 validators
    auto fb1 = MakeFinalizedBlock(store, {cybou::ProtocolOperationV1{admission_op}}, TEST_VALIDATOR_SET, TEST_VAL_NODES, std::nullopt, std::nullopt, std::nullopt, verifier.get());
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1));

    // Active validator set is now 5
    const auto val_set_h1 = store.GetValidatorSet();
    BOOST_REQUIRE(val_set_h1.has_value());
    BOOST_CHECK_EQUAL(val_set_h1->Size(), 5);
    BOOST_CHECK_EQUAL(val_set_h1->QuorumThreshold(), 4); // floor(2*5/3) + 1 = 4
    BOOST_CHECK(val_set_h1->FindValidator(node5.pub) != nullptr);

    // Prepare node list of 5 nodes
    auto all_5_nodes = TEST_VAL_NODES;
    all_5_nodes.push_back(node5);

    // Block 2: regular operation, signed by 4 of the 5 validators (quorum satisfied)
    std::vector<MockVal> four_signers = {all_5_nodes[0], all_5_nodes[1], all_5_nodes[2], all_5_nodes[4]};
    auto account_op = ValidOp();
    account_op.creation_work.network_id = store.GetNetworkId();
    account_op.proof_of_possession = *cybou::SignUserMessage(
        AUTH_PRIVKEY, cybou::ComputeAccountPopDigest(store.GetNetworkId(), ACCOUNT_ID, AUTH_KEY));
    auto fb2 = MakeFinalizedBlock(store, {account_op}, *val_set_h1, four_signers, std::nullopt, std::nullopt, std::nullopt, verifier.get());
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb2));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);

    // Block 3: remove the 5th validator
    const cybou::ValidatorRemovalOpV1 removal_op{
        .version = cybou::VALIDATOR_REMOVAL_OP_VERSION,
        .validator_id = node5.pub,
        .effective_epoch = 0,
        .operator_signature = CreateStoreMockSignatureBundle(keyset_id),
    };
    auto fb3 = MakeFinalizedBlock(store, {cybou::ProtocolOperationV1{removal_op}}, *val_set_h1, four_signers, std::nullopt, std::nullopt, std::nullopt, verifier.get());
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb3));

    // Active validator set is back to 4
    const auto val_set_h3 = store.GetValidatorSet();
    BOOST_REQUIRE(val_set_h3.has_value());
    BOOST_CHECK_EQUAL(val_set_h3->Size(), 4);
    BOOST_CHECK(val_set_h3->FindValidator(node5.pub) == nullptr);
    BOOST_CHECK(*val_set_h3 == TEST_VALIDATOR_SET);
}

BOOST_AUTO_TEST_CASE(store_rejects_removal_of_last_validator_in_authority_mode)
{
    auto db = MemoryDb();

    // Setup 1-validator Authority Mode network
    const cybou::ValidatorSetV1 authority_val_set{
        .version = cybou::VALIDATOR_SET_VERSION,
        .validators = {
            cybou::ValidatorV1{.validator_id = TEST_VAL_NODES[0].pub, .consensus_public_key = TEST_VAL_NODES[0].pub, .weight = 1},
        },
    };

    cybou::CybouState auth_genesis_state{
        .onboarding_pool = 20000,
        .security_reward_pool = 1000,
        .pending_fee_pool = 500,
        .accounts{},
        .validator_set = authority_val_set,
    };

    cybou::CybouNetworkDefinitionV1 auth_definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = uint256::ONE,
        .genesis_state_root = cybou::CybouStateHash(auth_genesis_state),
        .protocol_parameters = PARAMS,
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(authority_val_set),
    };

    const uint256 keyset_id{uint256::FromUserHex("aa").value()};
    const cybou::OperatorAuthorityKeySet authority{
        .keyset_id = keyset_id,
        .ed25519_public_key = {1},
        .mldsa65_public_key = {1},
        .active_from_epoch = 0,
        .retired_from_epoch = std::nullopt,
    };
    auto verifier = std::make_shared<MockStoreAuthorityVerifier>();

    auth_definition.operator_authority = authority;
    cybou::CybouStateStore store{db, auth_definition, verifier};
    BOOST_REQUIRE(store.InitializeGenesis(auth_genesis_state));
    BOOST_CHECK_EQUAL(store.GetValidatorSet()->Size(), 1);

    // Attempt to remove the sole validator
    const cybou::ValidatorRemovalOpV1 remove_last_op{
        .version = cybou::VALIDATOR_REMOVAL_OP_VERSION,
        .validator_id = TEST_VAL_NODES[0].pub,
        .effective_epoch = 0,
        .operator_signature = CreateStoreMockSignatureBundle(keyset_id),
    };

    auto fb = MakeFinalizedBlock(store, {cybou::ProtocolOperationV1{remove_last_op}}, authority_val_set, {TEST_VAL_NODES[0]}, std::nullopt, std::nullopt, std::nullopt, verifier.get());
    const auto result = store.CommitFinalizedBlock(fb);
    BOOST_CHECK(!result);
    BOOST_CHECK(result.error == cybou::BlockTransitionError::INVALID_OPERATION);
    BOOST_CHECK(result.op_result.validator_removal_result.error == cybou::ValidatorRemovalError::CANNOT_REMOVE_LAST_VALIDATOR);

    // State is untouched: validator set still has 1 validator
    BOOST_CHECK_EQUAL(store.GetValidatorSet()->Size(), 1);
}

BOOST_AUTO_TEST_SUITE_END()
