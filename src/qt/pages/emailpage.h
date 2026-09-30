// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_EMAILPAGE_H
#define BITCOIN_QT_PAGES_EMAILPAGE_H

#include <qt/cybouproduct.h>

#include <QCoreApplication>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QFrame;
class QLabel;
class QLineEdit;
class QMenu;
class QListWidget;
class QPushButton;
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
    QListWidget* m_list{nullptr};
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
    void refreshEmptyHint();
    void updateLayoutMode();
    void closeDetail();
};

#endif // BITCOIN_QT_PAGES_EMAILPAGE_H
