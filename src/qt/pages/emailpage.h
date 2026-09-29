// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_EMAILPAGE_H
#define BITCOIN_QT_PAGES_EMAILPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>
#include <QWidget>
#include <functional>

class CybouDesktopModel;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QStackedWidget;
class QTextEdit;
class QToolButton;

/**
 * Full Email UI shell, backed by local mailbox indexes and the planned
 * encrypted Object Storage transport:
 *
 *  - Mail content is not a consensus operation. Delivery will use encrypted
 *    objects and recipient mailbox indexes once Object Storage is integrated.
 *  - The composer currently keeps a bounded text-only local draft; delivery
 *    stays disabled until encrypted object upload and mailbox sync are wired.
 *  - Local client owns Inbox / Sent / read-state: folders and unread
 *    counters live here and never touch consensus state.
 *
 * The store is in-memory for now: the page renders the actual capability
 * state and keeps drafts locally, but nothing is transmitted until core wires
 * encrypted Object Storage delivery. Send stays disabled and
 * says why — the UI never pretends to send.
 */
class EmailPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(EmailPage)

public:
    EmailPage(CybouDesktopModel* model, std::function<void()> identity_requested,
        QWidget* parent = nullptr);
    void loadScreenshotFixture();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    /** Temporary local draft ceiling until the encrypted object profile lands. */
    static constexpr qint64 kMaxDraftBytes = 64 * 1024;

    enum Folder {
        FOLDER_INBOX = 0,
        FOLDER_SENT,
        FOLDER_DRAFTS,
        FOLDER_COUNT,
    };

    enum class Finality {
        Draft,           /**< Local only, not a protocol object yet. */
        PendingFinality, /**< Waiting for encrypted Object Storage delivery. */
        Final,           /**< Encrypted object reached its required storage state. */
    };

    struct Message {
        QString id;
        Folder folder{FOLDER_INBOX};
        QString from;
        QString to;
        QString subject;
        QString body;
        QDateTime received;
        bool read{false};
        Finality finality{Finality::Draft};
        bool has_evidence{false};
    };

    CybouDesktopModel* m_model;
    std::function<void()> m_identity_requested;
    QVector<Message> m_messages;
    qint64 m_next_id{1};
    Folder m_folder{FOLDER_INBOX};
    int m_current_message{-1};

    void refreshMailboxView();

    QStackedWidget* m_right_stack{nullptr};
    QListWidget* m_folders{nullptr};
    QLineEdit* m_search{nullptr};
    QListWidget* m_list{nullptr};
    QWidget* m_reader{nullptr};
    QLabel* m_reader_hint{nullptr};
    QLabel* m_reader_subject{nullptr};
    QLabel* m_reader_avatar{nullptr};
    QLabel* m_reader_peer{nullptr};
    QLabel* m_reader_meta{nullptr};
    QLabel* m_reader_body{nullptr};
    QLabel* m_chip_encrypted{nullptr};
    QLabel* m_chip_verified{nullptr};
    QLabel* m_chip_protected{nullptr};
    QVector<QPushButton*> m_reply_buttons;
    QWidget* m_evidence{nullptr};
    QLabel* m_evidence_title{nullptr};
    QPushButton* m_security_details{nullptr};
    QVector<QLabel*> m_evidence_states;
    QPushButton* m_compose_button{nullptr};
    QFrame* m_banner{nullptr};
    QLabel* m_banner_text{nullptr};
    QPushButton* m_banner_action{nullptr};

    QLineEdit* m_to{nullptr};
    QLabel* m_to_hint{nullptr};
    QLineEdit* m_subject{nullptr};
    QTextEdit* m_body{nullptr};
    QProgressBar* m_size_meter{nullptr};
    QLabel* m_size_label{nullptr};
    QPushButton* m_send{nullptr};
    QLabel* m_send_hint{nullptr};

    void sendNow();

    void rebuildFolderList();
    void rebuildMessageList();
    void showMessage(const Message& message);
    void clearReader();
    void updateGates();
    void openComposer();
    void openComposerWith(const QString& to, const QString& subject, const QString& body_prefix);
    void closeComposer();
    void saveDraft();
    qint64 payloadBytes() const;
    bool recipientWellFormed() const;
    static QString finalityText(Finality finality);
    static QString peerName(const QString& account_hex);
};

#endif // BITCOIN_QT_PAGES_EMAILPAGE_H
