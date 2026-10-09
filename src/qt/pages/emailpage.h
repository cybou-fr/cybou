// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_EMAILPAGE_H
#define CYBOU_QT_PAGES_EMAILPAGE_H

#include <qt/cybouproduct.h>

#include <QCoreApplication>
#include <QHash>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QProgressBar;
class QStackedWidget;
class QVBoxLayout;
class MailCompose;
class MailReader;

/**
 * CYBOU Mail: a Gmail-familiar three-pane surface over the product model
 * (docs/cybou/82_MAIL_UI_UX.md). Folders, read state, stars and search are
 * local mailbox state. Layout: folders + list + reader at >= 1400 px window
 * width; below that the reader replaces the list with an explicit Back.
 */
class EmailPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(EmailPage)

public:
    enum class View {
        Inbox,
        Starred,
        Sent,
        Drafts,
        Archive,
        Trash,
    };

    EmailPage(CybouDesktopModel* model, std::function<void()> home_requested, QWidget* parent = nullptr);

    View view() const { return m_view; }
    void setView(View view);
    /** Messages currently listed (after folder + search filtering). */
    QStringList visibleMessageIds() const;
    void openMessage(const QString& id);
    void setSearchText(const QString& text);
    QString searchText() const;
    std::function<void()> onSearchRequested;
    QString currentMessageId() const { return m_current_id; }
    bool isDetailOpen() const { return m_detail_open; }
    /** Moves messages the way a drop on a folder does (Undo offered). */
    void moveMessagesTo(const QStringList& ids, View target);
    /** Trash only: removes the messages from this mailbox for good, after confirmation. */
    void deleteForever(const QStringList& ids);
    /** Builds the context menu for the given messages (exposed for tests). */
    QMenu* buildContextMenu(const QStringList& ids, QWidget* parent);
    /** Opens the composer; draft may prefill it (reply, forward, draft). */
    void openCompose(const CybouMailItem& draft = {});
    MailReader* reader() const { return m_reader; }
    MailCompose* composer() const { return m_compose; }
    bool isComposing() const;
    bool threePane() const { return m_three_pane; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    CybouDesktopModel* const m_model;
    const std::function<void()> m_home_requested;
    View m_view{View::Inbox};
    QString m_current_id;
    bool m_three_pane{true};
    bool m_detail_open{false};

    QFrame* m_rail{nullptr};
    QPushButton* m_compose_button{nullptr};
    QListWidget* m_folders{nullptr};
    /** Rail list of recent contacts; a click starts a message to them. */
    QVBoxLayout* m_contact_rows{nullptr};
    void rebuildContacts();
    QWidget* m_list_pane{nullptr};
    QLineEdit* m_search{nullptr};
    QPushButton* m_empty_trash{nullptr};
    QFrame* m_banner{nullptr};
    QLabel* m_banner_text{nullptr};
    QPushButton* m_banner_action{nullptr};
    QWidget* m_move_status{nullptr};
    QLabel* m_move_status_text{nullptr};
    QProgressBar* m_move_progress{nullptr};
    QPushButton* m_move_retry{nullptr};
    QHash<QString, View> m_failed_moves;
    QListWidget* m_list{nullptr};
    QHash<QString, QListWidgetItem*> m_rows;
    QHash<QString, CybouMailItem> m_rendered_mail;
    QHash<QString, QStringList> m_rendered_meta;
    QLabel* m_list_empty{nullptr};
    QStackedWidget* m_detail{nullptr};
    QWidget* m_detail_empty{nullptr};
    QLabel* m_empty_hint{nullptr};
    MailReader* m_reader{nullptr};
    MailCompose* m_compose{nullptr};
    QPoint m_press_pos;
    QString m_press_id;

    QStringList selectedMessageIds() const;
    int folderRowAt(const QPoint& viewport_pos) const;

    CybouMailItem replyTo(const QString& id) const;
    CybouMailItem forwardOf(const QString& id) const;

    bool matches(const CybouMailItem& item, const QString& needle) const;
    bool inView(const CybouMailItem& item) const;
    void rebuildFolders();
    void rebuildList();
    void refreshBanner();
    void refreshMoveStatus();
    void refreshEmptyHint();
    void updateLayoutMode();
    void closeDetail();
};

#endif // CYBOU_QT_PAGES_EMAILPAGE_H
