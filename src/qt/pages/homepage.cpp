// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/homepage.h>

#include <cybou/mail_service.h>
#include <cybou/wallet_service.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSysInfo>
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
    std::function<void()> identity_requested, std::function<void()> wallet_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_diagnostics_requested{std::move(diagnostics_requested)},
      m_identity_requested{std::move(identity_requested)},
      m_wallet_requested{std::move(wallet_requested)}
{
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(18);

    auto* left = new QVBoxLayout;
    left->setSpacing(16);
    left->addWidget(buildIdentityHero());

    auto* stats = new QHBoxLayout;
    stats->setSpacing(14);
    stats->addWidget(buildMailCard(), 1);
    stats->addWidget(buildFilesCard(), 1);
    stats->addWidget(buildDevicesCard(), 1);
    left->addLayout(stats);
    left->addStretch();

    root->addLayout(left, 3);
    root->addWidget(buildActivityCard(), 2);

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
    m_chip_protected = Pill(tr("Protected"), Tint::Mint, hero);
    m_chip_ready = Pill(tr("Ready to use"), Tint::Blue, hero);
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
    connect(open, &QToolButton::clicked, this, [] {});
    header->addWidget(open, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    m_mail_metric = new QLabel{card};
    m_mail_metric->setObjectName(QStringLiteral("metric"));
    layout->addWidget(m_mail_metric);
    layout->addWidget(MutedText(tr("Private and encrypted communication."), card));

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
    m_files_meter = new QProgressBar{card};
    m_files_meter->setObjectName(QStringLiteral("usageMeter"));
    m_files_meter->setRange(0, 100);
    m_files_meter->setValue(0);
    m_files_meter->setTextVisible(false);
    layout->addWidget(m_files_meter);
    m_files_caption = MutedText({}, card);
    layout->addWidget(m_files_caption);
    layout->addStretch();
    return card;
}

QWidget* HomePage::buildDevicesCard()
{
    auto* card = Card(this);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->addWidget(Chip(Glyph::Monitor, Tint::Indigo, card, 38, 19));
    auto* title = new QLabel{tr("Devices"), card};
    title->setObjectName(QStringLiteral("serviceTitle"));
    header->addWidget(title, 0, Qt::AlignVCenter);
    header->addStretch();
    layout->addLayout(header);

    m_devices_metric = new QLabel{card};
    m_devices_metric->setObjectName(QStringLiteral("metric"));
    layout->addWidget(m_devices_metric);
    layout->addWidget(MutedText(tr("All your devices are synced and protected."), card));

    m_device_rows = new QWidget{card};
    auto* rows = new QVBoxLayout{m_device_rows};
    rows->setContentsMargins(0, 2, 0, 0);
    rows->setSpacing(8);
    layout->addWidget(m_device_rows);
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
        m_identity_subtitle->setText(tr("Your CYBOU identity for messages, payments and files."));
        m_chip_protected->setVisible(true);
        m_chip_ready->setVisible(true);
        m_share_button->setVisible(!status.account_id.isEmpty());
        m_manage_button->setText(tr("Manage identity"));
    } else {
        m_identity_name->setText(tr("Set up your CYBOU identity"));
        m_identity_subtitle->setText(tr("One identity for encrypted mail, storage, payments and backup — registered by a permissionless protocol operation, controlled by keys that stay on this device."));
        m_chip_protected->setVisible(false);
        m_chip_ready->setVisible(false);
        m_share_button->setVisible(false);
        m_manage_button->setText(tr("Create identity"));
    }

    // Mail stat: unread from the local mailbox.
    QStringList senders;
    if (auto* mail = m_model->mailService()) {
        for (const auto& item : mail->GetMessages(cybou::MailFolder::INBOX)) {
            if (!item.read) {
                const QString from = QString::fromStdString(item.sender.Value().GetHex());
                if (!senders.contains(from)) senders.append(from);
            }
        }
    }
    const int unread = senders.isEmpty() ? 0 : static_cast<int>(m_model->mailService()->GetUnreadCount());
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

    // Files stat: Object Storage is not live yet — the meter stays at zero
    // until the node reports real usage.
    m_files_metric->setText(tr("No files yet"));
    m_files_meter->setValue(0);
    m_files_caption->setText(tr("Encrypted storage for your files and documents."));

    // Devices: this node only (device authorization lands with the vault sync).
    clearLayout(m_device_rows->layout());
    auto* rows = qobject_cast<QVBoxLayout*>(m_device_rows->layout());
    rows->addWidget(ActivityRow(Glyph::Monitor, Tint::Indigo, QSysInfo::machineHostName(),
        tr("This device \u00b7 Active now"), {}, m_device_rows));
    m_devices_metric->setText(tr("1 device"));

    // Recent activity: unread mail + finalized ledger entries + sync.
    clearLayout(m_activity_rows->layout());
    auto* activity = qobject_cast<QVBoxLayout*>(m_activity_rows->layout());
    int rows_shown = 0;
    if (auto* mail = m_model->mailService()) {
        const auto inbox = mail->GetMessages(cybou::MailFolder::INBOX);
        for (int i = inbox.size() - 1; i >= 0 && rows_shown < 4; --i) {
            const auto& item = inbox.at(i);
            const QString from = QString::fromStdString(item.sender.Value().GetHex());
            activity->addWidget(ActivityRow(Glyph::Envelope, Tint::Mint,
                tr("Message from %1").arg(from.left(12) + QStringLiteral("…")),
                item.subject.empty() ? tr("(no subject)") : QString::fromStdString(item.subject),
                relTime(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(item.timestamp))),
                m_activity_rows, !item.read));
            ++rows_shown;
        }
    }
    if (auto* wallet = m_model->walletService()) {
        const auto entries = wallet->GetLedgerEntries();
        for (int i = entries.size() - 1; i >= 0 && rows_shown < 7; --i) {
            const auto& entry = entries.at(i);
            const QString when = relTime(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(entry.timestamp)));
            switch (entry.kind) {
            case cybou::WalletEntryKind::PAYMENT: {
                const QString party = entry.counterparty.IsNull() ? QString{}
                    : QString::fromStdString(entry.counterparty.Value().GetHex()).left(12) + QStringLiteral("…");
                activity->addWidget(ActivityRow(entry.amount >= 0 ? Glyph::ArrowDownLeft : Glyph::ArrowUpRight,
                    entry.amount >= 0 ? Tint::Mint : Tint::Indigo,
                    entry.amount >= 0 ? tr("Received CYBOU from %1").arg(party) : tr("Sent CYBOU to %1").arg(party),
                    cybouAmountText(static_cast<quint64>(std::abs(entry.amount))), when, m_activity_rows));
                break;
            }
            case cybou::WalletEntryKind::ONBOARDING_BONUS:
                activity->addWidget(ActivityRow(Glyph::Sparkles, Tint::Amber, tr("Onboarding bonus"),
                    cybouAmountText(static_cast<quint64>(entry.amount)), when, m_activity_rows));
                break;
            case cybou::WalletEntryKind::MAIL_FEE:
                activity->addWidget(ActivityRow(Glyph::Envelope, Tint::Blue, tr("Mail service fee"),
                    cybouAmountText(static_cast<quint64>(std::abs(entry.amount))), when, m_activity_rows));
                break;
            case cybou::WalletEntryKind::LOCK_TO_SYSTEM:
                activity->addWidget(ActivityRow(Glyph::Lock, Tint::Violet, tr("Locked to System Balance"),
                    cybouAmountText(static_cast<quint64>(entry.amount)), when, m_activity_rows));
                break;
            }
            ++rows_shown;
        }
    }
    if (m_model->lastSync().isValid()) {
        activity->addWidget(ActivityRow(Glyph::Refresh, Tint::Neutral, tr("Devices synced"),
            status.last_finalized_height >= 0
                ? tr("Finalized height %1").arg(status.last_finalized_height)
                : tr("All data is up to date"),
            relTime(m_model->lastSync()), m_activity_rows));
        ++rows_shown;
    }
    m_activity_empty->setVisible(rows_shown == 0);
    m_activity_rows->setVisible(rows_shown > 0);
}
