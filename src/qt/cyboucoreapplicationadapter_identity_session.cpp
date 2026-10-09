// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboucoreapplicationadapter_internal.h>

#include <exception>
#include <QElapsedTimer>
#include <QPointer>

using namespace cybou::qt_detail;

CybouCoreApplicationAdapter::IdentitySession::IdentitySession(CybouCoreApplicationAdapter* adapter, cybou::CybouNodeRuntime& rt, cybou::CybouKeyStore& ks,
    std::filesystem::path identity_root, int interval, cybou::StorageTransport* override_transport)
    : owner{adapter}, generation{adapter->m_session_generation}, runtime{rt}, keystore{ks}, root{std::move(identity_root)}, refresh_ms{interval},
      transport_override{override_transport}
{
    opening = std::jthread{[this](std::stop_token stop) { Run(stop); }};
}

CybouCoreApplicationAdapter::IdentitySession::~IdentitySession()
{
    if (opening.joinable()) opening.join();
    if (stager) stager->Stop();
    if (local) local->Stop();
    if (network) network->Stop();
}

bool CybouCoreApplicationAdapter::IdentitySession::Open()
{
    try {
        std::filesystem::create_directories(root);
        db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root, "app.db", true);
        local_db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root, "local.db", true);
        local = std::make_unique<cybou::LocalApplicationService>(*local_db);
        if (!local->MigrateFrom(*db)) throw std::runtime_error{"local application migration failed; source preserved"};
        stager = std::make_unique<cybou::LocalContentStager>(runtime.GetChunkBlobStore(), runtime.GetChunkRetention(),
            runtime.GetNetworkBinding(), local_db->Account(), local_db.get());
        transport = std::make_unique<cybou::RuntimeStorageTransport>(runtime);
        // Place as many remote replicas as the lease pays for (the network's storage_replica_target).
        storage = std::make_unique<cybou::StorageService>(runtime,
            transport_override ? *transport_override : static_cast<cybou::StorageTransport&>(*transport), *db,
            static_cast<std::uint8_t>(runtime.GetNetworkGenesis().GetProtocolParameters().storage_replica_target));
        publication = std::make_unique<cybou::PublicationService>(runtime, keystore, *db,
            runtime.GetIdentityOperationCoordinator(keystore), local_db.get());
        application = std::make_unique<cybou::ApplicationService>(runtime, keystore, *db, *storage);
        opened_recovery_key = keystore.GetRecoveryPublicKey();
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void CybouCoreApplicationAdapter::IdentitySession::Run(std::stop_token stop)
{
    const bool ready = !stop.stop_requested() && Open();
    if (ready) {
        network = std::make_unique<cybou::NetworkSyncService>([this] {
            try { Refresh(); }
            catch (const std::exception& e) {
                catching_up = false;
                qWarning() << "CYBOU network sync failed:" << e.what();
            }
        }, refresh_ms);
    }
    StateToGui([owner = owner, ready] {
        owner->setReady(ready);
        Q_EMIT owner->applicationLoadChanged(ready ? CybouApplicationLoadState::Loading : CybouApplicationLoadState::Failed,
            0, 0, ready ? QString{} : tr("Your encrypted data could not be opened. Existing data was preserved."));
    });
}

void CybouCoreApplicationAdapter::IdentitySession::Refresh()
{
    QElapsedTimer stage;
    stage.start();
    // Existing local data is usable before any network I/O on this pass.
    Snapshot(CybouRestoreStepState::Running, false);
    // Same Identity, new key material (IdentityRotate finalized elsewhere): reopen.
    if (keystore.HasKey() && keystore.GetAccountId() == std::optional{db->Account()} &&
        (keystore.GetRecoveryPublicKey() != opened_recovery_key || !db->IsUnlocked())) {
        StateToGui([owner = owner] { owner->identityKeysChanged(); });
        return;
    }
    cybou::NetworkSyncService::ProcessOutbox(*local, *publication, *db);
    const auto progress = application->Scan(16, 1);
    // Network DB locks have been released before importing semantic records.
    // Each import is a short local transaction; disk-only commands use no
    // network service mutex and never wait for subsequent durability I/O.
    for (const auto& record : application->ListMail(true))
        if (!local->ImportMail(record)) throw std::runtime_error{"cannot import local Mail"};
    for (const auto& record : application->ListFiles(true))
        if (!local->ImportFile(record)) throw std::runtime_error{"cannot import local Files"};
    const auto scan_ms = stage.restart();
    StateToGui([owner = owner, scanned = progress.scanned_height, total = progress.finalized_height, unavailable = progress.unavailable_roots] {
        if (!owner->m_initial_projection_ready)
            Q_EMIT owner->applicationLoadChanged(CybouApplicationLoadState::Loading, scanned, total, unavailable ?
                    tr("Waiting for encrypted content from peers… You can open the local view while CYBOU retries.") : QString{});
    });
    // Restore progress (and the no-pause scan loop) follows the history scan only. Content that
    // cannot be fetched is retried in the background with backoff and never keeps the restore
    // banner up or the scan loop spinning; publication cleanup still waits for a complete index.
    const bool history_scanned = progress.scanned_height >= progress.finalized_height;
    catching_up = !history_scanned;
    storage_projection.Refresh(progress.Complete());
    AdvanceRotation();
    const auto storage_ms = stage.restart();
    Snapshot(history_scanned ? CybouRestoreStepState::Done : CybouRestoreStepState::Running, progress.Complete() && progress.unavailable_roots == 0);
    if (qEnvironmentVariableIsSet("CYBOU_PROFILE_MAIL_MOVES"))
        qInfo() << "MAIL-DND-LATENCY refresh_scan_ms" << scan_ms << "refresh_storage_ms" << storage_ms
            << "refresh_snapshot_ms" << stage.elapsed();
}

