// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/emailpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>
#include <qt/pages/mailcompose.h>
#include <qt/pages/mailreader.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace CybouUi;

namespace {

/** Window width at which Mail shows folders, list and reader together. */
constexpr int kThreePaneWindowWidth = 1400;

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
    return item.folder == CybouMailFolder::Sent || item.folder == CybouMailFolder::Drafts
        ? EmailPage::tr("To: %1").arg(item.to_name) : item.from_name;
}

QWidget* MailRow(const CybouMailItem& item, QWidget* parent)
{
    auto* row = new QWidget{parent};
    row->setObjectName(QStringLiteral("mailRow"));
    row->setAccessibleName(EmailPage::tr("%1, %2%3").arg(RowPeer(item), item.subject,
        item.unread ? EmailPage::tr(", unread") : QString{}));
    auto* layout = new QHBoxLayout{row};
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);
    const QString peer = item.folder == CybouMailFolder::Inbox || item.folder == CybouMailFolder::Archive ||
            item.folder == CybouMailFolder::Trash ? item.from_name : item.to_name;
    layout->addWidget(Avatar(peer.left(1), PeerColor(peer), row, 34), 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* top = new QHBoxLayout;
    top->setSpacing(8);
    auto* who = new ElidedLabel{RowPeer(item), row};
    who->setStyleSheet(item.unread ? QStringLiteral("font-weight: 700; color: %1;").arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name())
                                   : QStringLiteral("color: %1;").arg(CybouTheme::color(CybouTheme::TEXT_SECONDARY).name()));
    top->addWidget(who, 1);
    if (!item.attachments.isEmpty()) {
        auto* clip = new QLabel{row};
        clip->setPixmap(glyphPixmap(Glyph::File, {14, 14}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
        clip->setToolTip(EmailPage::tr("Has attachments"));
        top->addWidget(clip);
    }
    if (item.starred) {
        auto* star = new QLabel{row};
        star->setPixmap(glyphPixmap(Glyph::Star, {14, 14}, CybouTheme::color(CybouTheme::AMBER)));
        top->addWidget(star);
    }
    const bool pending = CybouProduct::contentPending(item.state) || item.state == CybouContentState::NeedsAttention;
    auto* when = new QLabel{pending && !item.draft ? CybouProduct::mailStateText(item) : shortTime(item.time), row};
    when->setObjectName(QStringLiteral("rowMeta"));
    if (pending && !item.draft) {
        when->setStyleSheet(QStringLiteral("color: %1;").arg(CybouTheme::color(
            item.state == CybouContentState::NeedsAttention ? CybouTheme::ROSE : CybouTheme::AMBER).name()));
    }
    top->addWidget(when);
    text->addLayout(top);

    auto* subject = new ElidedLabel{item.subject.isEmpty() ? EmailPage::tr("(no subject)") : item.subject, row};
    subject->setStyleSheet(item.unread ? QStringLiteral("font-weight: 700; color: %1;").arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name())
                                       : QStringLiteral("color: %1;").arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    text->addWidget(subject);
    auto* preview = new ElidedLabel{item.preview, row};
    preview->setObjectName(QStringLiteral("rowSub"));
    text->addWidget(preview);
    layout->addLayout(text, 1);
    return row;
}

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
    auto* labels_title = new QLabel{tr("Labels"), m_rail};
    labels_title->setObjectName(QStringLiteral("eyebrow"));
    rail->addSpacing(6);
    rail->addWidget(labels_title);
    rail->addWidget(MutedText(tr("No labels yet"), m_rail));
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
    m_list->setAccessibleName(tr("Messages"));
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setUniformItemSizes(true);
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
    auto* empty_label = MutedText(tr("Select a message to read it."), m_detail_empty);
    empty_label->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_label);
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
    };

    connect(m_folders, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) setView(static_cast<View>(row));
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildList(); });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        openMessage(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        openMessage(item->data(Qt::UserRole).toString());
    });
    connect(m_model, &CybouDesktopModel::mailChanged, this, [this] {
        rebuildFolders();
        rebuildList();
    });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refreshBanner(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refreshBanner(); });

    rebuildFolders();
    m_folders->setCurrentRow(0);
    rebuildList();
    refreshBanner();
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
    case View::Sent: return item.folder == CybouMailFolder::Sent;
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
    const int current = m_folders->currentRow();
    const QSignalBlocker blocker{m_folders};
    m_folders->clear();
    int drafts = 0;
    for (const auto& item : m_model->mailItems()) {
        if (item.folder == CybouMailFolder::Drafts) ++drafts;
    }
    for (int i = 0; i <= static_cast<int>(View::Trash); ++i) {
        const auto view = static_cast<View>(i);
        int count = 0;
        if (view == View::Inbox) count = m_model->unreadMailCount();
        if (view == View::Drafts) count = drafts;
        auto* item = new QListWidgetItem{m_folders};
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
        name->setStyleSheet(QStringLiteral("color: %1; background: transparent;%2").arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name(), count > 0 ? QStringLiteral(" font-weight: 700;") : QString{}));
        layout->addWidget(name, 1);
        if (count > 0) {
            auto* badge = new QLabel{QString::number(count), row};
            badge->setObjectName(QStringLiteral("folderCount"));
            badge->setStyleSheet(QStringLiteral("font-weight: 700; color: %1;").arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
            layout->addWidget(badge);
        }
        m_folders->setItemWidget(item, row);
    }
    m_folders->setCurrentRow(current < 0 ? 0 : current);
}

