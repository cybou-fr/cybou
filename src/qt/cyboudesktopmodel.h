// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPMODEL_H
#define BITCOIN_QT_CYBOUDESKTOPMODEL_H

#include <qt/cybouproduct.h>

#include <cybou/recovery_phrase.h>
#include <cybou/diagnostics.h>

#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QObject>
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
}

class CybouApplicationBackend;

struct CybouCapabilities {
    bool account_creation{false};
    bool payments{false};
    bool mail{false};
    bool files{false};
    bool sharing{false};
    bool version_history{false};
    /** Identity Authority preview is computed from finalized history. */
    bool authority{false};
    /** Live network validation (pre-finalization) exists. False until core provides it. */
    bool validation{false};
};

/**
 * Identity-centric desktop status. Pages render product state only;
 * finalized_height is shown in Diagnostics and Security Details.
 */
struct CybouDesktopStatus {
    QString network_name{"CYBOU DEV"};
    /** Canonical network identifier once core exposes it; empty until then. */
    QString network_id{};

    bool node_running{false};
    bool online{false};
    bool syncing{false};
    int peer_count{0};
    quint64 finalized_height{0};
    bool finality_known{false};
    QString sync_error;
    QString data_directory;

    CybouIdentityState identity_state{CybouIdentityState::None};
    CybouIdentityStep identity_step{CybouIdentityStep::PreparingKeys};
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
};

/**
 * Canonical network totals shown only to the proven network authority: the
 * unlocked Identity's recovery phrase derives the genesis PoA finalizer key.
 * Every value is read from this node's own validated finalized state.
 */
struct CybouNetworkAuthorityStatus {
    bool proven{false};
    quint64 finalized_height{0};
    quint64 identities{0};
    quint64 names{0};
    quint64 pending_name_commits{0};
    quint64 total_balance{0};
    quint64 total_system_balance{0};
    quint64 onboarding_pool{0};
    quint64 security_reward_pool{0};
    quint64 pending_fee_pool{0};
};