void CybouCoreApplicationAdapter::IdentitySession::AdvanceRotation()
{
    if (!rotation) return;
    const auto job = storage_projection.jobs.find(rotation->job_id);
    if (job == storage_projection.jobs.end()) return;
    if (job->second.phase == cybou::PublicationJobPhase::PROTECTED) {
        const bool ok = publication->VerifyRecoveryBridge(rotation->job_id,
            std::span<const unsigned char, 32>{rotation->entropy.data(), 32}, *storage);
        rotation.reset();
        ToGui([owner = owner, ok] {
            owner->finishRotation(ok, ok ? QString{} : tr("Your recovery data could not be verified."));
        });
    } else if (job->second.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
        rotation.reset();
        ToGui([owner = owner, error = QString::fromStdString(job->second.error)] {
            owner->finishRotation(false, error.isEmpty() ? tr("Your recovery data could not be secured.") : error);
        });
    }
}

void CybouCoreApplicationAdapter::IdentitySession::Snapshot(CybouRestoreStepState restore, bool initial_complete)
{
    const auto revision = local->Revision();
    if (revision % 2 != 0) return;
    auto items = mail.Snapshot();
    auto file_items = files.FilesSnapshot();
    if (local->Revision() != revision) return;
    StateToGui([owner = owner, revision, items = std::move(items), file_items = std::move(file_items), restore, initial_complete]() mutable {
        if (owner->m_session->local->Revision() != revision) return;
        owner->applySnapshot(std::move(items), std::move(file_items), true, restore, initial_complete);
    });
}

void CybouCoreApplicationAdapter::IdentitySession::LocalFilesChanged()
{
    const auto delta_revision = ++files_delta_revision;
    const auto records = local->ListFiles();
    const auto outbox = local->Outbox();
    StateToGui([owner = owner, generation = generation, delta_revision, records, outbox] {
        // Staging and short commands may complete together. The latest complete
        // local catalog supersedes any older delta still queued on the GUI.
        if (owner->m_session->files_delta_revision.load() != delta_revision) return;
        const QPointer<CybouCoreApplicationAdapter> guard{owner};
        std::map<std::string, cybou::PublicationJobResult> status;
        for (const auto& pending : outbox) {
            if (const auto* batch = std::get_if<cybou::FilesMutationBatch>(&pending.document))
                for (const auto& change : batch->mutations) status[ToHex(change.item_id)] = pending.status;
        }
        QVector<CybouFileItem> updated;
        QStringList removed;
        std::set<QString> present;
        const auto trash = cybou::FilesTrashParent();
        for (const auto& record : records) {
            const auto& source = record.item;
            const auto id = QString::fromStdString(ToHex(source.item_id));
            present.insert(id);
            const auto old = std::find_if(owner->m_last_files.begin(), owner->m_last_files.end(), [&](const auto& item) { return item.id == id; });
            CybouFileItem item = old != owner->m_last_files.end() ? *old : CybouFileItem{};
            item.id = id;
            item.name = QString::fromStdString(source.name);
            item.folder = source.kind == cybou::FileItemKind::FOLDER;
            item.parent_id = source.parent_id && *source.parent_id != trash ? QString::fromStdString(ToHex(*source.parent_id)) : QString{};
            item.trashed = source.parent_id == std::optional{trash};
            auto parent = source.parent_id;
            for (std::size_t depth = 0; parent && depth < 64 && !item.trashed; ++depth) {
                if (*parent == trash) { item.trashed = true; break; }
                const auto ancestor = std::find_if(records.begin(), records.end(), [&](const auto& r) { return r.item.item_id == *parent; });
                parent = ancestor != records.end() ? ancestor->item.parent_id : std::nullopt;
            }
            item.logical_size = source.logical_size;
            item.modified = QDateTime::fromMSecsSinceEpoch(source.modified_ms);
            item.starred = record.starred;
            if (source.root_chunk_id) item.content_root_id = ChunkHex(*source.root_chunk_id);
            if (const auto pending = status.find(ToHex(source.item_id)); pending != status.end()) {
                item.state = StateOf(pending->second);
                item.operation_state = OperationOf(pending->second);
                item.operation_id = pending->second.operation_id.IsNull() ? QString{} : QString::fromStdString(pending->second.operation_id.GetHex());
                item.finalized_height = pending->second.finalized_height;
            }
            owner->m_pending_files.remove(id);
            updated.push_back(item);
        }
        for (const auto& old : owner->m_last_files) if (!present.contains(old.id)) removed.push_back(old.id);
        const auto previous = owner->m_last_files;
        owner->m_last_files = updated;
        // Semantic row deltas, never a network scan or a full model reset.
        for (const auto& item : updated) {
            const auto old = std::find_if(previous.begin(), previous.end(), [&](const auto& value) { return value.id == item.id; });
            if (old == previous.end() || *old != item) Q_EMIT owner->fileItemChanged(item);
            if (!guard || owner->m_session_generation != generation) return;
        }
        if (!removed.empty()) Q_EMIT owner->fileItemsRemoved(removed);
    });
}

