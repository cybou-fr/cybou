// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/emailpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>
#include <qt/pages/mailcompose.h>
#include <qt/pages/mailreader.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFrame>
#include <QMenu>
#include <QPointer>
#include <memory>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QScrollBar>
#include <QSet>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace CybouUi;

namespace {

/** Window width at which Mail shows folders, list and reader together. */
constexpr int kThreePaneWindowWidth = 1400;
constexpr int kRankRole = Qt::UserRole + 1;
constexpr int kOperationRole = Qt::UserRole + 2;
constexpr int kOnlineRole = Qt::UserRole + 3;
class MessageItem final : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;
    bool operator<(const QListWidgetItem& other) const override { return data(kRankRole).toInt() < other.data(kRankRole).toInt(); }
};

QRgb PeerColor(const QString& peer)
{
    static const QRgb palette[] = {
        CybouTheme::BRAND_TEAL, CybouTheme::BLUE, CybouTheme::INDIGO,
        CybouTheme::VIOLET, CybouTheme::AMBER, CybouTheme::ROSE,
    };
    uint hash = 0;
    for (const QChar ch : peer) hash = (hash * 31) ^ ch.unicode();
    return palette[hash % std::size(palette)];
}

QString ViewName(EmailPage::View view)
{
    switch (view) {
    case EmailPage::View::Inbox: return EmailPage::tr("Inbox");
    case EmailPage::View::Starred: return EmailPage::tr("Starred");
    case EmailPage::View::Sent: return EmailPage::tr("Sent");
    case EmailPage::View::Drafts: return EmailPage::tr("Drafts");
    case EmailPage::View::Archive: return EmailPage::tr("Archive");
    case EmailPage::View::Trash: return EmailPage::tr("Trash");
    }
    return {};
}

Glyph ViewGlyph(EmailPage::View view)
{
    switch (view) {
    case EmailPage::View::Inbox: return Glyph::Inbox;
    case EmailPage::View::Starred: return Glyph::Star;
    case EmailPage::View::Sent: return Glyph::Send;
    case EmailPage::View::Drafts: return Glyph::FileText;
    case EmailPage::View::Archive: return Glyph::Archive;
    case EmailPage::View::Trash: return Glyph::Trash;
    }
    return Glyph::Inbox;
}

/** Peer shown in a list row: sender for received mail, recipient otherwise. */
QString RowPeer(const CybouMailItem& item)
{
    return item.outgoing || item.folder == CybouMailFolder::Sent || item.folder == CybouMailFolder::Drafts
        ? EmailPage::tr("To: %1").arg(item.to_name) : item.from_name;
}

