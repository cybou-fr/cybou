// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_CYBOUDESKTOPMODEL_H
#define CYBOU_QT_CYBOUDESKTOPMODEL_H

#include <qt/cybouproduct.h>

#include <cybou/recovery_phrase.h>
#include <cybou/diagnostics.h>

#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace cybou {
class CybouIdentityService;
class CybouWalletService;
class CybouNameService;
class CybouNodeRuntime;
}

class CybouApplicationBackend;

struct CybouFeatureAvailability {
    bool account_creation{false};
    bool payments{false};
    bool mail{false};
    bool files{false};
    bool sharing{false};
    bool version_history{false};
};

enum class CybouGeoAdmissionStatus : quint8 { Waiting, Ready };

/**
 * Identity-centric desktop status. Pages render product state only;
 * finalized_height is shown in Diagnostics and Security Details.
 */
struct CybouDesktopStatus {
    QString network_name{"CYBOU DEV"};
    /** Canonical network identifier once core exposes it; empty until then. */
    QString network_binding{};

    bool node_running{false};
    bool online{false};
    bool syncing{false};
    int peer_count{0};
    quint64 finalized_height{0};
    bool finality_known{false};
    /** Local runtime observation only; does not grant access to operator controls. */
    bool background_finalizer_active{false};
    /** Minutes this Identity's submitted work has waited with no new block; 0 when finality flows. */
    int finality_stall_minutes{0};
    /** Median finalized height connected peers announce (unverified), to show sync progress. */
    quint64 sync_target_height{0};
    QString sync_error;
    QString data_directory;
    CybouGeoAdmissionStatus geo_admission{CybouGeoAdmissionStatus::Waiting};

    CybouIdentityState identity_state{CybouIdentityState::None};
    CybouIdentityStep identity_step{CybouIdentityStep::PreparingKeys};
    /** What core is doing right now while creating or restoring (user-facing). */
    QString identity_progress;
    QString account_id;
    QString primary_name;
    QString name_claim_status;
    bool name_claim_pending{false};
    quint64 creation_height{0};
    quint32 key_epoch{0};

    quint64 balance{0};
    quint64 system_balance{0};

    quint64 storage_used{0};
    quint64 storage_quota{0};

    /** Network storage of this Identity's active publications, billing-unit bytes (DEC-279). */
    quint64 quota_used{0};
};

/**
 * Canonical network totals shown only to the proven network authority: the
 * unlocked Identity's recovery phrase derives the genesis PoA finalizer key.
 * Every value is read from this node's own validated finalized state.
 */
/** Local finalizer loop state of this desktop; never a canonical network property. */
enum class CybouFinalizerState : quint8 {
    /** The vault-backed PoA signer is not active (locked vault or key mismatch). */
    SignerUnavailable,
    /** The signer is active and the block production loop is running. */
    Finalizing,
    /** The operator paused block production; the signer stays active. */
    Paused,
    /** Signing safety stopped finalization fail-closed. */
    SafetyHalt,
};

struct CybouNetworkAuthorityStatus {
    bool proven{false};
    /** Local runtime signer state; this is not a canonical network property. */
    bool signer_enabled{false};
    CybouFinalizerState finalizer{CybouFinalizerState::SignerUnavailable};
    /** Locally executed candidate operations waiting for the next block. */
    quint64 candidates{0};
    /** Their OperationIDs (hex), in pool order. */
    QStringList candidate_ids;
    QList<quint64> candidate_wait_seconds;
    quint64 oldest_candidate_age_seconds{0};
    quint64 finalized_height{0};
    quint64 identities{0};
    quint64 names{0};
    quint64 pending_name_commits{0};
    quint64 total_balance{0};
    quint64 total_system_balance{0};
    quint64 storage_escrow{0};

    // Safety and settlement preview
    QString safety_journal_status;
    quint64 next_settlement_period{0};
    quint64 next_settlement_start_utc{0};
    quint64 next_settlement_due_utc{0};
    bool settlement_due{false};
    quint64 preview_payouts_count{0};
    quint64 preview_payouts_amount{0};
};


/**
 * Canonical CYBOU amount rendering.
 *
 * CYBOU is indivisible (decimals = 0, 1 CYBOU = minimum unit), so the
 * rendering is an integer with locale grouping â€” never a decimal fraction.
 */
