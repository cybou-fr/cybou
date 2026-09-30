// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/authority.h>

#include <cybou/canonical_cbor.h>
#include <cybou/identity_service.h>
#include <cybou/network_definition.h>
#include <cybou/node_runtime.h>
#include <cybou/publication_service.h>
#include <test/cybou_test_helpers.h>

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

namespace {

/** Authority runtime with short epochs so Age is observable in a test. */
struct ShortEpochNetwork {
    std::filesystem::path directory;
    cybou::CybouState genesis;
    cybou::CybouNetworkDefinition definition;
    std::unique_ptr<cybou::CybouNodeRuntime> runtime;

    ShortEpochNetwork()
    {
        static int sequence{0};
        directory = std::filesystem::temp_directory_path() / ("cybou-authority-" + std::to_string(sequence++));
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
        std::array<unsigned char, 32> seed{};
        seed[0] = 0x5a;
        genesis = cybou::CreateDevGenesisState();
        definition = cybou::CreateDevNetworkDefinition(genesis, cybou::TestPoaFinalizerPublicKey(0x5a));
        definition.protocol_parameters.account_creation_work_bits = 0;
        definition.protocol_parameters.epoch_blocks = 2;
        runtime = std::make_unique<cybou::CybouNodeRuntime>(cybou::NodeRuntimeConfig{
            .network_definition = definition, .data_dir = directory / "runtime",
            .poa_finalizer_recovery_entropy = seed, .memory_only = true, .wipe_data = true});
        if (!runtime->InitializeGenesis(genesis)) throw std::runtime_error{"genesis failed"};
    }

    std::unique_ptr<cybou::CybouIdentityService> CreateIdentity(const std::string& name)
    {
        auto service = std::make_unique<cybou::CybouIdentityService>(*runtime, directory / name);
        if (!service->PrepareNewIdentity() || !service->CreateIdentitySync("correct horse battery staple").success) {
            throw std::runtime_error{"identity creation failed"};
        }
        return service;
    }
};

/** One finalized RootPublication = one generic activity event. */
void Publish(ShortEpochNetwork& net, cybou::CybouIdentityService& identity, cybou::KVStore& staging,
    cybou::PrivateApplicationStore& db, const std::string& job)
{
    cybou::PublicationService publication{*net.runtime, identity.GetKeyStore(), db,
        net.runtime->GetIdentityOperationCoordinator(identity.GetKeyStore()), staging};
    cybou::FilesMutationBatch batch;
    const auto id = *cybou::NewPrivateItemId();
    batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, id,
        cybou::FileItem{.item_id = id, .kind = cybou::FileItemKind::FOLDER, .name = job}});
    const auto result = publication.PublishFiles(job, batch);
    BOOST_REQUIRE_MESSAGE(result.phase == cybou::PublicationJobPhase::WAITING_FINALITY, result.error);
    BOOST_REQUIRE(net.runtime->ProduceBlock());
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_authority_tests)

BOOST_AUTO_TEST_CASE(tier_is_integer_log2_and_saturates)
{
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(0, 16), 0U);
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(1, 16), 1U);
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(2, 16), 1U);
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(3, 16), 2U);
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(1023, 16), 10U);
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(UINT64_MAX, 16), 16U);
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(UINT64_MAX, 100), 63U);
    BOOST_CHECK_EQUAL(cybou::SaturatingAdd(UINT64_MAX, 1), UINT64_MAX);
    BOOST_CHECK_EQUAL(cybou::SaturatingMul(UINT64_MAX, 2), UINT64_MAX);
}

BOOST_AUTO_TEST_CASE(budgets_have_nonzero_base_and_hard_ceilings)
{
    const cybou::AuthorityPolicy policy;
    const auto base = cybou::AuthorityBudgetsForTier(0, policy);
    BOOST_CHECK_EQUAL(base.protocol_operations_per_epoch, policy.protocol_base);
    BOOST_CHECK_GT(base.storage_bytes, 0U);
    BOOST_CHECK_GT(base.bandwidth_bytes_per_epoch, 0U);
    const auto higher = cybou::AuthorityBudgetsForTier(3, policy);
    BOOST_CHECK_GT(higher.protocol_operations_per_epoch, base.protocol_operations_per_epoch);
    const auto top = cybou::AuthorityBudgetsForTier(UINT32_MAX, policy);
    BOOST_CHECK_EQUAL(top.protocol_operations_per_epoch, policy.protocol_ceiling);
    BOOST_CHECK_EQUAL(top.storage_bytes, policy.storage_ceiling);
    BOOST_CHECK_EQUAL(top.bandwidth_bytes_per_epoch, policy.bandwidth_ceiling);
}

