// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/homepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

using namespace CybouUi;

namespace {

/** Deterministic accent color for a peer identifier (hex account / name). */
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

/** Remove and delete all items (and their widgets) from a layout. */
void clearLayout(QLayout* layout)
{
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) widget->deleteLater();
        delete item;
    }
}

} // namespace

HomePage::HomePage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
    std::function<void()> identity_requested, std::function<void()> wallet_requested,
    std::function<void()> mail_requested, std::function<void()> files_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_diagnostics_requested{std::move(diagnostics_requested)},
      m_identity_requested{std::move(identity_requested)},
      m_wallet_requested{std::move(wallet_requested)},
      m_mail_requested{std::move(mail_requested)},
      m_files_requested{std::move(files_requested)}
{
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(18);

    auto* left = new QVBoxLayout;
    left->setSpacing(16);
    left->addWidget(buildIdentityHero());

    auto* stats = new QVBoxLayout;
    stats->setSpacing(14);
    stats->addWidget(buildMailCard());
    stats->addWidget(buildFilesCard());
    left->addLayout(stats);
    left->addStretch();

    auto* activity = buildActivityCard();
    activity->setMinimumWidth(240);
    activity->setMaximumWidth(380);
    // Keep the activity card at its content height; the page scrolls when
    // the summary cards need more room instead of stretching an empty feed.
    activity->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    root->addLayout(left, 2);
    root->addWidget(activity, 0, Qt::AlignTop);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(3000);
    refresh();
}

QWidget* HomePage::buildIdentityHero()
{
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* layout = new QVBoxLayout{hero};
    layout->setContentsMargins(30, 26, 30, 26);
    layout->setSpacing(10);

    layout->addWidget(Eyebrow(tr("YOUR IDENTITY"), hero));
    m_identity_name = HeroTitle({}, hero, true);
    layout->addWidget(m_identity_name);
    m_identity_subtitle = HeroSubtitle({}, hero);
    layout->addWidget(m_identity_subtitle);

    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    m_chip_protected = Pill(tr("PQ identity signing"), Tint::Mint, hero);
    m_chip_ready = Pill(tr("Offline"), Tint::Blue, hero);
    chips->addWidget(m_chip_protected);
    chips->addWidget(m_chip_ready);
    chips->addStretch();
    layout->addLayout(chips);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_share_button = new QPushButton{tr("Share identity"), hero};
    m_share_button->setObjectName(QStringLiteral("primaryButton"));
    connect(m_share_button, &QPushButton::clicked, this, [this] {
        const QString account = m_model->status().account_id;
        if (account.isEmpty()) return;
        QGuiApplication::clipboard()->setText(account);
        m_share_button->setText(tr("Copied!"));
        QTimer::singleShot(1500, this, [this] { m_share_button->setText(tr("Share identity")); });
    });
    m_manage_button = new QPushButton{tr("Manage identity"), hero};
    m_manage_button->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_manage_button, &QPushButton::clicked, this, [this] { m_identity_requested(); });
    actions->addWidget(m_share_button);
    actions->addWidget(m_manage_button);
    actions->addStretch();
    layout->addLayout(actions);
    return hero;
}

