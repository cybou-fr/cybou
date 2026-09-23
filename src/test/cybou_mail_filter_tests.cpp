// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/mail_filter.h>

#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/network_definition.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
#include <cybou/state_store.h>
#include <cybou/validator.h>
#include <dbwrapper.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <vector>

namespace {

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
        .pending_fee_pool = 0,
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

std::unique_ptr<CDBWrapper> MakeTestDB(const std::filesystem::path& path)
{
    return std::make_unique<CDBWrapper>(DBParams{
        .path = path,
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    });
}

cybou::FinalizedBlockV1 MakeFinalizedBlock(
    const cybou::CybouStateStore& store,
    const std::vector<cybou::ProtocolOperationV1>& ops)
{
    const auto loaded = store.LoadState();
    const uint64_t height = store.GetFinalizedHeight().value_or(0) + 1;
    const uint256 parent = store.GetFinalizedTip().value_or(uint256{});
    const uint256 net_id = store.GetNetworkId();

    cybou::CybouState candidate = loaded && loaded.state ? *loaded.state : GenesisState();
    const cybou::ProtocolExecutionContextV1 ctx{
        .network_id = net_id,
        .block_height = height,
        .params = PARAMS,
    };
    for (const auto& op : ops) {
        cybou::ApplyProtocolOperation(op, ctx, candidate);
    }
    if (candidate.pending_fee_pool > 0) {
        cybou::RoutePendingFees(candidate);
    }

    cybou::CybouBlockV1 block{
        .version = cybou::CYBOU_BLOCK_VERSION,
        .parent_block_id = parent,
        .height = height,
        .operations = ops,
        .resulting_state_root = cybou::CybouStateHash(candidate),
    };

    const uint256 block_id = cybou::ComputeBlockId(block);
    const uint256 val_set_comm = cybou::ComputeValidatorSetCommitment(candidate.validator_set);
    const uint256 digest = cybou::ComputeBftCommitDigest(net_id, block_id, height, 0, val_set_comm);

    cybou::BftFinalityCertificateV1 cert{
        .version = cybou::BFT_FINALITY_CERTIFICATE_VERSION,
        .network_id = net_id,
        .block_id = block_id,
        .height = height,
        .round = 0,
        .validator_set_commitment = val_set_comm,
        .commit_votes = {},
    };
    for (size_t i = 0; i < 3; ++i) {
        cert.commit_votes.push_back(cybou::BftCommitVoteV1{
            .validator_id = TEST_VAL_NODES[i].pub,
            .signature = *cybou::SignValidatorVote(TEST_VAL_NODES[i].priv, digest),
        });
    }

    return cybou::FinalizedBlockV1{
        .block = std::move(block),
        .certificate = std::move(cert),
    };
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(cybou_mail_filter_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(recipient_discovery_tag_derivation)
{
    const uint256 recipient_1{uint256::FromUserHex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20").value()};
    const uint256 recipient_2{uint256::FromUserHex("201f1e1d1c1b1a191817161514131211100f0e0d0c0b0a090807060504030201").value()};
    const uint256 salt_a{uint256::FromUserHex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa").value()};
    const uint256 salt_b{uint256::FromUserHex("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb").value()};

    // Deterministic tag derivation
    const uint256 tag_1a = cybou::ComputeRecipientDiscoveryTag(recipient_1, salt_a);
    const uint256 tag_1a_repeat = cybou::ComputeRecipientDiscoveryTag(recipient_1, salt_a);
    BOOST_CHECK(tag_1a == tag_1a_repeat);
    BOOST_CHECK(!tag_1a.IsNull());

    // Unlinkability: different salt for same recipient generates completely different tag
    const uint256 tag_1b = cybou::ComputeRecipientDiscoveryTag(recipient_1, salt_b);
    BOOST_CHECK(tag_1a != tag_1b);

    // Different recipient with same salt generates completely different tag
    const uint256 tag_2a = cybou::ComputeRecipientDiscoveryTag(recipient_2, salt_a);
    BOOST_CHECK(tag_1a != tag_2a);
    BOOST_CHECK(tag_1b != tag_2a);
}

BOOST_AUTO_TEST_CASE(mail_discovery_filter_empty_block)
{
    const uint256 block_id{uint256::FromUserHex("11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff").value()};
    const auto filter = cybou::BuildMailDiscoveryFilter(block_id, {});

    BOOST_CHECK_EQUAL(filter.version, cybou::MAIL_DISCOVERY_FILTER_VERSION);
    BOOST_CHECK(filter.block_id == block_id);
    BOOST_CHECK_EQUAL(filter.num_elements, 0U);
    BOOST_CHECK(!filter.encoded_filter.empty());

    const uint256 any_tag{uint256::FromUserHex("cafebabecafebabecafebabecafebabecafebabecafebabecafebabecafebabe").value()};
    BOOST_CHECK(!filter.Match(any_tag));

    std::vector<uint256> query_tags{any_tag, uint256::ONE};
    BOOST_CHECK(!filter.MatchAny(query_tags));

    // Serialization roundtrip
    const auto serialized = cybou::SerializeMailDiscoveryFilter(filter);
    const auto deserialized = cybou::DeserializeMailDiscoveryFilter(serialized);
    BOOST_REQUIRE(deserialized.has_value());
    BOOST_CHECK(*deserialized == filter);
    BOOST_CHECK_EQUAL(deserialized->num_elements, 0U);
    BOOST_CHECK(!deserialized->Match(any_tag));
}

BOOST_AUTO_TEST_CASE(mail_discovery_filter_positive_and_negative_matching)
{
    const uint256 block_id{uint256::FromUserHex("4242424242424242424242424242424242424242424242424242424242424242").value()};

    std::vector<uint256> tags;
    for (uint8_t i = 1; i <= 8; ++i) {
        uint256 recipient;
        recipient.begin()[0] = i;
        uint256 salt;
        salt.begin()[0] = i * 10;
        tags.push_back(cybou::ComputeRecipientDiscoveryTag(recipient, salt));
    }

    const auto filter = cybou::BuildMailDiscoveryFilter(block_id, tags);
    BOOST_CHECK_EQUAL(filter.num_elements, 8U);

    // All inserted tags must match
    for (const auto& tag : tags) {
        BOOST_CHECK(filter.Match(tag));
    }

    // MatchAny returns true when at least one tag is present
    BOOST_CHECK(filter.MatchAny(tags));

    std::vector<uint256> mixed_tags{
        uint256::FromUserHex("deadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef").value(),
        tags[3],
    };
    BOOST_CHECK(filter.MatchAny(mixed_tags));

    // Non-member tags should not match
    const uint256 absent_tag{uint256::FromUserHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff").value()};
    BOOST_CHECK(!filter.Match(absent_tag));

    std::vector<uint256> absent_tags;
    for (uint8_t i = 1; i <= 5; ++i) {
        uint256 t;
        t.begin()[0] = 0xee;
        t.begin()[1] = i;
        absent_tags.push_back(t);
    }
    BOOST_CHECK(!filter.MatchAny(absent_tags));

    // Block ID keying: same tags encoded under another block ID should not match queries against this filter
    const uint256 other_block_id{uint256::FromUserHex("9999999999999999999999999999999999999999999999999999999999999999").value()};
    cybou::CybouMailDiscoveryFilterV1 filter_with_wrong_block = filter;
    filter_with_wrong_block.block_id = other_block_id;
    // With different siphash keys, matching against the original tags will fail
    size_t wrong_matches = 0;
    for (const auto& tag : tags) {
        if (filter_with_wrong_block.Match(tag)) {
            ++wrong_matches;
        }
    }
    // High probability of zero matches due to M = 784,931
    BOOST_CHECK_EQUAL(wrong_matches, 0U);
}

BOOST_AUTO_TEST_CASE(mail_discovery_filter_header_chaining)
{
    const uint256 block_1{uint256::FromUserHex("0101010101010101010101010101010101010101010101010101010101010101").value()};
    const uint256 block_2{uint256::FromUserHex("0202020202020202020202020202020202020202020202020202020202020202").value()};

    const uint256 tag1 = cybou::ComputeRecipientDiscoveryTag(uint256::ONE, uint256::FromUserHex("0a").value());
    const uint256 tag2 = cybou::ComputeRecipientDiscoveryTag(uint256::ONE, uint256::FromUserHex("0b").value());

    const auto filter1 = cybou::BuildMailDiscoveryFilter(block_1, std::span<const uint256>(&tag1, 1));
    const auto filter2 = cybou::BuildMailDiscoveryFilter(block_2, std::span<const uint256>(&tag2, 1));

    const uint256 hash1 = filter1.ComputeFilterHash();
    const uint256 hash2 = filter2.ComputeFilterHash();
    BOOST_CHECK(!hash1.IsNull());
    BOOST_CHECK(!hash2.IsNull());
    BOOST_CHECK(hash1 != hash2);

    const uint256 prev_header{uint256::ZERO};
    const uint256 header1 = filter1.ComputeFilterHeader(prev_header);
    const uint256 header2 = filter2.ComputeFilterHeader(header1);

    BOOST_CHECK(!header1.IsNull());
    BOOST_CHECK(!header2.IsNull());
    BOOST_CHECK(header1 != prev_header);
    BOOST_CHECK(header2 != header1);

    // Chaining determinism: computing header with different prev_header gives different result
    const uint256 diff_prev{uint256::ONE};
    BOOST_CHECK(filter1.ComputeFilterHeader(diff_prev) != header1);
}

BOOST_AUTO_TEST_CASE(mail_discovery_filter_serialization_validation)
{
    const uint256 block_id{uint256::FromUserHex("7777777777777777777777777777777777777777777777777777777777777777").value()};
    const uint256 tag{uint256::FromUserHex("8888888888888888888888888888888888888888888888888888888888888888").value()};
    const auto filter = cybou::BuildMailDiscoveryFilter(block_id, std::span<const uint256>(&tag, 1));

    auto bytes = cybou::SerializeMailDiscoveryFilter(filter);
    BOOST_CHECK(cybou::DeserializeMailDiscoveryFilter(bytes).has_value());

    // Truncated bytes
    std::vector<unsigned char> short_bytes(bytes.begin(), bytes.begin() + 10);
    BOOST_CHECK(!cybou::DeserializeMailDiscoveryFilter(short_bytes).has_value());

    // Corrupted version
    bytes[0] = 99;
    BOOST_CHECK(!cybou::DeserializeMailDiscoveryFilter(bytes).has_value());

    bytes = cybou::SerializeMailDiscoveryFilter(filter);
    bytes[33] ^= 1; // Declared element count must agree with the encoded GCS count.
    BOOST_CHECK(!cybou::DeserializeMailDiscoveryFilter(bytes).has_value());
}

BOOST_AUTO_TEST_CASE(state_store_mail_filter_persistence_and_retrieval)
{
    const auto db = MakeTestDB(m_args.GetDataDirNet() / "cybou_filter_test_db");
    cybou::CybouStateStore store(*db, TestNetworkDefinition());

    // Genesis initialization persists empty mail filter for genesis block
    BOOST_REQUIRE(store.InitializeGenesis(GenesisState()));

    const uint256 genesis_id = TestNetworkDefinition().genesis_block_id;
    const auto gen_filter = store.GetBlockMailFilter(genesis_id);
    BOOST_REQUIRE(gen_filter.has_value());
    BOOST_CHECK(gen_filter->block_id == genesis_id);
    BOOST_CHECK_EQUAL(gen_filter->num_elements, 0U);

    // Non-existent block filter returns nullopt
    BOOST_CHECK(!store.GetBlockMailFilter(uint256::FromUserHex("1234").value()).has_value());

    // Setup an account via AccountCreateOp
    const std::array<unsigned char, 32> priv = []{
        std::array<unsigned char, 32> k{};
        k[0] = 0x55;
        return k;
    }();
    const uint256 pub = *cybou::DeriveEd25519PublicKey(priv);
    const cybou::AccountId sender_acc{uint256::FromUserHex("55").value()};

    cybou::AccountCreateOpV1 create_op{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = sender_acc,
        .initial_authorization = cybou::AccountAuthorizationV1{.authorization_descriptor = pub},
        .creation_work = {
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = store.GetNetworkId(),
            .account_id = sender_acc,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(cybou::AccountAuthorizationV1{.authorization_descriptor = pub}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    create_op.proof_of_possession = *cybou::SignUserMessage(
        priv, cybou::ComputeAccountPopDigest(store.GetNetworkId(), sender_acc, pub));

    const cybou::AccountId recipient_acc{uint256::FromUserHex("66").value()};
    const std::array<unsigned char, 32> recipient_priv = []{
        std::array<unsigned char, 32> k{};
        k[0] = 0x66;
        return k;
    }();
    const uint256 recipient_pub = *cybou::DeriveEd25519PublicKey(recipient_priv);
    cybou::AccountCreateOpV1 recipient_create{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = recipient_acc,
        .initial_authorization = cybou::AccountAuthorizationV1{.authorization_descriptor = recipient_pub},
        .creation_work = {
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = store.GetNetworkId(),
            .account_id = recipient_acc,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(cybou::AccountAuthorizationV1{.authorization_descriptor = recipient_pub}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    recipient_create.proof_of_possession = *cybou::SignUserMessage(
        recipient_priv, cybou::ComputeAccountPopDigest(store.GetNetworkId(), recipient_acc, recipient_pub));

    const auto finalized_block1 = MakeFinalizedBlock(store, {create_op, recipient_create});
    const auto res1 = store.CommitFinalizedBlock(finalized_block1);
    BOOST_REQUIRE(res1);

    const uint256 block1_id = cybou::ComputeBlockId(finalized_block1.block);
    const auto filter1 = store.GetBlockMailFilter(block1_id);
    BOOST_REQUIRE(filter1.has_value());
    BOOST_CHECK(filter1->block_id == block1_id);
    // Block 1 only had AccountCreateOp, no mail operations
    BOOST_CHECK_EQUAL(filter1->num_elements, 0U);

    // Block 2: Sender sends MailOpV1
    const uint256 mail_salt{uint256::FromUserHex("aabbccdd").value()};
    const uint256 discovery_tag = cybou::ComputeRecipientDiscoveryTag(recipient_acc.Value(), mail_salt);
    const std::vector<unsigned char> ciphertext{0x01, 0x02, 0x03, 0x04};
    const uint256 content_comm = cybou::ComputeMailContentCommitment(mail_salt, ciphertext);

    const cybou::MailOpV1 mail_op{
        .version = cybou::MAIL_OP_VERSION,
        .recipient = recipient_acc,
        .content_commitment = content_comm,
        .discovery_tag = discovery_tag,
        .ciphertext = ciphertext,
    };

    const cybou::AuthorizedOperationPayloadV1 payload{mail_op};
    const uint256 digest = cybou::ComputeUserOperationDigest(store.GetNetworkId(), sender_acc, 0, payload);

    cybou::AuthorizedOperationV1 auth_op{
        .version = cybou::AUTHORIZED_OPERATION_VERSION,
        .account_id = sender_acc,
        .nonce = 0,
        .payload = payload,
        .signature = *cybou::SignUserMessage(priv, std::span<const unsigned char>{digest.begin(), digest.size()}),
    };

    const auto finalized_block2 = MakeFinalizedBlock(store, {auth_op});
    const auto res2 = store.CommitFinalizedBlock(finalized_block2);
    BOOST_REQUIRE(res2);

    const uint256 block2_id = cybou::ComputeBlockId(finalized_block2.block);
    const auto filter2 = store.GetBlockMailFilter(block2_id);
    BOOST_REQUIRE(filter2.has_value());
    BOOST_CHECK(filter2->block_id == block2_id);
    BOOST_CHECK_EQUAL(filter2->num_elements, 1U);

    // The filter in the store correctly matches the recipient's discovery tag!
    BOOST_CHECK(filter2->Match(discovery_tag));

    // Random tag does not match
    BOOST_CHECK(!filter2->Match(uint256::FromUserHex("9999").value()));
}

BOOST_AUTO_TEST_SUITE_END()