inline QString cybouAmountText(quint64 amount)
{
    return QLocale{}.toString(amount) + QStringLiteral(" CYBOU");
}

/** Global connection wording: Offline, Connecting, Syncing, Synced or Needs attention. */
QString cybouConnectionText(const CybouDesktopStatus& status);

class CybouDesktopModel : public QObject
{
    Q_OBJECT

public:
    explicit CybouDesktopModel(QString network_name, QObject* parent = nullptr);
    ~CybouDesktopModel() override;

    const CybouDesktopStatus& status() const { return m_status; }
    const CybouFeatureAvailability& featureAvailability() const { return m_availability; }

    /** True when the desktop is fed by a deterministic UI fixture, not core. */
    bool fixtureMode() const { return m_fixture_mode; }
    /** Last finalized .cybou name seen for this data folder (public; shown while locked). */
    QString rememberedName() const;
    void setFixtureMode(bool fixture) { m_fixture_mode = fixture; }

    /** True once the user requested identity creation and core has not
        picked the request up yet. UI-side request tracking only. */
    bool identityCreationRequestPending() const { return m_identity_request_pending; }

    /* ---- Core adapter entries (doc 73). Unchanged values are a no-op. ---- */
    void setNodeStatus(bool running, int peer_count, bool online, const QString& data_directory = {});
    void setFeatureAvailability(const CybouFeatureAvailability& featureAvailability);
    void setNetworkInfo(const QString& network_name, const QString& network_binding);
    void setFinalizedHeight(quint64 finalized_height);
    void setPeerCount(int peer_count);
    void setGeoAdmissionStatus(CybouGeoAdmissionStatus status);
    const cybou::NodeDiagnosticsSnapshot& networkDiagnostics() const { return m_network_diagnostics; }
    void setNetworkDiagnostics(cybou::NodeDiagnosticsSnapshot snapshot);
    /** Authority-only view; `proven` is false for every other Identity. */
    const CybouNetworkAuthorityStatus& networkAuthority() const { return m_network_authority; }
    bool isNetworkAuthority() const { return m_network_authority.proven; }
    void setNetworkAuthority(const CybouNetworkAuthorityStatus& status);
    /* Central Authority operator commands; the controller carries them out. */
    void requestFinalizationPaused(bool paused);
    void requestFinalizeNow();
    void requestStorageSettlement();
    void setBackgroundFinalizerActive(bool active);
    void setSyncing(bool syncing);
    void setSyncError(const QString& error);
    void setLastSync(const QDateTime& when);
    QDateTime lastSync() const { return m_last_sync; }
    void setIdentityState(CybouIdentityState state, const QString& account_id = {},
        quint64 creation_height = 0);
    void setIdentityStep(CybouIdentityStep step);
    void setPrimaryName(const QString& name);
    void setBalances(quint64 balance, quint64 system_balance);
    void setStorageUsage(quint64 used, quint64 quota);
    /** Adapter entry: finalized network storage of this Identity's publications. */
    void setResourceUsage(quint64 quota_used);
    /** Recomputes storage_used from the Identity's files (live mode only). */
    void refreshStorageUsed();

    /**
     * Connects the Mail/Files application backend (not owned). Every
     * persistent Mail/Files action becomes a command on it, and the Mail and
     * Files collections below are the projection it reports back. Without a
     * backend, Mail and Files featureAvailability stay off.
     */
    void setApplicationBackend(CybouApplicationBackend* backend);
    CybouApplicationBackend* applicationBackend() const { return m_backend; }
    /** Requests Mail/Files; they turn on only while the backend can serve them. */
    void requestApplicationFeatureAvailability(bool mail, bool files);

    /* ---- Product collections (fed by adapters or fixtures). ---- */
    const QVector<CybouNameItem>& names() const { return m_names; }
    void setNames(QVector<CybouNameItem> names);

