// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/homepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace {

QFrame* Card(QWidget* parent)
{
    auto* card = new QFrame{parent};
    card->setObjectName("card");
    return card;
}

/** Small rounded chip with a CYBOU monochrome line icon. */
QLabel* IconChip(CybouTheme::NavIcon icon, QWidget* parent)
{
    auto* chip = new QLabel{parent};
    chip->setObjectName("iconChip");
    chip->setFixedSize(40, 40);
    chip->setAlignment(Qt::AlignCenter);
    chip->setPixmap(CybouTheme::iconPixmap(icon, {22, 22}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    return chip;
}

QFrame* ServiceCard(const QString& title, const QString& description, CybouTheme::NavIcon icon, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(8);
    auto* header = new QHBoxLayout;
    header->addWidget(IconChip(icon, card));
    auto* heading = new QLabel{title, card};
    heading->setObjectName("serviceTitle");
    heading->setAlignment(Qt::AlignVCenter);
    header->addWidget(heading);
    header->addStretch();
    auto* badge = new QLabel{QObject::tr("Planned"), card};
    badge->setObjectName("neutralBadge");
    header->addWidget(badge, 0, Qt::AlignVCenter);
    auto* body = new QLabel{description, card};
    body->setObjectName("mutedText");
    body->setWordWrap(true);
    layout->addLayout(header);
    layout->addWidget(body);
    layout->addStretch();
    return card;
}

} // namespace

HomePage::HomePage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
    std::function<void()> identity_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_diagnostics_requested{std::move(diagnostics_requested)},
      m_identity_requested{std::move(identity_requested)}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    // Hero: typography plus a soft mint accent — no decorative artwork.
    auto* hero = new QFrame{this};
    hero->setObjectName("card");
    hero->setMinimumHeight(190);
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(34, 26, 34, 26);
    hero_layout->setSpacing(24);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(8);
    auto* eyebrow = new QLabel{tr("WELCOME TO CYBOU"), hero};
    eyebrow->setObjectName("eyebrow");
    auto* title = new QLabel{tr("Protected communication,\nunder your control."), hero};
    title->setObjectName("heroTitle");
    auto* subtitle = new QLabel{tr("Sovereign communication infrastructure built around identity, Email, Storage and Backup."), hero};
    subtitle->setObjectName("heroSubtitle");
    subtitle->setWordWrap(true);
    hero_text->addWidget(eyebrow);
    hero_text->addWidget(title);
    hero_text->addWidget(subtitle);
    hero_text->addStretch();
    auto* accent = new QLabel{hero};
    accent->setObjectName("heroAccent");
    accent->setFixedSize(150, 150);
    accent->setPixmap(QPixmap{QStringLiteral(":/icons/cybou")}.scaled(84, 84, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    accent->setAlignment(Qt::AlignCenter);
    hero_layout->addLayout(hero_text, 1);
    hero_layout->addWidget(accent, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    auto* primary_row = new QHBoxLayout;
    primary_row->setSpacing(16);

    auto* network_card = Card(this);
    auto* network_layout = new QVBoxLayout{network_card};
    network_layout->setContentsMargins(24, 20, 24, 20);
    network_layout->setSpacing(10);
    auto* network_header = new QHBoxLayout;
    network_header->addWidget(IconChip(CybouTheme::NavIcon::Network, network_card));
    auto* network_label = new QLabel{tr("Network"), network_card};
    network_label->setObjectName("cardLabel");
    network_label->setAlignment(Qt::AlignVCenter);
    network_header->addWidget(network_label);
    network_header->addStretch();
    network_layout->addLayout(network_header);
    auto* network_title_row = new QHBoxLayout;
    m_network_name = new QLabel{network_card};
    m_network_name->setObjectName("cardTitle");
    m_node_state = new QLabel{network_card};
    m_node_state->setObjectName("statusBadge");
    network_title_row->addWidget(m_network_name);
    network_title_row->addStretch();
    network_title_row->addWidget(m_node_state);
    auto* metrics = new QHBoxLayout;
    auto* peers_box = new QVBoxLayout;
    auto* peers_label = new QLabel{tr("Connections"), network_card};
    peers_label->setObjectName("metricCaption");
    m_peer_count = new QLabel{network_card};
    m_peer_count->setObjectName("metric");
    peers_box->addWidget(peers_label);
    peers_box->addWidget(m_peer_count);
    auto* height_box = new QVBoxLayout;
    auto* height_label = new QLabel{tr("Current height"), network_card};
    height_label->setObjectName("metricCaption");
    m_height = new QLabel{network_card};
    m_height->setObjectName("metric");
    height_box->addWidget(height_label);
    height_box->addWidget(m_height);
    metrics->addLayout(peers_box);
    metrics->addSpacing(48);
    metrics->addLayout(height_box);
    metrics->addStretch();
    network_layout->addLayout(network_title_row);
    network_layout->addSpacing(8);
    network_layout->addLayout(metrics);

    auto* identity_card = Card(this);
    auto* identity_layout = new QVBoxLayout{identity_card};
    identity_layout->setContentsMargins(24, 20, 24, 20);
    identity_layout->setSpacing(8);
    auto* identity_header = new QHBoxLayout;
    identity_header->addWidget(IconChip(CybouTheme::NavIcon::Identity, identity_card));
    auto* identity_label = new QLabel{tr("Identity"), identity_card};
    identity_label->setObjectName("cardLabel");
    identity_label->setAlignment(Qt::AlignVCenter);
    identity_header->addWidget(identity_label);
    identity_header->addStretch();
    identity_layout->addLayout(identity_header);
    m_identity_state = new QLabel{identity_card};
    m_identity_state->setObjectName("cardTitle");
    auto* identity_body = new QLabel{tr("A CYBOU identity will provide protocol-native access to communication services."), identity_card};
    identity_body->setObjectName("mutedText");
    identity_body->setWordWrap(true);
    auto* identity_button = new QPushButton{tr("View identity"), identity_card};
    identity_button->setObjectName("secondaryButton");
    connect(identity_button, &QPushButton::clicked, this, [this] { m_identity_requested(); });
    identity_layout->addWidget(m_identity_state);
    identity_layout->addWidget(identity_body);
    identity_layout->addStretch();
    identity_layout->addWidget(identity_button, 0, Qt::AlignLeft);

    primary_row->addWidget(network_card, 1);
    primary_row->addWidget(identity_card, 1);
    root->addLayout(primary_row);

    auto* services_header = new QHBoxLayout;
    auto* services_title = new QLabel{tr("Services"), this};
    services_title->setObjectName("sectionTitle");
    auto* services_note = new QLabel{tr("Some services will become available in a future version."), this};
    services_note->setObjectName("mutedText");
    services_header->addWidget(services_title);
    services_header->addStretch();
    services_header->addWidget(services_note);
    root->addLayout(services_header);
    auto* services = new QHBoxLayout;
    services->setSpacing(14);
    services->addWidget(ServiceCard(tr("Email"), tr("Encrypted asynchronous communication."), CybouTheme::NavIcon::Email, this));
    services->addWidget(ServiceCard(tr("Storage"), tr("Encrypted distributed object storage."), CybouTheme::NavIcon::Storage, this));
    services->addWidget(ServiceCard(tr("Backup"), tr("Resilient encrypted backup built on CYBOU Storage."), CybouTheme::NavIcon::Backup, this));
    root->addLayout(services);

    auto* footer = Card(this);
    auto* footer_layout = new QHBoxLayout{footer};
    footer_layout->setContentsMargins(22, 14, 18, 14);
    m_footer_state = new QLabel{footer};
    m_footer_state->setObjectName("bodyText");
    auto* diagnostics = new QPushButton{tr("Open node diagnostics"), footer};
    diagnostics->setObjectName("secondaryButton");
    connect(diagnostics, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
    footer_layout->addWidget(m_footer_state);
    footer_layout->addStretch();
    footer_layout->addWidget(diagnostics);
    root->addWidget(footer);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    refresh();
}

void HomePage::refresh()
{
    const auto& status = m_model->status();
    m_network_name->setText(status.network_name);
    m_node_state->setText(status.node_running ? tr("Running") : tr("Starting"));
    m_peer_count->setText(QString::number(status.peer_count));
    m_height->setText(QString::number(status.height));
    m_identity_state->setText(status.identity_state == CybouIdentityState::Active
        ? tr("Identity active")
        : tr("No identity created"));
    m_footer_state->setText(status.node_running
        ? tr("Your node is running correctly. Keep CYBOU online to support the network.")
        : tr("The CYBOU node is starting."));
}
