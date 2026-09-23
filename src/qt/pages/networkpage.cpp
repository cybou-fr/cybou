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

    auto* card = new QFrame{this};
    card->setObjectName("card");
    auto* card_layout = new QVBoxLayout{card};
    card_layout->setContentsMargins(28, 26, 28, 26);
    card_layout->setSpacing(18);
    auto* form = new QFormLayout;
    form->setHorizontalSpacing(48);
    form->setVerticalSpacing(14);
    m_network = new QLabel{card};
    m_status = new QLabel{card};
    m_connections = new QLabel{card};
    m_height = new QLabel{card};
    m_network_id = new QLabel{card};
    m_network_id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_network_id->setWordWrap(true);
    m_data_directory = new QLabel{card};
    m_data_directory->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_data_directory->setWordWrap(true);
    form->addRow(tr("Network"), m_network);
    form->addRow(tr("Node status"), m_status);
    form->addRow(tr("Connections"), m_connections);
    form->addRow(tr("Current height"), m_height);
    form->addRow(tr("Network ID"), m_network_id);
    form->addRow(tr("Data directory"), m_data_directory);
    auto* diagnostics = new QPushButton{tr("Open node diagnostics"), card};
    diagnostics->setObjectName("secondaryButton");
    connect(diagnostics, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
    card_layout->addLayout(form);
    card_layout->addWidget(diagnostics, 0, Qt::AlignLeft);
    root->addWidget(card);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    refresh();
}

void NetworkPage::refresh()
{
    const auto& status = m_model->status();
    m_network->setText(status.network_name);
    m_status->setText(status.node_running ? tr("Running") : tr("Starting"));
    m_connections->setText(QString::number(status.peer_count));
    m_height->setText(QString::number(status.height));
    m_network_id->setText(status.network_id.isEmpty() ? tr("Not available yet") : status.network_id);
    m_data_directory->setText(status.data_directory.isEmpty() ? tr("Available after node startup") : status.data_directory);
}