    const QVector<CybouMailItem>& mailItems() const { return m_mail; }
    void setMailItems(QVector<CybouMailItem> items);
    void upsertMailItem(const CybouMailItem& item);
    void removeMailItem(const QString& id);
    int unreadMailCount() const;
    const CybouMailItem* mailItem(const QString& id) const;
    /* Mailbox organization: backend commands; the projection follows its reply. */
    void requestMailRead(const QString& id, bool read);
    void requestMailStarred(const QString& id, bool starred);
    using CommandDone = std::function<void(bool, const QString&)>;
    bool requestApplicationRefresh(CommandDone done = {});
    bool applicationRefreshing() const { return m_application_refreshing; }
    QDateTime lastApplicationRefresh() const { return m_last_application_refresh; }
    QString applicationRefreshError() const { return m_application_refresh_error; }
    void requestMoveMail(const QString& id, CybouMailFolder folder, CommandDone done = {});
    const QVector<CybouApplicationTask>& applicationTasks() const { return m_application_tasks; }
    QString resolvedMailId(const QString& id) const { return m_mail_ids.value(id, id); }
    /** Saves a draft in the Identity's private mailbox; returns its id. */
    QString requestSaveMailDraft(CybouMailItem draft, CommandDone done = {});
    void requestDeleteMail(const QString& id, CommandDone done = {});
    /**
     * Hands a composed message to the Mail backend. The message appears in
     * Sent as Preparing at once (optimistic); every later state comes from
     * the backend. Returns the message id, or empty when Mail is unavailable.
     */
    QString requestSendMail(CybouMailItem message, CommandDone done = {});
    /** Adapter entry: lifecycle update for an outgoing message. */
    void setMailState(const QString& id, CybouContentState state);
    /** Adapter entry: per-attachment lifecycle and optional progress. */
    void setAttachmentState(const QString& message_id, const QString& attachment_id,
        CybouContentState state, int progress_percent = -1);
    /** Adapter entry: retrieval progress for a received attachment. */
    void setAttachmentRetrieval(const QString& message_id, const QString& attachment_id,
        CybouRetrievalState retrieval);
    /** Retries a message in Needs attention. */
    void requestRetryMail(const QString& id);
    /** Trash only: removes messages from this mailbox for good (local tombstone; history is not erased). */
    void requestDeleteMailForever(const QStringList& ids, CommandDone done = {});
    void requestAttachmentDownload(const QString& message_id, const QString& attachment_id,
        const QString& destination);
    /** Local attachment for Compose: read metadata only; content is
        encrypted and chunked by the backend after Send. */
    CybouAttachmentItem localAttachment(const QString& path) const;

    const QVector<CybouFileItem>& fileItems() const { return m_files; }
    void setFileItems(QVector<CybouFileItem> items);
    void upsertFileItem(const CybouFileItem& item);
    void removeFileItems(const QStringList& ids);

    const QVector<CybouActivityItem>& activity() const { return m_activity; }
    void setActivity(QVector<CybouActivityItem> items);
    void addActivity(const CybouActivityItem& item);

    const QVector<CybouWalletEntry>& walletEntries() const { return m_wallet_entries; }
    void setWalletEntries(QVector<CybouWalletEntry> entries);
    /** Deterministic network service fee for a payment, when known. */
    std::optional<quint64> paymentFee() const { return m_payment_fee; }
    void setPaymentFee(std::optional<quint64> fee) { m_payment_fee = fee; }
    bool paymentPending() const { return m_payment_pending; }
    /**
     * Irreversibly moves `amount` from Balance to System Balance (network
     * service budget). Runs off the
     * GUI thread; the result arrives via systemLockFinished.
     */
    bool requestLockToSystemBalance(quint64 amount);
    /** What a network service fee paid for ("Mail: subject", "File: name"), from its OperationID. */
    QString feePurpose(const QString& operation_id) const;
    /**
     * Sends CYBOU to a .cybou name from Balance. Resolution and submission
     * run off the GUI thread; the result arrives via paymentFinished.
     * Returns false when a payment is already in flight or inputs are invalid.
     */
    bool requestPayment(const QString& to_name, quint64 amount);
    /** Adapter entry: payment finished (ok) or failed with a reason. */
    void setPaymentFinished(bool ok, const QString& error = {});

    /*
     * ---- Operation lifecycle, shared by Wallet, Mail, Files and Identity. ----
     * Product items carry an operation_id; the continuous status lives here
     * once, not copied into every item.
     */
    void setOperationStatus(const CybouOperationStatus& status);
    std::optional<CybouOperationStatus> operationStatus(const QString& operation_id) const;
    /**
     * The operation state to show for an item whose own state is `own`.
     * PoA finality and failure always win.
     */
    CybouOperationState displayedOperationState(const QString& operation_id, CybouOperationState own) const;

