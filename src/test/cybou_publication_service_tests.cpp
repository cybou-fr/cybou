// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/publication_service.h>
#include <test/cybou_publication_builder.h>
#include <cybou/chunk_retention.h>
#include <cybou/node_runtime.h>
#include <test/cybou_service_test_fixture.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <memory>

namespace {

cybou::RetentionKey JobKey(const cybou::AccountId& account, std::string_view id)
{
    const auto& value = account.Value();
    return {.holder = cybou::RetentionTag("CYBOU/RETENTION/IDENTITY/v1", std::span{value.begin(), value.size()}),
        .reference = cybou::RetentionTag("CYBOU/RETENTION/PUBLICATION-JOB/v1",
            std::span{reinterpret_cast<const unsigned char*>(id.data()), id.size()})};
}

cybou::PreparedPublicationBundle Prepare(cybou::CybouNodeRuntime& runtime,
    cybou::KVStore& proof_db, const std::string& index_id)
{
    cybou::test::PublicationBuilder stager{runtime.GetChunkBlobStore(),
        std::span<const unsigned char, 32>{runtime.GetNetworkBinding().begin(), 32}};
    const std::vector<unsigned char> metadata{2, 3, 0, 0};
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
        BOOST_CHECK(!publication.MarkProtected("self-job")); // not finalized yet
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
    // Only StorageService's durability report moves a finalized job to PROTECTED.
    BOOST_CHECK(publication.MarkProtected("mail-job"));
    BOOST_CHECK(publication.GetJob("mail-job")->phase == cybou::PublicationJobPhase::PROTECTED);
    BOOST_CHECK(publication.Resume("mail-job").phase == cybou::PublicationJobPhase::PROTECTED);
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

BOOST_AUTO_TEST_CASE(stage_releases_pin_when_private_leaf_index_write_fails)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("owner.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
    cybou::KVStore staging_db{cybou::KVStoreOptions{.memory_only = true}};
    cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(),
        application_db, coordinator};
    cybou::MailMessage message;
    message.message_id = *cybou::NewPrivateItemId();
    message.recipient_account_id = *account;
    message.client_timestamp_ms = 1;
    cybou::MailAttachment attachment;
    attachment.attachment_id = *cybou::NewPrivateItemId();
    attachment.filename = "payload.bin";
    message.attachments.push_back(attachment);
    size_t offset{0};
    const auto result = publication.PublishMail("write-failure", std::move(message), {{0,
        cybou::NewContent{.source = [&](std::span<unsigned char> out) -> std::optional<std::size_t> {
            if (offset == 0) { out[0] = 0x5a; ++offset; return 1; }
            identity->GetKeyStore().Clear();
            return 0;
        }}}});
    BOOST_CHECK_EQUAL(result.error, "Cannot save staged chunk order");
    BOOST_CHECK(fixture.runtime->GetChunkRetention().Pinned(JobKey(*account, "write-failure")).empty());
}

BOOST_AUTO_TEST_CASE(cancel_queued_publication_releases_its_retention_pin)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("owner.cybou");
    const auto account = identity->GetAccountId();
    BOOST_REQUIRE(account);
    cybou::KVStore staging_db{cybou::KVStoreOptions{.memory_only = true}};
    const auto prepared = Prepare(*fixture.runtime, staging_db, "blocker");
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
    cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(),
        application_db, coordinator};
    BOOST_CHECK(publication.SubmitPrepared("blocker-job", prepared).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);

    cybou::MailMessage message;
    message.message_id = *cybou::NewPrivateItemId();
    message.recipient_account_id = *account;
    message.client_timestamp_ms = 1;
    cybou::MailAttachment attachment;
    attachment.attachment_id = *cybou::NewPrivateItemId();
    attachment.filename = "queued.bin";
    message.attachments.push_back(attachment);
    size_t offset{0};
    const auto queued = publication.PublishMail("abandoned-job", std::move(message), {{0,
        cybou::NewContent{.source = [&](std::span<unsigned char> out) -> std::optional<std::size_t> {
            if (offset++ == 0) { out[0] = 0x33; return 1; }
            return 0;
        }}}});
    BOOST_CHECK(queued.phase == cybou::PublicationJobPhase::QUEUED);
    BOOST_CHECK(!fixture.runtime->GetChunkRetention().Pinned(JobKey(*account, "abandoned-job")).empty());
    BOOST_CHECK(!publication.CancelPublication("blocker-job"));
    BOOST_REQUIRE(publication.CancelPublication("abandoned-job"));
    BOOST_CHECK(fixture.runtime->GetChunkRetention().Pinned(JobKey(*account, "abandoned-job")).empty());
    BOOST_CHECK(!application_db.Has("publication/job/abandoned-job"));
    BOOST_CHECK(!application_db.Has("publication/intent/abandoned-job"));
    BOOST_CHECK(!application_db.Has("publication/leaves/abandoned-job"));
    const auto jobs = publication.Jobs();
    BOOST_CHECK(std::find(jobs.begin(), jobs.end(), "abandoned-job") == jobs.end());
}

BOOST_AUTO_TEST_CASE(interrupted_encryption_releases_orphan_pins_on_reopen)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("publication-interrupted.cybou");
    cybou::PrivateApplicationStore application_db{identity->GetKeyStore(), fixture.directory / "application"};
    auto& coordinator = fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore());
    const std::string job{"interrupted-encryption"};
    cybou::ChunkId chunk{};
    chunk.fill(42);
    const std::array<cybou::ChunkId, 1> chunks{chunk};
    auto& retention = fixture.runtime->GetChunkRetention();
    BOOST_REQUIRE(retention.Pin(JobKey(application_db.Account(), job), chunks));
    BOOST_REQUIRE(application_db.Put("publication/staging-attempt", std::span{
        reinterpret_cast<const unsigned char*>(job.data()), job.size()}));
    BOOST_REQUIRE(retention.IsPinned(chunk));
    cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(), application_db, coordinator};
    BOOST_CHECK(!retention.IsPinned(chunk));
    BOOST_CHECK(!application_db.Has("publication/staging-attempt"));
}

BOOST_AUTO_TEST_SUITE_END()
