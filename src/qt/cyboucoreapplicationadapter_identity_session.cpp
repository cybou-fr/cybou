// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboucoreapplicationadapter_internal.h>

#include <exception>
#include <QElapsedTimer>

using namespace cybou::qt_detail;

CybouCoreApplicationAdapter::IdentitySession::IdentitySession(CybouCoreApplicationAdapter* adapter, cybou::CybouNodeRuntime& rt, cybou::CybouKeyStore& ks,
    std::filesystem::path identity_root, int interval, cybou::StorageTransport* override_transport)
    : owner{adapter}, generation{adapter->m_session_generation}, runtime{rt}, keystore{ks}, root{std::move(identity_root)}, refresh_ms{interval},
      transport_override{override_transport}
{
    scheduler.Start(*this);
}

CybouCoreApplicationAdapter::IdentitySession::~IdentitySession()
{
    scheduler.Stop();
}

bool CybouCoreApplicationAdapter::IdentitySession::Open()
{
    try {
        std::filesystem::create_directories(root);
        try {
            db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root);
        } catch (const cybou::PrivateApplicationStoreKeyMismatch&) {
            // Encrypted under keys replaced by IdentityRotate: the projection
            // is rebuilt from finalized history (RecoveryBridge included).
            std::filesystem::remove_all(root / "app.db");
            db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root);
        }
        transport = std::make_unique<cybou::RuntimeStorageTransport>(runtime);
        // Place as many remote replicas as the lease pays for (the network's storage_replica_target).
        storage = std::make_unique<cybou::StorageService>(runtime,
            transport_override ? *transport_override : static_cast<cybou::StorageTransport&>(*transport), *db,
            static_cast<std::uint8_t>(runtime.GetNetworkGenesis().GetProtocolParameters().storage_replica_target));
        publication = std::make_unique<cybou::PublicationService>(runtime, keystore, *db,
            runtime.GetIdentityOperationCoordinator(keystore));
        application = std::make_unique<cybou::ApplicationService>(runtime, keystore, *db, *storage);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void CybouCoreApplicationAdapter::IdentitySession::Run(std::stop_token stop)
{
    const bool ready = Open();
    StateToGui([owner = owner, ready] {
        owner->setReady(ready);
        Q_EMIT owner->applicationLoadChanged(ready ? CybouApplicationLoadState::Loading : CybouApplicationLoadState::Failed,
            0, 0, ready ? QString{} : tr("Your encrypted data could not be opened. Lock your Identity and try again."));
    });
    if (!ready) return;
    while (!stop.stop_requested()) {
        // Recovery scans back to back; caught-up sessions wait for commands or the tick.
        auto pending = scheduler.Take(stop, catching_up ? 0 : refresh_ms);
        // One failing command must not drop the rest of an already dequeued batch.
        for (auto& task : pending) {
            try { task(*this); }
            catch (const std::exception&) { qWarning() << "CYBOU application command failed"; }
        }

        // После выполнения команды сразу возвращаемся к очередям.
        // Refresh запускается только тогда, когда Take() не нашёл команд.
        if (!pending.empty()) continue;

        try {
            if (!stop.stop_requested()) Refresh();
        } catch (const std::exception& e) {
            // A failed refresh leaves the last snapshot in place; the next tick retries.
            // Never silently: a refresh that always fails freezes Mail and Files.
            catching_up = false; // Retry at the normal interval, never spin after a failed scan.
            qWarning() << "CYBOU application refresh failed:" << e.what();
            StateToGui([owner = owner] {
                if (!owner->m_initial_projection_ready)
                    Q_EMIT owner->applicationLoadChanged(CybouApplicationLoadState::Failed, 0, 0,
                        tr("Your data could not be prepared yet. CYBOU will retry automatically."));
            });
        }
    }
    // Commands issued just before locking (a saved draft, a send) still run.
    auto remaining = scheduler.Drain();
    for (auto& task : remaining) {
        try { task(*this); }
        catch (const std::exception&) { qWarning() << "CYBOU application command failed during shutdown"; }
    }
}

void CybouCoreApplicationAdapter::IdentitySession::Refresh()
{
    QElapsedTimer stage;
    stage.start();
    // Same Identity, new key material (IdentityRotate finalized elsewhere): reopen.
    if (!db->IsUnlocked() && keystore.HasKey() && keystore.GetAccountId() == std::optional{db->Account()}) {
        StateToGui([owner = owner] { owner->identityKeysChanged(); });
        return;
    }
    const auto progress = application->Scan();
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
    auto items = mail.Snapshot();
    auto file_items = files.FilesSnapshot();
    StateToGui([owner = owner, items = std::move(items), file_items = std::move(file_items), restore, initial_complete]() mutable {
        owner->applySnapshot(std::move(items), std::move(file_items), true, restore, initial_complete);
    });
}
