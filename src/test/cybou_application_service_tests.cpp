// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/application_service.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/node_runtime.h>
#include <cybou/publication_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/storage_service.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_storage_test_network.h>

#include <boost/test/unit_test.hpp>

#include <memory>
#include <set>

namespace {

/** One Identity's application stack: the objects the desktop adapter will own. */
struct Party {
    std::unique_ptr<cybou::CybouIdentityService> identity;
    std::filesystem::path root;
    std::unique_ptr<cybou::PrivateApplicationStore> db;
    std::unique_ptr<cybou::KVStore> staging;
    std::unique_ptr<cybou::IdentityOperationCoordinator> coordinator;
    std::unique_ptr<cybou::StorageService> storage;
    std::unique_ptr<cybou::PublicationService> publication;
    std::unique_ptr<cybou::ApplicationService> application;

    Party(CybouServiceTestFixture& fixture, ProviderNetwork& network, const std::string& name)
        : identity{fixture.CreateIdentity(name + ".vault")}, root{fixture.directory / name}
    {
        coordinator = std::make_unique<cybou::IdentityOperationCoordinator>(*fixture.runtime,
            identity->GetKeyStore(), root / "operation.cyiop");
        staging = std::make_unique<cybou::KVStore>(cybou::KVStoreOptions{.memory_only = true});
        Open(fixture, network);
    }

    /** (Re)opens the Application DB and the services over it. */
    void Open(CybouServiceTestFixture& fixture, ProviderNetwork& network)
    {
        application.reset();
        publication.reset();
        storage.reset();
        db.reset(); // release the LevelDB lock before reopening
        db = std::make_unique<cybou::PrivateApplicationStore>(identity->GetKeyStore(), root / "app");
        storage = std::make_unique<cybou::StorageService>(*fixture.runtime, network, *db);
        publication = std::make_unique<cybou::PublicationService>(*fixture.runtime, identity->GetKeyStore(),
            *db, *coordinator, *staging);
        application = std::make_unique<cybou::ApplicationService>(*fixture.runtime, identity->GetKeyStore(),
            *db, *storage);
    }

    /** Deletes the local Application DB entirely, then rebuilds services. */
    void DestroyApplicationDb(CybouServiceTestFixture& fixture, ProviderNetwork& network)
    {
        application.reset();
        publication.reset();
        storage.reset();
        db.reset();
        std::filesystem::remove_all(root / "app");
        Open(fixture, network);
    }

    cybou::AccountId Account() const { return *identity->GetAccountId(); }
};

void Finalize(CybouServiceTestFixture& fixture, ProviderNetwork& network)
{
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    network.Sync();
}

cybou::EncryptedTreeSource BytesSource(const std::vector<unsigned char>& bytes)
{
    auto offset = std::make_shared<std::size_t>(0);
    return [&bytes, offset](std::span<unsigned char> out) -> std::optional<std::size_t> {
        const auto n = std::min(out.size(), bytes.size() - *offset);
        std::copy_n(bytes.begin() + *offset, n, out.begin());
        *offset += n;
        return n;
    };
}

std::optional<std::vector<unsigned char>> Download(CybouServiceTestFixture& fixture, cybou::StorageService& storage,
    const cybou::ChunkId& root, const cybou::ContentKey& key)
{
    std::vector<unsigned char> out;
    std::set<cybou::ChunkId> seen;
    const auto written = cybou::FetchEncryptedChunkTree(
        std::span<const unsigned char, 32>{fixture.runtime->GetNetworkId().begin(), 32}, key, root,
        [&](const cybou::ChunkId& id) { return storage.Fetch(id); },
        [](std::span<const unsigned char>) { return true; },
        [&](const cybou::ChunkId& id) { return seen.insert(id).second; },
        [&](std::span<const unsigned char> data) {
            out.insert(out.end(), data.begin(), data.end());
            return true;
        },
        64ULL << 20);
    if (!written) return std::nullopt;
    return out;
}

/** Drops every locally cached chunk, so reads must come from remote providers. */
void EvictLocal(CybouServiceTestFixture& fixture, const std::vector<cybou::ChunkId>& chunks)
{
    for (const auto& id : chunks) fixture.runtime->GetChunkBlobStore().Remove(id);
}

cybou::MailMessage Message(const cybou::AccountId& to, const std::string& subject, const std::string& body)
{
    cybou::MailMessage message;
    message.message_id = *cybou::NewPrivateItemId();
    message.recipient_account_id = to;
    message.client_timestamp_ms = 1'790'000'000'000ULL;
    message.subject = subject;
    message.body = body;
    return message;
}

} // namespace