QWidget* HomePage::buildMailCard()
{
    auto* card = Card(this);
    card->setMinimumWidth(0);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->addWidget(Chip(Glyph::Envelope, Tint::Mint, card, 38, 19));
    auto* title = new QLabel{tr("Mail"), card};
    title->setObjectName(QStringLiteral("serviceTitle"));
    header->addWidget(title, 0, Qt::AlignVCenter);
    header->addStretch();
    auto* open = IconButton(Glyph::ChevronRight, card);
    connect(open, &QToolButton::clicked, this, [this] { m_mail_requested(); });
    header->addWidget(open, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    m_mail_metric = new QLabel{card};
    m_mail_metric->setObjectName(QStringLiteral("metric"));
    layout->addWidget(m_mail_metric);
    layout->addWidget(MutedText(tr("Read and organize mail. Protected sending is not available yet."), card));

    m_mail_avatars = new QWidget{card};
    auto* avatar_row = new QHBoxLayout{m_mail_avatars};
    avatar_row->setContentsMargins(0, 2, 0, 0);
    avatar_row->setSpacing(0);
    layout->addWidget(m_mail_avatars);
    layout->addStretch();
    return card;
}

QWidget* HomePage::buildFilesCard()
{
    auto* card = Card(this);
    card->setMinimumWidth(0);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->addWidget(Chip(Glyph::Folder, Tint::Blue, card, 38, 19));
    auto* title = new QLabel{tr("Files"), card};
    title->setObjectName(QStringLiteral("serviceTitle"));
    header->addWidget(title, 0, Qt::AlignVCenter);
    header->addStretch();
    layout->addLayout(header);

    m_files_metric = new QLabel{card};
    m_files_metric->setObjectName(QStringLiteral("metric"));
    layout->addWidget(m_files_metric);
    m_files_caption = MutedText({}, card);
    layout->addWidget(m_files_caption);
    auto* open = new QPushButton{tr("Open Files"), card};
    open->setObjectName(QStringLiteral("secondaryButton"));
    connect(open, &QPushButton::clicked, this, [this] { m_files_requested(); });
    layout->addWidget(open);
    layout->addStretch();
    return card;
}

QWidget* HomePage::buildActivityCard()
{
    auto* card = Card(this);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(6);

    QLabel* link = nullptr;
    layout->addLayout(SectionHeader(tr("Recent activity"), {}, link, card));

    m_activity_rows = new QWidget{card};
    auto* rows = new QVBoxLayout{m_activity_rows};
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(2);
    layout->addWidget(m_activity_rows);

    m_activity_empty = MutedText(tr("No activity yet. Messages, payments and sync events will appear here."), card);
    m_activity_empty->setAlignment(Qt::AlignCenter);
    m_activity_empty->setMinimumHeight(80);
    layout->addWidget(m_activity_empty);
    return card;
}

void HomePage::refresh()
{
    const auto& status = m_model->status();
    const bool active = status.identity_state == CybouIdentityState::Active;

    // Identity hero.
    if (active) {
        m_identity_name->setText(status.primary_name.isEmpty() ? tr("Identity active") : status.primary_name);
        m_identity_subtitle->setText(tr("Your identity for CYBOU services. Mail sending and Files sync are not available yet."));
        m_chip_protected->setText(tr("PQ identity signing"));
        m_chip_ready->setText(status.online ? tr("Online") : tr("Offline"));
        m_chip_protected->setVisible(true);
        m_chip_ready->setVisible(true);
        m_share_button->setVisible(!status.account_id.isEmpty());
        m_manage_button->setText(tr("Manage identity"));
    } else {
        m_identity_name->setText(tr("Set up your CYBOU identity"));
        m_identity_subtitle->setText(tr("Create or restore an identity to use CYBOU. Mail sending and Backup are not available yet."));
        m_chip_protected->setVisible(false);
        m_chip_ready->setVisible(false);
        m_share_button->setVisible(false);
        m_manage_button->setText(tr("Create identity"));
    }

    // Mail stat: unread count and senders from the product model.
    QStringList senders;
    for (const auto& item : m_model->mailItems()) {
        if (item.folder == CybouMailFolder::Inbox && item.unread && !senders.contains(item.from_name))
            senders.append(item.from_name);
    }
    const int unread = m_model->unreadMailCount();
    m_mail_metric->setText(unread > 0 ? tr("%1 unread").arg(unread) : tr("No unread mail"));
    clearLayout(m_mail_avatars->layout());
    auto* avatar_row = qobject_cast<QHBoxLayout*>(m_mail_avatars->layout());
    avatar_row->setSpacing(-6);
    const int shown = qMin(senders.size(), 3);
    for (int i = 0; i < shown; ++i) {
        avatar_row->addWidget(Avatar(senders.at(i).left(2), peerColor(senders.at(i)), m_mail_avatars, 30));
    }
    if (senders.size() > shown) {
        avatar_row->addWidget(Avatar(QStringLiteral("+%1").arg(senders.size() - shown), CybouTheme::TEXT_MUTED, m_mail_avatars, 30));
    }
    avatar_row->addStretch();

    int file_count{0};
    for (const auto& file : m_model->fileItems()) {
        if (!file.folder && !file.trashed) ++file_count;
    }
    m_files_metric->setText(file_count == 0 ? tr("No files yet") : tr("%1 files").arg(file_count));
    m_files_caption->setText(CybouProduct::sizeText(status.storage_used));

    clearLayout(m_activity_rows->layout());
    auto* activity = qobject_cast<QVBoxLayout*>(m_activity_rows->layout());
    int rows_shown = 0;
    for (const auto& item : m_model->activity()) {
        if (rows_shown >= 6) break;
        activity->addWidget(ActivityRow(Glyph::Sparkles, Tint::Mint, item.title, item.subtitle,
            relTime(item.time), m_activity_rows));
        ++rows_shown;
    }
    m_activity_empty->setVisible(rows_shown == 0);
    m_activity_rows->setVisible(rows_shown > 0);
}