/** Paint only visible semantic rows. No per-message widgets or plaintext body layout. */
class MailDelegate final : public QStyledItemDelegate
{
public:
    using Lookup = std::function<const CybouMailItem*(const QString&)>;
    MailDelegate(Lookup lookup, QObject* parent) : QStyledItemDelegate{parent}, m_lookup{std::move(lookup)} {}

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        return {0, std::max(62, 2 * QFontMetrics{option.font}.height() + 20)};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& supplied, const QModelIndex& index) const override
    {
        const auto* item = m_lookup(index.data(Qt::UserRole).toString());
        if (!item) return;
        QStyleOptionViewItem option{supplied};
        initStyleOption(&option, index);
        option.text.clear();
        const auto* style = option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, option.widget);
        painter->save();
        painter->setClipRect(option.rect);
        painter->setRenderHint(QPainter::Antialiasing);
        const int left = option.rect.left(), center = option.rect.center().y();
        if (item->unread) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(CybouTheme::color(CybouTheme::MINT));
            painter->drawEllipse(QPoint{left + 10, center}, 4, 4);
        }
        const QString peer = item->outgoing || item->folder == CybouMailFolder::Sent || item->draft ? item->to_name : item->from_name;
        painter->drawPixmap(left + 24, center - 16, avatarPixmap(peer.left(1), PeerColor(peer), 32));
        const int text_left = left + 66;
        int right = option.rect.right() - 12;
        const int width = std::max(0, right - text_left);
        QFont normal{option.widget ? option.widget->font() : option.font};
        const int line_height = QFontMetrics{normal}.height() + 2;
        const int top = center - line_height - 1;
        const auto operation = static_cast<CybouOperationState>(index.data(kOperationRole).toInt());
        const bool pending = !item->draft && (CybouProduct::itemPending(item->state, operation) || item->state == CybouContentState::NeedsAttention);
        const bool online = index.data(kOnlineRole).toBool();
        QFont meta{normal};
        meta.setPixelSize(12);
        if (item->unread) meta.setWeight(QFont::Bold);
        painter->setFont(meta);
        const QString meta_text = pending
            ? (item->state == CybouContentState::Securing ? CybouProduct::contentStateText(item->state)
                : CybouProduct::contentWithOperationText(item->state, operation, online)) : shortTime(item->time);
        const QFontMetrics meta_metrics{meta};
        const int meta_width = std::min(width / 2, meta_metrics.horizontalAdvance(meta_text) + (pending ? 13 : 0) + 6);
        painter->setPen(CybouTheme::color(item->state == CybouContentState::NeedsAttention ? CybouTheme::ROSE
            : item->unread ? CybouTheme::TEXT_PRIMARY : CybouTheme::TEXT_MUTED));
        painter->drawText(QRect{right - meta_width + (pending ? 13 : 0), top, std::max(0, meta_width - (pending ? 13 : 0)), line_height},
            Qt::AlignRight | Qt::AlignVCenter, meta_metrics.elidedText(meta_text, Qt::ElideRight, std::max(0, meta_width - (pending ? 13 : 0))));
        if (pending) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(CybouTheme::color(stateColor(item->state, operation)));
            painter->drawEllipse(QPoint{right - meta_width + 4, top + line_height / 2}, 3, 3);
        }
        right -= meta_width + 8;
        const auto icon = [&](Glyph glyph, QRgb color, int size) {
            painter->drawPixmap(right - size, top + (line_height - size) / 2, glyphPixmap(glyph, {size, size}, CybouTheme::color(color)));
            right -= size + 8;
        };
        if (!pending && !item->draft && (item->state == CybouContentState::Protected || item->state == CybouContentState::Received))
            icon(Glyph::Lock, CybouTheme::BRAND_TEAL_DARK, 12);
        if (!pending && item->below_support_rate) {
            const QString below = EmailPage::tr("Below support rate");
            const int below_width = std::min(std::max(0, (right - text_left) / 2), meta_metrics.horizontalAdvance(below) + 6);
            painter->setFont(meta);
            painter->setPen(CybouTheme::color(CybouTheme::ROSE));
            painter->drawText(QRect{right - below_width, top, below_width, line_height}, Qt::AlignRight | Qt::AlignVCenter,
                meta_metrics.elidedText(below, Qt::ElideRight, below_width));
            right -= below_width + 8;
        }
        if (item->starred) icon(Glyph::Star, CybouTheme::AMBER, 14);
        if (!item->attachments.isEmpty()) icon(Glyph::File, CybouTheme::TEXT_MUTED, 14);
        QFont name{normal};
        if (item->unread) name.setWeight(QFont::Bold);
        painter->setFont(name);
        painter->setPen(CybouTheme::color(item->unread ? CybouTheme::TEXT_PRIMARY : CybouTheme::TEXT_SECONDARY));
        const int name_width = std::max(0, right - text_left);
        painter->drawText(QRect{text_left, top, name_width, line_height}, Qt::AlignLeft | Qt::AlignVCenter,
            QFontMetrics{name}.elidedText(RowPeer(*item), Qt::ElideRight, name_width));
        QFont subject_font{normal};
        subject_font.setWeight(item->unread ? QFont::Bold : QFont::DemiBold);
        const QFontMetrics subject_metrics{subject_font};
        const QString subject = subject_metrics.elidedText(item->subject.isEmpty() ? EmailPage::tr("(no subject)") : item->subject, Qt::ElideRight, width);
        painter->setFont(subject_font);
        painter->setPen(CybouTheme::color(CybouTheme::TEXT_PRIMARY));
        painter->drawText(QRect{text_left, center + 2, width, line_height}, Qt::AlignLeft | Qt::AlignVCenter, subject);
        const int used = subject_metrics.horizontalAdvance(subject);
        if (!item->preview.isEmpty() && used < width - 24) {
            painter->setFont(normal);
            painter->setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
            painter->drawText(QRect{text_left + used, center + 2, width - used, line_height}, Qt::AlignLeft | Qt::AlignVCenter,
                QFontMetrics{normal}.elidedText(QStringLiteral(" — ") + item->preview, Qt::ElideRight, width - used));
        }
        painter->restore();
    }
private:
    const Lookup m_lookup;
};

} // namespace

