// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/networkpage.h>

#include <qt/cyboudesktopmodel.h>

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

NetworkPage::NetworkPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_diagnostics_requested{std::move(diagnostics_requested)}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);
    auto* heading = new QLabel{tr("Network"), this};
    heading->setObjectName("pageTitle");
    root->addWidget(heading);

    // Big metric tiles: the numbers a user checks first.
    auto* metrics_row = new QHBoxLayout;
    metrics_row->setSpacing(16);
    const auto make_metric_card = [this](const QString& caption, QLabel*& value_label) {
        auto* metric_card = new QFrame{this};
        metric_card->setObjectName("card");
        auto* metric_layout = new QVBoxLayout{metric_card};
        metric_layout->setContentsMargins(22, 18, 22, 18);
        metric_layout->setSpacing(4);
        auto* caption_label = new QLabel{caption, metric_card};
        caption_label->setObjectName("cardLabel");
        value_label = new QLabel{metric_card};
        value_label->setObjectName("metric");
        metric_layout->addWidget(caption_label);
        metric_layout->addWidget(value_label);
        return metric_card;
    };
    metrics_row->addWidget(make_metric_card(tr("Node status"), m_status_metric));
    metrics_row->addWidget(make_metric_card(tr("Connections"), m_connections_metric));
    metrics_row->addWidget(make_metric_card(tr("Current height"), m_height_metric));
    root->addLayout(metrics_row);

    auto* card = new QFrame{this};
    card->setObjectName("card");
    auto* card_layout = new QVBoxLayout{card};
    card_layout->setContentsMargins(28, 26, 28, 26);
    card_layout->setSpacing(18);
    m_network = new QLabel{card};
    m_network_id = new QLabel{card};
    m_network_id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_network_id->setWordWrap(true);
    m_data_directory = new QLabel{card};
    m_data_directory->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_data_directory->setWordWrap(true);
    auto* form = new QFormLayout;
    form->setHorizontalSpacing(48);
    form->setVerticalSpacing(14);
    form->addRow(tr("Network"), m_network);
    form->addRow(tr("Network ID"), m_network_id);
    form->addRow(tr("Data directory"), m_data_directory);
    auto* diagnostics = new QPushButton{tr("Open node diagnostics"), card};
    diagnostics->setObjectName("secondaryButton");
    connect(diagnostics, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
    card_layout->addLayout(form);
    card_layout->addWidget(diagnostics, 0, Qt::AlignLeft);
    root->addWidget(card);

    auto* about_card = new QFrame{this};
    about_card->setObjectName("card");
    auto* about_layout = new QVBoxLayout{about_card};
    about_layout->setContentsMargins(28, 24, 28, 24);
    about_layout->setSpacing(10);
    auto* about_title = new QLabel{tr("About CYBOU-DEV"), about_card};
    about_title->setObjectName("sectionTitle");
    about_layout->addWidget(about_title);
    const QStringList facts{
        tr("A development network for testing the CYBOU protocol."),
        tr("Balances have no real-world value; do not treat them as assets."),
        tr("The chain may be reset as the protocol and economics evolve."),
        tr("Onboarding and SystemBalance parameters here are DEV-only."),
    };
    QStringList bullets;
    for (const QString& fact : facts) bullets.append(QStringLiteral("\u2022 %1").arg(fact));
    auto* about_body = new QLabel{bullets.join(QStringLiteral("\n")), about_card};
    about_body->setObjectName("bodyText");
    about_body->setWordWrap(true);
    about_layout->addWidget(about_body);
    root->addWidget(about_card);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    refresh();
}

void NetworkPage::refresh()
{
    const auto& status = m_model->status();
    m_network->setText(status.network_name);
    m_status_metric->setText(status.node_running ? tr("Running") : tr("Starting"));
    m_connections_metric->setText(QString::number(status.peer_count));
    m_height_metric->setText(QString::number(status.height));
    m_network_id->setText(status.network_id.isEmpty() ? tr("Not available yet") : status.network_id);
    m_data_directory->setText(status.data_directory.isEmpty() ? tr("Available after node startup") : status.data_directory);
}