BOOST_AUTO_TEST_CASE(age_and_capped_activity_come_from_finalized_history)
{
    ShortEpochNetwork net;
    auto identity = net.CreateIdentity("authority.vault");
    const auto account = *identity->GetAccountId();
    cybou::AuthorityPolicy policy;
    policy.activity_cap_per_epoch = 2;
    cybou::AuthorityIndex index{*net.runtime, policy};
    index.Sync();

    // A fresh Identity: onboarding credit gives no Authority.
    const auto fresh = index.Get(account);
    BOOST_REQUIRE(fresh);
    BOOST_CHECK_EQUAL(fresh->activity, 0U);
    BOOST_CHECK_EQUAL(fresh->system_contribution, 0U);
    BOOST_CHECK(!fresh->enforced);
    BOOST_CHECK(fresh->budgets == cybou::AuthorityBudgetsForTier(fresh->tier, policy));

    cybou::KVStore staging{cybou::KVStoreOptions{.memory_only = true}};
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), net.directory / "app"};
    for (int i = 0; i < 4; ++i) Publish(net, *identity, staging, db, "folder-" + std::to_string(i));
    index.Sync();
    const auto later = index.Get(account);
    BOOST_REQUIRE(later);
    // Four operations over the epochs they landed in; each epoch credits at most two.
    BOOST_CHECK_GE(later->activity, 2U);
    BOOST_CHECK_LE(later->activity, 4U);
    BOOST_CHECK_GT(later->age, fresh->age);
    BOOST_CHECK_EQUAL(later->earned, later->age + later->activity);
    BOOST_CHECK_EQUAL(later->effective, later->earned); // no penalty evidence exists
    BOOST_CHECK_EQUAL(later->tier, cybou::AuthorityTier(later->effective, policy.max_tier));

    // Deterministic: a fresh index over the same history agrees exactly.
    cybou::AuthorityIndex rebuilt{*net.runtime, policy};
    rebuilt.Sync();
    const auto again = rebuilt.Get(account);
    BOOST_REQUIRE(again);
    BOOST_CHECK_EQUAL(again->activity, later->activity);
    BOOST_CHECK_EQUAL(again->age, later->age);
    BOOST_CHECK_EQUAL(again->effective, later->effective);

    // Unknown Identities have no Authority record.
    BOOST_CHECK(!index.Get(cybou::AccountId{uint256::ONE}));
}

BOOST_AUTO_TEST_CASE(checkpoint_resumes_scan_and_is_only_a_rebuildable_cache)
{
    ShortEpochNetwork net;
    auto identity = net.CreateIdentity("checkpoint.vault");
    const auto account = *identity->GetAccountId();
    cybou::KVStore staging{cybou::KVStoreOptions{.memory_only = true}};
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), net.directory / "app"};
    for (int i = 0; i < 3; ++i) Publish(net, *identity, staging, db, "cp-" + std::to_string(i));
    const auto checkpoint = net.directory / "authority-index.bin";
    const auto height = net.runtime->GetFinalizedHeight().value_or(0);

    std::optional<cybou::AuthorityRecord> scanned;
    {
        cybou::AuthorityIndex index{*net.runtime, checkpoint};
        BOOST_CHECK_EQUAL(index.ScannedHeight(), 0U);
        BOOST_CHECK_EQUAL(index.Sync(), height);
        scanned = index.Get(account);
        BOOST_REQUIRE(scanned);
    }
    BOOST_REQUIRE(std::filesystem::exists(checkpoint));

    // Restart: resumes at the saved height with identical tallies, no rescan.
    {
        cybou::AuthorityIndex resumed{*net.runtime, checkpoint};
        BOOST_CHECK_EQUAL(resumed.ScannedHeight(), height);
        const auto again = resumed.Get(account);
        BOOST_REQUIRE(again);
        BOOST_CHECK_EQUAL(again->activity, scanned->activity);
        BOOST_CHECK_EQUAL(again->effective, scanned->effective);
        // New finalized history continues from the checkpoint.
        Publish(net, *identity, staging, db, "cp-after");
        BOOST_CHECK_EQUAL(resumed.Sync(), height + 1);
        cybou::AuthorityIndex full{*net.runtime};
        full.Sync();
        BOOST_CHECK_EQUAL(resumed.Get(account)->activity, full.Get(account)->activity);
    }

    // A different policy never reuses the tallies: rescan from 0.
    {
        cybou::AuthorityPolicy other;
        other.activity_cap_per_epoch = 1;
        cybou::AuthorityIndex different{*net.runtime, checkpoint, other};
        BOOST_CHECK_EQUAL(different.ScannedHeight(), 0U);
    }

    // A corrupt or truncated file is ignored, never trusted.
    {
        std::ofstream corrupt{checkpoint, std::ios::binary | std::ios::trunc};
        corrupt << "not an authority checkpoint";
    }
    cybou::AuthorityIndex recovered{*net.runtime, checkpoint};
    BOOST_CHECK_EQUAL(recovered.ScannedHeight(), 0U);
    BOOST_CHECK_EQUAL(recovered.Sync(), height + 1);
}

BOOST_AUTO_TEST_SUITE_END()