EmailPage::EmailPage(CybouDesktopModel* model, std::function<void()> home_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_home_requested{std::move(home_requested)}
{
    setObjectName(QStringLiteral("mailPage"));
    setMinimumWidth(0);
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    // ---- Folder rail -------------------------------------------------------
    m_rail = new QFrame{this};
    m_rail->setObjectName(QStringLiteral("mailRail"));
    m_rail->setFixedWidth(196);
    auto* rail = new QVBoxLayout{m_rail};
    rail->setContentsMargins(0, 0, 0, 0);
    rail->setSpacing(10);
    m_compose_button = new QPushButton{tr("Compose"), m_rail};
    m_compose_button->setObjectName(QStringLiteral("primaryButton"));
    m_compose_button->setProperty("cybouId", QStringLiteral("compose"));
    m_compose_button->setIcon(QIcon{glyphPixmap(Glyph::Compose, {18, 18}, QColor{Qt::white})});
    m_compose_button->setMinimumHeight(44);
    m_compose_button->setCursor(Qt::PointingHandCursor);
    rail->addWidget(m_compose_button);
    m_folders = new QListWidget{m_rail};
    m_folders->setObjectName(QStringLiteral("folderList"));
    m_folders->setAccessibleName(tr("Mail folders"));
    m_folders->setFrameShape(QFrame::NoFrame);
    m_folders->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_folders->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_folders->setFixedHeight(6 * 40 + 4);
    rail->addWidget(m_folders);
    auto* contacts_title = new QLabel{tr("Contacts"), m_rail};
    contacts_title->setObjectName(QStringLiteral("eyebrow"));
    rail->addSpacing(6);
    rail->addWidget(contacts_title);
    auto* contacts_host = new QWidget{m_rail};
    contacts_host->setObjectName(QStringLiteral("mailContacts"));
    m_contact_rows = new QVBoxLayout{contacts_host};
    m_contact_rows->setContentsMargins(0, 0, 0, 0);
    m_contact_rows->setSpacing(2);
    rail->addWidget(contacts_host);
    rail->addStretch();
    root->addWidget(m_rail);

    // ---- Message list --------------------------------------------------------
    m_list_pane = new QFrame{this};
    m_list_pane->setObjectName(QStringLiteral("card"));
    m_list_pane->setMinimumWidth(0);
    auto* list_layout = new QVBoxLayout{m_list_pane};
    list_layout->setContentsMargins(10, 10, 10, 6);
    list_layout->setSpacing(8);
    m_search = new QLineEdit{m_list_pane};
    m_search->setObjectName(QStringLiteral("mailSearch"));
    m_search->setPlaceholderText(tr("Search mail"));
    m_search->setAccessibleName(tr("Search mail"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(QIcon{glyphPixmap(Glyph::Search, {16, 16}, CybouTheme::color(CybouTheme::TEXT_MUTED))},
        QLineEdit::LeadingPosition);
    m_search->setMinimumHeight(38);
    list_layout->addWidget(m_search);
    m_empty_trash = new QPushButton{tr("Empty Trash"), m_list_pane};
    m_empty_trash->setObjectName(QStringLiteral("secondaryButton"));
    m_empty_trash->setProperty("cybouId", QStringLiteral("mailEmptyTrash"));
    m_empty_trash->setIcon(QIcon{glyphPixmap(Glyph::Trash, {16, 16}, CybouTheme::color(CybouTheme::ROSE))});
    m_empty_trash->hide();
    connect(m_empty_trash, &QPushButton::clicked, this, [this] {
        QStringList ids;
        for (const auto& item : m_model->mailItems()) {
            if (item.folder == CybouMailFolder::Trash && !item.draft) ids << item.id;
        }
        deleteForever(ids);
    });
    list_layout->addWidget(m_empty_trash, 0, Qt::AlignRight);

    m_banner = new QFrame{m_list_pane};
    m_banner->setObjectName(QStringLiteral("identityBanner"));
    auto* banner_layout = new QHBoxLayout{m_banner};
    banner_layout->setContentsMargins(12, 8, 12, 8);
    m_banner_text = MutedText({}, m_banner);
    banner_layout->addWidget(m_banner_text, 1);
    m_banner_action = new QPushButton{tr("Set up Identity"), m_banner};
    m_banner_action->setObjectName(QStringLiteral("secondaryButton"));
    banner_layout->addWidget(m_banner_action);
    connect(m_banner_action, &QPushButton::clicked, this, [this] { if (m_home_requested) m_home_requested(); });
    list_layout->addWidget(m_banner);

    m_list = new QListWidget{m_list_pane};
    m_list->setObjectName(QStringLiteral("messageList"));
    m_list->setItemDelegate(new MailDelegate{[this](const QString& id) -> const CybouMailItem* {
        const auto it = m_rendered_mail.constFind(id);
        return it == m_rendered_mail.cend() ? nullptr : &it.value();
    }, m_list});
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setAccessibleName(tr("Messages"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->viewport()->installEventFilter(this);
    m_list->installEventFilter(this);
    list_layout->addWidget(m_list, 1);
    m_list_empty = MutedText({}, m_list_pane);
    m_list_empty->setAlignment(Qt::AlignCenter);
    list_layout->addWidget(m_list_empty);
    root->addWidget(m_list_pane, 5);

    // ---- Reader / composer ---------------------------------------------------
    m_detail = new QStackedWidget{this};
    m_detail->setMinimumWidth(0);
    m_detail_empty = new QFrame{m_detail};
    m_detail_empty->setObjectName(QStringLiteral("card"));
    auto* empty_layout = new QVBoxLayout{m_detail_empty};
    empty_layout->addStretch();
    auto* empty_icon = new QLabel{m_detail_empty};
    empty_icon->setPixmap(glyphPixmap(Glyph::Envelope, {56, 56}, CybouTheme::color(CybouTheme::BORDER_MEDIUM)));
    empty_icon->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_icon);
    auto* empty_title = SectionTitle(tr("No message selected"), m_detail_empty);
    empty_title->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_title);
    m_empty_hint = MutedText({}, m_detail_empty);
    m_empty_hint->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(m_empty_hint);
    auto* empty_compose = new QPushButton{tr("Compose"), m_detail_empty};
    empty_compose->setObjectName(QStringLiteral("secondaryButton"));
    connect(empty_compose, &QPushButton::clicked, this, [this] { openCompose(); });
    empty_layout->addWidget(empty_compose, 0, Qt::AlignHCenter);
    empty_layout->addStretch();
    m_detail->addWidget(m_detail_empty);
    m_reader = new MailReader{m_model, m_detail};
    m_detail->addWidget(m_reader);
    m_compose = new MailCompose{m_model, m_detail};
    m_detail->addWidget(m_compose);
    root->addWidget(m_detail, 7);

    connect(m_compose_button, &QPushButton::clicked, this, [this] { openCompose(); });
    m_reader->onBack = [this] { closeDetail(); };
    m_reader->onReply = [this](const QString& id) { openCompose(replyTo(id)); };
    m_reader->onForward = [this](const QString& id) { openCompose(forwardOf(id)); };
    m_compose->onClosed = [this] { closeDetail(); };
    m_compose->onSent = [this](const QString& id) {
        setView(View::Sent);
        openMessage(id);
        m_model->notify(tr("Sending… It shows as Sent once it is stored securely."));
    };

    connect(m_folders, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) setView(static_cast<View>(row));
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildList(); });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = m_list->itemAt(pos);
        if (!item) return;
        if (!item->isSelected()) {
            m_list->clearSelection();
            item->setSelected(true);
        }
        auto* menu = buildContextMenu(selectedMessageIds(), this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(m_list->viewport()->mapToGlobal(pos));
    });
    m_folders->viewport()->setAcceptDrops(true);
    m_folders->viewport()->installEventFilter(this);
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        openMessage(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        openMessage(item->data(Qt::UserRole).toString());
    });
    connect(m_model, &CybouDesktopModel::mailChanged, this, [this] {
        rebuildFolders();
        rebuildList();
        refreshEmptyHint();
    });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refreshBanner(); });
    connect(m_model, &CybouDesktopModel::featureAvailabilityChanged, this, [this] { refreshBanner(); });
    connect(m_model, &CybouDesktopModel::applicationLoadChanged, this,
        [this, previous = m_model->applicationLoadState()]() mutable {
            const auto state = m_model->applicationLoadState();
            if (state != previous) { previous = state; refreshBanner(); }
        });
    connect(m_model, &CybouDesktopModel::contactsChanged, this, [this] { rebuildContacts(); });
    connect(m_model, &CybouDesktopModel::mailIdReplaced, this, [this](const QString& old_id, const QString& new_id) {
        if (m_current_id == old_id) m_current_id = new_id;
        if (m_press_id == old_id) m_press_id = new_id;
        if (auto* row = m_rows.take(old_id)) {
            if (auto* duplicate = m_rows.take(new_id)) delete duplicate;
            row->setData(Qt::UserRole, new_id);
            m_rows.insert(new_id, row);
        }
        m_rendered_mail.remove(old_id);
        m_rendered_meta.remove(old_id);
    });

    // Familiar mail shortcuts; each has a visible button equivalent.
    const auto shortcut = [this](const QKeySequence& keys, auto&& action) {
        auto* sc = new QShortcut{keys, this};
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, std::forward<decltype(action)>(action));
    };
    const auto typing = [this] {
        const QWidget* focus = QApplication::focusWidget();
        return focus && (focus->inherits("QLineEdit") || focus->inherits("QTextEdit"));
    };
    shortcut(QKeySequence{QStringLiteral("Ctrl+N")}, [this] { openCompose(); });
    shortcut(QKeySequence{Qt::Key_C}, [this, typing] { if (!typing()) openCompose(); });
    shortcut(QKeySequence{Qt::Key_Slash}, [this, typing] {
        if (!typing()) { if (onSearchRequested) onSearchRequested(); else m_search->setFocus(); }
    });
    shortcut(QKeySequence{Qt::Key_R}, [this, typing] {
        if (!typing() && m_detail->currentWidget() == m_reader) openCompose(replyTo(m_reader->messageId()));
    });
    shortcut(QKeySequence{Qt::Key_F}, [this, typing] {
        if (!typing() && m_detail->currentWidget() == m_reader) openCompose(forwardOf(m_reader->messageId()));
    });
    shortcut(QKeySequence::Delete, [this, typing] {
        if (typing() || m_detail->currentWidget() != m_reader) return;
        moveMessagesTo({m_reader->messageId()}, View::Trash);
    });
    shortcut(QKeySequence{Qt::Key_Escape}, [this] {
        if (m_detail->currentWidget() == m_reader) closeDetail();
    });

    rebuildFolders();
    rebuildContacts();
    m_folders->setCurrentRow(0);
    rebuildList();
    refreshBanner();
    refreshEmptyHint();
}

