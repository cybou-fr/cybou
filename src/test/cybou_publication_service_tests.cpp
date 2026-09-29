// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/publication_service.h>
#include <cybou/canonical_cbor.h>
#include <cybou/node_runtime.h>
#include <test/cybou_service_test_fixture.h>

#include <boost/test/unit_test.hpp>

#include <memory>

namespace {

cybou::PreparedPublicationBundle Prepare(cybou::CybouNodeRuntime& runtime,
    cybou::KVStore& proof_db, const std::string& index_id)
{
    cybou::PublicationBundleStager stager{runtime.GetChunkBlobStore(), proof_db, index_id,
        std::span<const unsigned char, 32>{runtime.GetNetworkId().begin(), 32}};
    const auto metadata = cybou::EncodeCanonicalCbor(cybou::CborValue::ArrayValue({
        cybou::CborValue::Unsigned(2), cybou::CborValue::Unsigned(1)
    }));
    auto main = stager.StageTree([](std::span<unsigned char>) -> std::optional<std::size_t> { return 0; }, metadata);
    if (!main) throw std::runtime_error{"cannot stage test publication"};
    auto prepared = stager.Finish(*main);
    if (!prepared) throw std::runtime_error{"cannot finish test publication"};
    return *prepared;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_publication_service_tests)

BOOST_AUTO_TEST_CASE(self_publication_waits_for_finality_and_resumes_exact_operation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("publication-owner.cybou");
    cybou::KVStore proof_db{cybou::KVStoreOptions{.memory_only = true}};
    const auto prepared = Prepare(*fixture.runtime, proof_db, "self-pub");
    const auto app_path = fixture.directory / "application";
    uint256 operation_id;
    {
        cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), app_path};
        auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
        cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(),
            application_db, coordinator};
        const auto submitted = publication.SubmitPrepared("self-job", prepared);
        BOOST_CHECK(submitted.phase == cybou::PublicationJobPhase::WAITING_FINALITY);
        BOOST_REQUIRE(!submitted.operation_id.IsNull());
        operation_id = submitted.operation_id;
        BOOST_CHECK(!fixture.runtime->FindFinalizedRootPublication(operation_id));
        const auto retried = publication.Resume("self-job");
        BOOST_CHECK(retried.phase == cybou::PublicationJobPhase::WAITING_FINALITY);
        BOOST_CHECK(retried.operation_id == operation_id);
    }
    {
        cybou::PrivateApplicationStore reopened{identity->GetKeyStore(), app_path};
        auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
        cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(), reopened, coordinator};
        BOOST_REQUIRE(fixture.runtime->ProduceBlock());
        const auto status = publication.GetJob("self-job");
        BOOST_REQUIRE(status);
        BOOST_CHECK(status->phase == cybou::PublicationJobPhase::SECURING);
        BOOST_CHECK(status->operation_id == operation_id);
        BOOST_CHECK(status->finalized_height > 0);
        const auto finalized = fixture.runtime->FindFinalizedRootPublication(operation_id);
        BOOST_REQUIRE(finalized);
        BOOST_CHECK_EQUAL(finalized->recipient_capsules.size(), 1U);
        BOOST_CHECK(finalized->root_chunk_id == prepared.root_chunk_id);
    }
}

BOOST_AUTO_TEST_CASE(one_recipient_has_recipient_and_owner_capsules)
{
    CybouServiceTestFixture fixture;
    auto sender = fixture.CreateIdentity("sender.cybou");
    auto recipient = fixture.CreateIdentity("recipient.cybou");
    const auto recipient_id = recipient->GetAccountId();
    BOOST_REQUIRE(recipient_id);
    cybou::KVStore proof_db{cybou::KVStoreOptions{.memory_only = true}};
    const auto prepared = Prepare(*fixture.runtime, proof_db, "mail-pub");
    cybou::PrivateApplicationStore application_db{sender->GetKeyStore(), fixture.directory / "application"};
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(sender->GetKeyStore());
    cybou::PublicationService publication{*fixture.runtime, sender->GetKeyStore(), application_db, coordinator};
    const auto submitted = publication.SubmitPrepared("mail-job", prepared, *recipient_id);
    BOOST_CHECK(submitted.phase == cybou::PublicationJobPhase::WAITING_FINALITY);
    BOOST_REQUIRE(!submitted.operation_id.IsNull());
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    const auto finalized = fixture.runtime->FindFinalizedRootPublication(submitted.operation_id);
    BOOST_REQUIRE(finalized);
    BOOST_CHECK_EQUAL(finalized->recipient_capsules.size(), 2U);
    BOOST_CHECK(finalized->recipient_capsules[0].wrapped_content_key !=
        finalized->recipient_capsules[1].wrapped_content_key);
    BOOST_CHECK(publication.Resume("mail-job").phase == cybou::PublicationJobPhase::SECURING);
}

BOOST_AUTO_TEST_CASE(corrupt_private_job_is_not_overwritten)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("owner.cybou");
    cybou::KVStore proof_db{cybou::KVStoreOptions{.memory_only = true}};
    const auto prepared = Prepare(*fixture.runtime, proof_db, "corrupt-pub");
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    BOOST_REQUIRE(application_db.Put("publication/job/corrupt-job", std::vector<unsigned char>{1, 2, 3}));
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
    cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(), application_db, coordinator};
    const auto result = publication.SubmitPrepared("corrupt-job", prepared);
    BOOST_CHECK(result.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION);
    BOOST_CHECK(!publication.GetJob("corrupt-job"));
    const std::vector<unsigned char> original{1, 2, 3};
    BOOST_CHECK(application_db.Get("publication/job/corrupt-job") == original);
}

BOOST_AUTO_TEST_SUITE_END()
