// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>
#include <cybou/bft.h>
#include <cybou/block_feed.h>
#include <cybou/network_definition.h>
#include <cybou/signing.h>
#include <dbwrapper.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>
#include <boost/asio.hpp>

#include <array>
#include <memory>
#include <thread>

namespace {
class TestAuthorityVerifier final : public cybou::OperatorAuthoritySignatureVerifier
{
public:
    bool Verify(const cybou::OperatorAuthorityKeySet& keyset,
                const cybou::SignatureBundleV1& bundle,
                std::span<const unsigned char>) const override
    {
        return cybou::IsPresent(bundle) && bundle.authority_keyset_id == keyset.keyset_id;
    }
};
}

BOOST_FIXTURE_TEST_SUITE(cybou_authority_node_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(produces_and_persists_verified_authority_blocks)
{
    std::array<unsigned char, 32> validator_key{};
    validator_key[0] = 0x61;
    const auto validator_public_key = *cybou::DeriveEd25519PublicKey(validator_key);
    const auto validator_id = uint256::FromUserHex("ab").value();
    const cybou::ValidatorSetV1 validator_set{
        .version = cybou::VALIDATOR_SET_VERSION,
        .validators = {{.validator_id = validator_id, .consensus_public_key = validator_public_key, .weight = 1}},
    };
    const cybou::CybouState genesis{
        .onboarding_pool = 12000,
        .security_reward_pool = 100,
        .pending_fee_pool = 0,
        .accounts = {},
        .validator_set = validator_set,
    };
    const cybou::CybouProtocolParameters params{
        .account_creation_work_bits = 0,
        .account_creation_epoch_lag = 1,
        .max_account_creates_per_block = 2,
        .onboarding_bonus = 6000,
        .epoch_blocks = 10,
    };
    const cybou::CybouNetworkDefinitionV1 definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = uint256::ONE,
        .genesis_state_root = cybou::CybouStateHash(genesis),
        .protocol_parameters = params,
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(validator_set),
    };
    CDBWrapper db{{
        .path = m_args.GetDataDirNet() / "cybou-authority-node-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
    cybou::CybouStateStore store{db, definition};
    BOOST_REQUIRE(store.InitializeGenesis(genesis));

    std::array<unsigned char, 32> wrong_key{};
    wrong_key[0] = 0x62;
    cybou::CybouAuthorityNode wrong_producer{store, wrong_key};
    BOOST_CHECK(wrong_producer.ProduceNextBlock().error == cybou::AuthorityProductionError::VALIDATOR_KEY_MISMATCH);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 0);

    std::array<unsigned char, 32> account_key{};
    account_key[0] = 0x63;
    const auto account_public_key = *cybou::DeriveEd25519PublicKey(account_key);
    const cybou::AccountId account_id{uint256::FromUserHex("ac").value()};
    cybou::AccountCreateOpV1 create{
        .version = cybou::ACCOUNT_CREATE_OP_VERSION,
        .account_id = account_id,
        .initial_authorization = {.authorization_descriptor = account_public_key},
        .creation_work = {
            .version = cybou::ACCOUNT_CREATION_WORK_VERSION,
            .network_id = store.GetNetworkId(),
            .account_id = account_id,
            .initial_authorization_commitment = cybou::ComputeAuthCommitment(
                cybou::AccountAuthorizationV1{.authorization_descriptor = account_public_key}),
            .work_epoch = 0,
            .nonce = 0,
        },
    };
    create.proof_of_possession = *cybou::SignUserMessage(
        account_key, cybou::ComputeAccountPopDigest(store.GetNetworkId(), account_id, account_public_key));

    cybou::CybouAuthorityNode producer{store, validator_key};
    BOOST_CHECK(producer.SubmitOperation(create));
    BOOST_CHECK(!producer.SubmitOperation(create)); // Duplicate account cannot execute.
    BOOST_CHECK_EQUAL(producer.PendingCount(), 1U);
    const auto first = producer.ProduceNextBlock();
    BOOST_REQUIRE(first);
    BOOST_REQUIRE(first.finalized_block.has_value());
    BOOST_CHECK_EQUAL(producer.PendingCount(), 0U);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1U);
    BOOST_CHECK(first.finalized_block->block.operations.size() == 1);
    BOOST_CHECK(cybou::VerifyFinalityCertificate(
        first.finalized_block->certificate, validator_set, store.GetNetworkId()) ==
        cybou::FinalityVerificationError::NONE);
    BOOST_CHECK(store.GetBlock(cybou::ComputeBlockId(first.finalized_block->block)) == first.finalized_block);
    BOOST_CHECK(store.GetBlockAtHeight(1) == first.finalized_block);
    BOOST_CHECK(!store.GetBlockAtHeight(0).has_value());
    BOOST_CHECK(store.LoadState().state->accounts.at(account_id).system_balance == 6000);

    const auto second = producer.ProduceNextBlock();
    BOOST_REQUIRE(second);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 2U);
    BOOST_CHECK(second.finalized_block->block.parent_block_id ==
        cybou::ComputeBlockId(first.finalized_block->block));
    BOOST_CHECK(store.GetBlockAtHeight(2) == second.finalized_block);
    BOOST_CHECK(!store.GetBlockAtHeight(3).has_value());
    db.Erase("cybou/block-height/v1/1");
    BOOST_CHECK(store.GetBlockAtHeight(1) == first.finalized_block);

    CDBWrapper observer_db{{
        .path = m_args.GetDataDirNet() / "cybou-observer-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
    cybou::CybouStateStore observer{observer_db, definition};
    BOOST_REQUIRE(observer.InitializeGenesis(genesis));
    auto tampered = *first.finalized_block;
    tampered.block.resulting_state_root = uint256::ONE;
    BOOST_CHECK(!observer.CommitFinalizedBlock(tampered));
    BOOST_CHECK_EQUAL(*observer.GetFinalizedHeight(), 0U);
    boost::asio::io_context io;
    boost::asio::ip::tcp::acceptor acceptor(io, {
        boost::asio::ip::address_v4::loopback(), 0,
    });
    const auto port = acceptor.local_endpoint().port();
    for (uint64_t height = 1; height <= 2; ++height) {
        std::jthread server([&] {
            boost::asio::ip::tcp::socket socket(io);
            acceptor.accept(socket);
            cybou::ServeFinalizedBlockRequest(store, socket);
        });
        const bool synced = cybou::SyncNextFinalizedBlock(observer, "127.0.0.1", port);
        server.join();
        BOOST_REQUIRE(synced);
        BOOST_CHECK_EQUAL(*observer.GetFinalizedHeight(), height);
    }
    {
        std::jthread server([&] {
            boost::asio::ip::tcp::socket socket(io);
            acceptor.accept(socket);
            cybou::ServeFinalizedBlockRequest(store, socket);
        });
        const auto wrong_network = cybou::FetchFinalizedBlock(
            "127.0.0.1", port, uint256::FromUserHex("ff").value(), 1);
        server.join();
        BOOST_CHECK(!wrong_network.has_value());
    }
    {
        std::jthread server([&] {
            boost::asio::ip::tcp::socket socket(io);
            acceptor.accept(socket);
            cybou::ServeFinalizedBlockRequest(store, socket);
        });
        const bool advanced = cybou::SyncNextFinalizedBlock(observer, "127.0.0.1", port);
        server.join();
        BOOST_CHECK(!advanced);
        BOOST_CHECK_EQUAL(*observer.GetFinalizedHeight(), 2U);
    }
    {
        std::jthread oversized_server([&] {
            boost::asio::ip::tcp::socket socket(io);
            acceptor.accept(socket);
            const uint32_t oversized = cybou::MAX_FINALIZED_BLOCK_FEED_BYTES + 1;
            std::array<unsigned char, 4> length{};
            for (size_t i = 0; i < length.size(); ++i) {
                length[i] = static_cast<unsigned char>(oversized >> (8 * i));
            }
            boost::asio::write(socket, boost::asio::buffer(length));
        });
        const auto oversized = cybou::FetchFinalizedBlock(
            "127.0.0.1", port, store.GetNetworkId(), 3);
        oversized_server.join();
        BOOST_CHECK(!oversized.has_value());
    }
    BOOST_REQUIRE(observer.LoadState());
    BOOST_CHECK(observer.GetFinalizedHead() == store.GetFinalizedHead());
    BOOST_CHECK(observer.GetStateRoot() == store.GetStateRoot());
}

BOOST_AUTO_TEST_CASE(authority_block_transitions_to_four_validator_set)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = 0x71;
    const auto pub = *cybou::DeriveEd25519PublicKey(seed);
    const cybou::ValidatorSetV1 initial_set{
        .version = cybou::VALIDATOR_SET_VERSION,
        .validators = {{.validator_id = uint256::FromUserHex("11").value(),
                        .consensus_public_key = pub, .weight = 1}},
    };
    const cybou::CybouState genesis{
        .onboarding_pool = 100,
        .security_reward_pool = 100,
        .pending_fee_pool = 0,
        .accounts = {},
        .validator_set = initial_set,
    };
    const auto keyset_id = uint256::FromUserHex("aa").value();
    const cybou::CybouNetworkDefinitionV1 definition{
        .protocol_version = cybou::CYBOU_NETWORK_DEFINITION_VERSION,
        .genesis_block_id = uint256::ONE,
        .genesis_state_root = cybou::CybouStateHash(genesis),
        .protocol_parameters = cybou::CybouProtocolParameters{},
        .initial_validator_set_commitment = cybou::ComputeValidatorSetCommitment(initial_set),
        .operator_authority = cybou::OperatorAuthorityKeySet{
            .keyset_id = keyset_id,
            .ed25519_public_key = {1},
            .mldsa65_public_key = {1},
            .active_from_epoch = 0,
            .retired_from_epoch = std::nullopt,
        },
    };
    CDBWrapper db{{
        .path = m_args.GetDataDirNet() / "cybou-authority-transition-test",
        .cache_bytes = 1 << 20,
        .memory_only = true,
        .wipe_data = true,
        .obfuscate = false,
    }};
    cybou::CybouStateStore store{db, definition, std::make_shared<TestAuthorityVerifier>()};
    BOOST_REQUIRE(store.InitializeGenesis(genesis));
    cybou::CybouAuthorityNode producer{store, seed};

    for (uint8_t i = 2; i <= 4; ++i) {
        std::array<unsigned char, 32> next_seed{};
        next_seed[0] = static_cast<unsigned char>(0x70 + i);
        cybou::SignatureBundleV1 signature;
        signature.authority_keyset_id = keyset_id;
        signature.classical_signature.fill(0x11);
        signature.pq_signature.fill(0x22);
        uint256 next_id;
        next_id.begin()[0] = i;
        const cybou::ValidatorAdmissionOpV1 admission{
            .version = cybou::VALIDATOR_ADMISSION_OP_VERSION,
            .validator_id = next_id,
            .consensus_public_key = *cybou::DeriveEd25519PublicKey(next_seed),
            .activation_epoch = 0,
            .operator_signature = signature,
        };
        BOOST_REQUIRE(producer.SubmitOperation(admission));
    }
    BOOST_CHECK_EQUAL(producer.PendingCount(), 3U);
    const auto transition = producer.ProduceNextBlock();
    BOOST_REQUIRE(transition);
    BOOST_CHECK_EQUAL(store.GetValidatorSet()->Size(), 4U);
    BOOST_CHECK(transition.finalized_block->certificate.validator_set_commitment ==
        cybou::ComputeValidatorSetCommitment(initial_set));
    BOOST_CHECK(producer.ProduceNextBlock().error == cybou::AuthorityProductionError::NOT_AUTHORITY_MODE);
    BOOST_CHECK_EQUAL(*store.GetFinalizedHeight(), 1U);
}

BOOST_AUTO_TEST_SUITE_END()
