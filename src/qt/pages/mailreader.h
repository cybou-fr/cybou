// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_MAILREADER_H
#define BITCOIN_QT_PAGES_MAILREADER_H

#include <qt/cybouproduct.h>

#include <QCoreApplication>
#include <QFrame>

#include <functional>

class CybouDesktopModel;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

/**
 * Message reader: sender header, compact security line, body, attachments
 * and local actions. Evidence (OperationID, finalized height) is only in
 * Security Details, never in the main reader.
 */
class MailReader : public QFrame
{
    Q_DECLARE_TR_FUNCTIONS(MailReader)

public:
    std::function<void()> onBack;
    std::function<void(const QString& id)> onReply;
    std::function<void(const QString& id)> onForward;
    std::function<void(const QString& message_id, const QString& attachment_id)> onSaveAttachment;
    std::function<void(const QString& message_id, const QString& attachment_id)> onDownloadAttachment;

    explicit MailReader(CybouDesktopModel* model, QWidget* parent = nullptr);

    void showMessage(const QString& id);
    QString messageId() const { return m_id; }
    void setBackVisible(bool visible);
    /** Opens the Security Details dialog for the current message. */
    void showSecurityDetails();

private:
    CybouDesktopModel* const m_model;
    QString m_id;
    QToolButton* m_back{nullptr};
    QToolButton* m_star{nullptr};
    QToolButton* m_archive{nullptr};
    QLabel* m_subject{nullptr};
    QLabel* m_avatar{nullptr};
    QLabel* m_sender{nullptr};
    QLabel* m_sender_name{nullptr};
    QLabel* m_recipient{nullptr};
    QLabel* m_time{nullptr};
    QLabel* m_security{nullptr};
    QLabel* m_body{nullptr};
    QWidget* m_attachments{nullptr};
    QVBoxLayout* m_attachment_rows{nullptr};
    QPushButton* m_reply{nullptr};
    QPushButton* m_forward{nullptr};

    void refresh();
};

#endif // BITCOIN_QT_PAGES_MAILREADER_H