void CybouCoreApplicationAdapter::IdentitySession::LocalMailChanged()
{
    const auto records = local->ListMail();
    const auto drafts = local->ListDrafts();
    const auto outbox = local->Outbox();
    StateToGui([owner = owner, generation = generation, records, drafts, outbox] {
        const QPointer<CybouCoreApplicationAdapter> guard{owner};
        QSet<QString> present;
        for (const auto& record : records) {
            const auto id = QString::fromStdString(ToHex(record.message.message_id));
            present.insert(id);
            auto item = owner->m_last_mail.value(id);
            item.id = id;
            item.folder = FolderOf(record.folder);
            item.outgoing = record.outgoing;
            item.unread = !record.read;
            item.starred = record.starred;
            item.subject = QString::fromStdString(record.message.subject);
            item.body = QString::fromStdString(record.message.body);
            item.preview = item.body.simplified().left(90);
            item.time = QDateTime::fromMSecsSinceEpoch(record.message.client_timestamp_ms);
            item.from_address = QString::fromStdString(record.sender.Value().GetHex());
            item.to_address = QString::fromStdString(record.message.recipient_account_id.Value().GetHex());
            if (item.from_name.isEmpty()) item.from_name = CybouProduct::shortId(item.from_address);
            if (item.to_name.isEmpty()) item.to_name = CybouProduct::shortId(item.to_address);
            for (const auto& pending : outbox) if (pending.job_id == id) {
                item.state = StateOf(pending.status);
                item.operation_state = OperationOf(pending.status);
            }
            const auto previous = owner->m_last_mail.constFind(id);
            const bool changed = previous == owner->m_last_mail.cend() || *previous != item;
            owner->m_last_mail.insert(id, item);
            if (changed) Q_EMIT owner->mailItemChanged(item);
            if (!guard || owner->m_session_generation != generation) return;
        }
        for (const auto& draft : drafts) {
            const auto id = QString::fromStdString(draft.draft_id);
            present.insert(id);
            auto item = owner->m_last_mail.value(id);
            item.id = id;
            item.draft = true;
            item.folder = CybouMailFolder::Drafts;
            item.to_name = QString::fromStdString(draft.to);
            item.subject = QString::fromStdString(draft.subject);
            item.body = QString::fromStdString(draft.body);
            item.time = QDateTime::fromMSecsSinceEpoch(draft.updated_ms);
            const auto previous = owner->m_last_mail.constFind(id);
            const bool changed = previous == owner->m_last_mail.cend() || *previous != item;
            owner->m_last_mail.insert(id, item);
            if (changed) Q_EMIT owner->mailItemChanged(item);
            if (!guard || owner->m_session_generation != generation) return;
        }
        const auto old_ids = owner->m_last_mail.keys();
        for (const auto& id : old_ids) if (!present.contains(id)) {
            owner->m_last_mail.remove(id);
            Q_EMIT owner->mailItemRemoved(id);
            if (!guard || owner->m_session_generation != generation) return;
        }
    });
}