void EmailPage::refreshEmptyHint()
{
    const int unread = m_model->unreadMailCount();
    m_empty_hint->setText(unread == 0 ? tr("You're all caught up.")
        : unread == 1 ? tr("1 unread message in your Inbox.") : tr("%1 unread messages in your Inbox.").arg(unread));
}

QStringList EmailPage::selectedMessageIds() const
{
    QStringList ids;
    for (const auto* item : m_list->selectedItems()) ids << item->data(Qt::UserRole).toString();
    return ids;
}

int EmailPage::folderRowAt(const QPoint& viewport_pos) const
{
    const auto* item = m_folders->itemAt(viewport_pos);
    return item ? m_folders->row(item) : -1;
}

void EmailPage::moveMessagesTo(const QStringList& ids, View target)
{
    struct Before { QString id; CybouMailFolder folder; };
    struct Batch { QVector<Before> succeeded; int remaining{0}; int failed{0}; };
    QVector<Before> before;
    for (const auto& id : ids) {
        const auto* item = m_model->mailItem(id);
        if (!item || item->draft || (target == View::Inbox && (item->outgoing || item->folder == CybouMailFolder::Sent))) continue;
        before.append({id, item->folder});
    }
    if (before.isEmpty() || target == View::Sent || target == View::Drafts) return;
    if (target == View::Starred) {
        for (const auto& entry : before) m_model->requestMailStarred(entry.id, true);
        return;
    }
    const auto batch = std::make_shared<Batch>();
    batch->remaining = before.size();
    const auto folder = target == View::Archive ? CybouMailFolder::Archive
        : target == View::Trash ? CybouMailFolder::Trash : CybouMailFolder::Inbox;
    m_model->notify(target == View::Archive ? tr("Archiving…") : tr("Moving messages…"));
    const QPointer<EmailPage> guard{this};
    for (const auto& entry : before) {
        m_model->requestMoveMail(entry.id, folder, [guard, batch, entry, target](bool ok, const QString&) {
            if (!guard) return;
            if (ok) {
                batch->succeeded.append(entry);
                if (entry.id == guard->m_current_id) guard->closeDetail();
            } else ++batch->failed;
            if (--batch->remaining != 0) return;
            const int count = batch->succeeded.size();
            QString text = target == View::Archive ? tr("%1 conversations archived").arg(count)
                : target == View::Trash ? tr("%1 conversations moved to Trash").arg(count)
                : tr("%1 conversations moved to Inbox").arg(count);
            if (batch->failed) text += tr(" · %1 could not be moved. Try again.").arg(batch->failed);
            guard->m_model->notify(text, count ? tr("Undo") : QString{}, count ? std::function<void()>{[model = guard->m_model, batch] {
                for (const auto& previous : batch->succeeded) model->requestMoveMail(previous.id, previous.folder);
            }} : std::function<void()>{});
        });
    }
}