BOOST_AUTO_TEST_SUITE(cybou_application_service_tests)

BOOST_AUTO_TEST_CASE(text_mail_reaches_offline_recipient_and_rebuilds_sent)
{
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    Party alice{fixture, network, "alice"};
    Party bob{fixture, network, "bob"};
    Party carol{fixture, network, "carol"};
    network.Sync();

    // Alice sends "Hello" to Bob, who is offline (not scanning).
    const auto hello = Message(bob.Account(), "Hello", "Hello Bob, this is Alice.");
    auto sent = alice.publication->PublishMail("mail-hello", hello);
    BOOST_REQUIRE_MESSAGE(sent.phase == cybou::PublicationJobPhase::WAITING_FINALITY, sent.error);
    // Before finality nothing is remote and the job is not Protected.
    BOOST_CHECK_EQUAL(network.puts, 0);
    Finalize(fixture, network);
    auto jobs = alice.publication->ProcessDurability(*alice.storage);
    BOOST_REQUIRE_EQUAL(jobs.size(), 1U);
    BOOST_CHECK(jobs.front().second.phase == cybou::PublicationJobPhase::PROTECTED);
    const auto operation = jobs.front().second.operation_id;
    const auto publication = fixture.runtime->FindFinalizedRootPublication(operation);
    BOOST_REQUIRE(publication);
    BOOST_CHECK_EQUAL(publication->recipient_capsules.size(), 2U); // Bob + self
    // Exactly the same job never publishes twice.
    BOOST_CHECK(alice.publication->PublishMail("mail-hello", hello).operation_id == operation);

    // Carol publishes something unrelated to Bob.
    const auto carol_note = Message(carol.Account(), "Note to self", "Unrelated");
    BOOST_REQUIRE(carol.publication->PublishMail("carol-note", carol_note).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    carol.publication->ProcessDurability(*carol.storage);

    // Bob reconnects later; his cache has nothing local.
    EvictLocal(fixture, {publication->root_chunk_id});
    auto progress = bob.application->Scan();
    BOOST_CHECK(progress.Complete());
    auto inbox = bob.application->ListMail();
    BOOST_REQUIRE_EQUAL(inbox.size(), 1U);
    BOOST_CHECK_EQUAL(inbox.front().message.subject, "Hello");
    BOOST_CHECK_EQUAL(inbox.front().message.body, hello.body);
    BOOST_CHECK(inbox.front().sender == alice.Account()); // from the outer authorization
    BOOST_CHECK(!inbox.front().outgoing);
    BOOST_CHECK(inbox.front().folder == cybou::MailFolder::INBOX);
    BOOST_CHECK(!inbox.front().read);
    // Carol's publication left no record at all in Bob's DB.
    BOOST_CHECK(!bob.application->PublicationState(carol.publication->GetJob("carol-note")->operation_id));
    // Rescanning is idempotent.
    bob.application->Scan();
    BOOST_CHECK_EQUAL(bob.application->ListMail().size(), 1U);
    // Local mailbox state.
    BOOST_CHECK(bob.application->SetMailRead(hello.message_id, true));
    BOOST_CHECK(bob.application->GetMail(hello.message_id)->read);
    BOOST_CHECK(!bob.application->MoveMail(hello.message_id, cybou::MailFolder::SENT));

    // Alice's Sent comes from her self capsule, even after deleting her DB.
    alice.application->Scan();
    BOOST_REQUIRE_EQUAL(alice.application->ListMail().size(), 1U);
    alice.DestroyApplicationDb(fixture, network);
    BOOST_CHECK(alice.application->ListMail().empty());
    BOOST_CHECK(alice.application->Scan().Complete());
    const auto rebuilt = alice.application->ListMail();
    BOOST_REQUIRE_EQUAL(rebuilt.size(), 1U);
    BOOST_CHECK_EQUAL(rebuilt.front().message.subject, "Hello");
    BOOST_CHECK(rebuilt.front().outgoing);
    BOOST_CHECK(rebuilt.front().folder == cybou::MailFolder::SENT);
}

BOOST_AUTO_TEST_CASE(unavailable_root_does_not_block_later_mail)
{
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    Party alice{fixture, network, "alice"};
    Party bob{fixture, network, "bob"};
    network.Sync();

    const auto first = Message(bob.Account(), "First", "one");
    BOOST_REQUIRE(alice.publication->PublishMail("mail-1", first).phase == cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    BOOST_REQUIRE(alice.publication->ProcessDurability(*alice.storage).front().second.phase ==
        cybou::PublicationJobPhase::PROTECTED);
    const auto first_root = fixture.runtime->FindFinalizedRootPublication(
        alice.publication->GetJob("mail-1")->operation_id)->root_chunk_id;
    // Storage is unreachable and the root is not cached: temporarily unavailable.
    EvictLocal(fixture, {first_root});
    network.SetAllOffline(true);
    const auto second = Message(bob.Account(), "Second", "two");
    BOOST_REQUIRE(alice.publication->PublishMail("mail-2", second).phase == cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);

    auto progress = bob.application->Scan();
    BOOST_CHECK_EQUAL(progress.scanned_height, progress.finalized_height); // scanning continued
    BOOST_CHECK_EQUAL(progress.unavailable_roots, 1U);
    auto mail = bob.application->ListMail();
    BOOST_REQUIRE_EQUAL(mail.size(), 1U);
    BOOST_CHECK_EQUAL(mail.front().message.subject, "Second");
    BOOST_CHECK(bob.application->PublicationState(alice.publication->GetJob("mail-1")->operation_id) ==
        cybou::AccessibleRootState::TEMPORARILY_UNAVAILABLE);

    // Storage comes back: the retry indexes the first message.
    network.SetAllOffline(false);
    progress = bob.application->Scan();
    BOOST_CHECK(progress.Complete());
    BOOST_CHECK_EQUAL(bob.application->ListMail().size(), 2U);
}

BOOST_AUTO_TEST_CASE(files_catalog_and_content_survive_rebuild)
{
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    Party owner{fixture, network, "owner"};
    network.Sync();

    std::vector<unsigned char> report(700 * 1024);
    for (std::size_t i{0}; i < report.size(); ++i) report[i] = static_cast<unsigned char>(i * 7 + 3);
    const auto work_id = *cybou::NewPrivateItemId();
    const auto report_id = *cybou::NewPrivateItemId();

    // Create "Work" and upload report.pdf in one publication.
    cybou::FilesMutationBatch create;
    cybou::FileItem work{.item_id = work_id, .kind = cybou::FileItemKind::FOLDER, .name = "Work"};
    cybou::FileItem file{.item_id = report_id, .kind = cybou::FileItemKind::FILE, .name = "report.pdf"};
    create.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, work_id, work});
    create.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, report_id, file});
    std::vector<std::pair<std::size_t, cybou::NewContent>> content;
    content.emplace_back(1, cybou::NewContent{BytesSource(report)});
    BOOST_REQUIRE(owner.publication->PublishFiles("files-create", create, std::move(content)).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    owner.application->Scan();
    auto uploaded = owner.application->GetFile(report_id);
    BOOST_REQUIRE(uploaded);
    BOOST_CHECK_EQUAL(uploaded->item.logical_size, report.size());
    BOOST_REQUIRE(uploaded->item.root_chunk_id && uploaded->item.content_key);

    // Rename and move into Work: a full-item UPSERT reusing the same content.
    auto renamed = uploaded->item;
    renamed.name = "report-final.pdf";
    renamed.parent_id = work_id;
    cybou::FilesMutationBatch move;
    move.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, report_id, renamed});
    BOOST_REQUIRE(owner.publication->PublishFiles("files-move", move).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    // A trashed-then-deleted scratch folder.
    const auto scratch_id = *cybou::NewPrivateItemId();
    cybou::FilesMutationBatch scratch;
    scratch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, scratch_id,
        cybou::FileItem{.item_id = scratch_id, .parent_id = cybou::FilesTrashParent(),
            .kind = cybou::FileItemKind::FOLDER, .name = "Scratch"}});
    // A second change while the first is unresolved is queued, not failed.
    const auto scratch_result = owner.publication->PublishFiles("files-scratch", scratch);
    BOOST_REQUIRE_MESSAGE(scratch_result.phase == cybou::PublicationJobPhase::QUEUED, scratch_result.error);
    Finalize(fixture, network);
    // The queued change is rebuilt against the new nonce and submitted.
    owner.publication->ProcessDurability(*owner.storage);
    BOOST_REQUIRE(owner.publication->GetJob("files-scratch")->phase == cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    cybou::FilesMutationBatch remove;
    remove.mutations.push_back({cybou::FileMutationKind::DELETE_ITEM, scratch_id, std::nullopt});
    BOOST_REQUIRE(owner.publication->PublishFiles("files-delete", remove).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    for (const auto& [id, status] : owner.publication->ProcessDurability(*owner.storage)) {
        BOOST_CHECK_MESSAGE(status.phase == cybou::PublicationJobPhase::PROTECTED, id);
    }

    const auto check_catalog = [&] {
        const auto files = owner.application->ListFiles();
        BOOST_CHECK_EQUAL(files.size(), 2U); // Work + report; Scratch deleted
        const auto current = owner.application->GetFile(report_id);
        BOOST_REQUIRE(current);
        BOOST_CHECK_EQUAL(current->item.name, "report-final.pdf");
        BOOST_CHECK(current->item.parent_id == work_id);
        BOOST_CHECK(!owner.application->GetFile(scratch_id));
        return current->item;
    };
    BOOST_CHECK(owner.application->Scan().Complete());
    check_catalog();

    const auto upload_operation = owner.publication->GetJob("files-create")->operation_id;
    const auto reuse_operation = owner.publication->GetJob("files-move")->operation_id;
    BOOST_REQUIRE(owner.storage->GetDurability(upload_operation));
    BOOST_CHECK(owner.storage->GetDurability(upload_operation)->state == cybou::DurabilityState::PROTECTED);

    // Restart with no local DB and no local chunks: rebuild, then download.
    owner.DestroyApplicationDb(fixture, network);
    BOOST_CHECK(owner.application->Scan().Complete());
    const auto own_publications = owner.db->Get("storage/owned-publications");
    BOOST_REQUIRE(own_publications);
    BOOST_CHECK_GE(own_publications->size(), 32U);
    const auto finalized_upload = fixture.runtime->FindFinalizedRootPublication(upload_operation);
    BOOST_REQUIRE(finalized_upload);
    const auto has_provider_proof = [&](const cybou::ChunkId& id) {
        for (const auto& provider : network.Endpoints()) {
            if (network.GetProof(provider, upload_operation, id)) return true;
        }
        return false;
    };
    BOOST_CHECK(has_provider_proof(finalized_upload->root_chunk_id));
    BOOST_CHECK(has_provider_proof(*uploaded->item.root_chunk_id));
    const auto rebuilt_durability = owner.storage->GetDurability(upload_operation);
    BOOST_REQUIRE(rebuilt_durability);
    BOOST_CHECK(rebuilt_durability->state == cybou::DurabilityState::PROTECTED);
    const auto rebuilt_reuse_durability = owner.storage->GetDurability(reuse_operation);
    BOOST_REQUIRE(rebuilt_reuse_durability);
    BOOST_CHECK(rebuilt_reuse_durability->state == cybou::DurabilityState::PROTECTED);
    const auto item = check_catalog();
    // The content root is fetched from providers when not cached locally.
    EvictLocal(fixture, {*item.root_chunk_id});
    const auto downloaded = Download(fixture, *owner.storage, *item.root_chunk_id, *item.content_key);
    BOOST_REQUIRE(downloaded);
    BOOST_CHECK(*downloaded == report);
}

BOOST_AUTO_TEST_CASE(mail_attachment_and_files_reuse_protected_content)
{
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    Party alice{fixture, network, "alice"};
    Party bob{fixture, network, "bob"};
    network.Sync();

    // Alice mails a new attachment to Bob.
    std::vector<unsigned char> contract(400 * 1024, 0x5a);
    auto message = Message(bob.Account(), "Contract", "Signed copy attached.");
    message.attachments.push_back({.attachment_id = *cybou::NewPrivateItemId(), .filename = "contract.pdf"});
    std::vector<std::pair<std::size_t, cybou::NewContent>> attachments;
    attachments.emplace_back(0, cybou::NewContent{BytesSource(contract)});
    BOOST_REQUIRE(alice.publication->PublishMail("mail-contract", message, std::move(attachments)).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    BOOST_REQUIRE(alice.publication->ProcessDurability(*alice.storage).front().second.phase ==
        cybou::PublicationJobPhase::PROTECTED);

    bob.application->Scan();
    const auto received = bob.application->GetMail(message.message_id);
    BOOST_REQUIRE(received);
    BOOST_REQUIRE_EQUAL(received->message.attachments.size(), 1U);
    const auto& attachment = received->message.attachments.front();
    BOOST_CHECK_EQUAL(attachment.logical_size, contract.size());
    BOOST_CHECK(*Download(fixture, *bob.storage, attachment.root_chunk_id, attachment.content_key) == contract);

    // Mail attachment -> Bob's Files: a catalog reference to the same content, no upload.
    const auto saved_id = *cybou::NewPrivateItemId();
    cybou::FilesMutationBatch save;
    save.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, saved_id,
        cybou::FileItem{.item_id = saved_id, .kind = cybou::FileItemKind::FILE, .name = attachment.filename,
            .logical_size = attachment.logical_size, .root_chunk_id = attachment.root_chunk_id,
            .content_key = attachment.content_key}});
    const int puts_before = network.puts;
    BOOST_REQUIRE(bob.publication->PublishFiles("save-contract", save).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    bob.publication->ProcessDurability(*bob.storage);
    // Only the small Files metadata root was placed (2 replicas), not the contract again.
    BOOST_CHECK_LE(network.puts - puts_before, 2 * 1);
    bob.application->Scan();
    const auto saved = bob.application->GetFile(saved_id);
    BOOST_REQUIRE(saved);
    BOOST_CHECK(*Download(fixture, *bob.storage, *saved->item.root_chunk_id, *saved->item.content_key) == contract);

    // Files -> Mail: Bob forwards his protected file to Alice by reference.
    auto forward = Message(alice.Account(), "Fwd: Contract", "Here it is.");
    forward.attachments.push_back({.attachment_id = *cybou::NewPrivateItemId(), .filename = saved->item.name,
        .logical_size = saved->item.logical_size, .root_chunk_id = *saved->item.root_chunk_id,
        .content_key = *saved->item.content_key});
    BOOST_REQUIRE(bob.publication->PublishMail("mail-forward", forward).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    alice.application->Scan();
    const auto forwarded = alice.application->GetMail(forward.message_id);
    BOOST_REQUIRE(forwarded);
    BOOST_CHECK(*Download(fixture, *alice.storage, forwarded->message.attachments.front().root_chunk_id,
        forwarded->message.attachments.front().content_key) == contract);
}

BOOST_AUTO_TEST_CASE(rotation_bridge_restores_pre_rotation_content_on_clean_machine)
{
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    Party owner{fixture, network, "owner"};
    network.Sync();

    // Machine A: upload a file under the original KEM key.
    std::vector<unsigned char> original(300 * 1024);
    for (std::size_t i{0}; i < original.size(); ++i) original[i] = static_cast<unsigned char>(i ^ 0x3c);
    const auto file_id = *cybou::NewPrivateItemId();
    cybou::FilesMutationBatch upload;
    upload.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, file_id,
        cybou::FileItem{.item_id = file_id, .kind = cybou::FileItemKind::FILE, .name = "pre-rotation.bin"}});
    std::vector<std::pair<std::size_t, cybou::NewContent>> content;
    content.emplace_back(0, cybou::NewContent{BytesSource(original)});
    BOOST_REQUIRE(owner.publication->PublishFiles("files-upload", upload, std::move(content)).phase ==
        cybou::PublicationJobPhase::WAITING_FINALITY);
    Finalize(fixture, network);
    BOOST_REQUIRE(owner.publication->ProcessDurability(*owner.storage).front().second.phase ==
        cybou::PublicationJobPhase::PROTECTED);

    // New mnemonic: the bridge must be finalized, durable and verified first.
    auto next_entropy = cybou::GenerateRecoveryEntropy();
    BOOST_REQUIRE(next_entropy);
    const auto next_words = cybou::EncodeRecoveryWords(*next_entropy);
    const std::span<const unsigned char, 32> next{next_entropy->data(), 32};
    const auto bridge = owner.publication->PublishRecoveryBridge("bridge-1", next);
    BOOST_REQUIRE_MESSAGE(bridge.phase == cybou::PublicationJobPhase::WAITING_FINALITY, bridge.error);
    BOOST_CHECK(!owner.publication->VerifyRecoveryBridge("bridge-1", next, *owner.storage)); // not final yet
    Finalize(fixture, network);
    for (const auto& [id, status] : owner.publication->ProcessDurability(*owner.storage)) {
        BOOST_CHECK_MESSAGE(status.phase == cybou::PublicationJobPhase::PROTECTED, id);
    }
    // A different phrase cannot open the bridge.
    std::array<unsigned char, 32> wrong{};
    wrong.fill(0x11);
    BOOST_CHECK(!owner.publication->VerifyRecoveryBridge("bridge-1", wrong, *owner.storage));
    BOOST_REQUIRE(owner.publication->VerifyRecoveryBridge("bridge-1", next, *owner.storage));

    // Only now rotate the Identity.
    const auto pending = owner.identity->RotateIdentitySync(next_words, "correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(pending.phase == cybou::IdentityOperationPhase::ACCEPTED, pending.error);
    BOOST_REQUIRE(fixture.runtime->ProduceBlock());
    BOOST_REQUIRE(owner.identity->ResumeIdentityRotationSync("correct horse battery staple").phase ==
        cybou::IdentityOperationPhase::FINALIZED);
    network.Sync();
    const auto account = owner.Account();

    // Machine B: fresh install, new mnemonic only, no local DB and no cached content.
    const auto restored_path = fixture.directory / "machine-b.vault";
    fixture.vaults.push_back(restored_path);
    cybou::CybouIdentityService restored{*fixture.runtime, restored_path};
    const auto recovered = restored.RestoreIdentitySync(next_words, "another correct horse battery staple");
    BOOST_REQUIRE_MESSAGE(recovered.success, recovered.error_message);
    BOOST_CHECK(*restored.GetAccountId() == account);
    cybou::crypto::CleanseMemory(next_entropy->data(), next_entropy->size());

    cybou::PrivateApplicationStore db{restored.GetKeyStore(), fixture.directory / "machine-b-app"};
    cybou::StorageService storage{*fixture.runtime, network, db};
    cybou::ApplicationService application{*fixture.runtime, restored.GetKeyStore(), db, storage};
    const auto progress = application.Scan();
    BOOST_CHECK(progress.Complete());
    BOOST_REQUIRE_EQUAL(application.RecoveryBridges().size(), 1U);
    const auto file = application.GetFile(file_id);
    BOOST_REQUIRE_MESSAGE(file, "pre-rotation file was not recovered");
    EvictLocal(fixture, {*file->item.root_chunk_id});
    const auto downloaded = Download(fixture, storage, *file->item.root_chunk_id, *file->item.content_key);
    BOOST_REQUIRE(downloaded);
    BOOST_CHECK(*downloaded == original);
}

BOOST_AUTO_TEST_CASE(drafts_persist_locally_and_are_never_published)
{
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    Party owner{fixture, network, "owner"};
    cybou::MailDraft draft{.draft_id = "draft-1", .to = "alice.cybou", .subject = "Plans",
        .body = "Half-written message", .updated_ms = 10,
        .attachments = {{.name = "notes.txt", .logical_size = 12, .source_path = "C:/tmp/notes.txt"},
            {.name = "report.pdf", .logical_size = 99, .reference_id = "ref-abc"}}};
    BOOST_REQUIRE(owner.application->SaveDraft(draft));
    cybou::MailDraft newer{.draft_id = "draft-2", .body = "Newer", .updated_ms = 20};
    BOOST_REQUIRE(owner.application->SaveDraft(newer));
    BOOST_CHECK(!owner.application->SaveDraft({.draft_id = "Bad/Id"}));
    const auto height = fixture.runtime->GetFinalizedHeight();

    // Survives reopening the Application DB (a restart).
    owner.Open(fixture, network);
    auto drafts = owner.application->ListDrafts();
    BOOST_REQUIRE_EQUAL(drafts.size(), 2U);
    BOOST_CHECK(drafts[0] == newer); // newest first
    BOOST_CHECK(drafts[1] == draft);
    draft.body = "Edited";
    BOOST_REQUIRE(owner.application->SaveDraft(draft));
    BOOST_CHECK_EQUAL(owner.application->ListDrafts().size(), 2U);
    BOOST_REQUIRE(owner.application->DeleteDraft("draft-2"));
    drafts = owner.application->ListDrafts();
    BOOST_REQUIRE_EQUAL(drafts.size(), 1U);
    BOOST_CHECK_EQUAL(drafts[0].body, "Edited");
    // Never published: no operation was submitted and no chunk left the device.
    BOOST_CHECK(fixture.runtime->GetFinalizedHeight() == height);
    BOOST_CHECK(!fixture.runtime->ProduceBlock() || fixture.runtime->GetBlockAtHeight(*height + 1)->block.operations.empty());
    BOOST_CHECK_EQUAL(network.puts, 0);
}

BOOST_AUTO_TEST_SUITE_END()
