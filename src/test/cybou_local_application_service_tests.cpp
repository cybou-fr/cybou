// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/local_application_service.h>
#include <cybou/network_sync_service.h>
#include <cybou/chunk_blob_store.h>
#include <cybou/crypto/cleanse.h>
#include <test/cybou_service_test_fixture.h>
#include <boost/test/unit_test.hpp>
#include <future>
#include <chrono>
namespace {
cybou::MailRecord Mail(cybou::CybouKeyStore& identity)
{
    cybou::MailRecord record;
    record.sender = *identity.GetAccountId();
    record.message.message_id = *cybou::NewPrivateItemId();
    record.message.recipient_account_id = record.sender;
    record.message.subject = "Local independence";
    record.message.client_timestamp_ms = 1;
    record.folder = cybou::MailFolder::INBOX;
    return record;
}
}
BOOST_AUTO_TEST_SUITE(cybou_local_application_service_tests)
BOOST_AUTO_TEST_CASE(local_moves_continue_while_network_executor_is_blocked)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-independent.vault");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "local", "local.db", true};
    cybou::LocalApplicationService local{db};
    const auto mail = Mail(identity->GetKeyStore());
    BOOST_REQUIRE(local.ImportMail(mail));
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    cybou::NetworkSyncService network{[] {}, 10000};
    network.Post([&] { entered.set_value(); gate.wait(); });
    entered.get_future().wait();
    std::promise<bool> done;
    auto completed = done.get_future();
    local.Post([&] {
        bool ok{true};
        for (int i = 0; i < 50; ++i)
            ok = local.MoveMail(mail.message.message_id, i % 2 ? cybou::MailFolder::ARCHIVE : cybou::MailFolder::TRASH) && ok;
        done.set_value(ok);
    });
    const bool independent = completed.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
    release.set_value(); // release even if the assertion below fails
    BOOST_CHECK(independent);
    BOOST_REQUIRE(completed.get());
    BOOST_REQUIRE(local.GetMail(mail.message.message_id));
    BOOST_CHECK(local.GetMail(mail.message.message_id)->folder == cybou::MailFolder::ARCHIVE);
}
BOOST_AUTO_TEST_CASE(migration_is_atomic_restartable_and_source_is_preserved)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-migration.vault");
    cybou::PrivateApplicationStore source{identity->GetKeyStore(), fixture.directory / "migration"};
    cybou::LocalApplicationService previous{source};
    const auto mail = Mail(identity->GetKeyStore());
    BOOST_REQUIRE(previous.ImportMail(mail));
    BOOST_REQUIRE(previous.SaveDraft({.draft_id = "draft-local", .body = "never sent"}));
    // A missing indexed record makes transfer fail without partial destination rows.
    BOOST_REQUIRE(source.Put("files/index", mail.message.message_id));
    {
        cybou::PrivateApplicationStore destination{identity->GetKeyStore(), fixture.directory / "migration", "local.db", true};
        cybou::LocalApplicationService local{destination};
        BOOST_CHECK(!local.MigrateFrom(source));
        BOOST_CHECK(local.ListMail().empty());
        BOOST_CHECK(!destination.Has("local/import-complete"));
        BOOST_REQUIRE(source.Erase("files/index"));
        BOOST_REQUIRE(local.MigrateFrom(source));
        BOOST_REQUIRE(local.MoveMail(mail.message.message_id, cybou::MailFolder::ARCHIVE));
    }
    cybou::PrivateApplicationStore reopened{identity->GetKeyStore(), fixture.directory / "migration", "local.db", true};
    cybou::LocalApplicationService local{reopened};
    BOOST_REQUIRE(local.MigrateFrom(source));
    BOOST_REQUIRE_EQUAL(local.ListDrafts().size(), 1U);
    BOOST_CHECK(local.GetMail(mail.message.message_id)->folder == cybou::MailFolder::ARCHIVE);
    BOOST_CHECK(previous.GetMail(mail.message.message_id)->folder == cybou::MailFolder::INBOX);
}
BOOST_AUTO_TEST_CASE(data_key_survives_prepared_rotation_and_locked_store_fails_closed)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-rotation.vault");
    cybou::RecoveryEntropy entropy{};
    entropy.fill(77);
    auto next = identity->GetKeyStore().CreateIdentityRotationMaterial(entropy);
    BOOST_REQUIRE(next);
    {
        cybou::PrivateApplicationStore store{identity->GetKeyStore(), fixture.directory / "rotation", "local.db", true};
        BOOST_REQUIRE(store.Put("local/secret", std::vector<unsigned char>{1, 2, 3}));
        BOOST_REQUIRE(store.PrepareKeyRotation(entropy));
    }
    // A crash before key promotion must leave current access usable.
    {
        cybou::PrivateApplicationStore old_restart{identity->GetKeyStore(), fixture.directory / "rotation", "local.db", true};
        BOOST_REQUIRE(old_restart.Get("local/secret"));
    }
    BOOST_REQUIRE(identity->GetKeyStore().LoadMaterial(std::move(*next)));
    cybou::PrivateApplicationStore reopened{identity->GetKeyStore(), fixture.directory / "rotation", "local.db", true};
    BOOST_REQUIRE(reopened.Get("local/secret"));
    BOOST_CHECK_EQUAL(reopened.Get("local/secret")->size(), 3U);
    identity->GetKeyStore().Clear();
    BOOST_CHECK(!reopened.IsUnlocked());
    BOOST_CHECK(!reopened.Get("local/secret"));
    BOOST_CHECK(!reopened.Put("local/secret", std::vector<unsigned char>{4}));
}
BOOST_AUTO_TEST_CASE(outbox_content_survives_source_loss_restart_and_late_confirmation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-outbox.vault");
    const auto account = *identity->GetAccountId();
    const auto item_id = *cybou::NewPrivateItemId();
    cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), account};
    cybou::FilesMutationBatch original;
    original.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
        cybou::FileItem{.item_id = item_id, .kind = cybou::FileItemKind::FILE, .name = "first", .modified_ms = 1}});
    const auto source_path = fixture.directory / "source.bin";
    { std::ofstream file{source_path, std::ios::binary}; file << std::string(1024 * 1024, '*'); }
    auto source = std::make_shared<std::ifstream>(source_path, std::ios::binary);
    std::vector<cybou::NewContent> children{{[source](std::span<unsigned char> out) -> std::optional<std::size_t> {
        source->read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        if (source->bad()) return std::nullopt;
        return static_cast<std::size_t>(source->gcount());
    }}};
    const auto content = stager.Prepare("files-first", children, [&](std::span<const cybou::EncryptedTreeSummary> trees) {
        original.mutations[0].item->root_chunk_id = trees[0].root_chunk_id;
        original.mutations[0].item->content_key = trees[0].content_key;
        original.mutations[0].item->logical_size = trees[0].plaintext_bytes;
        return cybou::EncodePrivateApplicationDocument(original);
    });
    BOOST_REQUIRE(content);
    source->close();
    BOOST_REQUIRE(std::filesystem::remove(source_path));
    {
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "outbox", "local.db", true};
        cybou::LocalApplicationService local{db};
        BOOST_REQUIRE(local.QueuePublication("files-first", *content, original));
        auto renamed = original;
        renamed.mutations[0].item->name = "latest";
        BOOST_REQUIRE(local.QueuePublication("files-second", *content, renamed));
    }
    cybou::PrivateApplicationStore reopened{identity->GetKeyStore(), fixture.directory / "outbox", "local.db", true};
    cybou::LocalApplicationService local{reopened};
    const auto pending = local.Outbox();
    BOOST_REQUIRE_EQUAL(pending.size(), 2U);
    BOOST_CHECK_EQUAL(pending[0].sequence + 1, pending[1].sequence);
    for (const auto& leaf : pending[0].content.leaves) BOOST_CHECK(fixture.runtime->GetChunkBlobStore().Has(leaf));
    cybou::Hash256 op;
    op.begin()[0] = 17;
    BOOST_REQUIRE(local.SetPublicationStatus("files-first", {.phase = cybou::PublicationJobPhase::SECURING, .operation_id = op, .finalized_height = 1}));
    cybou::FileRecord confirmed;
    confirmed.item = *original.mutations[0].item;
    confirmed.order.height = 1;
    confirmed.operation_id = op;
    BOOST_REQUIRE(local.ImportFile(confirmed));
    BOOST_CHECK_EQUAL(local.GetFile(item_id)->item.name, "latest");
}
BOOST_AUTO_TEST_CASE(staging_restart_releases_only_unaccepted_content)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-staging.vault");
    cybou::FilesMutationBatch document;
    const auto id = *cybou::NewPrivateItemId();
    document.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, id,
        cybou::FileItem{.item_id = id, .kind = cybou::FileItemKind::FOLDER, .name = "local", .modified_ms = 1}});
    std::vector<cybou::ChunkId> orphan, accepted;
    {
        cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "staging", "local.db", true};
        cybou::LocalApplicationService local{db};
        cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
            fixture.runtime->GetNetworkBinding(), db.Account(), &db};
        std::vector<cybou::NewContent> children;
        const auto metadata = [&](std::span<const cybou::EncryptedTreeSummary>) { return cybou::EncodePrivateApplicationDocument(document); };
        const auto interrupted = stager.Prepare("interrupted", children, metadata);
        BOOST_REQUIRE(interrupted);
        orphan = interrupted->leaves;
        const auto committed = stager.Prepare("committed", children, metadata);
        BOOST_REQUIRE(committed);
        accepted = committed->leaves;
        BOOST_REQUIRE(local.QueuePublication("committed", *committed, document));
        // Closing after successful preparation without accepting the intent
        // exercises the same persistent journal state as interruption there.
    }
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "staging", "local.db", true};
    cybou::LocalContentStager recovered{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), db.Account(), &db};
    for (const auto& chunk : orphan) BOOST_CHECK(!fixture.runtime->GetChunkRetention().IsPinned(chunk));
    for (const auto& chunk : accepted) BOOST_CHECK(fixture.runtime->GetChunkRetention().IsPinned(chunk));
    BOOST_CHECK(db.Has("outbox/job/committed"));
}
BOOST_AUTO_TEST_CASE(network_handoff_retains_content_and_reuses_exact_operation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-handoff.vault");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "handoff", "local.db", true};
    cybou::LocalApplicationService local{db};
    cybou::PrivateApplicationStore network_db{identity->GetKeyStore(), fixture.directory / "handoff", "app.db", true};
    cybou::PublicationService publication{*fixture.runtime, identity->GetKeyStore(), network_db,
        fixture.runtime->GetIdentityOperationCoordinator(identity->GetKeyStore())};
    cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), db.Account(), &db};
    cybou::FilesMutationBatch document;
    const auto id = *cybou::NewPrivateItemId();
    document.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, id,
        cybou::FileItem{.item_id = id, .kind = cybou::FileItemKind::FOLDER, .name = "handoff", .modified_ms = 1}});
    std::vector<cybou::NewContent> children;
    const auto content = stager.Prepare("handoff", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
        return cybou::EncodePrivateApplicationDocument(document);
    });
    BOOST_REQUIRE(content);
    BOOST_REQUIRE(local.QueuePublication("handoff", *content, document));
    auto changed = *content;
    changed.bundle.content_key[0] ^= 1;
    BOOST_CHECK(!local.QueuePublication("handoff", changed, document));
    cybou::NetworkSyncService::ProcessOutbox(local, publication, network_db);
    const auto first = local.Outbox().front().status.operation_id;
    BOOST_REQUIRE(!first.IsNull());
    BOOST_REQUIRE(stager.Release("handoff"));
    for (const auto& leaf : content->leaves) BOOST_CHECK(fixture.runtime->GetChunkRetention().IsPinned(leaf));
    cybou::NetworkSyncService::ProcessOutbox(local, publication, network_db);
    BOOST_CHECK(local.Outbox().front().status.operation_id == first);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    cybou::NetworkSyncService::ProcessOutbox(local, publication, network_db);
    BOOST_CHECK(local.Outbox().front().status.operation_id == first);
    BOOST_CHECK(local.Outbox().front().status.phase == cybou::PublicationJobPhase::SECURING);
}
BOOST_AUTO_TEST_CASE(short_local_commands_continue_during_content_preparation)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-preparation.vault");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "preparation", "local.db", true};
    cybou::LocalApplicationService local{db};
    cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), db.Account(), &db};
    const auto mail = Mail(identity->GetKeyStore());
    BOOST_REQUIRE(local.ImportMail(mail));
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    std::promise<bool> prepared;
    auto completion = prepared.get_future();
    stager.Post([&] {
        std::vector<cybou::NewContent> children{{[&](std::span<unsigned char>) -> std::optional<std::size_t> {
            entered.set_value(); gate.wait(); return 0;
        }}};
        const auto result = stager.Prepare("slow-content", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
            return cybou::EncodePrivateApplicationDocument(mail.message);
        });
        prepared.set_value(result.has_value());
    });
    entered.get_future().wait();
    std::promise<bool> saved;
    auto local_completion = saved.get_future();
    local.Post([&] {
        bool ok{true};
        for (int i = 0; i < 50; ++i)
            ok = local.MoveMail(mail.message.message_id, i % 2 ? cybou::MailFolder::ARCHIVE : cybou::MailFolder::TRASH) && ok;
        ok = local.SaveDraft({.draft_id = "unblocked", .body = "local"}) && ok;
        cybou::FilesMutationBatch folder;
        const auto id = *cybou::NewPrivateItemId();
        folder.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, id,
            cybou::FileItem{.item_id = id, .kind = cybou::FileItemKind::FOLDER, .name = "unblocked", .modified_ms = 1}});
        std::vector<cybou::NewContent> children;
        const auto content = stager.Prepare("short-folder", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
            return cybou::EncodePrivateApplicationDocument(folder);
        });
        ok = content && local.QueuePublication("short-folder", *content, folder) && ok;
        saved.set_value(ok);
    });
    const bool independent = local_completion.wait_for(std::chrono::seconds{2}) == std::future_status::ready;
    const bool content_still_blocked = completion.wait_for(std::chrono::milliseconds{0}) == std::future_status::timeout;
    release.set_value();
    BOOST_CHECK(independent);
    BOOST_CHECK(content_still_blocked);
    BOOST_REQUIRE(local_completion.get());
    BOOST_REQUIRE(completion.get());
    BOOST_CHECK(local.GetMail(mail.message.message_id)->folder == cybou::MailFolder::ARCHIVE);
}
BOOST_AUTO_TEST_CASE(accepting_prepared_mail_preserves_newer_draft_edits)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-edit-during-send.vault");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "edit-send", "local.db", true};
    cybou::LocalApplicationService local{db};
    cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), db.Account(), &db};
    auto mail = Mail(identity->GetKeyStore());
    cybou::MailDraft draft{.draft_id = "editing", .body = "accepted text"};
    BOOST_REQUIRE(local.SaveDraft(draft));
    BOOST_REQUIRE(local.BindDraftToMessage(draft.draft_id, mail.message.message_id));
    BOOST_REQUIRE(local.CheckDraftSendPayload(draft, true));
    mail.message.body = draft.body;
    std::vector<cybou::NewContent> children;
    const auto content = stager.Prepare("edit-send", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
        return cybou::EncodePrivateApplicationDocument(mail.message);
    });
    BOOST_REQUIRE(content);
    draft.body = "newer local edit";
    BOOST_REQUIRE(local.SaveDraft(draft));
    BOOST_REQUIRE(local.QueuePublication("edit-send", *content, mail.message, std::nullopt, draft.draft_id));
    BOOST_REQUIRE_EQUAL(local.ListDrafts().size(), 1U);
    BOOST_CHECK_EQUAL(local.ListDrafts().front().body, draft.body);
    BOOST_CHECK_EQUAL(local.GetMail(mail.message.message_id)->message.body, "accepted text");
    BOOST_CHECK(!local.DeleteAcceptedDraft(draft.draft_id));
    BOOST_CHECK_EQUAL(local.ListDrafts().front().body, "newer local edit");
    draft.body = "accepted text";
    BOOST_REQUIRE(local.SaveDraft(draft));
    BOOST_CHECK(local.DeleteAcceptedDraft(draft.draft_id));
    BOOST_CHECK(local.ListDrafts().empty());
}
BOOST_AUTO_TEST_CASE(stopping_content_preparation_cancels_before_local_acceptance)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("local-stop-staging.vault");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "stop-staging", "local.db", true};
    cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), db.Account(), &db};
    const auto mail = Mail(identity->GetKeyStore());
    std::promise<void> entered;
    auto started = entered.get_future();
    std::promise<bool> result;
    auto finished = result.get_future();
    stager.Post([&] {
        bool first{true};
        std::vector<cybou::NewContent> children{{[&](std::span<unsigned char> bytes) -> std::optional<std::size_t> {
            if (first) { first = false; entered.set_value(); }
            std::fill(bytes.begin(), bytes.end(), 42);
            return bytes.size(); // no EOF: only stopping can finish this source
        }}};
        result.set_value(stager.Prepare("stopped", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
            return cybou::EncodePrivateApplicationDocument(mail.message);
        }).has_value());
    });
    started.wait();
    const auto before = std::chrono::steady_clock::now();
    stager.Stop();
    BOOST_CHECK(std::chrono::steady_clock::now() - before < std::chrono::seconds{2});
    BOOST_CHECK(!finished.get());
    BOOST_CHECK(!db.Has("outbox/job/stopped"));
    BOOST_REQUIRE(db.Get("staging/jobs"));
    BOOST_CHECK(db.Get("staging/jobs")->empty());
}
BOOST_AUTO_TEST_CASE(active_outbox_is_bounded_and_survives_restart)
{
    CybouServiceTestFixture fixture;
    auto identity = fixture.CreateIdentity("active-outbox.vault");
    cybou::PrivateApplicationStore db{identity->GetKeyStore(), fixture.directory / "active-outbox", "local.db", true};
    cybou::LocalContentStager stager{fixture.runtime->GetChunkBlobStore(), fixture.runtime->GetChunkRetention(),
        fixture.runtime->GetNetworkBinding(), db.Account(), &db};
    const auto mail = Mail(identity->GetKeyStore());
    std::vector<cybou::NewContent> children;
    const auto content = stager.Prepare("active-first", children, [&](std::span<const cybou::EncryptedTreeSummary>) {
        return cybou::EncodePrivateApplicationDocument(mail.message);
    });
    BOOST_REQUIRE(content);
    {
        cybou::LocalApplicationService local{db};
        BOOST_REQUIRE(local.QueuePublication("active-first", *content, mail.message));
        BOOST_REQUIRE(local.QueuePublication("active-second", *content, mail.message));
        BOOST_REQUIRE(local.QueuePublication("active-third", *content, mail.message));
        BOOST_REQUIRE(local.SetPublicationStatus("active-first", {.phase = cybou::PublicationJobPhase::PROTECTED}));
        BOOST_REQUIRE_EQUAL(local.PendingOutbox(1).size(), 1U);
        BOOST_CHECK_EQUAL(local.PendingOutbox(1).front().job_id, "active-second");
        BOOST_CHECK(local.PendingOutbox(0).empty());
        BOOST_CHECK_EQUAL(local.Outbox().size(), 3U);
    }
    {
        cybou::LocalApplicationService local{db};
        BOOST_REQUIRE_EQUAL(local.PendingOutbox(16).size(), 2U);
        BOOST_REQUIRE(local.SetPublicationStatus("active-first", {.phase = cybou::PublicationJobPhase::NEEDS_ATTENTION}));
        BOOST_CHECK_EQUAL(local.PendingOutbox(1).front().job_id, "active-first");
        // Rebuild an older store's missing index once, retaining all active jobs.
        BOOST_REQUIRE(db.Erase("outbox/active"));
        BOOST_CHECK_EQUAL(local.PendingOutbox(16).size(), 3U);
        BOOST_REQUIRE(local.SetPublicationStatus("active-first", {.phase = cybou::PublicationJobPhase::PROTECTED}));
        // Completed payloads are not decoded by subsequent network passes.
        BOOST_REQUIRE(db.Put("outbox/job/active-first", std::vector<unsigned char>{0}));
        BOOST_CHECK_EQUAL(local.PendingOutbox(16).size(), 2U);
        BOOST_CHECK_THROW(local.Outbox(), std::runtime_error);
    }
}
BOOST_AUTO_TEST_SUITE_END()