void EmailPage::deleteForever(const QStringList& ids)
{
    if (ids.isEmpty()) return;
    QString question = ids.size() == 1
        ? tr("Delete this message forever? It is removed from this mailbox and cannot be restored here.")
        : tr("Delete %1 messages forever? They are removed from this mailbox and cannot be restored here.").arg(ids.size());
    // Eligible own publications may be revoked; recipient copies are outside managed purge.
    const bool sent = std::any_of(ids.begin(), ids.end(), [this](const QString& id) {
        const auto* item = m_model->mailItem(id);
        return item && item->outgoing;
    });
    if (sent) {
        question += QStringLiteral("\n\n") + tr("Eligible sent publications can be revoked after finalization, freeing publication quota and initiating managed purge. Recipients and other holders may retain copies.");
    }
    if (QMessageBox::question(this, tr("Delete forever"), question) != QMessageBox::Yes) return;
    m_model->notify(tr("Deleting messages…"));
    struct Batch { int remaining; int committed{0}; int failed{0}; };
    auto batch = std::make_shared<Batch>(Batch{static_cast<int>(ids.size())});
    const QPointer<EmailPage> guard{this};
    for (const auto& id : ids) m_model->requestDeleteMailForever({id}, [guard, batch, id](bool ok, const QString&) {
        if (!guard) return;
        if (ok) {
            ++batch->committed;
            if (guard->m_current_id == id) guard->closeDetail();
        } else ++batch->failed;
        if (--batch->remaining != 0) return;
        if (batch->failed) guard->m_model->notify(tr("%1 messages deleted; %2 could not be deleted. Their content is kept. Try again.")
            .arg(batch->committed).arg(batch->failed));
        else guard->m_model->notify(batch->committed == 1 ? tr("Message deleted") : tr("%1 messages deleted").arg(batch->committed));
    });
}

QMenu* EmailPage::buildContextMenu(const QStringList& ids, QWidget* parent)
{
    auto* menu = new QMenu{parent};
    menu->setObjectName(QStringLiteral("mailContextMenu"));
    if (ids.isEmpty()) return menu;
    const auto* first = m_model->mailItem(ids.first());
    if (!first) return menu;
    const bool single = ids.size() == 1;
    const auto add = [menu](const QString& text, const char* id, auto&& fn) {
        auto* action = menu->addAction(text);
        action->setObjectName(QLatin1String{id});
        QObject::connect(action, &QAction::triggered, menu, std::forward<decltype(fn)>(fn));
        return action;
    };
    if (single) {
        add(first->draft ? tr("Edit draft") : tr("Open"), "mailOpen", [this, id = first->id] { openMessage(id); });
        if (!first->draft) {
            add(tr("Reply"), "mailReply", [this, id = first->id] { openCompose(replyTo(id)); });
            add(tr("Forward"), "mailForward", [this, id = first->id] { openCompose(forwardOf(id)); });
        }
        menu->addSeparator();
    }
    const bool any_unread = std::any_of(ids.begin(), ids.end(), [this](const QString& id) {
        const auto* item = m_model->mailItem(id);
        return item && item->unread;
    });
    add(any_unread ? tr("Mark as read") : tr("Mark as unread"), "mailMarkRead", [this, ids, any_unread] {
        for (const auto& id : ids) m_model->requestMailRead(id, any_unread);
    });
    const bool all_starred = std::all_of(ids.begin(), ids.end(), [this](const QString& id) {
        const auto* item = m_model->mailItem(id);
        return item && item->starred;
    });
    add(all_starred ? tr("Remove star") : tr("Star"), "mailStar", [this, ids, all_starred] {
        for (const auto& id : ids) m_model->requestMailStarred(id, !all_starred);
    });
    if (!first->draft) {
        menu->addSeparator();
        if (first->folder == CybouMailFolder::Archive || first->folder == CybouMailFolder::Trash) {
            add(tr("Move to Inbox"), "mailToInbox", [this, ids] { moveMessagesTo(ids, View::Inbox); });
        }
        if (first->folder != CybouMailFolder::Archive) {
            add(tr("Archive"), "mailArchive", [this, ids] { moveMessagesTo(ids, View::Archive); });
        }
        if (first->folder != CybouMailFolder::Trash) {
            add(tr("Move to Trash"), "mailTrash", [this, ids] { moveMessagesTo(ids, View::Trash); });
        } else {
            add(tr("Delete forever"), "mailDeleteForever", [this, ids] { deleteForever(ids); });
        }
    } else {
        menu->addSeparator();
        add(tr("Discard draft"), "mailDiscard", [this, ids] {
            for (const auto& id : ids) m_model->requestDeleteMail(id);
        });
    }
    return menu;
}