    const QVector<CybouContact>& contacts() const { return m_contacts; }
    void setContacts(QVector<CybouContact> contacts);
    /** Live mode: people this Identity mailed, heard from or paid, most recent first. */
    void rebuildContacts();
    /** The network's support name (genesis-granted to the authority), e.g. "cybou.cybou". */
    static QString supportName();
    /** Fee a message to support pays (about 5x a short message); empty when unknown. */
    std::optional<quint64> supportMailFee() const;

    CybouApplicationLoadState applicationLoadState() const { return m_application_load_state; }
    quint64 applicationLoadScanned() const { return m_application_load_scanned; }
    quint64 applicationLoadTotal() const { return m_application_load_total; }
    const QString& applicationLoadError() const { return m_application_load_error; }
    const CybouRestoreProgress& restoreProgress() const { return m_restore_progress; }
    void setRestoreProgress(const CybouRestoreProgress& progress);

    /** Sets identity service provider and enables account_creation capability. */
    void setIdentityService(cybou::CybouIdentityService* identity_service);
    cybou::CybouIdentityService* identityService() const { return m_identity_service; }

    /** Sets wallet service provider and updates payments capability. */
    void setWalletService(cybou::CybouWalletService* wallet_service);
    cybou::CybouWalletService* walletService() const { return m_wallet_service; }

    /** Sets node runtime provider for verified history and chain lookups. */
    void setNodeRuntime(cybou::CybouNodeRuntime* runtime) { m_node_runtime = runtime; }
    cybou::CybouNodeRuntime* nodeRuntime() const { return m_node_runtime; }

    /* ---- Technical diagnostics & blockchain inspection (console and inspector). ---- */
    CybouFileChunkDiagnostics inspectFileChunks(const QString& file_id) const;
    void setFixtureChunkDiagnostics(const QString& file_id, CybouFileChunkDiagnostics diag);
    CybouBlockExplorerInfo inspectBlock(const QString& id_or_height) const;
    void setFixtureBlock(const QString& key, CybouBlockExplorerInfo info);
    CybouOperationExplorerInfo inspectOperation(const QString& op_id) const;
    void setFixtureOperation(const QString& op_id, CybouOperationExplorerInfo info);
    QVector<CybouHistoryItem> inspectHistory(int page, int page_size = 10) const;
    void setFixtureHistory(QVector<CybouHistoryItem> items);

    /**
     * Short confirmation for the user ("Moved to Trash"), optionally with
     * one action such as Undo. Rendered by the shell's notifier.
     */
    void notify(const QString& text, const QString& action_label = {}, std::function<void()> action = {});

    /* ---- Identity vault helpers (hide backend types from pages). ---- */
    /** True when a local vault already exists (unlock instead of create). */
    bool hasLocalVault() const;
    /** Generates the 24 recovery words for a new Identity, locally. */
    std::optional<QStringList> prepareNewIdentityWords();
    void discardPreparedIdentity();
    /** Re-authenticates with the vault password and returns the words. */
    std::optional<QStringList> revealRecoveryWords(const QString& vault_password);
    /** The recovery word list (for autocomplete and per-word checks). */
    static const QStringList& recoveryWordList();
    static bool isRecoveryWord(const QString& word);
    /** True when the phrase has 24 words that decode to valid entropy. */
    bool recoveryPhraseValid(const QString& phrase) const;
    /** Fresh 24 words for replacing the recovery phrase (IdentityRotate). */
    std::optional<QStringList> generateRotationWords();
    /** True when an encrypted rotation candidate awaits finality. */
    bool hasPendingRecoveryRotation() const;
    /** True while an IdentityRotate request is in flight. */
    bool recoveryRotationPending() const { return m_recovery_rotation_pending; }
    void setKeyEpoch(quint32 key_epoch);
    /** Empty when label is a valid .cybou label, else a user-facing reason. */
    QString nameLabelProblem(const QString& label) const;
    QString recipientNameProblem(const QString& label) const;
    /** Adapter entry: the pending name claim finished (success or not). */
    void setNameClaimFinished();

