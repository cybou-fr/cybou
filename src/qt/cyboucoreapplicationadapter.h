// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUCOREAPPLICATIONADAPTER_H
#define BITCOIN_QT_CYBOUCOREAPPLICATIONADAPTER_H

#include <qt/cybouapplicationbackend.h>

#include <QHash>
#include <QSet>

#include <filesystem>
#include <memory>

namespace cybou {
class CybouNodeRuntime;
class CybouIdentityService;
}

/**
 * The one meeting point between the desktop and the core application
 * services (ApplicationService, PublicationService, StorageService).
 *
 * While the Identity is unlocked it owns a worker thread that runs every core
 * call, periodically scans finalized history and advances publication
 * durability, and posts product snapshots back to the GUI thread. Pages never
 * see core types; core never sees Qt types.
 *
 * Current scope is live Mail with attachments and Files, including reuse of
 * protected content between them.
 */
class CybouCoreApplicationAdapter final : public CybouApplicationBackend
{
    Q_OBJECT

public:
    CybouCoreApplicationAdapter(cybou::CybouNodeRuntime& runtime, cybou::CybouIdentityService& identity,
        std::filesystem::path data_directory, QObject* parent = nullptr);
    ~CybouCoreApplicationAdapter() override;

    /** Worker tick interval; tests shorten it. */
    void setRefreshInterval(int ms);

    bool mailAvailable() const override { return m_mail_ready; }
    bool filesAvailable() const override { return m_mail_ready; }

    void openIdentity() override;
    void closeIdentity() override;

    void saveMailDraft(const CybouMailItem& draft) override;
    void sendMail(const CybouMailItem& message) override;
    void retryMail(const QString& id) override;
    void setMailRead(const QString& id, bool read) override;
    void setMailStarred(const QString& id, bool starred) override;
    void moveMail(const QString& id, CybouMailFolder folder) override;
    void deleteMail(const QString& id) override;
    void downloadAttachment(const QString& message_id, const QString& attachment_id,
        const QString& destination) override;
    void saveAttachmentToFiles(const QString& message_id, const QString& attachment_id,
        const QString& file_id) override;

    void prepareIdentityRotation(const QStringList& new_words,
        std::function<void(bool ok, const QString& error)> done) override;

    void uploadFile(const QString& file_id, const QString& source_path, const QString& parent_id) override;
    void downloadFile(const QString& file_id, const QString& destination) override;
    void createFolder(const QString& folder_id, const QString& name, const QString& parent_id) override;
    void renameFile(const QString& id, const QString& name) override;
    void moveFile(const QString& id, const QString& parent_id) override;
    void copyFile(const QString& id, const QString& copy_id, const QString& parent_id) override;
    void setFileStarred(const QString& id, bool starred) override;
    void trashFile(const QString& id) override;
    void restoreFile(const QString& id) override;
    void deleteFile(const QString& id) override;

private:
    struct Session;
    cybou::CybouNodeRuntime& m_runtime;
    cybou::CybouIdentityService& m_identity;
    const std::filesystem::path m_data_directory;
    std::unique_ptr<Session> m_session;
    bool m_mail_ready{false};
    int m_refresh_ms{3000};
    /** Draft edits and deletes not yet reflected by a worker snapshot. Drafts
        persist in the encrypted Application DB and are never published. */
    QHash<QString, CybouMailItem> m_pending_drafts;
    QSet<QString> m_deleted_drafts;
    /** Sends not yet taken over by the worker (or refused before publication). */
    QHash<QString, CybouMailItem> m_pending_sends;
    /** Model client IDs of created items -> private item IDs. */
    QHash<QString, QString> m_client_ids;
    /** Starred is local-only Files state. */
    QSet<QString> m_starred_files;
    QVector<CybouFileItem> m_last_files;
    /** Items shown before the worker's snapshot includes them. */
    QHash<QString, CybouFileItem> m_pending_files;
    void showPendingFile(const CybouFileItem& item);
    QString resolveFileId(const QString& id) const { return m_client_ids.value(id, id); }
    void emitFiles();

    /** GUI-thread handlers for worker results. */
    void applySnapshot(QVector<CybouMailItem> items, QVector<CybouFileItem> files, bool ready,
        CybouRestoreStepState restore);
    void setReady(bool ready);
    void notAvailable();
    /** Pending "secure data before rotation" request, answered exactly once. */
    std::function<void(bool, const QString&)> m_rotation_done;
    void finishRotation(bool ok, const QString& error);
};

#endif // BITCOIN_QT_CYBOUCOREAPPLICATIONADAPTER_H