bool EmailPage::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_list || watched == m_list->viewport()) && event->type() == QEvent::ContextMenu &&
        static_cast<QContextMenuEvent*>(event)->reason() == QContextMenuEvent::Keyboard) {
        if (auto* current = m_list->currentItem()) {
            m_list->scrollToItem(current);
            Q_EMIT m_list->customContextMenuRequested(m_list->visualItemRect(current).center());
        }
        event->accept();
        return true;
    }
    if (watched == m_list->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            const auto* item = m_list->itemAt(mouse->position().toPoint());
            m_press_pos = mouse->position().toPoint();
            m_press_id = mouse->button() == Qt::LeftButton && item ? item->data(Qt::UserRole).toString() : QString{};
        } else if (event->type() == QEvent::MouseMove && !m_press_id.isEmpty()) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if ((mouse->buttons() & Qt::LeftButton) &&
                (mouse->position().toPoint() - m_press_pos).manhattanLength() >= QApplication::startDragDistance()) {
                QStringList ids = selectedMessageIds();
                if (!ids.contains(m_press_id)) ids = QStringList{m_press_id};
                const QString id = std::exchange(m_press_id, {});
                const auto* item = m_model->mailItem(id);
                startIdDrag(m_list, mailIdsMime(), ids, ids.size() == 1 && item
                    ? (item->subject.isEmpty() ? tr("(no subject)") : item->subject).left(40)
                    : tr("%1 conversations").arg(ids.size()));
                return true;
            }
        }
        return false;
    }
    if (watched == m_folders->viewport()) {
        const auto droppable = [this](const QPoint& pos) {
            const int row = folderRowAt(pos);
            return row == static_cast<int>(View::Inbox) || row == static_cast<int>(View::Starred) ||
                row == static_cast<int>(View::Archive) || row == static_cast<int>(View::Trash);
        };
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* drag = static_cast<QDragMoveEvent*>(event);
            const bool ok = drag->mimeData()->hasFormat(mailIdsMime()) && droppable(drag->position().toPoint());
            if (ok) {
                drag->acceptProposedAction();
                const QSignalBlocker blocker{m_folders};
                m_folders->setCurrentRow(folderRowAt(drag->position().toPoint()));
            } else {
                drag->ignore();
            }
            return true;
        }
        if (event->type() == QEvent::DragLeave) {
            const QSignalBlocker blocker{m_folders};
            m_folders->setCurrentRow(static_cast<int>(m_view));
            return true;
        }
        if (event->type() == QEvent::Drop) {
            auto* drop = static_cast<QDropEvent*>(event);
            const int row = folderRowAt(drop->position().toPoint());
            {
                const QSignalBlocker blocker{m_folders};
                m_folders->setCurrentRow(static_cast<int>(m_view));
            }
            if (!droppable(drop->position().toPoint())) return true;
            moveMessagesTo(dragIds(drop->mimeData(), mailIdsMime()), static_cast<View>(row));
            drop->acceptProposedAction();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EmailPage::setSearchText(const QString& text)
{
    m_search->setText(text);
}

QString EmailPage::searchText() const
{
    return m_search ? m_search->text() : QString{};
}

void EmailPage::setView(View view)
{
    m_view = view;
    if (m_folders->currentRow() != static_cast<int>(view)) m_folders->setCurrentRow(static_cast<int>(view));
    closeDetail();
    rebuildList();
}

bool EmailPage::inView(const CybouMailItem& item) const
{
    switch (m_view) {
    case View::Inbox: return item.folder == CybouMailFolder::Inbox;
    case View::Starred: return item.starred && item.folder != CybouMailFolder::Trash;
    // Mail to yourself is one message filed in Inbox; Sent lists it too.
    case View::Sent: return item.folder == CybouMailFolder::Sent ||
        (item.outgoing && item.folder == CybouMailFolder::Inbox);
    case View::Drafts: return item.folder == CybouMailFolder::Drafts;
    case View::Archive: return item.folder == CybouMailFolder::Archive;
    case View::Trash: return item.folder == CybouMailFolder::Trash;
    }
    return false;
}

bool EmailPage::matches(const CybouMailItem& item, const QString& needle) const
{
    if (needle.isEmpty()) return true;
    const auto has = [&needle](const QString& text) { return text.contains(needle, Qt::CaseInsensitive); };
    if (has(item.from_name) || has(item.to_name) || has(item.subject) || has(item.body)) return true;
    return std::any_of(item.attachments.begin(), item.attachments.end(),
        [&has](const CybouAttachmentItem& attachment) { return has(attachment.name); });
}

QStringList EmailPage::visibleMessageIds() const
{
    QStringList ids;
    for (int i = 0; i < m_list->count(); ++i) ids << m_list->item(i)->data(Qt::UserRole).toString();
    return ids;
}

void EmailPage::rebuildFolders()
{
    const QSignalBlocker blocker{m_folders};
    int drafts = 0;
    for (const auto& item : m_model->mailItems()) {
        if (item.folder == CybouMailFolder::Drafts) ++drafts;
    }
    for (int i = 0; i <= static_cast<int>(View::Trash); ++i) {
        const auto view = static_cast<View>(i);
        int count = 0;
        if (view == View::Inbox) count = m_model->unreadMailCount();
        if (view == View::Drafts) count = drafts;
        auto* item = m_folders->item(i);
        if (item) {
            item->setData(Qt::AccessibleTextRole, count > 0 ? tr("%1, %2").arg(ViewName(view)).arg(count) : ViewName(view));
            auto* row = m_folders->itemWidget(item);
            auto* badge = row->findChild<QLabel*>(QStringLiteral("folderCount"));
            badge->setText(QString::number(count));
            badge->setVisible(count > 0);
            auto* name = row->findChild<QLabel*>(QStringLiteral("folderName"));
            name->setStyleSheet(QStringLiteral("color: %1; background: transparent;%2").arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name(), count > 0 ? QStringLiteral(" font-weight: 700;") : QString{}));
            continue;
        }
        item = new QListWidgetItem{m_folders};
        item->setData(Qt::AccessibleTextRole, count > 0 ? tr("%1, %2").arg(ViewName(view)).arg(count) : ViewName(view));
        item->setSizeHint(QSize{0, 40});
        auto* row = new QWidget{m_folders};
        row->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* layout = new QHBoxLayout{row};
        layout->setContentsMargins(10, 0, 10, 0);
        layout->setSpacing(10);
        auto* icon = new QLabel{row};
        icon->setPixmap(glyphPixmap(ViewGlyph(view), {18, 18}, CybouTheme::color(CybouTheme::TEXT_SECONDARY)));
        layout->addWidget(icon);
        auto* name = new QLabel{ViewName(view), row};
        name->setObjectName(QStringLiteral("folderName"));
        name->setStyleSheet(QStringLiteral("color: %1; background: transparent;%2").arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name(), count > 0 ? QStringLiteral(" font-weight: 700;") : QString{}));
        layout->addWidget(name, 1);
        {
            auto* badge = new QLabel{QString::number(count), row};
            badge->setObjectName(QStringLiteral("folderCount"));
            badge->setVisible(count > 0);
            badge->setStyleSheet(QStringLiteral("font-weight: 700; color: %1;").arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
            layout->addWidget(badge);
        }
        m_folders->setItemWidget(item, row);
    }
    m_folders->setCurrentRow(static_cast<int>(m_view));
}

