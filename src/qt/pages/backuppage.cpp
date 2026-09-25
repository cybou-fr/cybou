// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/backuppage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QCheckBox>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

/** One "What's protected" category card with an honest state pill. */
QWidget* protectedCard(Glyph glyph, Tint tint, const QString& title, const QString& description,
    const QString& pill_text, Tint pill_tint, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(8);
    auto* header = new QHBoxLayout;
    header->addWidget(Chip(glyph, tint, card, 36, 18));
    auto* heading = new QLabel{title, card};
    heading->setObjectName(QStringLiteral("serviceTitle"));
    header->addWidget(heading, 0, Qt::AlignVCenter);
    header->addStretch();
    layout->addLayout(header);
    layout->addWidget(Pill(pill_text, pill_tint, card), 0, Qt::AlignLeft);
    layout->addWidget(MutedText(description, card));
    return card;
}

} // namespace

BackupPage::BackupPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent},
      m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    // ---- Hero + status panel ------------------------------------------------
    auto* hero_row = new QHBoxLayout;
    hero_row->setSpacing(16);

    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QVBoxLayout{hero};
    hero_layout->setContentsMargins(30, 26, 30, 26);
    hero_layout->setSpacing(10);
    hero_layout->addWidget(Eyebrow(tr("BACKUP"), hero));
    hero_layout->addWidget(HeroTitle(tr("Your data stays yours,\neven if a device is lost."), hero));
    hero_layout->addWidget(HeroSubtitle(tr("Your identity, messages, files and device data are encrypted locally before anything leaves this device \u2014 resilience comes from protocol replication, not from a single provider."), hero));
    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    chips->addWidget(Pill(tr("End-to-end encrypted"), Tint::Mint, hero));
    chips->addWidget(Pill(tr("Bound to your identity"), Tint::Blue, hero));
    chips->addWidget(Pill(tr("Replication planned"), Tint::Neutral, hero));
    chips->addStretch();
    hero_layout->addLayout(chips);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_backup_now = new QPushButton{tr("Back up now"), hero};
    m_backup_now->setObjectName(QStringLiteral("primaryButton"));
    m_backup_now->setIcon(QIcon{glyphPixmap(Glyph::CloudUp, {16, 16}, QColor{0xffffff})});
    connect(m_backup_now, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Backup is not wired to the node in this build. No backup was created."));
    });
    m_restore = new QPushButton{tr("Restore"), hero};
    m_restore->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_restore, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Backup is not wired to the node in this build. Nothing was restored."));
    });
    m_settings = new QPushButton{tr("Backup settings"), hero};
    m_settings->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_settings, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("The backup schedule and retention settings activate with the Backup service."));
    });
    actions->addWidget(m_backup_now);
    actions->addWidget(m_restore);
    actions->addWidget(m_settings);
    actions->addStretch();
    hero_layout->addLayout(actions);
    hero_layout->addStretch();
    hero_row->addWidget(hero, 3);

    auto* status = new QFrame{this};
    status->setObjectName(QStringLiteral("heroPanel"));
    auto* status_layout = new QVBoxLayout{status};
    status_layout->setContentsMargins(24, 20, 24, 20);
    status_layout->setSpacing(4);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(SectionTitle(tr("Backup status"), status));
        header->addStretch();
        m_status_pill = Pill(tr("Planned"), Tint::Neutral, status);
        header->addWidget(m_status_pill, 0, Qt::AlignVCenter);
        status_layout->addLayout(header);
        status_layout->addSpacing(6);

        auto add_row = [this, status, status_layout](Glyph glyph, const QString& title, QLabel*& value_out) {
            auto* row = new QHBoxLayout;
            row->setSpacing(10);
            auto* icon = new QLabel{status};
            icon->setPixmap(glyphPixmap(glyph, {16, 16}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
            row->addWidget(icon, 0, Qt::AlignVCenter);
            auto* label = new QLabel{title, status};
            label->setObjectName(QStringLiteral("mutedText"));
            row->addWidget(label, 1);
            auto* value = new QLabel{status};
            value->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
                .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
            value_out = value;
            row->addWidget(value, 0, Qt::AlignVCenter);
            status_layout->addLayout(row);
        };
        add_row(Glyph::History, tr("Last backup"), m_last_backup);
        add_row(Glyph::Clock, tr("Next backup"), m_next_backup);
        add_row(Glyph::Database, tr("Backup size"), m_backup_size);
        add_row(Glyph::Lock, tr("Encryption"), m_encryption);
        add_row(Glyph::ShieldCheck, tr("Status"), m_health);
    }
    hero_row->addWidget(status, 2);
    root->addLayout(hero_row);

    // ---- What's protected ----------------------------------------------------
    QLabel* link = nullptr;
    root->addLayout(SectionHeader(tr("What's protected"), tr("All local data"), link, this));
    auto* protected_row = new QHBoxLayout;
    protected_row->setSpacing(14);
    protected_row->addWidget(protectedCard(Glyph::User, Tint::Mint, tr("Identity"),
        tr("Your identity, settings and contacts."),
        tr("Protected"), Tint::Mint, this), 1);
    protected_row->addWidget(protectedCard(Glyph::Envelope, Tint::Blue, tr("Mail"),
        tr("Your messages and their local index."),
        tr("Local only"), Tint::Blue, this), 1);
    protected_row->addWidget(protectedCard(Glyph::File, Tint::Indigo, tr("Files"),
        tr("Your files and documents."),
        tr("Planned"), Tint::Neutral, this), 1);
    protected_row->addWidget(protectedCard(Glyph::Monitor, Tint::Violet, tr("Devices"),
        tr("Your device settings and app data."),
        tr("Planned"), Tint::Neutral, this), 1);
    root->addLayout(protected_row);

    // ---- Schedule + restore points -------------------------------------------
    auto* bottom_row = new QHBoxLayout;
    bottom_row->setSpacing(14);

    auto* schedule = Card(this);
    auto* schedule_layout = new QVBoxLayout{schedule};
    schedule_layout->setContentsMargins(22, 18, 22, 18);
    schedule_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Clock, Tint::Mint, schedule, 36, 18));
        auto* heading = new QLabel{tr("Backup schedule"), schedule};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        schedule_layout->addLayout(header);
        auto* toggle_row = new QHBoxLayout;
        m_auto_backup = new QCheckBox{tr("Automatic backup"), schedule};
        m_auto_backup->setEnabled(false);
        m_auto_backup->setToolTip(tr("Activates with the Backup service."));
        toggle_row->addWidget(m_auto_backup);
        toggle_row->addStretch();
        schedule_layout->addLayout(toggle_row);
        schedule_layout->addWidget(MutedText(tr("Your data will be backed up automatically once a day when you're connected."), schedule));
    }
    bottom_row->addWidget(schedule, 1);

    auto* points = Card(this);
    auto* points_layout = new QVBoxLayout{points};
    points_layout->setContentsMargins(22, 18, 22, 18);
    points_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::History, Tint::Blue, points, 36, 18));
        auto* heading = new QLabel{tr("Restore points"), points};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        points_layout->addLayout(header);
        auto* row = new QHBoxLayout;
        m_restore_points = new QLabel{points};
        m_restore_points->setObjectName(QStringLiteral("metric"));
        row->addWidget(m_restore_points);
        auto* view = new QPushButton{tr("View restore points"), points};
        view->setObjectName(QStringLiteral("secondaryButton"));
        connect(view, &QPushButton::clicked, this, [this] {
            QMessageBox::information(this, tr("Not available yet"),
                tr("No restore points exist \u2014 the Backup service is not live in this build."));
        });
        row->addWidget(view, 0, Qt::AlignVCenter);
        row->addStretch();
        points_layout->addLayout(row);
        points_layout->addWidget(MutedText(tr("You will be able to restore your data from previous backups if needed."), points));
    }
    bottom_row->addWidget(points, 1);
    root->addLayout(bottom_row);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });

    refresh();
}

void BackupPage::refresh()
{
    const auto& status = m_model->status();
    const bool identity_active = status.identity_state == CybouIdentityState::Active;
    const bool usable = identity_active && m_model->capabilities().backup;

    m_backup_now->setEnabled(usable);
    m_restore->setEnabled(usable);

    m_last_backup->setText(m_sets.isEmpty() ? tr("Never") :
        QLocale{}.toString(m_sets.last().at, QLocale::ShortFormat));
    m_next_backup->setText(usable ? tr("in 24 hours") : tr("\u2014"));
    qint64 total = 0;
    for (const BackupSet& set : m_sets) total += set.size;
    m_backup_size->setText(total > 0 ? tr("%1 bytes").arg(QLocale{}.toString(total)) : tr("0 bytes"));
    m_encryption->setText(tr("End-to-end"));
    m_health->setText(usable ? tr("Healthy") : tr("Waiting for the Backup service"));
    m_status_pill->setText(usable ? tr("Protected") : tr("Planned"));
    m_status_pill->setProperty("tint", usable ? "mint" : "neutral");
    m_status_pill->style()->unpolish(m_status_pill);
    m_status_pill->style()->polish(m_status_pill);
    m_restore_points->setText(tr("%1 restore points available").arg(m_sets.size()));
}
