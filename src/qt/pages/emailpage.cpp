// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/emailpage.h>

#include <cybou/mail_service.h>
#include <cybou/hex.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QBrush>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

/** Deterministic accent color for a peer account (avatar disc). */
QRgb peerColor(const QString& peer)
{
    static const QRgb palette[] = {
        CybouTheme::BRAND_TEAL, CybouTheme::BLUE, CybouTheme::INDIGO,
        CybouTheme::VIOLET, CybouTheme::AMBER, CybouTheme::ROSE,
    };
    uint hash = 0;
    for (const QChar ch : peer) hash = (hash * 31) ^ ch.unicode();
    return palette[hash % std::size(palette)];
}

QLabel* noteLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    return label;
}

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("cardLabel"));
    return label;
}

} // namespace

EmailPage::EmailPage(CybouDesktopModel* model, std::function<void()> identity_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_identity_requested{std::move(identity_requested)}
{
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(14, 16, 14, 16);
    root->setSpacing(12);

    // ---- Left rail: compose, folders, labels ------------------------------
    auto* rail = new QFrame{this};
    rail->setObjectName(QStringLiteral("card"));
    rail->setMinimumWidth(176);
    rail->setMaximumWidth(220);
    rail->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* rail_layout = new QVBoxLayout{rail};
    rail_layout->setContentsMargins(14, 14, 14, 14);
    rail_layout->setSpacing(10);

    m_compose_button = new QPushButton{tr("Compose"), rail};
    m_compose_button->setObjectName(QStringLiteral("primaryButton"));
    m_compose_button->setIcon(QIcon{glyphPixmap(Glyph::Compose, {16, 16}, QColor{0xffffff})});
    connect(m_compose_button, &QPushButton::clicked, this, [this] { openComposer(); });
    rail_layout->addWidget(m_compose_button);

    m_folders = new QListWidget{rail};
    m_folders->setObjectName(QStringLiteral("folderList"));
    m_folders->setUniformItemSizes(true);
    m_folders->setSelectionMode(QAbstractItemView::SingleSelection);
    m_folders->setFocusPolicy(Qt::NoFocus);
    connect(m_folders, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= FOLDER_COUNT) return;
        m_folder = static_cast<Folder>(row);
        m_current_message = -1;
        rebuildMessageList();
        clearReader();
    });
    rail_layout->addWidget(m_folders, 1);

    // Labels: local visual indexes (sketch). Messages carry no label data
    // until the local label index lands, so rows render without counts.
    auto* labels_header_row = new QHBoxLayout;
    labels_header_row->setContentsMargins(8, 0, 4, 0);
    auto* labels_header = new QLabel{tr("LABELS"), rail};
    labels_header->setObjectName(QStringLiteral("eyebrow"));
    labels_header_row->addWidget(labels_header, 0, Qt::AlignVCenter);
    labels_header_row->addStretch();
    auto* add_label = IconButton(Glyph::Plus, rail, tr("Create label (planned)"));
    add_label->setFixedSize(22, 22);
    add_label->setIconSize(QSize{14, 14});
    labels_header_row->addWidget(add_label, 0, Qt::AlignVCenter);
    rail_layout->addLayout(labels_header_row);
    struct LabelDef { const char* name; Tint tint; };
    const LabelDef labels[]{
        {QT_TR_NOOP("Project"), Tint::Blue},
        {QT_TR_NOOP("Personal"), Tint::Violet},
        {QT_TR_NOOP("Finance"), Tint::Mint},
        {QT_TR_NOOP("Team"), Tint::Amber},
    };
    for (const auto& label : labels) {
        auto* row_widget = new QWidget{rail};
        auto* row = new QHBoxLayout{row_widget};
        row->setContentsMargins(8, 5, 8, 5);
        row->setSpacing(10);
        row->addWidget(Dot(label.tint, row_widget, 8), 0, Qt::AlignVCenter);
        auto* name = new QLabel{tr(label.name), row_widget};
        name->setObjectName(QStringLiteral("bodyText"));
        row->addWidget(name, 1);
        row_widget->setToolTip(tr("Label indexes are a local-client feature and arrive with the mailbox label work."));
        rail_layout->addWidget(row_widget);
    }
    root->addWidget(rail);

    // ---- Middle: search + message list ------------------------------------
    auto* middle = new QFrame{this};
    middle->setObjectName(QStringLiteral("card"));
    middle->setMinimumWidth(0);
    middle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* middle_layout = new QVBoxLayout{middle};
    middle_layout->setContentsMargins(14, 14, 14, 14);
    middle_layout->setSpacing(10);

    // Identity banner across the top of the mail view (hidden once active).
    m_banner = new QFrame{middle};
    m_banner->setObjectName(QStringLiteral("identityBanner"));
    auto* banner_layout = new QGridLayout{m_banner};
    banner_layout->setContentsMargins(14, 10, 14, 10);
    banner_layout->setSpacing(10);
    auto* banner_chip = new QLabel{m_banner};
    banner_chip->setPixmap(glyphPixmap(Glyph::Info, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    banner_layout->addWidget(banner_chip, 0, 0, Qt::AlignTop);
    m_banner_text = new QLabel{m_banner};
    m_banner_text->setObjectName(QStringLiteral("bodyText"));
    m_banner_text->setWordWrap(true);
    m_banner_text->setMinimumWidth(0);
    m_banner_text->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    banner_layout->addWidget(m_banner_text, 0, 1);
    m_banner_action = new QPushButton{tr("Create identity"), m_banner};
    m_banner_action->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_banner_action, &QPushButton::clicked, this, [this] { m_identity_requested(); });
    banner_layout->addWidget(m_banner_action, 1, 1, Qt::AlignLeft);
    banner_layout->setColumnStretch(1, 1);
    middle_layout->addWidget(m_banner);

    auto* search_row = new QHBoxLayout;
    search_row->setSpacing(8);
    m_search = new QLineEdit{middle};
    m_search->setPlaceholderText(tr("Search messages, people or files\u2026"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildMessageList(); });
    search_row->addWidget(m_search, 1);
    auto* filter = IconButton(Glyph::Sliders, middle, tr("Filter (planned)"));
    search_row->addWidget(filter, 0, Qt::AlignVCenter);
    middle_layout->addLayout(search_row);

    m_list = new QListWidget{middle};
    m_list->setObjectName(QStringLiteral("messageList"));
    m_list->setFocusPolicy(Qt::NoFocus);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        auto* item = m_list->currentItem();
        if (!item) return;
        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= m_messages.size()) return;
        m_current_message = index;
        Message& message = m_messages[index];
        if (m_folder == FOLDER_INBOX && !message.read) {
            message.read = true;
            if (auto* service = m_model->mailService()) {
                const auto id_opt = cybou::ParseUint256UserHex(message.id.toStdString());
                if (id_opt) service->MarkAsRead(*id_opt, true);
            }
            rebuildFolderList();
            rebuildMessageList();
        }
        showMessage(message);
    });
    middle_layout->addWidget(m_list, 1);
    root->addWidget(middle, 5);

    // ---- Right: reading pane and composer share a stack -------------------
    m_right_stack = new QStackedWidget{this};
    m_right_stack->setMinimumWidth(0);
    m_right_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    root->addWidget(m_right_stack, 7);

    auto* mail_view = new QWidget{m_right_stack};
    auto* mail_layout = new QVBoxLayout{mail_view};
    mail_layout->setContentsMargins(0, 0, 0, 0);
    mail_layout->setSpacing(0);

    // Reader card: headers, chips, body, protocol evidence, local actions.
    m_reader = Card(mail_view);
    auto* reader_layout = new QVBoxLayout{m_reader};
    reader_layout->setContentsMargins(22, 18, 22, 18);
    reader_layout->setSpacing(10);

    auto* subject_row = new QHBoxLayout;
    m_reader_subject = new QLabel{m_reader};
    m_reader_subject->setObjectName(QStringLiteral("pageTitle"));
    m_reader_subject->setWordWrap(true);
    m_reader_subject->setMinimumWidth(0);
    m_reader_subject->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    subject_row->addWidget(m_reader_subject, 1);
    auto* nav_left = IconButton(Glyph::ChevronLeft, m_reader, tr("Previous message"));
    auto* nav_right = IconButton(Glyph::ChevronRight, m_reader, tr("Next message"));
    subject_row->addWidget(nav_left, 0, Qt::AlignVCenter);
    subject_row->addWidget(nav_right, 0, Qt::AlignVCenter);
    reader_layout->addLayout(subject_row);

    auto* peer_row = new QHBoxLayout;
    peer_row->setSpacing(10);
    m_reader_avatar = new QLabel{m_reader};
    m_reader_avatar->setFixedSize(40, 40);
    peer_row->addWidget(m_reader_avatar, 0, Qt::AlignVCenter);
    auto* peer_column = new QVBoxLayout;
    peer_column->setSpacing(0);
    m_reader_peer = new QLabel{m_reader};
    m_reader_peer->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
        .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    m_reader_meta = new QLabel{m_reader};
    m_reader_meta->setObjectName(QStringLiteral("rowSub"));
    m_reader_meta->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    peer_column->addWidget(m_reader_peer);
    peer_column->addWidget(m_reader_meta);
    peer_row->addLayout(peer_column);
    peer_row->addStretch();
    m_star_button = IconButton(Glyph::Star, m_reader, tr("Star (planned)"));
    auto* more_button = IconButton(Glyph::DotsV, m_reader, tr("Message actions"));
    auto* more_menu = new QMenu{more_button};
    auto* mark_unread = more_menu->addAction(tr("Mark as unread"));
    connect(mark_unread, &QAction::triggered, this, [this] {
        if (m_current_message < 0 || m_current_message >= m_messages.size()) return;
        Message& message = m_messages[m_current_message];
        message.read = false;
        if (auto* service = m_model->mailService()) {
            const auto id_opt = cybou::ParseUint256UserHex(message.id.toStdString());
            if (id_opt) service->MarkAsRead(*id_opt, false);
        }
        rebuildFolderList();
        rebuildMessageList();
    });
    more_button->setMenu(more_menu);
    more_button->setPopupMode(QToolButton::InstantPopup);
    peer_row->addWidget(m_star_button, 0, Qt::AlignVCenter);
    peer_row->addWidget(more_button, 0, Qt::AlignVCenter);
    reader_layout->addLayout(peer_row);

    auto* chips_row = new QHBoxLayout;
    chips_row->setSpacing(6);
    m_chip_encrypted = Pill(tr("Encrypted"), Tint::Mint, m_reader);
    m_chip_verified = Pill(tr("Identity verified"), Tint::Blue, m_reader);
    m_chip_protected = Pill(tr("Protected"), Tint::Mint, m_reader);
    chips_row->addWidget(m_chip_encrypted);
    chips_row->addWidget(m_chip_verified);
    chips_row->addWidget(m_chip_protected);
    chips_row->addStretch();
    reader_layout->addLayout(chips_row);

    m_reader_hint = noteLabel(tr("Select a message to read it. Read-state stays local to this client."), m_reader);
    m_reader_hint->setAlignment(Qt::AlignCenter);
    m_reader_hint->setMinimumHeight(160);
    reader_layout->addWidget(m_reader_hint, 1);

    m_reader_body = new QLabel{m_reader};
    m_reader_body->setObjectName(QStringLiteral("bodyText"));
    m_reader_body->setWordWrap(true);
    m_reader_body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_reader_body->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_reader_body->setVisible(false);
    reader_layout->addWidget(m_reader_body, 1);

    auto* evidence_title = new QLabel{tr("Protocol evidence"), m_reader};
    evidence_title->setObjectName(QStringLiteral("sectionTitle"));
    evidence_title->setVisible(false);
    reader_layout->addWidget(evidence_title);
    m_evidence_title = evidence_title;
    m_evidence = new QFrame{m_reader};
    auto* evidence_layout = new QVBoxLayout{m_evidence};
    evidence_layout->setContentsMargins(0, 0, 0, 0);
    evidence_layout->setSpacing(4);
    for (const QString& field : {tr("Transaction inclusion proof"), tr("BFT finality certificate"),
                                 tr("Historical sender-key authorization"), tr("Salted, domain-separated content commitment")}) {
        auto* row = new QHBoxLayout;
        auto* name = new QLabel{field, m_evidence};
        name->setObjectName(QStringLiteral("mutedText"));
        auto* state = new QLabel{tr("pending"), m_evidence};
        state->setObjectName(QStringLiteral("neutralBadge"));
        m_evidence_states.append(state);
        row->addWidget(name, 1);
        row->addWidget(state, 0, Qt::AlignVCenter);
        evidence_layout->addLayout(row);
    }
    m_evidence->setVisible(false);
    reader_layout->addWidget(m_evidence);

    auto* reader_actions = new QHBoxLayout;
    reader_actions->setSpacing(8);
    auto* reply = new QPushButton{tr("Reply"), m_reader};
    reply->setObjectName(QStringLiteral("secondaryButton"));
    reply->setIcon(QIcon{glyphPixmap(Glyph::Reply, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    auto* reply_all = new QPushButton{tr("Reply all"), m_reader};
    reply_all->setObjectName(QStringLiteral("secondaryButton"));
    reply_all->setIcon(QIcon{glyphPixmap(Glyph::Reply, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    auto* forward = new QPushButton{tr("Forward"), m_reader};
    forward->setObjectName(QStringLiteral("secondaryButton"));
    forward->setIcon(QIcon{glyphPixmap(Glyph::Forward, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    reader_actions->addWidget(reply);
    reader_actions->addWidget(reply_all);
    reader_actions->addWidget(forward);
    reader_actions->addStretch();
    reader_layout->addLayout(reader_actions);
    m_reply_buttons = {reply, reply_all, forward};
    auto do_reply = [this](bool all) {
        if (m_current_message < 0 || m_current_message >= m_messages.size()) return;
        const Message& message = m_messages.at(m_current_message);
        if (message.folder == FOLDER_SENT) return;
        const QString quoted = QStringLiteral("\n\n--- %1 ---\n%2")
            .arg(peerName(message.from), message.body);
        openComposerWith(message.from, tr("Re: %1").arg(message.subject), quoted);
    };
    connect(reply, &QPushButton::clicked, this, [do_reply] { do_reply(false); });
    connect(reply_all, &QPushButton::clicked, this, [do_reply] { do_reply(true); });
    connect(forward, &QPushButton::clicked, this, [this] {
        if (m_current_message < 0 || m_current_message >= m_messages.size()) return;
        const Message& message = m_messages.at(m_current_message);
        const QString quoted = QStringLiteral("\n\n--- %1 ---\n%2")
            .arg(peerName(message.from), message.body);
        openComposerWith({}, tr("Fwd: %1").arg(message.subject), quoted);
    });

    mail_layout->addWidget(m_reader, 1);

    // ---- Composer ----------------------------------------------------------
    auto* composer = new QFrame{m_right_stack};
    composer->setObjectName(QStringLiteral("card"));
    auto* compose_layout = new QVBoxLayout{composer};
    compose_layout->setContentsMargins(24, 20, 24, 20);
    compose_layout->setSpacing(10);

    auto* compose_title = new QLabel{tr("New message"), composer};
    compose_title->setObjectName(QStringLiteral("pageTitle"));
    compose_layout->addWidget(compose_title);
    compose_layout->addWidget(noteLabel(tr("CYBOU mail is a first-class protocol operation (MailTx): exactly one recipient, text only, no attachments."), composer));

    compose_layout->addWidget(fieldLabel(tr("To"), composer));
    m_to = new QLineEdit{composer};
    m_to->setObjectName(QStringLiteral("recipientEdit"));
    m_to->setPlaceholderText(tr("Recipient address"));
    compose_layout->addWidget(m_to);
    m_to_hint = noteLabel(QString{}, composer);
    m_to_hint->setVisible(false);
    compose_layout->addWidget(m_to_hint);

    compose_layout->addWidget(fieldLabel(tr("Subject"), composer));
    m_subject = new QLineEdit{composer};
    m_subject->setPlaceholderText(tr("Subject"));
    compose_layout->addWidget(m_subject);

    compose_layout->addWidget(fieldLabel(tr("Message"), composer));
    m_body = new QTextEdit{composer};
    m_body->setObjectName(QStringLiteral("composeBody"));
    m_body->setAcceptRichText(false);
    compose_layout->addWidget(m_body, 1);

    auto* meter_row = new QHBoxLayout;
    m_size_meter = new QProgressBar{composer};
    m_size_meter->setObjectName(QStringLiteral("sizeMeter"));
    m_size_meter->setRange(0, static_cast<int>(kMaxMailTxBytes));
    m_size_meter->setValue(0);
    m_size_meter->setTextVisible(false);
    m_size_meter->setFixedHeight(10);
    meter_row->addWidget(m_size_meter, 1);
    m_size_label = noteLabel(QString{}, composer);
    meter_row->addWidget(m_size_label);
    compose_layout->addLayout(meter_row);
    compose_layout->addWidget(noteLabel(tr("Fee: deterministic and size-based \u2014 priority fees are not part of the protocol. Distribution: 3 Security, 1 Onboarding."), composer));

    auto* actions = new QHBoxLayout;
    m_send = new QPushButton{tr("Send"), composer};
    m_send->setObjectName(QStringLiteral("sendButton"));
    m_send->setProperty("primary", true);
    connect(m_send, &QPushButton::clicked, this, [this] { sendNow(); });
    auto* save_draft = new QPushButton{tr("Save draft"), composer};
    save_draft->setObjectName(QStringLiteral("secondaryButton"));
    connect(save_draft, &QPushButton::clicked, this, [this] { saveDraft(); });
    auto* cancel = new QPushButton{tr("Cancel"), composer};
    cancel->setObjectName(QStringLiteral("secondaryButton"));
    connect(cancel, &QPushButton::clicked, this, [this] { closeComposer(); });
    actions->addWidget(m_send);
    actions->addWidget(save_draft);
    actions->addWidget(cancel);
    actions->addStretch();
    compose_layout->addLayout(actions);
    m_send_hint = noteLabel(QString{}, composer);
    compose_layout->addWidget(m_send_hint);

    m_right_stack->addWidget(mail_view);
    m_right_stack->addWidget(composer);

    // ---- Wiring -------------------------------------------------------------
    const auto refresh_meter = [this] {
        const qint64 bytes = payloadBytes();
        m_size_meter->setValue(static_cast<int>(qMin<qint64>(bytes, kMaxMailTxBytes)));
        m_size_meter->setProperty("overLimit", bytes > kMaxMailTxBytes);
        m_size_meter->style()->unpolish(m_size_meter);
        m_size_meter->style()->polish(m_size_meter);
        m_size_label->setText(tr("%1 / %2 bytes")
            .arg(QLocale{}.toString(bytes))
            .arg(QLocale{}.toString(kMaxMailTxBytes)));
        const bool too_long = bytes > kMaxMailTxBytes;
        m_size_label->setStyleSheet(too_long ? QStringLiteral("color: #dc2626;") : QString{});
        updateGates();
    };
    connect(m_to, &QLineEdit::textChanged, this, [this, refresh_meter] {
        const bool separators = m_to->text().contains(QRegularExpression(QStringLiteral("[,;]")));
        m_to_hint->setText(tr("CYBOU mail v1 supports exactly one recipient."));
        m_to_hint->setVisible(separators);
        m_to_hint->setStyleSheet(QStringLiteral("color: #dc2626;"));
        refresh_meter();
    });
    connect(m_subject, &QLineEdit::textChanged, this, refresh_meter);
    connect(m_body, &QTextEdit::textChanged, this, refresh_meter);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { updateGates(); syncMailbox(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { updateGates(); syncMailbox(); });

    auto* sync_timer = new QTimer{this};
    connect(sync_timer, &QTimer::timeout, this, [this] { syncMailbox(); });
    sync_timer->start(3000);

    syncMailbox();
    refresh_meter();
}

void EmailPage::loadScreenshotFixture()
{
    Message message;
    message.id = QStringLiteral("screenshot-fixture");
    message.folder = FOLDER_INBOX;
    message.from = QStringLiteral("8a4c51f1d65c7e93b10d42e6a7295f8c");
    message.to = QStringLiteral("4d21b67a93c8520fbe16d4305a728c91");
    message.subject = tr("A protected update for the project team");
    message.body = tr("Hello,\n\nThe latest review is complete. The protocol evidence below is attached to this message, and the content remains protected on this device.\n\nBest,\nAlex");
    message.received = QDateTime::currentDateTime();
    message.read = true;
    message.finality = Finality::Final;
    message.has_evidence = true;
    m_messages = {message};
    m_folder = FOLDER_INBOX;
    m_current_message = -1;
    rebuildFolderList();
    rebuildMessageList();
    m_list->setCurrentRow(0);
}

void EmailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (!m_list) return;
    const int selected_row = m_list->currentRow();
    rebuildMessageList();
    if (selected_row >= 0 && selected_row < m_list->count()) {
        m_list->setCurrentRow(selected_row);
    }
}

qint64 EmailPage::payloadBytes() const
{
    // The strict MailTx size covers everything the protocol commits to:
    // subject and body, UTF-8 encoded. No attachments exist to count.
    return m_to->text().toUtf8().size() + m_subject->text().toUtf8().size() + m_body->toPlainText().toUtf8().size();
}

bool EmailPage::recipientWellFormed() const
{
    const QString to = m_to->text().trimmed();
    return !to.isEmpty() && !to.contains(QRegularExpression(QStringLiteral("[,;\\s]")));
}

QString EmailPage::finalityText(Finality finality)
{
    switch (finality) {
    case Finality::Draft: return tr("Local draft");
    case Finality::PendingFinality: return tr("Pending BFT finality");
    case Finality::Final: return tr("Final");
    }
    return {};
}

QString EmailPage::peerName(const QString& account_hex)
{
    if (account_hex.isEmpty()) return tr("unknown");
    if (account_hex.size() <= 16) return account_hex;
    return account_hex.left(12) + QStringLiteral("\u2026");
}

void EmailPage::rebuildFolderList()
{
    struct FolderDef { const char* name; Glyph glyph; Tint tint; };
    const FolderDef defs[]{
        {QT_TR_NOOP("Inbox"), Glyph::Inbox, Tint::Mint},
        {QT_TR_NOOP("Starred"), Glyph::Star, Tint::Amber},
        {QT_TR_NOOP("Sent"), Glyph::Send, Tint::Blue},
        {QT_TR_NOOP("Drafts"), Glyph::FileText, Tint::Violet},
        {QT_TR_NOOP("Archive"), Glyph::Archive, Tint::Indigo},
        {QT_TR_NOOP("Trash"), Glyph::Trash, Tint::Rose},
    };
    static_assert(std::size(defs) == FOLDER_COUNT);

    QSignalBlocker blocker{m_folders};
    const int previous = m_folders->currentRow();
    m_folders->clear();
    for (int folder = 0; folder < FOLDER_COUNT; ++folder) {
        int count = 0;
        for (const Message& message : m_messages) {
            if (message.folder != folder) continue;
            if (folder == FOLDER_INBOX) {
                if (!message.read) ++count;
            } else {
                ++count;
            }
        }
        auto* row_widget = new QFrame{m_folders};
        auto* row = new QHBoxLayout{row_widget};
        row->setContentsMargins(10, 7, 10, 7);
        row->setSpacing(10);
        auto* icon = new QLabel{row_widget};
        icon->setPixmap(glyphPixmap(defs[folder].glyph, {17, 17}, CybouTheme::color(tintInk(defs[folder].tint))));
        row->addWidget(icon, 0, Qt::AlignVCenter);
        auto* name = new QLabel{tr(defs[folder].name), row_widget};
        name->setObjectName(QStringLiteral("bodyText"));
        row->addWidget(name, 1);
        if (count > 0) {
            auto* badge = new QLabel{QString::number(count), row_widget};
            badge->setObjectName(QStringLiteral("pill"));
            badge->setProperty("tint", folder == FOLDER_INBOX ? "mint" : "neutral");
            row->addWidget(badge, 0, Qt::AlignVCenter);
        }
        auto* item = new QListWidgetItem{m_folders};
        item->setData(Qt::UserRole, folder);
        item->setSizeHint(row_widget->sizeHint().expandedTo(QSize{0, 36}));
        m_folders->setItemWidget(item, row_widget);
    }
    m_folders->setCurrentRow(previous >= 0 ? previous : 0);
}

void EmailPage::rebuildMessageList()
{
    const QString needle = m_search->text().trimmed().toLower();
    const int list_width = m_list->viewport()->width();
    const bool compact = list_width < 420;
    const bool narrow = list_width < 300;
    m_list->clear();
    bool has_rows = false;
    for (int i = 0; i < m_messages.size(); ++i) {
        const Message& message = m_messages.at(i);
        if (message.folder != m_folder) continue;
        if (!needle.isEmpty() &&
            !message.subject.toLower().contains(needle) &&
            !message.from.toLower().contains(needle) &&
            !message.to.toLower().contains(needle) &&
            !message.body.toLower().contains(needle)) {
            continue;
        }
        has_rows = true;
        const QString peer = m_folder == FOLDER_SENT || m_folder == FOLDER_DRAFTS ? message.to : message.from;

        auto* row = new QFrame{m_list};
        row->setMinimumWidth(0);
        auto* row_layout = new QHBoxLayout{row};
        row_layout->setContentsMargins(10, 9, 10, 9);
        row_layout->setSpacing(10);
        row_layout->addWidget(Avatar(peerName(peer).left(2), peerColor(peer), row, 36), 0, Qt::AlignTop);

        auto* main = new QVBoxLayout;
        main->setSpacing(2);
        const QString title_color = message.read
            ? CybouTheme::color(CybouTheme::TEXT_SECONDARY).name()
            : CybouTheme::color(CybouTheme::TEXT_PRIMARY).name();
        auto* name = new QLabel{peerName(peer), row};
        name->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;").arg(title_color));
        auto* subject = new QLabel{message.subject.isEmpty() ? tr("(no subject)") : message.subject, row};
        subject->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;").arg(title_color));
        subject->setWordWrap(true);
        subject->setMinimumWidth(0);
        subject->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        QString snippet_text = message.body;
        snippet_text.replace(QLatin1Char('\n'), QLatin1Char(' '));
        main->addWidget(name);
        main->addWidget(subject);
        if (!snippet_text.isEmpty() && !narrow) {
            auto* snippet = new QLabel{snippet_text, row};
            snippet->setObjectName(QStringLiteral("rowSub"));
            snippet->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
            snippet->setMaximumWidth(260);
            snippet->setWordWrap(true);
            snippet->setMinimumWidth(0);
            snippet->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            main->addWidget(snippet);
        }
        row_layout->addLayout(main, 1);

        if (!compact) {
            auto* side = new QVBoxLayout;
            side->setSpacing(4);
            auto* time = new QLabel{QLocale{}.toString(message.received, QLocale::ShortFormat), row};
            time->setObjectName(QStringLiteral("rowMeta"));
            time->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
            side->addWidget(time, 0, Qt::AlignRight);
            if (message.finality == Finality::Final) {
                auto* encrypted = new QLabel{tr("Encrypted"), row};
                encrypted->setObjectName(QStringLiteral("pill"));
                encrypted->setProperty("tint", "mint");
                side->addWidget(encrypted, 0, Qt::AlignRight);
            }
            row_layout->addLayout(side);
        }

        auto* item = new QListWidgetItem{m_list};
        item->setData(Qt::UserRole, i);
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 66}));
        m_list->setItemWidget(item, row);
    }
    if (!has_rows) {
        auto* item = new QListWidgetItem{m_list};
        item->setFlags(Qt::NoItemFlags);
        const bool indexed = m_folder == FOLDER_INBOX || m_folder == FOLDER_SENT || m_folder == FOLDER_DRAFTS;
        item->setText(indexed
            ? tr("No mail here yet.\nMessages appear once your identity is active and the Email service reaches BFT finality.")
            : tr("Nothing here.\nThis local folder fills up as the mailbox index grows."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 120});
        item->setData(Qt::UserRole, -1);
    }
}

void EmailPage::clearReader()
{
    m_current_message = -1;
    m_reader_hint->setVisible(true);
    m_reader_subject->clear();
    m_reader_avatar->clear();
    m_reader_peer->clear();
    m_reader_meta->clear();
    m_reader_body->setVisible(false);
    m_reader_body->clear();
    m_evidence_title->setVisible(false);
    m_evidence->setVisible(false);
    for (QPushButton* button : m_reply_buttons) button->setEnabled(false);
    m_star_button->setEnabled(false);
}

void EmailPage::showMessage(const Message& message)
{
    m_reader_hint->setVisible(false);
    m_reader_body->setVisible(true);
    m_reader_subject->setText(message.subject.isEmpty() ? tr("(no subject)") : message.subject);

    const bool outgoing = message.folder == FOLDER_SENT || message.folder == FOLDER_DRAFTS;
    const QString peer = outgoing ? message.to : message.from;
    m_reader_avatar->setPixmap(avatarPixmap(peerName(peer).left(2), peerColor(peer), 40));
    m_reader_peer->setText(peerName(peer));
    m_reader_meta->setText(outgoing
        ? tr("to %1 \u00b7 %2 \u00b7 %3").arg(peerName(message.to), finalityText(message.finality),
            QLocale{}.toString(message.received, QLocale::ShortFormat))
        : tr("to you \u00b7 %1 \u00b7 %2").arg(finalityText(message.finality),
            QLocale{}.toString(message.received, QLocale::ShortFormat)));
    m_reader_body->setText(message.body);

    // Chips mirror real message state; drafts carry no protocol guarantees.
    const bool final = message.finality == Finality::Final;
    m_chip_encrypted->setVisible(final);
    m_chip_verified->setVisible(final);
    m_chip_protected->setVisible(message.finality != Finality::Draft);
    m_star_button->setEnabled(final);

    // Evidence rows mirror the mail-evidence rules: a message is only
    // trustworthy once all four are verified; drafts carry none.
    m_evidence_title->setVisible(true);
    m_evidence->setVisible(true);
    const bool verified = final && message.has_evidence;
    if (m_evidence_states.size() >= 4) {
        // 0: Transaction inclusion proof
        m_evidence_states[0]->setText(verified ? tr("verified") : tr("pending"));
        m_evidence_states[0]->setObjectName(verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));

        // 1: BFT finality certificate
        m_evidence_states[1]->setText(verified ? tr("verified") : tr("pending"));
        m_evidence_states[1]->setObjectName(verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));

        // 2: Historical sender-key authorization
        // MailEvidenceBundle supplies current signing key but does not yet prove historical canonical state at block height
        m_evidence_states[2]->setText(tr("not available yet"));
        m_evidence_states[2]->setObjectName(QStringLiteral("neutralBadge"));

        // 3: Salted, domain-separated content commitment
        m_evidence_states[3]->setText(verified ? tr("verified") : tr("pending"));
        m_evidence_states[3]->setObjectName(verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));

        for (QLabel* state : m_evidence_states) {
            state->style()->unpolish(state);
            state->style()->polish(state);
        }
    }
    for (QPushButton* button : m_reply_buttons) button->setEnabled(message.folder == FOLDER_INBOX);
}

void EmailPage::updateGates()
{
    const auto& status = m_model->status();
    const bool identity_active = status.identity_state == CybouIdentityState::Active;
    const bool email_available = m_model->capabilities().email;

    m_banner->setVisible(!identity_active);
    m_banner_text->setText(identity_active
        ? QString{}
        : tr("Email needs an active CYBOU identity. Identity creation is a permissionless protocol operation \u2014 your keys never leave this device."));
    m_banner_action->setVisible(!identity_active);

    // Send gate: the button is honest about every missing precondition.
    QString gate_reason;
    if (!recipientWellFormed()) {
        gate_reason = tr("Enter exactly one recipient.");
    } else if (payloadBytes() > kMaxMailTxBytes) {
        gate_reason = tr("Message exceeds the strict MailTx size limit.");
    } else if (m_body->toPlainText().trimmed().isEmpty()) {
        gate_reason = tr("Write a message first.");
    } else if (!identity_active) {
        gate_reason = tr("Create an identity to send mail.");
    } else if (!email_available) {
        gate_reason = tr("Email service is planned \u2014 this draft stays local until MailTx is live.");
    }
    m_send->setEnabled(gate_reason.isEmpty());
    m_send_hint->setText(gate_reason);
    m_send_hint->setVisible(!gate_reason.isEmpty());
}

void EmailPage::openComposer()
{
    m_right_stack->setCurrentIndex(1);
    m_to->setFocus();
}

void EmailPage::openComposerWith(const QString& to, const QString& subject, const QString& body_prefix)
{
    m_to->setText(to);
    m_subject->setText(subject);
    m_body->setPlainText(body_prefix);
    openComposer();
}

void EmailPage::closeComposer()
{
    m_right_stack->setCurrentIndex(0);
}

void EmailPage::sendNow()
{
    // Reachable only once core reports the email capability. The client
    // hands the MailTx to the local node; finality arrives asynchronously.
    const QString to_str = m_to->text().trimmed();
    const QString subject_str = m_subject->text();
    const QString body_str = m_body->toPlainText();

    if (auto* service = m_model->mailService()) {
        const auto rec_u256 = cybou::ParseUint256UserHex(to_str.toStdString());
        if (!rec_u256 || rec_u256->IsNull()) {
            m_send_hint->setText(tr("Invalid recipient account ID."));
            m_send_hint->setVisible(true);
            return;
        }
        const cybou::AccountId recipient{*rec_u256};
        auto res = service->SendMail(recipient, subject_str.toStdString(), body_str.toStdString());
        if (!res) {
            m_send_hint->setText(tr("Send failed: %1").arg(QString::fromStdString(res.error_message)));
            m_send_hint->setVisible(true);
            return;
        }
    } else {
        Message outgoing;
        outgoing.id = QStringLiteral("local-%1").arg(m_next_id++);
        outgoing.folder = FOLDER_SENT;
        outgoing.to = to_str;
        outgoing.subject = subject_str;
        outgoing.body = body_str;
        outgoing.received = QDateTime::currentDateTime();
        outgoing.read = true;
        outgoing.finality = Finality::PendingFinality;
        m_messages.append(outgoing);
    }

    m_to->clear();
    m_subject->clear();
    m_body->clear();
    syncMailbox();
    closeComposer();
}

void EmailPage::saveDraft()
{
    const QString to_str = m_to->text().trimmed();
    const QString subject_str = m_subject->text();
    const QString body_str = m_body->toPlainText();

    if (body_str.trimmed().isEmpty() && subject_str.trimmed().isEmpty()) {
        closeComposer();
        return;
    }

    if (auto* service = m_model->mailService()) {
        service->SaveDraft(to_str.toStdString(), subject_str.toStdString(), body_str.toStdString());
    } else {
        Message draft;
        draft.id = QStringLiteral("local-%1").arg(m_next_id++);
        draft.folder = FOLDER_DRAFTS;
        draft.to = to_str;
        draft.subject = subject_str;
        draft.body = body_str;
        draft.received = QDateTime::currentDateTime();
        draft.read = true;
        draft.finality = Finality::Draft;
        m_messages.append(draft);
    }

    m_to->clear();
    m_subject->clear();
    m_body->clear();
    syncMailbox();
    closeComposer();
}

void EmailPage::syncMailbox()
{
    auto* service = m_model->mailService();
    if (!service) {
        rebuildFolderList();
        rebuildMessageList();
        return;
    }

    service->SyncMailbox();

    QList<Message> loaded_messages;
    const std::vector<cybou::MailFolder> folders = {
        cybou::MailFolder::INBOX,
        cybou::MailFolder::SENT,
        cybou::MailFolder::DRAFTS
    };

    for (const auto f : folders) {
        const auto items = service->GetMessages(f);
        for (const auto& item : items) {
            Message msg;
            msg.id = QString::fromStdString(item.mail_id.GetHex());
            msg.from = QString::fromStdString(item.sender.Value().GetHex());
            msg.to = QString::fromStdString(item.recipient.Value().GetHex());
            msg.subject = QString::fromStdString(item.subject);
            msg.body = QString::fromStdString(item.body);
            msg.received = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(item.timestamp));
            msg.read = item.read;
            msg.has_evidence = item.evidence_bundle.has_value();

            if (item.folder == cybou::MailFolder::INBOX) {
                msg.folder = FOLDER_INBOX;
            } else if (item.folder == cybou::MailFolder::SENT) {
                msg.folder = FOLDER_SENT;
            } else {
                msg.folder = FOLDER_DRAFTS;
            }

            if (item.finality == cybou::MailFinalityStatus::FINAL) {
                msg.finality = Finality::Final;
            } else if (item.finality == cybou::MailFinalityStatus::PENDING_FINALITY) {
                msg.finality = Finality::PendingFinality;
            } else {
                msg.finality = Finality::Draft;
            }

            loaded_messages.append(msg);
        }
    }

    bool changed = (m_messages.size() != loaded_messages.size());
    if (!changed) {
        for (int i = 0; i < m_messages.size(); ++i) {
            if (m_messages[i].id != loaded_messages[i].id ||
                m_messages[i].finality != loaded_messages[i].finality ||
                m_messages[i].read != loaded_messages[i].read ||
                m_messages[i].folder != loaded_messages[i].folder ||
                m_messages[i].has_evidence != loaded_messages[i].has_evidence) {
                changed = true;
                break;
            }
        }
    }

    if (changed) {
        const int current_row = m_list->currentRow();
        m_messages = std::move(loaded_messages);
        rebuildFolderList();
        rebuildMessageList();
        if (current_row >= 0 && current_row < m_list->count()) {
            m_list->setCurrentRow(current_row);
        }
    }
}