void EmailPage::rebuildList()
{
    const QString needle = m_search->text().trimmed();
    QVector<CybouMailItem> items;
    for (const auto& item : m_model->mailItems()) {
        if (inView(item) && matches(item, needle)) items.append(item);
    }
    std::sort(items.begin(), items.end(), [](const CybouMailItem& a, const CybouMailItem& b) { return a.time != b.time ? a.time > b.time : a.id < b.id; });
    m_empty_trash->setVisible(m_view == View::Trash && !items.isEmpty() && needle.isEmpty());

    if (m_model->status().identity_state != CybouIdentityState::Active) {
        items.clear();
        m_current_id.clear();
        m_press_id.clear();
        const QSignalBlocker search_blocker{m_search};
        m_search->clear();
    }
    const QSignalBlocker blocker{m_list};
    const int scroll = m_list->verticalScrollBar()->value();
    auto* top = m_list->itemAt(QPoint{1, 0});
    const QString anchor = top ? top->data(Qt::UserRole).toString() : QString{};
    const int offset = top ? m_list->visualItemRect(top).top() : 0;
    QStringList order, previous;
    for (int i{0}; i < m_list->count(); ++i) previous.append(m_list->item(i)->data(Qt::UserRole).toString());
    QSet<QString> visible;
    for (const auto& mail : items) { order.append(mail.id); visible.insert(mail.id); }
    for (auto it = m_rows.begin(); it != m_rows.end();) {
        if (visible.contains(it.key())) { ++it; continue; }
        m_rendered_mail.remove(it.key());
        m_rendered_meta.remove(it.key());
        delete it.value();
        it = m_rows.erase(it);
    }
    int rank{0};
    for (const auto& mail : items) {
        auto* item = m_rows.value(mail.id);
        if (!item) {
            item = new MessageItem{m_list};
            item->setData(Qt::UserRole, mail.id);
            item->setSizeHint(QSize{0, 62});
            m_rows.insert(mail.id, item);
        }
        if (!item->data(kRankRole).isValid() || item->data(kRankRole).toInt() != rank) item->setData(kRankRole, rank);
        ++rank;
        const auto operation = m_model->displayedOperationState(mail.operation_id, mail.operation_state);
        const QStringList meta{QString::number(static_cast<int>(operation)), QString::number(m_model->status().online), shortTime(mail.time)};
        if (!m_rendered_mail.contains(mail.id) || m_rendered_mail.value(mail.id) != mail || m_rendered_meta.value(mail.id) != meta) {
            QStringList accessible{RowPeer(mail), mail.subject.isEmpty() ? tr("(no subject)") : mail.subject};
            if (mail.unread) accessible.append(tr("unread"));
            if (mail.starred) accessible.append(tr("Starred"));
            if (!mail.attachments.isEmpty()) accessible.append(tr("Has attachments"));
            if (mail.below_support_rate) accessible.append(tr("Below support rate"));
            const QString state = mail.draft ? tr("Draft") : CybouProduct::contentWithOperationText(mail.state, operation, m_model->status().online);
            accessible.append(state);
            accessible.append(meta[2]);
            item->setData(Qt::AccessibleTextRole, accessible.join(QStringLiteral(", ")));
            item->setData(Qt::AccessibleDescriptionRole, mail.preview);
            QString tooltip = accessible.join(QStringLiteral(" · "));
            if (mail.state == CybouContentState::Protected) tooltip += QStringLiteral("\n") + tr("Protected: encrypted here and stored encrypted on the network");
            else if (mail.state == CybouContentState::Received) tooltip += QStringLiteral("\n") + tr("Received: end-to-end encrypted");
            if (mail.below_support_rate) tooltip += QStringLiteral("\n") + tr("This message paid less than the support rate; it may be sent by a modified client or be spam.");
            item->setToolTip(QStringLiteral("<qt>%1</qt>").arg(tooltip.toHtmlEscaped().replace(QLatin1Char{'\n'}, QStringLiteral("<br/>"))));
            item->setData(kOperationRole, static_cast<int>(operation));
            item->setData(kOnlineRole, m_model->status().online);
            m_rendered_mail.insert(mail.id, mail);
            m_rendered_meta.insert(mail.id, meta);
        }
    }
    if (order != previous) {
        m_list->sortItems(Qt::AscendingOrder);
        m_list->doItemsLayout();
    }
    if (order != previous && m_rows.contains(anchor)) {
        m_list->scrollToItem(m_rows.value(anchor), QAbstractItemView::PositionAtTop);
        m_list->verticalScrollBar()->setValue(m_list->verticalScrollBar()->value() - offset);
    } else m_list->verticalScrollBar()->setValue(scroll);
    const bool identity = m_model->status().identity_state == CybouIdentityState::Active;
    m_list_empty->setText(!identity ? QString{}
        : !needle.isEmpty() ? tr("No messages match “%1”.").arg(needle)
        : tr("%1 is empty.").arg(ViewName(m_view)));
    m_list_empty->setVisible(items.isEmpty() && identity);
    m_list->setVisible(!items.isEmpty());
    m_list->viewport()->update();
}

