// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_MAILCOMPOSE_H
#define BITCOIN_QT_PAGES_MAILCOMPOSE_H

#include <qt/cybouproduct.h>

#include <QCoreApplication>
#include <QFrame>

#include <functional>

class CybouDesktopModel;
class QCompleter;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QToolButton;
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
    void setBackVisible(bool visible);
    bool hasContent() const;

private:
    CybouDesktopModel* const m_model;
    QString m_draft_id;
    QToolButton* m_back{nullptr};
    QLineEdit* m_to{nullptr};
    QLabel* m_to_hint{nullptr};
    QCompleter* m_completer{nullptr};
    QLineEdit* m_subject{nullptr};
    QTextEdit* m_body{nullptr};
    QPushButton* m_send{nullptr};
    QLabel* m_send_hint{nullptr};

    CybouMailItem currentMessage() const;
    const CybouContact* resolvedContact() const;
    QString recipientProblem() const;
    void rebuildCompleter();
    void updateGates();
    void send();
    void saveDraftAndClose();
    void discard();
};

#endif // BITCOIN_QT_PAGES_MAILCOMPOSE_H