/**
 * Canonical CYBOU amount rendering.
 *
 * CYBOU is indivisible (decimals = 0, 1 CYBOU = minimum unit), so the
 * rendering is an integer with locale grouping — never a decimal fraction.
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
    const CybouCapabilities& capabilities() const { return m_capabilities; }

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
    void setCapabilities(const CybouCapabilities& capabilities);
    void setNetworkInfo(const QString& network_name, const QString& network_id);
    void setFinalizedHeight(quint64 finalized_height);
    void setPeerCount(int peer_count);
    const cybou::NodeDiagnosticsSnapshot& networkDiagnostics() const { return m_network_diagnostics; }
    void setNetworkDiagnostics(cybou::NodeDiagnosticsSnapshot snapshot);
    /** Authority-only view; `proven` is false for every other Identity. */
    const CybouNetworkAuthorityStatus& networkAuthority() const { return m_network_authority; }
    bool isNetworkAuthority() const { return m_network_authority.proven; }
    void setNetworkAuthority(const CybouNetworkAuthorityStatus& status);
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
    /** Recomputes storage_used from the Identity's files (live mode only). */
    void refreshStorageUsed();

    /**
     * Connects the Mail/Files application backend (not owned). Every
     * persistent Mail/Files action becomes a command on it, and the Mail and
     * Files collections below are the projection it reports back. Without a
     * backend, Mail and Files capabilities stay off.
     */
    void setApplicationBackend(CybouApplicationBackend* backend);
    CybouApplicationBackend* applicationBackend() const { return m_backend; }
    /** Requests Mail/Files; they turn on only while the backend can serve them. */
    void requestApplicationCapabilities(bool mail, bool files);

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
    void requestMoveMail(const QString& id, CybouMailFolder folder);
    /** Saves a draft in the Identity's private mailbox; returns its id. */
    QString requestSaveMailDraft(CybouMailItem draft);
    void requestDeleteMail(const QString& id);
    /**
     * Hands a composed message to the Mail backend. The message appears in
     * Sent as Preparing at once (optimistic); every later state comes from
     * the backend. Returns the message id, or empty when Mail is unavailable.
     */
    QString requestSendMail(CybouMailItem message);
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
     * service budget; one-time Identity Authority contribution). Runs off the
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

    /* ---- Identity Authority (derived preview; never social trust). ---- */
    const CybouAuthoritySummary& authority() const { return m_authority; }
    void setAuthority(const CybouAuthoritySummary& authority);

    /*
     * ---- Operation lifecycle, shared by Wallet, Mail, Files and Identity. ----
     * Product items carry an operation_id; the continuous status lives here
     * once, not copied into every item.
     */
    void setOperationStatus(const CybouOperationStatus& status);
    std::optional<CybouOperationStatus> operationStatus(const QString& operation_id) const;
    /**
     * The operation state to show for an item whose own state is `own`.
     * PoA finality and failure always win. Validated appears only while
     * validation is available and shown; otherwise it reads as Submitted.
     * It never affects balances or content durability.
     */
    CybouOperationState displayedOperationState(const QString& operation_id, CybouOperationState own) const;
    /** User preference: show "Validated" for operations (informational only). */
    bool validationStatusShown() const { return m_validation_shown; }
    void setValidationStatusShown(bool shown);

    const QVector<CybouContact>& contacts() const { return m_contacts; }
    void setContacts(QVector<CybouContact> contacts);
    /** Live mode: people this Identity mailed, heard from or paid, most recent first. */
    void rebuildContacts();
    /** The network's support name (genesis-granted to the authority), e.g. "cybou.cybou". */
    static QString supportName();
    /** Fee a message to support pays (about 5x a short message); empty when unknown. */
    std::optional<quint64> supportMailFee() const;

    const CybouRestoreProgress& restoreProgress() const { return m_restore_progress; }
    void setRestoreProgress(const CybouRestoreProgress& progress);

    /** Sets identity service provider and enables account_creation capability. */
    void setIdentityService(cybou::CybouIdentityService* identity_service);
    cybou::CybouIdentityService* identityService() const { return m_identity_service; }

    /** Sets wallet service provider and updates payments capability. */
    void setWalletService(cybou::CybouWalletService* wallet_service);
    cybou::CybouWalletService* walletService() const { return m_wallet_service; }

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
    QString requestCreateFolder(const QString& name, const QString& parent_id = {});
    void requestRenameFile(const QString& id, const QString& name);
    void requestMoveFile(const QString& id, const QString& parent_id);
    QString requestCopyFile(const QString& id, const QString& parent_id);
    void requestFileStarred(const QString& id, bool starred);
    void requestTrashFile(const QString& id);
    void requestRestoreFile(const QString& id);
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
    void capabilitiesChanged();
    void namesChanged();
    void mailChanged();
    void contactsChanged();
    /** Views showing `old_id` switch to `new_id` (temporary send id became permanent). */
    void mailIdReplaced(const QString& old_id, const QString& new_id);
    void filesChanged();
    void activityChanged();
    void walletChanged();
    void createIdentityRequested();
    void identityCreationFailed(const QString& reason);
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
    void authorityChanged();
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
    std::optional<quint64> m_payment_fee;
    bool m_recovery_rotation_pending{false};
    bool m_fixture_mode{false};
    CybouDesktopStatus m_status;
    CybouCapabilities m_capabilities;
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
    QVector<CybouWalletEntry> m_wallet_entries;
    QVector<CybouContact> m_contacts;
    CybouRestoreProgress m_restore_progress;
    CybouAuthoritySummary m_authority;
    QHash<QString, CybouOperationStatus> m_operations;
    cybou::NodeDiagnosticsSnapshot m_network_diagnostics;
    CybouNetworkAuthorityStatus m_network_authority;
    bool m_validation_shown{true};

    void refreshFinalizedName();
    /** True when private Mail/Files commands may be issued. */
    bool mailReady() const;
    bool filesReady() const;
    /** Opens or closes the backend's Identity session to match identity_state. */
    void syncIdentitySession();
    bool m_session_open{false};
    /** Mail/Files capabilities never exceed what the backend can do. */
    CybouCapabilities honest(CybouCapabilities capabilities) const;
    CybouCapabilities m_requested_capabilities;
};

#endif // BITCOIN_QT_CYBOUDESKTOPMODEL_H