void EmailPage::refreshBanner()
{
    const bool identity = m_model->status().identity_state == CybouIdentityState::Active;
    const bool connected = m_model->featureAvailability().mail;
    m_banner->setVisible(!identity || !connected);
    m_banner_text->setText(!identity ? tr("Mail needs your CYBOU Identity. Create or restore it on Home.")
        : m_model->applicationLoadState() == CybouApplicationLoadState::Loading ||
          m_model->applicationLoadState() == CybouApplicationLoadState::Opening
            ? tr("Your local view is still being prepared. Items appear progressively.")
                                     : tr("Mail is not connected yet. Messages will appear here once it is."));
    m_banner_action->setVisible(!identity);
    m_compose_button->setEnabled(identity);
    rebuildList();
}

void EmailPage::openMessage(const QString& id)
{
    const auto* item = m_model->mailItem(id);
    if (!item) return;
    m_current_id = id;
    if (item->draft) {
        openCompose(*item);
        return;
    }
    if (item->unread) m_model->requestMailRead(id, true);
    m_reader->showMessage(id);
    m_detail->setCurrentWidget(m_reader);
    m_detail_open = true;
    updateLayoutMode();
}

void EmailPage::openCompose(const CybouMailItem& draft)
{
    if (m_model->status().identity_state != CybouIdentityState::Active) return;
    m_current_id.clear();
    m_compose->start(draft);
    m_detail->setCurrentWidget(m_compose);
    m_detail_open = true;
    updateLayoutMode();
}

bool EmailPage::isComposing() const
{
    return m_detail_open && m_detail->currentWidget() == m_compose;
}

CybouMailItem EmailPage::replyTo(const QString& id) const
{
    CybouMailItem reply;
    const auto* item = m_model->mailItem(id);
    if (!item) return reply;
    const bool outgoing = item->outgoing || item->folder == CybouMailFolder::Sent;
    const QString label = outgoing ? item->to_name : item->from_name;
    const QString address = outgoing ? item->to_address : item->from_address;
    reply.to_name = label.endsWith(QStringLiteral(".cybou")) || address.isEmpty() ? label : address;
    reply.subject = item->subject.startsWith(QStringLiteral("Re:")) ? item->subject : tr("Re: %1").arg(item->subject);
    QString quoted;
    for (const auto& line : item->body.split(QLatin1Char{'\n'})) quoted += QStringLiteral("> %1\n").arg(line);
    reply.body = tr("\n\nOn %1, %2 wrote:\n%3").arg(QLocale{}.toString(item->time, QStringLiteral("MMM d, HH:mm")),
        item->from_name, quoted);
    return reply;
}

CybouMailItem EmailPage::forwardOf(const QString& id) const
{
    CybouMailItem forward;
    const auto* item = m_model->mailItem(id);
    if (!item) return forward;
    forward.subject = item->subject.startsWith(QStringLiteral("Fwd:")) ? item->subject : tr("Fwd: %1").arg(item->subject);
    forward.body = tr("\n\n---------- Forwarded message ----------\nFrom: %1\nTo: %2\nSubject: %3\n\n%4")
        .arg(item->from_name, item->to_name, item->subject, item->body);
    // Forwarding reuses the already protected attachment content.
    forward.attachments = item->attachments;
    return forward;
}

void EmailPage::rebuildContacts()
{
    while (QLayoutItem* item = m_contact_rows->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    QWidget* parent = m_contact_rows->parentWidget();
    const auto& contacts = m_model->contacts();
    if (contacts.isEmpty()) {
        m_contact_rows->addWidget(MutedText(tr("People you mail or pay appear here."), parent));
        return;
    }
    for (qsizetype i = 0; i < contacts.size() && i < 8; ++i) {
        const auto& contact = contacts.at(i);
        auto* button = new QToolButton{parent};
        button->setObjectName(QStringLiteral("mailContact"));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIcon(QIcon{avatarPixmap(contact.name.left(1).toUpper(), PeerColor(contact.name), 22)});
        button->setIconSize({22, 22});
        const QString label = contact.name.endsWith(QStringLiteral(".cybou")) ? contact.name : contact.display_name;
        button->setText(label);
        button->setToolTip(tr("Write to %1").arg(label));
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setStyleSheet(QStringLiteral("QToolButton { text-align: left; padding: 4px 6px; border: none; }"));
        connect(button, &QToolButton::clicked, this, [this, name = contact.name] {
            CybouMailItem draft;
            draft.to_name = name;
            openCompose(draft);
        });
        m_contact_rows->addWidget(button);
    }
}

void EmailPage::closeDetail()
{
    m_detail_open = false;
    m_current_id.clear();
    m_detail->setCurrentWidget(m_detail_empty);
    updateLayoutMode();
}

void EmailPage::updateLayoutMode()
{
    const QWidget* top = window();
    m_three_pane = top && top->width() >= kThreePaneWindowWidth;
    if (m_three_pane) {
        m_list_pane->setVisible(true);
        m_detail->setVisible(true);
    } else {
        // Folders + list, or the open message with an explicit Back.
        m_list_pane->setVisible(!m_detail_open);
        m_detail->setVisible(m_detail_open);
    }
    m_reader->setBackVisible(!m_three_pane);
    m_compose->setBackVisible(!m_three_pane);
}

void EmailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayoutMode();
}

void EmailPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateLayoutMode();
}
