// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/block_executor.h>
#include <cybou/identity.h>
#include <cybou/identity_authorization.h>
#include <cybou/identity_crypto.h>
#include <cybou/network_definition.h>
#include <cybou/payment.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
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

const std::string STATE_KEY{"cybou/state"};
const std::string HASH_KEY{"cybou/hash"};
const std::string HEAD_KEY{"cybou/head"};

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

const std::vector<MockValidatorNode> TEST_VAL_NODES = []{
    std::vector<MockValidatorNode> nodes;
    for (uint8_t i = 1; i <= 4; ++i) {
        nodes.push_back(MockValidatorNode::Create(i));
    }
    return nodes;
}();

const cybou::ValidatorSet TEST_VALIDATOR_SET = []{
    cybou::ValidatorSet val_set;
    for (const auto& node : TEST_VAL_NODES) {
        val_set.validators.push_back(cybou::Validator{
            .validator_id = node.validator_id,
            .consensus_public_key = node.consensus_pubkey,
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
        .identities{},
        .validator_set = TEST_VALIDATOR_SET,
        .names{},
    };
}

cybou::CybouNetworkDefinition TestNetworkDefinition()
{
    const auto genesis = GenesisState();
    return cybou::CybouNetworkDefinition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = uint256::ONE,
        .genesis_state_root = *cybou::CybouStateHash(genesis),
        .protocol_parameters = PARAMS,
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(TEST_VALIDATOR_SET),
        .operator_authority = std::nullopt,
    };
}

struct AccountCredentials {
    cybou::AccountId account_id;
    std::array<unsigned char, 32> root_seed{};
    std::array<unsigned char, 32> device_seed{};
    cybou::IdentityHybridPublicKey root_pubkey;
    cybou::IdentityHybridPublicKey device_pubkey;
    cybou::IdentityKeyId device_id{};
    cybou::IdentityAuthorization auth;

    static AccountCredentials Create(uint8_t id_byte, uint8_t root_byte, uint8_t dev_byte)
    {
        AccountCredentials creds;
        uint256 raw{};
        raw.begin()[0] = id_byte;
        creds.account_id = cybou::AccountId{raw};
        creds.root_seed[0] = root_byte;
        creds.device_seed[0] = dev_byte;
        creds.root_pubkey = *cybou::DeriveIdentityPublicKey(creds.root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT);
        creds.device_pubkey = *cybou::DeriveIdentityPublicKey(creds.device_seed, cybou::IdentityKeyPurpose::DEVICE);
        creds.device_id = *cybou::ComputeDeviceKeyId(creds.device_pubkey);
        creds.auth = cybou::IdentityAuthorization{creds.root_pubkey, creds.device_pubkey};
        return creds;
    }

    cybou::AccountCreateOp MakeCreateOp(const uint256& network_id) const
    {
        const auto commitment = *cybou::ComputeIdentityAuthorizationCommitment(auth);
        const auto digest = *cybou::ComputeAccountCreatePopDigest(network_id, account_id, auth);
        const auto root_pop = *cybou::SignIdentityMessage(root_seed, cybou::IdentityKeyPurpose::RECOVERY_ROOT, digest);
        const auto device_pop = *cybou::SignIdentityMessage(device_seed, cybou::IdentityKeyPurpose::DEVICE, digest);
        return cybou::AccountCreateOp{
            account_id,
            auth,
            {.network_id = network_id, .account_id = account_id, .authorization_commitment = commitment},
            root_pop,
            device_pop,
        };
    }
};

const AccountCredentials ACCOUNT_1 = AccountCredentials::Create(0x0a, 0x11, 0x12);
const AccountCredentials ACCOUNT_2 = AccountCredentials::Create(0x0b, 0x21, 0x22);

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

bool StatesEqual(const cybou::CybouState& a, const cybou::CybouState& b)
{
    return cybou::SerializeCybouState(a) == cybou::SerializeCybouState(b);
}

cybou::FinalizedBlock MakeFinalizedBlock(
    const cybou::CybouStateStore& store,
    const std::vector<cybou::ProtocolOperation>& ops,
    const cybou::ValidatorSet& val_set = TEST_VALIDATOR_SET,
    const std::vector<MockValidatorNode>& vals = TEST_VAL_NODES,
    std::optional<uint256> override_parent = std::nullopt,
    std::optional<uint64_t> override_height = std::nullopt,
    std::optional<uint256> override_network_id = std::nullopt)
{
    const auto loaded = store.LoadState();
    const uint64_t height = override_height.value_or(store.GetFinalizedHeight().value_or(0) + 1);
    const uint256 parent = override_parent.value_or(store.GetFinalizedTip().value_or(uint256{}));
    const uint256 net_id = override_network_id.value_or(store.GetNetworkId());

    cybou::CybouState candidate = loaded && loaded.state ? *loaded.state : GenesisState();
    const auto exec_res = cybou::ExecuteBlockOperations(candidate, ops, net_id, height, PARAMS);
    const uint256 state_root = exec_res.state_root.value_or(uint256{});

    cybou::CybouBlock block{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = parent,
        .height = height,
        .operations = ops,
        .resulting_state_root = state_root,
    };
    const uint256 block_id = cybou::ComputeBlockId(block);

    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(val_set);

    cybou::BftFinalityCertificate cert{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = net_id,
        .block_id = block_id,
        .height = height,
        .round = 0,
        .validator_set_commitment = val_set_commitment,
        .commit_votes = {},
    };
    for (const auto& v : vals) {
        cert.commit_votes.push_back(v.SignCommit(net_id, block_id, height, val_set_commitment));
    }

    return cybou::FinalizedBlock{
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
    BOOST_CHECK(StatesEqual(*loaded.state, genesis));
    BOOST_CHECK_EQUAL(loaded.state->onboarding_pool, 20000);
    BOOST_CHECK_EQUAL(loaded.state->security_reward_pool, 1000);
    BOOST_CHECK_EQUAL(loaded.state->pending_fee_pool, 500);

    const auto root{store.GetStateRoot()};
    BOOST_REQUIRE(root.has_value());
    BOOST_CHECK(*root == *cybou::CybouStateHash(genesis));

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

    const auto operation = ACCOUNT_1.MakeCreateOp(store.GetNetworkId());
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
        BOOST_REQUIRE(corrupt_bytes.has_value());
        corrupt_bytes->back() ^= 1;
        db.Write(STATE_KEY, *corrupt_bytes);
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
    auto bad_parent_fb = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())}, TEST_VALIDATOR_SET, TEST_VAL_NODES,
        uint256::FromUserHex("99").value());
    BOOST_CHECK(store.CommitFinalizedBlock(bad_parent_fb, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);

    // Block 1 with correct parent
    auto fb1 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_REQUIRE(store.GetFinalizedTip().has_value());
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);

    // Block 2 with wrong parent
    auto bad_parent_fb2 = MakeFinalizedBlock(store, {ACCOUNT_2.MakeCreateOp(store.GetNetworkId())}, TEST_VALIDATOR_SET, TEST_VAL_NODES,
        definition.genesis_block_id);
    BOOST_CHECK(store.CommitFinalizedBlock(bad_parent_fb2, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::PARENT_MISMATCH);

    // Block 2 with correct parent
    auto fb2 = MakeFinalizedBlock(store, {ACCOUNT_2.MakeCreateOp(store.GetNetworkId())});
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);

    const auto loaded{store.LoadState()};
    BOOST_REQUIRE(loaded);
    BOOST_CHECK_EQUAL(loaded.state->onboarding_pool, 8125); // Genesis pending fees route 125 to onboarding.
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_1.account_id).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_2.account_id).system_balance, 6000);
    BOOST_CHECK_EQUAL(loaded.state->accounts.at(ACCOUNT_2.account_id).creation_epoch, 0);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_rejects_replay_of_applied_block)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
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

    auto fb1 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    // Tamper with state root inside the block
    fb1.block.resulting_state_root = uint256::FromUserHex("beef").value();
    // Update certificate to match the new block ID
    const uint256 new_bid = cybou::ComputeBlockId(fb1.block);
    fb1.certificate.block_id = new_bid;
    const uint256 val_set_commitment = cybou::ComputeValidatorSetCommitment(TEST_VALIDATOR_SET);
    for (size_t i = 0; i < TEST_VAL_NODES.size(); ++i) {
        fb1.certificate.commit_votes[i] = TEST_VAL_NODES[i].SignCommit(
            store.GetNetworkId(), new_bid, 1, val_set_commitment);
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

    auto fb1 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
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

    auto fb1 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    const auto snapshot{store.LoadState()};
    BOOST_REQUIRE(snapshot);

    // Duplicate account creation in block 2
    auto fb2 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    const auto result{store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET)};
    BOOST_CHECK(result.error == cybou::BlockTransitionError::INVALID_OPERATION);
    BOOST_CHECK(result.op_result.error == cybou::BlockExecutionError::INVALID_ACCOUNT_CREATE);
    BOOST_CHECK(result.op_result.create_error == cybou::AccountCreateStateError::ACCOUNT_EXISTS);
    BOOST_REQUIRE(store.LoadState());
    BOOST_CHECK(StatesEqual(*store.LoadState().state, *snapshot.state));
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

    std::vector<cybou::ProtocolOperation> ops{
        ACCOUNT_1.MakeCreateOp(store.GetNetworkId()),
        ACCOUNT_2.MakeCreateOp(store.GetNetworkId()),
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
    auto fb = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    BOOST_CHECK(store.CommitFinalizedBlock(fb, TEST_VALIDATOR_SET).error ==
        cybou::BlockTransitionError::STATE_NOT_INITIALIZED);
}