    /* ---- UI -> core requests. The UI emits; adapters do the work. ---- */
    void requestCreateIdentity(const QString& vault_password);
    bool requestRestoreIdentity(const QString& recovery_phrase, const QString& vault_password);
    bool requestUnlockIdentity(const QString& vault_password);
    /**
     * Opens the vault off the GUI thread (password KDF) and calls done on
     * the GUI thread. Identity state updates happen before done runs.
     */
    void requestUnlockIdentityAsync(const QString& vault_password, std::function<void(bool ok)> done);
    /** Re-authenticates off the GUI thread, then returns the words. */
    void revealRecoveryWordsAsync(const QString& vault_password,
        std::function<void(std::optional<QStringList> words)> done);
    void requestLockVault();
    /** Controller lifecycle hooks; called in order around CybouIdentityService::Lock(). */
    bool beginVaultLock();
    void completeVaultLock();
    bool requestClaimName(const QString& label, const QString& vault_password);
    bool requestRecoveryRootRotation(const QStringList& new_phrase, const QString& vault_password,
        bool resume_pending = false);
    /**
     * Hands a local file to the Files backend. The backend reports the new
     * item (Preparing first). Returns its id, or empty when Files is
     * unavailable.
     */
    QString requestFileUpload(const QString& source_path, const QString& parent_id = {});
    void requestFileDownload(const QString& file_id, const QString& destination);
    /*
     * Private catalog organization. These are backend commands: the Files
     * projection changes only when the backend reports the result. Commands
     * that create an item return its client id, or empty when unavailable.
     */
    QString requestCreateFolder(const QString& name, const QString& parent_id = {}, CommandDone done = {});
    void requestRenameFile(const QString& id, const QString& name, CommandDone done = {});
    void requestMoveFile(const QString& id, const QString& parent_id, CommandDone done = {});
    QString requestCopyFile(const QString& id, const QString& parent_id, CommandDone done = {});
    void requestFileStarred(const QString& id, bool starred);
    void requestTrashFile(const QString& id, CommandDone done = {});
    void requestRestoreFile(const QString& id, CommandDone done = {});
    void requestDeleteFile(const QString& id);
    /** Permanently deletes everything in Trash as one change (one network fee). */
    void requestEmptyTrash();
    void requestRetryFile(const QString& id);
    void requestDiscardFile(const QString& id);
    const CybouFileItem* fileItem(const QString& id) const;
    /**
     * Mail attachment -> Files: asks the backend for an independent Files
     * catalog reference to the existing protected content. Returns the Files
     * item id; repeated calls reuse an existing one.
     */
    QString requestSaveAttachmentToFiles(const QString& message_id, const QString& attachment_id);
    /** Files -> Mail: an attachment that references existing protected content. */
    std::optional<CybouAttachmentItem> attachmentFromFile(const QString& file_id) const;
    /** Adapter entries for upload/download progress. */
    void setFileState(const QString& id, CybouContentState state, int progress_percent = -1);
    void setFileRetrieval(const QString& id, CybouRetrievalState retrieval);

Q_SIGNALS:
    void statusChanged();
    void networkAuthorityChanged();
    void notificationRequested(const QString& text, const QString& action_label, std::function<void()> action);
    void featureAvailabilityChanged();
    void namesChanged();
    void mailChanged();
    void contactsChanged();
    /** Views showing `old_id` switch to `new_id` (temporary send id became permanent). */
    void mailIdReplaced(const QString& old_id, const QString& new_id);
    void filesChanged();
    void activityChanged();
    void applicationTasksChanged();
    void applicationRefreshChanged();
    void applicationLoadChanged();
    void walletChanged();
    void createIdentityRequested();
    void identityCreationFailed(const QString& reason);
    /** Restore from a recovery phrase failed; reason is user-facing. */
    void identityRestoreFailed(const QString& reason);
    void nameClaimFailed(const QString& reason);
    /** outcome: Finalized, Pending (accepted/uncertain) or Failed. */
    void recoveryRotationFinished(CybouOperationOutcome outcome, const QString& error);
    void nameClaimRequested(const QString& label);
    void lockVaultRequested();
    void restoreIdentityRequested();
    void paymentRequested(const QString& to_name, quint64 amount);
    /** ok means submitted to the network, not finalized: finality shows in activity. */
    void paymentFinished(bool ok, const QString& error);
    void systemLockFinished(bool ok, const QString& error);
    void finalizationPauseRequested(bool paused);
    void finalizeNowRequested();
    void storageSettlementRequested();
    void operationStatusChanged(const QString& operation_id);

private:
    cybou::CybouIdentityService* m_identity_service{nullptr};
    cybou::CybouWalletService* m_wallet_service{nullptr};
    CybouApplicationBackend* m_backend{nullptr};
    std::unique_ptr<cybou::CybouNameService> m_name_service;
    std::jthread m_name_worker;
    std::jthread m_recovery_rotation_worker;
    std::jthread m_payment_worker;
    std::jthread m_vault_worker;