void EmailPage::rebuildList()
{
    const QString needle = m_search->text().trimmed();
    QVector<CybouMailItem> items;
    for (const auto& item : m_model->mailItems()) {
        if (inView(item) && matches(item, needle)) items.append(item);
    }
    std::sort(items.begin(), items.end(), [](const CybouMailItem& a, const CybouMailItem& b) { return a.time > b.time; });

    m_list->clear();
    for (const auto& mail : items) {
        auto* item = new QListWidgetItem{m_list};
        item->setData(Qt::UserRole, mail.id);
        item->setData(Qt::AccessibleTextRole, tr("%1, %2").arg(RowPeer(mail), mail.subject));
        item->setSizeHint(QSize{0, 76});
        m_list->setItemWidget(item, MailRow(mail, m_list));
        if (mail.id == m_current_id) m_list->setCurrentItem(item);
    }
    const bool identity = m_model->status().identity_state == CybouIdentityState::Active;
    m_list_empty->setText(!identity ? QString{}
        : !needle.isEmpty() ? tr("No messages match “%1”.").arg(needle)
        : tr("%1 is empty.").arg(ViewName(m_view)));
    m_list_empty->setVisible(items.isEmpty() && identity);
    m_list->setVisible(!items.isEmpty());
}

void EmailPage::refreshBanner()
{
    const bool identity = m_model->status().identity_state == CybouIdentityState::Active;
    const bool connected = m_model->capabilities().mail;
    m_banner->setVisible(!identity || !connected);
    m_banner_text->setText(!identity ? tr("Mail needs your CYBOU Identity. Create or restore it on Home.")
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
    if (item->unread) m_model->setMailRead(id, true);
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

CybouMailItem EmailPage::replyTo(const QString& id) const
{
    CybouMailItem reply;
    const auto* item = m_model->mailItem(id);
    if (!item) return reply;
    const bool outgoing = item->folder == CybouMailFolder::Sent;
    reply.to_name = outgoing ? item->to_name : item->from_name;
    reply.subject = item->subject.startsWith(QStringLiteral("Re:")) ? item->subject : tr("Re: %1").arg(item->subject);
    QString quoted;
    for (const auto& line : item->body.split(QLatin1Char{'\n'})) quoted += QStringLiteral("> %1\n").arg(line);
    reply.body = tr("\n\nOn %1, %2 wrote:\n%3").arg(QLocale{QLocale::English}.toString(item->time, QStringLiteral("MMM d, HH:mm")),
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
