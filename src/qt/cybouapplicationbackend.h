// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_CYBOUAPPLICATIONBACKEND_H
#define CYBOU_QT_CYBOUAPPLICATIONBACKEND_H

#include <qt/cybouproduct.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

/**
 * The one Qt-side application backend contract for Mail and Files.
 *
 * CybouDesktopModel turns page actions into commands on this interface and
 * renders what the backend reports back. The backend owns the Identity's
 * semantic Mail/Files state (the encrypted Application DB); the model only
 * holds the last reported projection plus UI-local state.
 *
 * Commands and events use product DTOs only. Publication, capsule, chunk,
 * provider and finality mechanics stay behind the implementation:
 *
 *   CybouApplicationBackend
 *     â”œâ”€â”€ CybouFixtureApplicationBackend   (deterministic UI fixtures)
 *     â””â”€â”€ CybouCoreApplicationAdapter      (core services, once available)
 *
 * Ids passed to commands that create items (outgoing mail, uploads, folders,
 * copies, saved attachments) are opaque client ids chosen by the model; the
 * backend reports the new item under that id.
 *
 * All calls and signals happen on the GUI thread.
 */
class CybouApplicationBackend : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    using CommandProgress = std::function<void(CybouCommandState, const QString&)>;
    ~CybouApplicationBackend() override = default;

    /** True when real Mail/Files work can be carried out right now. */
    virtual bool mailAvailable() const = 0;
    virtual bool filesAvailable() const = 0;

    /* ---- Identity session. Private semantic data exists only while open. ---- */
    /** The Identity is unlocked: open its Application DB and report snapshots. */
    virtual void openIdentity() = 0;
    /** The Identity locked or went away: drop plaintext state and indexes. */
    virtual void closeIdentity() = 0;
    /** Read current local semantic indexes; no forced sync/audit or publication. */
    virtual void refreshProjection(CommandProgress progress) = 0;

    /* ---- Mail commands. ---- */
    virtual void saveMailDraft(const CybouMailItem& draft, CommandProgress progress = {}) = 0;
    /** message.id is the client id; the backend drives its lifecycle. */
    virtual void sendMail(const CybouMailItem& message, const QString& draft_id = {}, CommandProgress progress = {}) = 0;
    virtual void retryMail(const QString& id) = 0;
    virtual void setMailRead(const QString& id, bool read) = 0;
    virtual void setMailStarred(const QString& id, bool starred) = 0;
    virtual void moveMail(const QString& id, CybouMailFolder folder, CommandProgress progress = {}) = 0;
    virtual void deleteMail(const QString& id, CommandProgress progress = {}) = 0;
    /** Removes messages that are in Trash from this mailbox for good (local; history is not erased). */
    virtual void deleteMailForever(const QStringList& ids, CommandProgress progress = {}) = 0;
    virtual void downloadAttachment(const QString& message_id, const QString& attachment_id,
        const QString& destination) = 0;
    /** Mail -> Files. The backend decides how protected content is reused. */
    virtual void saveAttachmentToFiles(const QString& message_id, const QString& attachment_id,
        const QString& file_id) = 0;

    /**
     * Must succeed before the recovery phrase is replaced: secures what is
     * needed for content published under the current keys to stay readable
     * with the new phrase. done(ok, error) runs on the GUI thread, possibly
     * much later; ok=false means the current phrase must stay active.
     */
    virtual void prepareIdentityRotation(const QStringList& new_words,
        std::function<void(bool ok, const QString& error)> done)
    {
        Q_UNUSED(new_words);
        done(false, tr("Your data cannot be secured for a new recovery phrase right now."));
    }

    /**
     * The Identity key material was replaced (a finalized IdentityRotate).
     * Session state keyed to the old material must be reopened; nothing the
     * user can see should be lost.
     */
    virtual void identityKeysChanged() {}

    /* ---- Files commands. ---- */
    /** Creation IDs are permanent random 32-byte private item IDs in raw-order hex.
     * Backends retain them unchanged in pending and indexed projections. */
    virtual void uploadFile(const QString& file_id, const QString& source_path, const QString& parent_id) = 0;
    virtual void downloadFile(const QString& file_id, const QString& destination) = 0;
    virtual void createFolder(const QString& folder_id, const QString& name, const QString& parent_id) = 0;
    virtual void renameFile(const QString& id, const QString& name) = 0;
    virtual void moveFile(const QString& id, const QString& parent_id) = 0;
    virtual void copyFile(const QString& id, const QString& copy_id, const QString& parent_id) = 0;
    virtual void setFileStarred(const QString& id, bool starred) = 0;
    /** Trash, restore and delete apply to a folder's contents too. */
    virtual void trashFile(const QString& id) = 0;
    virtual void restoreFile(const QString& id) = 0;
    virtual void deleteFile(const QString& id) = 0;
    /** Deletes several items permanently; a backend may publish them as one change (one fee). */
    virtual void deleteFiles(const QStringList& ids) { for (const auto& id : ids) deleteFile(id); }
    /** Resubmits a file change that needs attention. */
    virtual void retryFile(const QString& id) { Q_UNUSED(id); }
    /** Drops a file change that was never submitted or was rejected; in-flight work is kept. */
    virtual void discardFile(const QString& id) { Q_UNUSED(id); }
    /** Real chunk tree inspection and cryptographic integrity check for own files. */
    virtual CybouFileChunkDiagnostics inspectFileChunks(const QString& file_id) const
    {
        Q_UNUSED(file_id);
        return {};
    }

Q_SIGNALS:
    void availabilityChanged();
    /** Initial private projection preparation; counts refer to locally verified history. */
    void applicationLoadChanged(CybouApplicationLoadState state, quint64 scanned, quint64 total, const QString& error);

    /* Mail projection. */
    void mailSnapshot(const QVector<CybouMailItem>& items);
    void mailItemChanged(const CybouMailItem& item);
    void mailItemRemoved(const QString& id);
    /** A temporary local id (e.g. an outgoing message) now has its permanent id. */
    void mailItemReplaced(const QString& old_id, const QString& new_id);
    void mailStateChanged(const QString& id, CybouContentState state);
    void attachmentStateChanged(const QString& message_id, const QString& attachment_id,
        CybouContentState state, int progress_percent);
    void attachmentRetrievalChanged(const QString& message_id, const QString& attachment_id,
        CybouRetrievalState retrieval);

    /* Files projection. */
    void filesSnapshot(const QVector<CybouFileItem>& items);
    void fileItemChanged(const CybouFileItem& item);
    void fileItemsRemoved(const QStringList& ids);
    void fileStateChanged(const QString& id, CybouContentState state, int progress_percent);
    void fileRetrievalChanged(const QString& id, CybouRetrievalState retrieval);

    /** Mail/Files rows of the restore progress (Identity rows belong to core). */
    void restoreProgressChanged(CybouRestoreStepState mail, CybouRestoreStepState files);

    /** A command could not be carried out; text is user-facing. */
    void commandFailed(const QString& text);
};

#endif // CYBOU_QT_CYBOUAPPLICATIONBACKEND_H