BOOST_AUTO_TEST_CASE(commit_finalized_block_advances_height_monotonically)
{
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    auto fb1 = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1);
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);

    auto fb2 = MakeFinalizedBlock(store, {ACCOUNT_2.MakeCreateOp(store.GetNetworkId())});
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
    const cybou::FinalizedHead max_head{
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
    auto db{MemoryDb()};
    const auto definition{TestNetworkDefinition()};
    const auto net_id{cybou::NetworkId(definition)};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    // Block 1: Onboard Account 1 and Account 2
    auto op1 = ACCOUNT_1.MakeCreateOp(net_id);
    auto op2 = ACCOUNT_2.MakeCreateOp(net_id);

    auto fb1 = MakeFinalizedBlock(store, {op1, op2});
    const uint256 block1_id = cybou::ComputeBlockId(fb1.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb1, TEST_VALIDATOR_SET));
    BOOST_CHECK(*store.GetFinalizedTip() == block1_id);

    // In state, fund Account 1 balance
    auto current_state{*store.LoadState().state};
    current_state.accounts.at(ACCOUNT_1.account_id).balance = 5000;
    // Update store state for test
    const auto state_bytes = cybou::SerializeCybouState(current_state);
    BOOST_REQUIRE(state_bytes.has_value());
    db.Write(STATE_KEY, *state_bytes);
    db.Write(HASH_KEY, *cybou::CybouStateHash(current_state));

    // Block 2: Authorized payment from Account 1 -> Account 2
    cybou::AuthorizedPayment payment{};
    payment.payment.recipient = ACCOUNT_2.account_id;
    payment.payment.amount = 1500;
    payment.authorization.account_id = ACCOUNT_1.account_id;
    payment.authorization.device_id = ACCOUNT_1.device_id;
    payment.authorization.nonce = 0;
    payment.authorization.activation_nonce = 0;
    payment.authorization.kind = cybou::DeviceOperationKind::PAYMENT;
    payment.authorization.payload_commitment = *cybou::ComputePaymentPayloadCommitment(payment.payment);
    const auto payment_digest = *cybou::ComputeDeviceOperationDigest(net_id, payment.authorization);
    payment.authorization.signature = *cybou::SignIdentityMessage(
        ACCOUNT_1.device_seed, cybou::IdentityKeyPurpose::DEVICE, payment_digest);

    auto fb2 = MakeFinalizedBlock(store, {payment});
    const uint256 block2_id = cybou::ComputeBlockId(fb2.block);
    BOOST_REQUIRE(store.CommitFinalizedBlock(fb2, TEST_VALIDATOR_SET));

    const auto final_state{store.LoadState()};
    BOOST_REQUIRE(final_state);
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_1.account_id).balance, 3500); // 5000 - 1500
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_1.account_id).system_balance, 5999);
    BOOST_CHECK_EQUAL(final_state.state->identities.Find(ACCOUNT_1.account_id)->devices.at(ACCOUNT_1.device_id).next_nonce, 1);
    BOOST_CHECK_EQUAL(final_state.state->accounts.at(ACCOUNT_2.account_id).balance, 1500);
    BOOST_CHECK_EQUAL(final_state.state->pending_fee_pool, 1);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2);
    BOOST_CHECK(*store.GetFinalizedTip() == block2_id);

    // Verify GetBlock retrieves persisted finalized block
    const auto loaded_fb2 = store.GetBlock(block2_id);
    BOOST_REQUIRE(loaded_fb2.has_value());
    BOOST_CHECK(loaded_fb2->block == fb2.block);
    BOOST_CHECK(loaded_fb2->certificate == fb2.certificate);
}

BOOST_AUTO_TEST_CASE(store_genesis_validator_set_validation_and_access)
{
    auto db = MemoryDb();
    auto definition = TestNetworkDefinition();
    cybou::CybouStateStore store{db, definition};

    // 1. Rejects genesis with mismatched validator set commitment
    auto bad_genesis = GenesisState();
    bad_genesis.validator_set.validators.pop_back(); // 3 instead of 4
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
    auto fb = MakeFinalizedBlock(store, {ACCOUNT_1.MakeCreateOp(store.GetNetworkId())});
    const auto mismatch_res = store.CommitFinalizedBlock(fb, bad_val_set);
    BOOST_CHECK(mismatch_res.error == cybou::BlockTransitionError::VALIDATOR_SET_MISMATCH);
}

BOOST_AUTO_TEST_SUITE_END()
