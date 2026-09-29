// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUFIXTUREBACKEND_H
#define BITCOIN_QT_CYBOUFIXTUREBACKEND_H

#include <qt/cybouapplicationbackend.h>

#include <functional>

/**
 * Deterministic Mail/Files backend for UI fixtures (CYBOU_UI_FIXTURE).
 *
 * It keeps a seeded in-memory catalog and mailbox and answers commands
 * immediately, the way a local Application DB would. Content lifecycle
 * (Preparing -> Waiting for confirmation -> Securing -> Protected) and
 * retrieval (Downloading -> Verifying -> Decrypting -> Ready) advance only
 * when auto-advance is enabled, so tests can drive states explicitly.
 * It never touches core, the network or real content.
 */
class CybouFixtureApplicationBackend final : public CybouApplicationBackend
{
    Q_OBJECT

public:
    explicit CybouFixtureApplicationBackend(QObject* parent = nullptr);

    /** Replaces the fixture store; reported at once when the Identity is open. */
    void seed(QVector<CybouMailItem> mail, QVector<CybouFileItem> files);
    /** Simulates a progressive rebuild of the seeded data after a restore. */
    void seedProgressively(QVector<CybouMailItem> mail, QVector<CybouFileItem> files);
    /** Answers "is the network reachable" for lifecycle simulation. */
    void setOnlineProvider(std::function<bool()> online) { m_online = std::move(online); }
    /** Enables timed lifecycle transitions; step_ms 0 advances on the next event loop pass. */
    void setAutoAdvance(bool enabled, int step_ms = 900);

    bool mailAvailable() const override { return true; }
    bool filesAvailable() const override { return true; }

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

    void prepareIdentityRotation(const QStringList&, std::function<void(bool, const QString&)> done) override
    {
        done(true, {});
    }

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
    QVector<CybouMailItem> m_mail;
    QVector<CybouFileItem> m_files;
    bool m_open{false};
    bool m_auto_advance{false};
    int m_step_ms{900};
    std::function<bool()> m_online;

    CybouMailItem* mail(const QString& id);
    CybouFileItem* file(const QString& id);
    QStringList withDescendants(const QString& id) const;
    void changed(const CybouMailItem& item);
    void changed(const CybouFileItem& item);
    void later(int steps, std::function<void()> action);
    void runSend(const QString& id);
    void runUpload(const QString& id);
};

#endif // BITCOIN_QT_CYBOUFIXTUREBACKEND_H
