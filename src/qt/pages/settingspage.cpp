// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/settingspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouui.h>
#include <qt/guiutil.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
#include <QVBoxLayout>
#include <QSettings>

#include <utility>

namespace {

using namespace CybouUi;

/** Clickable category card: [icon chip] title / subtitle ……… chevron. */
QFrame* CategoryCard(Glyph glyph, Tint tint, const QString& title, const QString& subtitle,
    QWidget* body, QWidget* parent)
{
    auto* card = new QFrame{parent};
    card->setObjectName(QStringLiteral("card"));
    auto* outer = new QVBoxLayout{card};
    outer->setContentsMargins(22, 20, 22, 20);
    outer->setSpacing(14);

    auto* header = new QPushButton{card};
    header->setFlat(true);
    header->setStyleSheet(QStringLiteral(
        "QPushButton { border: none; background: transparent; text-align: left; padding: 0; }"
        "QPushButton:hover { background: transparent; }"));
    auto* row = new QHBoxLayout{header};
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(16);

    auto* chip = Chip(glyph, tint, header, 46, 23);
    chip->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(chip, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    text->setContentsMargins(0, 1, 0, 0);
    auto* title_label = new QLabel{title, header};
    title_label->setObjectName(QStringLiteral("serviceTitle"));
    title_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* subtitle_label = new QLabel{subtitle, header};
    subtitle_label->setObjectName(QStringLiteral("mutedText"));
    subtitle_label->setWordWrap(true);
    subtitle_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    text->addWidget(title_label);
    text->addWidget(subtitle_label);
    row->addLayout(text, 1);

    auto* chevron = new QLabel{header};
    chevron->setFixedSize(20, 20);
    chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {18, 18}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
    chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(chevron, 0, Qt::AlignTop);
    outer->addWidget(header);

    // Inline expandable body with the real controls for the category.
    body->setParent(card);
    outer->addWidget(body);
    body->setVisible(false);
    QObject::connect(header, &QPushButton::clicked, body, [body] { body->setVisible(!body->isVisible()); });

    return card;
}

QLabel* Note(const QString& text, QWidget* parent)
{
    auto* note = new QLabel{text, parent};
    note->setObjectName(QStringLiteral("mutedText"));
    note->setWordWrap(true);
    return note;
}

QLabel* PlannedBadge(QWidget* parent)
{
    auto* badge = new QLabel{SettingsPage::tr("Planned"), parent};
    badge->setObjectName(QStringLiteral("pill"));
    badge->setProperty("tint", "neutral");
    return badge;
}

/** Spacer row used by categories whose controls live on another page. */
QFrame* PlannedBody(const QString& text, QWidget* parent)
{
    auto* body = new QFrame{parent};
    auto* layout = new QVBoxLayout{body};
    layout->setContentsMargins(62, 0, 8, 4);
    layout->setSpacing(8);
    layout->addWidget(Note(text, body));
    return body;
}

} // namespace

SettingsPage::SettingsPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
    QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_diagnostics_requested{std::move(diagnostics_requested)}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 30, 34, 32);
    root->setSpacing(22);

    // ---- Hero --------------------------------------------------------------
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(30, 26, 30, 26);
    hero_layout->setSpacing(20);

    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(10);
    hero_text->addWidget(CybouUi::Eyebrow(tr("SETTINGS"), hero));
    hero_text->addWidget(CybouUi::HeroTitle(tr("Your identity, your control."), hero, true));
    auto* hero_sub = CybouUi::HeroSubtitle(
        tr("Configure your CYBOU experience. Manage your privacy, protection, notifications "
           "and connected devices — all in one place."), hero);
    hero_sub->setMinimumWidth(420);
    hero_text->addWidget(hero_sub);
    hero_text->addStretch();
    hero_layout->addLayout(hero_text, 1);

    auto* gear = CybouUi::Chip(CybouUi::Glyph::Gear, CybouUi::Tint::Mint, hero, 96, 48);
    hero_layout->addWidget(gear, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    // ---- Category grid -----------------------------------------------------
    auto* grid = new QGridLayout;
    grid->setSpacing(18);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    root->addLayout(grid, 1);

    // General.
    auto* general_body = new QFrame{this};
    {
        auto* layout = new QVBoxLayout{general_body};
        layout->setContentsMargins(62, 0, 8, 4);
        layout->setSpacing(12);

        m_run_in_background = new QCheckBox{tr("Keep CYBOU running in the background when the window is closed"), general_body};
        m_run_in_background->setToolTip(tr("When enabled, closing the window hides CYBOU and the node keeps running. Use File -> Quit CYBOU to shut down."));
        connect(m_run_in_background, &QCheckBox::toggled, this, [this](bool checked) {
            QSettings{}.setValue(QStringLiteral("desktop/run_in_background"), checked);
        });
        layout->addWidget(m_run_in_background);

        auto* startup_row = new QHBoxLayout;
        auto* startup = new QCheckBox{tr("Start CYBOU with the operating system"), general_body};
        startup->setEnabled(false);
        startup_row->addWidget(startup);
        startup_row->addWidget(PlannedBadge(general_body), 0, Qt::AlignVCenter);
        startup_row->addStretch();
        layout->addLayout(startup_row);

        layout->addWidget(Note(tr("Language and startup options will be available in a future settings update."), general_body));
    }

    // Notification preferences (granular controls planned).
    auto* notifications_body = PlannedBody(
        tr("CYBOU keeps you informed about new mail, sync status and security events. "
           "Granular per-category notification controls are planned."), this);

    // Appearance (theme selection planned).
    auto* appearance_body = PlannedBody(
        tr("Theme, color mode and visual preferences to match your environment are planned."), this);

    // Privacy.
    auto* privacy_body = PlannedBody(
        tr("Mail and storage content is end-to-end encrypted and is not shared with third parties. "
           "Identity visibility controls are planned."), this);

    // Protection.
    auto* protection_body = PlannedBody(
        tr("Recovery and device authorization use post-quantum signatures by design. "
           "Security actions for your identity live on the Identity page."), this);

    // Trusted devices.
    auto* devices_body = PlannedBody(
        tr("Devices authorized for your identity are listed on the Identity page, "
           "where you can authorize or remove them."), this);

    // Recovery.
    auto* recovery_body = PlannedBody(
        tr("Your recovery phrase and portable vault are created during identity setup. "
           "Recovery actions live on the Identity page."), this);

    // Advanced: real network, storage and diagnostics controls.
    auto* advanced_body = new QFrame{this};
    {
        auto* layout = new QVBoxLayout{advanced_body};
        layout->setContentsMargins(62, 0, 8, 4);
        layout->setSpacing(12);

        m_proxy_enabled = new QCheckBox{tr("Connect through a SOCKS5 proxy"), advanced_body};
        m_proxy_enabled->setEnabled(false);
        layout->addWidget(m_proxy_enabled);

        auto* proxy_row = new QHBoxLayout;
        proxy_row->setSpacing(10);
        auto* host_label = new QLabel{tr("Host"), advanced_body};
        host_label->setObjectName(QStringLiteral("mutedText"));
        m_proxy_host = new QLineEdit{advanced_body};
        m_proxy_host->setPlaceholderText(tr("Proxy host"));
        m_proxy_host->setMinimumWidth(200);
        m_proxy_host->setEnabled(false);
        auto* port_label = new QLabel{tr("Port"), advanced_body};
        port_label->setObjectName(QStringLiteral("mutedText"));
        m_proxy_port = new QSpinBox{advanced_body};
        m_proxy_port->setRange(1, 65535);
        m_proxy_port->setFixedWidth(110);
        m_proxy_port->setEnabled(false);
        proxy_row->addWidget(host_label);
        proxy_row->addWidget(m_proxy_host, 1);
        proxy_row->addWidget(port_label);
        proxy_row->addWidget(m_proxy_port);
        layout->addLayout(proxy_row);

        m_listen = new QCheckBox{tr("Allow incoming connections"), advanced_body};
        m_listen->setToolTip(tr("Other nodes connect to you, strengthening the network. Disable only if you are behind a restrictive firewall."));
        m_listen->setEnabled(false);
        layout->addWidget(m_listen);
        layout->addWidget(Note(tr("Proxy and inbound connection settings will be available in a future networking update."), advanced_body));

        auto* dir_row = new QHBoxLayout;
        m_data_directory = new QLabel{advanced_body};
        m_data_directory->setObjectName(QStringLiteral("bodyText"));
        m_data_directory->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_data_directory->setWordWrap(true);
        dir_row->addWidget(m_data_directory, 1);
        auto* open_dir = new QPushButton{tr("Open folder"), advanced_body};
        open_dir->setObjectName(QStringLiteral("secondaryButton"));
        connect(open_dir, &QPushButton::clicked, this, [this] {
            const QString dir = m_model->status().data_directory;
            if (!dir.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });
        dir_row->addWidget(open_dir, 0, Qt::AlignTop);
        layout->addLayout(dir_row);

        auto* tools_row = new QHBoxLayout;
        auto* diagnostics = new QPushButton{tr("Open diagnostics"), advanced_body};
        diagnostics->setObjectName(QStringLiteral("secondaryButton"));
        connect(diagnostics, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
        auto* debug_log = new QPushButton{tr("Open debug log"), advanced_body};
        debug_log->setObjectName(QStringLiteral("secondaryButton"));
        connect(debug_log, &QPushButton::clicked, this, [] { GUIUtil::openDebugLogfile(); });
        tools_row->addWidget(diagnostics);
        tools_row->addWidget(debug_log);
        tools_row->addStretch();
        layout->addLayout(tools_row);
    }

    grid->addWidget(CategoryCard(CybouUi::Glyph::User, CybouUi::Tint::Blue,
        tr("General"), tr("Account information, language, startup settings and application preferences."),
        general_body, this), 0, 0);
    grid->addWidget(CategoryCard(CybouUi::Glyph::Bell, CybouUi::Tint::Amber,
        tr("Notification preferences"), tr("Choose what you're notified about, and how. Email, sync, security, and system updates."),
        notifications_body, this), 0, 1);
    grid->addWidget(CategoryCard(CybouUi::Glyph::Palette, CybouUi::Tint::Violet,
        tr("Appearance"), tr("Theme, color mode, and visual preferences to match your environment."),
        appearance_body, this), 1, 0);
    grid->addWidget(CategoryCard(CybouUi::Glyph::ShieldCheck, CybouUi::Tint::Indigo,
        tr("Privacy"), tr("Control over personal data, identity visibility and metadata sharing."),
        privacy_body, this), 1, 1);
    grid->addWidget(CategoryCard(CybouUi::Glyph::Lock, CybouUi::Tint::Rose,
        tr("Protection"), tr("Security settings, encryption options and authentication methods."),
        protection_body, this), 2, 0);
    grid->addWidget(CategoryCard(CybouUi::Glyph::Monitor, CybouUi::Tint::Mint,
        tr("Trusted devices"), tr("View and manage devices connected to your identity."),
        devices_body, this), 2, 1);
    grid->addWidget(CategoryCard(CybouUi::Glyph::CloudUp, CybouUi::Tint::Blue,
        tr("Recovery"), tr("Backup settings, recovery options and identity restoration."),
        recovery_body, this), 3, 0);
    grid->addWidget(CategoryCard(CybouUi::Glyph::Sliders, CybouUi::Tint::Neutral,
        tr("Advanced"), tr("Network, synchronization, developer options and diagnostic tools."),
        advanced_body, this), 3, 1);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    refresh();
}

void SettingsPage::refresh()
{
    const QSignalBlocker background_blocker{m_run_in_background};
    m_run_in_background->setChecked(QSettings{}.value(QStringLiteral("desktop/run_in_background"), false).toBool());

    m_data_directory->setText(m_model->status().data_directory.isEmpty()
        ? tr("Available after node startup")
        : m_model->status().data_directory);
}
