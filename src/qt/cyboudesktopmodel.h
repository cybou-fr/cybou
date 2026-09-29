// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPMODEL_H
#define BITCOIN_QT_CYBOUDESKTOPMODEL_H

#include <qt/cybouproduct.h>

#include <QDateTime>
#include <QLocale>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>
#include <thread>

namespace cybou {
class CybouIdentityService;
class CybouWalletService;
class CybouNameService;
}

struct CybouCapabilities {
    bool account_creation{false};
    bool payments{false};
    bool mail{false};
    bool files{false};
    bool sharing{false};
    bool version_history{false};
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

    /* ---- Product collections (fed by adapters or fixtures). ---- */
    const QVector<CybouNameItem>& names() const { return m_names; }
    void setNames(QVector<CybouNameItem> names);

    const QVector<CybouMailItem>& mailItems() const { return m_mail; }
    void setMailItems(QVector<CybouMailItem> items);
    void upsertMailItem(const CybouMailItem& item);
    int unreadMailCount() const;
    const CybouMailItem* mailItem(const QString& id) const;
    /** Local mailbox state (never consensus state). */
    void setMailRead(const QString& id, bool read);
    void setMailStarred(const QString& id, bool starred);
    void moveMail(const QString& id, CybouMailFolder folder);
    /** Saves a local draft (never leaves the device); returns its id. */
    QString saveMailDraft(CybouMailItem draft);
    void deleteMail(const QString& id);
    /**
     * Hands a composed message to the Mail backend. The message appears in
     * Sent as Preparing; later states come only from the adapter.
     * Returns the message id, or empty when Mail is not connected.
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
    /** Retries a message in Needs attention from the retained local ciphertext. */
    void retrySendMail(const QString& id);
    void requestAttachmentDownload(const QString& message_id, const QString& attachment_id,
        const QString& destination);
    /** Local attachment for Compose: read metadata only; content is
        encrypted and chunked by the backend after Send. */
    CybouAttachmentItem localAttachment(const QString& path) const;

    const QVector<CybouFileItem>& fileItems() const { return m_files; }
    void setFileItems(QVector<CybouFileItem> items);
    void upsertFileItem(const CybouFileItem& item);

    const QVector<CybouActivityItem>& activity() const { return m_activity; }
    void setActivity(QVector<CybouActivityItem> items);
    void addActivity(const CybouActivityItem& item);

    const QVector<CybouWalletEntry>& walletEntries() const { return m_wallet_entries; }
    void setWalletEntries(QVector<CybouWalletEntry> entries);

    const QVector<CybouContact>& contacts() const { return m_contacts; }
    void setContacts(QVector<CybouContact> contacts);

    const CybouRestoreProgress& restoreProgress() const { return m_restore_progress; }
    void setRestoreProgress(const CybouRestoreProgress& progress);

    /** Sets identity service provider and enables account_creation capability. */
    void setIdentityService(cybou::CybouIdentityService* identity_service);
    cybou::CybouIdentityService* identityService() const { return m_identity_service; }

    /** Sets wallet service provider and updates payments capability. */
    void setWalletService(cybou::CybouWalletService* wallet_service);
    cybou::CybouWalletService* walletService() const { return m_wallet_service; }

    /* ---- Identity vault helpers (hide backend types from pages). ---- */
    /** True when a local vault already exists (unlock instead of create). */
    bool hasLocalVault() const;
    /** Generates the 24 recovery words for a new Identity, locally. */
    std::optional<QStringList> prepareNewIdentityWords();
    void discardPreparedIdentity();
    /** Re-authenticates with the vault password and returns the words. */
    std::optional<QStringList> revealRecoveryWords(const QString& vault_password);
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
    void requestLockVault();
    bool requestClaimName(const QString& label, const QString& vault_password);
    bool requestRecoveryRootRotation(const QStringList& new_phrase, const QString& vault_password,
        bool resume_pending = false);
    /**
     * Adds the file to the private catalog as Preparing and hands it to the
     * Files backend. Returns the new item id, or empty when Files is not
     * connected. Nothing is uploaded before RootPublication finality.
     */
    QString requestFileUpload(const QString& source_path, const QString& parent_id = {});
    void requestFileDownload(const QString& file_id, const QString& destination);
    /* Private catalog organization (encrypted catalog updates). */
    QString createFolder(const QString& name, const QString& parent_id = {});
    void renameFile(const QString& id, const QString& name);
    void moveFile(const QString& id, const QString& parent_id);
    void setFileStarred(const QString& id, bool starred);
    void trashFile(const QString& id);
    void restoreFile(const QString& id);
    void deleteFileForever(const QString& id);
    const CybouFileItem* fileItem(const QString& id) const;
    /**
     * Mail attachment -> Files: adds an independent private Files catalog
     * reference that reuses the existing protected content (no download or
     * re-upload). Returns the Files item id; repeated calls reuse it.
     */
    QString saveAttachmentToFiles(const QString& message_id, const QString& attachment_id);
    /** Files -> Mail: an attachment that references existing protected content. */
    std::optional<CybouAttachmentItem> attachmentFromFile(const QString& file_id) const;
    /** Adapter entries for upload/download progress. */
    void setFileState(const QString& id, CybouContentState state, int progress_percent = -1);
    void setFileRetrieval(const QString& id, CybouRetrievalState retrieval);

Q_SIGNALS:
    void statusChanged();
    void capabilitiesChanged();
    void namesChanged();
    void mailChanged();
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
    void fileUploadRequested(const QString& file_id, const QString& source_path);
    void fileDownloadRequested(const QString& file_id, const QString& destination);
    void mailSendRequested(const QString& id);
    void attachmentDownloadRequested(const QString& message_id, const QString& attachment_id,
        const QString& destination);

private:
    cybou::CybouIdentityService* m_identity_service{nullptr};
    cybou::CybouWalletService* m_wallet_service{nullptr};
    std::unique_ptr<cybou::CybouNameService> m_name_service;
    std::jthread m_name_worker;
    std::jthread m_recovery_rotation_worker;
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
    QVector<CybouWalletEntry> m_wallet_entries;
    QVector<CybouContact> m_contacts;
    CybouRestoreProgress m_restore_progress;

    void refreshFinalizedName();
};

#endif // BITCOIN_QT_CYBOUDESKTOPMODEL_H
