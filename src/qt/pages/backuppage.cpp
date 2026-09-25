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

/** iOS-style switch checkbox (sketch: the Automatic backup toggle). */
QCheckBox* SwitchCheckBox(const QString& text, QWidget* parent)
{
    auto* box = new QCheckBox{text, parent};
    box->setStyleSheet(QStringLiteral(
        "QCheckBox { background: transparent; border: none; color: %1; font-size: 14px; font-weight: 600; }"
        "QCheckBox::indicator { width: 40px; height: 22px; border-radius: 11px; background: %2; }"
        "QCheckBox::indicator:checked { background: %3; }"
        "QCheckBox::indicator::handle { width: 18px; height: 18px; margin: 2px; border-radius: 9px; background: #ffffff; }"
        "QCheckBox::indicator:checked::handle { margin-left: 20px; }")
        .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name())
        .arg(CybouTheme::color(CybouTheme::BORDER).name())
        .arg(CybouTheme::color(CybouTheme::MINT).name()));
    return box;
}

/** Status row: [icon] bold title / muted subtitle ……… value › */
void statusRow(QLayout* layout, Glyph glyph, const QString& title, const QString& subtitle,
    QLabel*& value_out, QWidget* parent)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* icon = new QLabel{parent};
    icon->setPixmap(glyphPixmap(glyph, {18, 18}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
    row->addWidget(icon, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    auto* t = new QLabel{title, parent};
    t->setObjectName(QStringLiteral("rowTitle"));
    t->setStyleSheet(QStringLiteral("font-weight: 700; background: transparent; border: none;"));
    text->addWidget(t);
    if (!subtitle.isEmpty()) {
        auto* s = new QLabel{subtitle, parent};
        s->setObjectName(QStringLiteral("rowSub"));
        s->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        text->addWidget(s);
    }
    row->addLayout(text, 1);
    auto* value = new QLabel{parent};
    value->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
        .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    value_out = value;
    row->addWidget(value, 0, Qt::AlignTop);
    auto* chevron = new QLabel{parent};
    chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {14, 14}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
    row->addWidget(chevron, 0, Qt::AlignTop);
    layout->addItem(row);
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
    auto* hero_outer = new QHBoxLayout{hero};
    hero_outer->setContentsMargins(30, 26, 30, 26);
    hero_outer->setSpacing(20);
    auto* hero_layout = new QVBoxLayout;
    hero_layout->setSpacing(10);
    hero_layout->addWidget(Eyebrow(tr("BACKUP"), hero));
    m_hero_title = HeroTitle(tr("Backup protection is coming."), hero);
    hero_layout->addWidget(m_hero_title);
    m_hero_subtitle = HeroSubtitle(tr("Your identity, messages, files and device data will be safely backed up and ready to recover once the Backup service activates."), hero);
    hero_layout->addWidget(m_hero_subtitle);
    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    m_hero_state_pill = Pill(tr("Planned"), Tint::Neutral, hero);
    chips->addWidget(m_hero_state_pill);
    chips->addWidget(Pill(tr("End-to-end encrypted by design"), Tint::Blue, hero));
    chips->addWidget(Pill(tr("Ready when Backup activates"), Tint::Indigo, hero));
    chips->addStretch();
    hero_layout->addLayout(chips);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_backup_now = new QPushButton{tr("Backup now"), hero};
    m_backup_now->setObjectName(QStringLiteral("primaryButton"));
    m_backup_now->setIcon(QIcon{glyphPixmap(Glyph::CloudUp, {16, 16}, QColor{0xffffff})});
    connect(m_backup_now, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Backup is not wired to the node in this build. No backup was created."));
    });
    m_restore = new QPushButton{tr("Restore"), hero};
    m_restore->setObjectName(QStringLiteral("secondaryButton"));
    m_restore->setIcon(QIcon{glyphPixmap(Glyph::Refresh, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    connect(m_restore, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Backup is not wired to the node in this build. Nothing was restored."));
    });
    m_settings = new QPushButton{tr("Backup settings"), hero};
    m_settings->setObjectName(QStringLiteral("secondaryButton"));
    m_settings->setIcon(QIcon{glyphPixmap(Glyph::Gear, {16, 16}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))});
    connect(m_settings, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("The backup schedule and retention settings activate with the Backup service."));
    });
    actions->addWidget(m_backup_now);
    actions->addWidget(m_restore);
    actions->addWidget(m_settings);
    actions->addStretch();
    hero_layout->addLayout(actions);
    auto* recovery_link = new QLabel{tr("View recovery options \u203a"), hero};
    recovery_link->setObjectName(QStringLiteral("sectionLink"));
    recovery_link->setCursor(Qt::PointingHandCursor);
    hero_layout->addWidget(recovery_link, 0, Qt::AlignLeft);
    hero_layout->addStretch();
    hero_outer->addLayout(hero_layout, 1);

    // Decorative shield emblem on the right of the hero (sketch).
    auto* emblem = new QLabel{hero};
    emblem->setFixedSize(120, 120);
    emblem->setAlignment(Qt::AlignCenter);
    emblem->setStyleSheet(QStringLiteral(
        "background: qradialgradient(cx:0.5, cy:0.4, radius:0.9, stop:0 #d9f6e7, stop:1 #b9ecd6);"
        "border-radius: 60px;"));
    emblem->setPixmap(glyphPixmap(Glyph::ShieldCheck, {56, 56}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    hero_outer->addWidget(emblem, 0, Qt::AlignVCenter);
    hero_row->addWidget(hero, 3);

    auto* status = new QFrame{this};
    status->setObjectName(QStringLiteral("heroPanel"));
    auto* status_layout = new QVBoxLayout{status};
    status_layout->setContentsMargins(24, 20, 24, 20);
    status_layout->setSpacing(14);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(SectionTitle(tr("Backup status"), status));
        header->addStretch();
        m_status_pill = Pill(tr("Planned"), Tint::Neutral, status);
        header->addWidget(m_status_pill, 0, Qt::AlignVCenter);
        status_layout->addLayout(header);

        statusRow(status_layout, Glyph::CloudUp, tr("Last backup completed"),
            tr("Scheduled backups will be encrypted and stored safely once active."), m_last_backup, status);
        statusRow(status_layout, Glyph::Clock, tr("Next backup"), {}, m_next_backup, status);
        statusRow(status_layout, Glyph::Database, tr("Backup size"), {}, m_backup_size, status);
        statusRow(status_layout, Glyph::Lock, tr("Encryption"), {}, m_encryption, status);
        statusRow(status_layout, Glyph::ShieldCheck, tr("Status"), {}, m_health, status);
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
        tr("Local vault only"), Tint::Mint, this), 1);
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

    // ---- Schedule + restore points + activity --------------------------------
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
        m_auto_backup = SwitchCheckBox(tr("Automatic backup"), schedule);
        m_auto_backup->setEnabled(false);
        m_auto_backup->setToolTip(tr("Activates with the Backup service."));
        toggle_row->addWidget(m_auto_backup);
        toggle_row->addStretch();
        schedule_layout->addLayout(toggle_row);
        schedule_layout->addWidget(MutedText(tr("Your data is backed up automatically once a day when you're connected."), schedule));
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
        auto* view = new QLabel{tr("View restore points \u203a"), points};
        view->setObjectName(QStringLiteral("sectionLink"));
        header->addWidget(view, 0, Qt::AlignVCenter);
        header->addStretch();
        points_layout->addLayout(header);
        m_restore_points = new QLabel{points};
        m_restore_points->setObjectName(QStringLiteral("metric"));
        points_layout->addWidget(m_restore_points);
        points_layout->addWidget(MutedText(tr("You can restore your data from previous backups if needed."), points));
    }
    bottom_row->addWidget(points, 1);

    auto* activity = Card(this);
    auto* activity_layout = new QVBoxLayout{activity};
    activity_layout->setContentsMargins(22, 18, 22, 18);
    activity_layout->setSpacing(10);
    {
        QLabel* view_all = nullptr;
        auto* header = SectionHeader(tr("Recent backup activity"), tr("View all \u203a"), view_all, activity);
        activity_layout->addLayout(header);
        auto* empty_title = new QLabel{tr("No backup activity yet."), activity};
        empty_title->setObjectName(QStringLiteral("bodyText"));
        empty_title->setStyleSheet(QStringLiteral("font-weight: 600; background: transparent; border: none;"));
        activity_layout->addWidget(empty_title);
        activity_layout->addWidget(MutedText(tr("Backup events will appear here once the Backup service is live."), activity));
        activity_layout->addStretch();
    }
    bottom_row->addWidget(activity, 1);
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
    if (m_settings) m_settings->setEnabled(usable);
    for (auto* btn : findChildren<QPushButton*>()) {
        btn->setEnabled(usable);
    }

    m_last_backup->setText(m_sets.isEmpty() ? tr("Never") : relTime(m_sets.last().at));
    m_next_backup->setText(usable ? tr("in 24 hours") : tr("\u2014"));
    qint64 total = 0;
    for (const BackupSet& set : m_sets) total += set.size;
    m_backup_size->setText(total > 0 ? tr("%1 bytes").arg(QLocale{}.toString(total)) : tr("0 bytes"));
    m_encryption->setText(tr("End-to-end encrypted"));
    m_health->setText(usable ? tr("Healthy") : tr("Waiting\u2026"));
    m_status_pill->setText(usable ? tr("Protected") : tr("Planned"));
    m_status_pill->setProperty("tint", usable ? "mint" : "neutral");
    m_status_pill->style()->unpolish(m_status_pill);
    m_status_pill->style()->polish(m_status_pill);

    if (m_hero_title && m_hero_subtitle) {
        if (usable) {
            m_hero_title->setText(tr("Your data is protected."));
            m_hero_subtitle->setText(tr("Your identity, messages, files and device data are safely backed up and ready to recover, whenever you need them."));
        } else {
            m_hero_title->setText(tr("Backup protection is coming."));
            m_hero_subtitle->setText(tr("Your identity, messages, files and device data will be safely backed up and ready to recover once the Backup service activates."));
        }
    }
    if (m_hero_state_pill) {
        m_hero_state_pill->setText(usable ? tr("Protected") : tr("Planned"));
        m_hero_state_pill->setProperty("tint", usable ? "mint" : "neutral");
        m_hero_state_pill->style()->unpolish(m_hero_state_pill);
        m_hero_state_pill->style()->polish(m_hero_state_pill);
    }

    m_restore_points->setText(tr("%1 restore points available").arg(m_sets.size()));
}
