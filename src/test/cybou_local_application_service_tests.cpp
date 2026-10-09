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
BOOST_AUTO_TEST_SUITE_END()
