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
    BOOST_CHECK_EQUAL(cybou::AuthorityTier(UINT64_MAX, 100), 64U);
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
    const auto& policy = net.definition.protocol_parameters.authority;
    cybou::AuthorityIndex index{*net.runtime};
    index.Sync();

    // A fresh Identity: onboarding credit gives no Authority.
    const auto fresh = index.Get(account);
    BOOST_REQUIRE(fresh);
    BOOST_CHECK_EQUAL(fresh->activity, 0U);
    BOOST_CHECK_EQUAL(fresh->system_contribution, 0U);
    BOOST_CHECK(fresh->enforced);
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
    cybou::AuthorityIndex rebuilt{*net.runtime};
    rebuilt.Sync();
    const auto again = rebuilt.Get(account);
    BOOST_REQUIRE(again);
    BOOST_CHECK_EQUAL(again->activity, later->activity);
    BOOST_CHECK_EQUAL(again->age, later->age);
    BOOST_CHECK_EQUAL(again->effective, later->effective);

    // Unknown Identities have no Authority record.
    BOOST_CHECK(!index.Get(cybou::AccountId{uint256::ONE}));
}

BOOST_AUTO_TEST_CASE(canonical_authority_preserves_debt_and_round_trips)
{
    ShortEpochNetwork net;
    auto identity = net.CreateIdentity("state.vault");
    const auto account = *identity->GetAccountId();
    auto state = *net.runtime->GetStore().LoadState().state;
    auto& owner = state.accounts.at(account);
    owner.authority.activity = 2;
    owner.authority.penalty_debt = 100;
    const auto record = cybou::ComputeAuthorityRecord(account, owner, owner.creation_epoch + 1, {});
    BOOST_CHECK_EQUAL(record.earned, 3U);
    BOOST_CHECK_EQUAL(record.effective, 0U);
    BOOST_CHECK_EQUAL(record.penalty_debt, 100U);
    const auto bytes = cybou::SerializeCybouState(state);
    BOOST_REQUIRE(bytes);
    const auto restored = cybou::DeserializeCybouState(*bytes);
    BOOST_REQUIRE(restored);
    BOOST_CHECK(restored->accounts.at(account) == owner);
    auto changed = state;
    ++changed.accounts.at(account).authority.penalty_debt;
    BOOST_CHECK(cybou::CybouStateHash(changed) != cybou::CybouStateHash(state));
    auto network = net.definition;
    ++network.protocol_parameters.authority.protocol_base;
    BOOST_CHECK(cybou::NetworkId(network) != cybou::NetworkId(net.definition));
    BOOST_CHECK(cybou::DeserializeNetworkDefinition(cybou::SerializeNetworkDefinition(network)) == network);
}
BOOST_AUTO_TEST_SUITE_END()
