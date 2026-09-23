// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/homepage.h>

#include <qt/cyboudesktopmodel.h>

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

QFrame* ServiceCard(const QString& title, const QString& description, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(7);
    auto* heading = new QLabel{title, card};
    heading->setObjectName("serviceTitle");
    auto* badge = new QLabel{QObject::tr("Planned"), card};
    badge->setObjectName("neutralBadge");
    badge->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    auto* body = new QLabel{description, card};
    body->setObjectName("mutedText");
    body->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(badge);
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

    auto* hero = new QFrame{this};
    hero->setObjectName("hero");
    hero->setMinimumHeight(190);
    hero->setStyleSheet("QFrame#hero { border-image: url(:/art/cybou-hero) 0 0 0 0 stretch stretch; border-radius: 16px; }");
    auto* hero_layout = new QVBoxLayout{hero};
    hero_layout->setContentsMargins(34, 26, 34, 26);
    hero_layout->setSpacing(7);
    auto* eyebrow = new QLabel{tr("WELCOME TO CYBOU"), hero};
    eyebrow->setObjectName("eyebrow");
    auto* title = new QLabel{tr("Protected communication,\nunder your control."), hero};
    title->setObjectName("heroTitle");
    auto* subtitle = new QLabel{tr("A decentralized infrastructure for identity, messaging and resilient data services."), hero};
    subtitle->setObjectName("heroSubtitle");
    subtitle->setWordWrap(true);
    subtitle->setMaximumWidth(610);
    hero_layout->addWidget(eyebrow);
    hero_layout->addWidget(title);
    hero_layout->addWidget(subtitle);
    hero_layout->addStretch();
    root->addWidget(hero);

    auto* primary_row = new QHBoxLayout;
    primary_row->setSpacing(16);

    auto* network_card = Card(this);
    auto* network_layout = new QVBoxLayout{network_card};
    network_layout->setContentsMargins(24, 20, 24, 20);
    network_layout->setSpacing(10);
    auto* network_label = new QLabel{tr("Network"), network_card};
    network_label->setObjectName("cardLabel");
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
    peers_label->setObjectName("mutedText");
    m_peer_count = new QLabel{network_card};
    m_peer_count->setObjectName("metric");
    peers_box->addWidget(peers_label);
    peers_box->addWidget(m_peer_count);
    auto* height_box = new QVBoxLayout;
    auto* height_label = new QLabel{tr("Current height"), network_card};
    height_label->setObjectName("mutedText");
    m_height = new QLabel{network_card};
    m_height->setObjectName("metric");
    height_box->addWidget(height_label);
    height_box->addWidget(m_height);
    metrics->addLayout(peers_box);
    metrics->addSpacing(48);
    metrics->addLayout(height_box);
    metrics->addStretch();
    network_layout->addWidget(network_label);
    network_layout->addLayout(network_title_row);
    network_layout->addSpacing(8);
    network_layout->addLayout(metrics);

    auto* identity_card = Card(this);
    auto* identity_layout = new QVBoxLayout{identity_card};
    identity_layout->setContentsMargins(24, 20, 24, 20);
    identity_layout->setSpacing(8);
    auto* identity_label = new QLabel{tr("Identity"), identity_card};
    identity_label->setObjectName("cardLabel");
    m_identity_state = new QLabel{identity_card};
    m_identity_state->setObjectName("cardTitle");
    auto* identity_body = new QLabel{tr("A CYBOU identity will provide protocol-native access to communication services."), identity_card};
    identity_body->setObjectName("mutedText");
    identity_body->setWordWrap(true);
    auto* identity_button = new QPushButton{tr("View identity"), identity_card};
    identity_button->setObjectName("secondaryButton");
    connect(identity_button, &QPushButton::clicked, this, [this] { m_identity_requested(); });
    identity_layout->addWidget(identity_label);
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
    auto* services_note = new QLabel{tr("Enabled only when the protocol backend is ready."), this};
    services_note->setObjectName("mutedText");
    services_header->addWidget(services_title);
    services_header->addStretch();
    services_header->addWidget(services_note);
    root->addLayout(services_header);
    auto* services = new QHBoxLayout;
    services->setSpacing(14);
    services->addWidget(ServiceCard(tr("Email"), tr("Encrypted asynchronous messaging."), this));
    services->addWidget(ServiceCard(tr("Storage"), tr("Secure distributed object storage."), this));
    services->addWidget(ServiceCard(tr("Backup"), tr("Resilient protection for selected data."), this));
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
    m_identity_state->setText(status.has_identity ? tr("Identity active") : tr("No identity created"));
    m_footer_state->setText(status.node_running
        ? tr("Your node is running. Network synchronization may still be in progress.")
        : tr("The CYBOU node is starting."));
}
