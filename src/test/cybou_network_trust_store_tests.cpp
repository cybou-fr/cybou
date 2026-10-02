// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_trust_store.h>
#include <cybou/state_store.h>
#include <test/cybou_test_helpers.h>
#include "cybou_test_setup.h"

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <vector>

namespace {

cybou::NetworkGenesis MakeTestGenesis(
    uint64_t generation,
    const std::array<unsigned char, 32>& net_secret,
    const cybou::IdentityHybridPublicKey& net_pub,
    const cybou::IdentityHybridPublicKey& poa_pub,
    const uint256& state_root)
{
    cybou::NetworkGenesis spec;
    spec.version = cybou::CYBOU_NETWORK_GENESIS_VERSION;
    spec.genesis_generation = generation;
    spec.network_public_key = net_pub;
    spec.genesis_state_root = state_root;
    spec.poa_finalizer_public_key = poa_pub;
    spec.protocol_parameters.account_creation_work_bits = 8;
    spec.protocol_parameters.max_account_creates_per_block = 16;
    spec.protocol_parameters.epoch_blocks = 100;
    spec.protocol_parameters.name_claim_work_bits = 8;
    spec.protocol_parameters.name_commit_min_depth = 2;
    spec.protocol_parameters.name_commit_max_lifetime = 10;
    spec.protocol_parameters.max_pending_name_commits = 32;

    const auto digest = cybou::ComputeNetworkGenesisDigest(spec);
    auto sig = cybou::SignIdentityMessage(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT,
        std::span<const unsigned char>{digest.begin(), digest.size()});
    BOOST_REQUIRE(sig.has_value());
    spec.signature = *sig;
    return spec;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_network_trust_store_tests)

BOOST_AUTO_TEST_CASE(test_network_trust_store_anti_rollback_and_conflict_handling)
{
    CybouTestSetup setup;
    cybou::NetworkTrustStore trust_store(setup.m_data_dir);

    std::array<unsigned char, 32> net_secret{};
    net_secret.fill(0x33);
    auto net_pub = cybou::DeriveIdentityPublicKey(net_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT);
    BOOST_REQUIRE(net_pub.has_value());
    const auto net_id = cybou::CanonicalSerializeNetworkPublicKey(*net_pub);

    std::array<unsigned char, 32> poa_secret{};
    poa_secret.fill(0x77);
    auto poa_pub = cybou::DeriveIdentityPublicKey(poa_secret, cybou::IdentityKeyPurpose::POA_FINALIZER);
    BOOST_REQUIRE(poa_pub.has_value());

    // 1. Initial genesis (gen = 1)
    const auto gen1 = MakeTestGenesis(1, net_secret, *net_pub, *poa_pub, uint256::ONE);
    auto verified_gen1 = cybou::VerifiedNetworkGenesis::Create(gen1);
    BOOST_REQUIRE(verified_gen1.has_value());

    // Initial state: trust store has no record -> ACCEPT_NEW
    BOOST_CHECK(trust_store.Evaluate(net_id, gen1) == cybou::NetworkTrustDecision::ACCEPT_NEW);
    BOOST_REQUIRE(trust_store.RecordAccepted(net_id, *verified_gen1));

    // Verify record was persisted
    auto record = trust_store.LoadRecord(net_id);
    BOOST_REQUIRE(record.has_value());
    BOOST_CHECK_EQUAL(record->highest_generation, 1);
    BOOST_CHECK(record->accepted_genesis_digest == cybou::ComputeNetworkGenesisDigest(gen1));

    // 2. Idempotent evaluation / replay of accepted gen1 -> ACCEPT_CURRENT
    BOOST_CHECK(trust_store.Evaluate(net_id, gen1) == cybou::NetworkTrustDecision::ACCEPT_CURRENT);

    // 3. Equivocation conflict: same generation 1, different state root / digest -> REJECT_CONFLICT
    const auto gen1_conflict = MakeTestGenesis(1, net_secret, *net_pub, *poa_pub, uint256{0x99});
    BOOST_CHECK(trust_store.Evaluate(net_id, gen1_conflict) == cybou::NetworkTrustDecision::REJECT_CONFLICT);
    auto verified_gen1_conflict = cybou::VerifiedNetworkGenesis::Create(gen1_conflict);
    BOOST_REQUIRE(verified_gen1_conflict.has_value());
    BOOST_CHECK(!trust_store.RecordAccepted(net_id, *verified_gen1_conflict));

    // 4. Upgrade to generation 2 -> ACCEPT_NEW
    const auto gen2 = MakeTestGenesis(2, net_secret, *net_pub, *poa_pub, uint256{0x02});
    auto verified_gen2 = cybou::VerifiedNetworkGenesis::Create(gen2);
    BOOST_REQUIRE(verified_gen2.has_value());

    BOOST_CHECK(trust_store.Evaluate(net_id, gen2) == cybou::NetworkTrustDecision::ACCEPT_NEW);
    BOOST_REQUIRE(trust_store.RecordAccepted(net_id, *verified_gen2));

    auto record2 = trust_store.LoadRecord(net_id);
    BOOST_REQUIRE(record2.has_value());
    BOOST_CHECK_EQUAL(record2->highest_generation, 2);
    BOOST_CHECK(record2->accepted_genesis_digest == cybou::ComputeNetworkGenesisDigest(gen2));

    // 5. Anti-rollback check: attempting to supply gen1 when highest is 2 -> REJECT_ROLLBACK
    BOOST_CHECK(trust_store.Evaluate(net_id, gen1) == cybou::NetworkTrustDecision::REJECT_ROLLBACK);
    BOOST_CHECK(!trust_store.RecordAccepted(net_id, *verified_gen1));

    // 6. Network key mismatch test: genesis signed by another network key -> REJECT_INVALID_KEY
    std::array<unsigned char, 32> other_secret{};
    other_secret.fill(0x44);
    auto other_pub = cybou::DeriveIdentityPublicKey(other_secret, cybou::IdentityKeyPurpose::NETWORK_ROOT);
    BOOST_REQUIRE(other_pub.has_value());
    const auto other_gen = MakeTestGenesis(3, other_secret, *other_pub, *poa_pub, uint256::ONE);
    BOOST_CHECK(trust_store.Evaluate(net_id, other_gen) == cybou::NetworkTrustDecision::REJECT_INVALID_KEY);
}

BOOST_AUTO_TEST_CASE(test_state_store_genesis_generation_and_id_mismatch)
{
    CybouTestSetup setup;
    cybou::KVStore db(cybou::KVStoreOptions{.path = setup.m_data_dir / "db", .memory_only = true});

    const auto definition = cybou::CreateDevNetworkDefinition(
        cybou::CreateDevGenesisState(), cybou::TestPoaFinalizerPublicKey());
    const auto genesis_state = cybou::CreateDevGenesisState();

    // 1. Initialize StateStore with generation = 1, genesis_id = ONE
    {
        cybou::CybouStateStore store(db, definition, 1, uint256::ONE);
        BOOST_REQUIRE(store.InitializeGenesis(genesis_state));
        auto loaded = store.LoadState();
        BOOST_REQUIRE(loaded);
        BOOST_CHECK_EQUAL(*store.GetStoredGenesisGeneration(), 1);
        BOOST_CHECK(*store.GetStoredGenesisId() == uint256::ONE);
    }

    // 2. Open with matching generation = 1 and genesis_id = ONE -> LoadState succeeds
    {
        cybou::CybouStateStore store(db, definition, 1, uint256::ONE);
        auto loaded = store.LoadState();
        BOOST_CHECK(loaded);
        BOOST_CHECK(loaded.error == cybou::StateLoadError::NONE);
    }

    // 3. Open with mismatched generation = 2 -> LoadState fails with GENESIS_GENERATION_MISMATCH
    {
        cybou::CybouStateStore store(db, definition, 2, uint256::ONE);
        auto loaded = store.LoadState();
        BOOST_CHECK(!loaded);
        BOOST_CHECK(loaded.error == cybou::StateLoadError::GENESIS_GENERATION_MISMATCH);
    }

    // 4. Open with mismatched genesis_id -> LoadState fails with GENESIS_ID_MISMATCH
    {
        cybou::CybouStateStore store(db, definition, 1, uint256{0x99});
        auto loaded = store.LoadState();
        BOOST_CHECK(!loaded);
        BOOST_CHECK(loaded.error == cybou::StateLoadError::GENESIS_ID_MISMATCH);
    }
}

BOOST_AUTO_TEST_SUITE_END()