    void finishUnlock();
    /** Submits (or resumes) IdentityRotate once the backend secured the old keys. */
    void startRecoveryRotation(cybou::RecoveryWords words, const QString& vault_password, bool resume_pending);
    bool m_payment_pending{false};
    /** Finality watch: no new block while own work waits means the network is not confirming. */
    QDateTime m_last_finality_change;
    QDateTime m_unconfirmed_since;
    void updateFinalityStall();
public:
    /** Re-evaluates the finality watch at  now (tests pass their own clock). */
    void checkFinalityStall(const QDateTime& now);
private:
    bool hasUnconfirmedWork() const;
    std::optional<quint64> m_payment_fee;
    bool m_recovery_rotation_pending{false};
    bool m_fixture_mode{false};
    CybouDesktopStatus m_status;
    CybouFeatureAvailability m_availability;
    QDateTime m_last_sync;
    bool m_identity_request_pending{false};
    QVector<CybouNameItem> m_names;
    QVector<CybouMailItem> m_mail;
    QVector<CybouFileItem> m_files;
    QVector<CybouActivityItem> m_activity;
    /** Events with no semantic record of their own (e.g. attachment saved to Files). */
    QVector<CybouActivityItem> m_extra_activity;
    /** Live mode: Recent activity is derived from Mail, Files and Wallet state. */
    void rebuildActivity();
    void scheduleRebuildActivity();
    void scheduleRebuildContacts();
    bool m_activity_update_scheduled{false};
    bool m_contacts_update_scheduled{false};
    QVector<CybouWalletEntry> m_wallet_entries;
    QVector<CybouContact> m_contacts;
    CybouRestoreProgress m_restore_progress;
    QHash<QString, CybouOperationStatus> m_operations;
    cybou::NodeDiagnosticsSnapshot m_network_diagnostics;
    CybouNetworkAuthorityStatus m_network_authority;
    bool m_vault_locking{false};
    cybou::CybouNodeRuntime* m_node_runtime{nullptr};
    QHash<QString, CybouFileChunkDiagnostics> m_fixture_chunk_diagnostics;
    QHash<QString, CybouBlockExplorerInfo> m_fixture_blocks;
    QHash<QString, CybouOperationExplorerInfo> m_fixture_operations;
    QVector<CybouHistoryItem> m_fixture_history;

    void refreshFinalizedName();
    /** True when private Mail/Files commands may be issued. */
    bool mailReady() const;
    bool filesReady() const;
    /** Opens or closes the backend's Identity session to match identity_state. */
    void syncIdentitySession();
    bool m_session_open{false};
    CybouApplicationLoadState m_application_load_state{CybouApplicationLoadState::Closed};
    quint64 m_application_load_scanned{0}, m_application_load_total{0};
    QString m_application_load_error;
    void setApplicationLoad(CybouApplicationLoadState state, quint64 scanned = 0, quint64 total = 0, const QString& error = {});
    quint64 m_mail_generation{0};
    bool m_application_refreshing{false};
    quint64 m_application_refresh_id{0};
    QDateTime m_last_application_refresh;
    QString m_application_refresh_error;
    CommandDone m_application_refresh_done;
    QVector<CybouApplicationTask> m_application_tasks;
    QHash<QString, QString> m_mail_ids;
    std::function<void(CybouCommandState, const QString&)> applicationCommand(
        const QString& item_id, const QString& title, CommandDone done,
        CybouTaskKind kind = CybouTaskKind::DraftSave, const QString& related_id = {},
        CybouTaskScope scope = CybouTaskScope::Mail);
    std::function<void(CybouCommandState, const QString&)> fileCommand(CommandDone done, const QString& item_id = {},
        const QString& title = {}, CybouTaskKind kind = CybouTaskKind::Move, const QString& related_id = {});
    /** Mail/Files featureAvailability never exceed what the backend can do. */
    CybouFeatureAvailability honest(CybouFeatureAvailability featureAvailability) const;
    CybouFeatureAvailability m_requested_availability;
};

#endif // CYBOU_QT_CYBOUDESKTOPMODEL_H
