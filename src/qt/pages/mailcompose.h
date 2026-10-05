// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_QT_PAGES_MAILCOMPOSE_H
#define CYBOU_QT_PAGES_MAILCOMPOSE_H

#include <qt/cybouproduct.h>

#include <QCoreApplication>
#include <QFrame>

#include <functional>

class CybouDesktopModel;
class QCompleter;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
class QTimer;
class QVBoxLayout;

/**
 * New message / reply / forward. One recipient (.cybou name with
 * autocomplete) until the multi-recipient contract exists; no Cc/Bcc.
 * Send hands the message to the model; delivery states come back through
 * the finality-first content lifecycle.
 */
class MailCompose : public QFrame
{
    Q_DECLARE_TR_FUNCTIONS(MailCompose)

public:
    std::function<void()> onClosed;
    std::function<void(const QString& id)> onSent;

    explicit MailCompose(CybouDesktopModel* model, QWidget* parent = nullptr);

    /** Starts a fresh message, or continues/replies based on draft. */
    void start(const CybouMailItem& draft = {});
    /** Adds local files as attachments (Attach file / drag & drop). */
    void addAttachments(const QStringList& paths);
    /** Adds already protected content (Files â†’ Send by CYBOU Mail). */
    void addProtectedAttachment(const CybouAttachmentItem& attachment);
    /** Pick reusable encrypted content from this Identity's Files catalog. */
    void chooseCybouFiles();
    const QVector<CybouAttachmentItem>& attachments() const { return m_attachments; }
    CybouMailItem snapshotForRebuild() const;
    void setBackVisible(bool visible);
    bool hasContent() const;

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    CybouDesktopModel* const m_model;
    QString m_draft_id;
    QString m_reply_address;
    QToolButton* m_back{nullptr};
    QLineEdit* m_to{nullptr};
    QLabel* m_to_hint{nullptr};
    QCompleter* m_completer{nullptr};
    QLineEdit* m_subject{nullptr};
    QTextEdit* m_body{nullptr};
    QWidget* m_attachment_area{nullptr};
    QVBoxLayout* m_attachment_rows{nullptr};
    QVector<CybouAttachmentItem> m_attachments;
    QLabel* m_drop_hint{nullptr};
    QPushButton* m_send{nullptr};
    QLabel* m_send_hint{nullptr};
    QLabel* m_save_hint{nullptr};
    QTimer* m_autosave{nullptr};
    bool m_loading{false};
    bool m_saving{false};
    bool m_sending{false};
    bool m_following_send{false};
    bool m_close_requested{false};
    quint64 m_revision{0};
    quint64 m_saved_revision{0};
    quint64 m_compose_generation{0};
    void edited();
    void saveDraft(bool close);
    void clearCompose();

    CybouMailItem currentMessage() const;
    const CybouContact* resolvedContact() const;
    QString recipientProblem() const;
    void rebuildCompleter();
    void rebuildAttachments();
    void updateGates();
    void send();
    void saveDraftAndClose();
    void discard();
};

#endif // CYBOU_QT_PAGES_MAILCOMPOSE_H
