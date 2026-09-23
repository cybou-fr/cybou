// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/networkpage.h>

#include <qt/cyboudesktopmodel.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace {

QFrame* metricCard(const QString& caption, QWidget* parent, QLabel*& value_out)
{
    auto* card = new QFrame{parent};
    card->setObjectName(QStringLiteral("card"));
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(4);
    auto* label = new QLabel{caption, card};
    label->setObjectName(QStringLiteral("cardLabel"));
    layout->addWidget(label);
    auto* value = new QLabel{card};
    value->setObjectName(QStringLiteral("metric"));
    value_out = value;
    layout->addWidget(value);
    return card;
}

/** One caption/value row inside a card; replaces QFormLayout, whose
    auto-created row labels do not survive the page stylesheet here. */
QWidget* infoRow(const QString& caption, QLabel*& value_out, QWidget* parent)
{
    auto* row = new QWidget{parent};
    auto* layout = new QHBoxLayout{row};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);
    auto* caption_label = new QLabel{caption, row};
    caption_label->setObjectName(QStringLiteral("mutedText"));
    caption_label->setFixedWidth(160);
    auto* value = new QLabel{row};
    value->setObjectName(QStringLiteral("bodyText"));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    layout->addWidget(caption_label);
    layout->addWidget(value, 1);
    value_out = value;
    return row;
}

/** Metric row inside a card: muted caption plus a big value. QFrame#card
    tiles must NOT be nested inside another card here — nested card frames
    render blank under the application stylesheet. */
QWidget* metricRow(const QString& caption, QLabel*& value_out, QWidget* parent)
{
    auto* row = new QWidget{parent};
    auto* layout = new QHBoxLayout{row};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(24);
    auto* caption_label = new QLabel{caption, row};
    caption_label->setObjectName(QStringLiteral("mutedText"));
    caption_label->setFixedWidth(160);
    caption_label->setAlignment(Qt::AlignVCenter);
    auto* value = new QLabel{row};
    value->setObjectName(QStringLiteral("metric"));
    value->setAlignment(Qt::AlignVCenter);
    layout->addWidget(caption_label);
    layout->addWidget(value, 1);
    value_out = value;
    return row;
}

} // namespace

NetworkPage::NetworkPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_diagnostics_requested{std::move(diagnostics_requested)}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);
    auto* heading = new QLabel{tr("Network"), this};
    heading->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(heading);

    // Big metric tiles: the numbers a user checks first.
    auto* metrics_row = new QHBoxLayout;
    metrics_row->setSpacing(16);
    metrics_row->addWidget(metricCard(tr("Node status"), this, m_status_metric), 1);
    metrics_row->addWidget(metricCard(tr("Connections"), this, m_connections_metric), 1);
    metrics_row->addWidget(metricCard(tr("Current height"), this, m_height_metric), 1);
    root->addLayout(metrics_row);

    auto* card = new QFrame{this};
    card->setObjectName(QStringLiteral("card"));
    auto* card_layout = new QVBoxLayout{card};
    card_layout->setContentsMargins(28, 26, 28, 26);
    card_layout->setSpacing(14);
    auto* node_title = new QLabel{tr("Node"), card};
    node_title->setObjectName(QStringLiteral("sectionTitle"));
    card_layout->addWidget(node_title);
    card_layout->addWidget(infoRow(tr("Network"), m_network, card));
    card_layout->addWidget(infoRow(tr("Network ID"), m_network_id, card));
    card_layout->addWidget(infoRow(tr("Data directory"), m_data_directory, card));
    auto* diagnostics = new QPushButton{tr("Open node diagnostics"), card};
    diagnostics->setObjectName(QStringLiteral("secondaryButton"));
    connect(diagnostics, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
    card_layout->addSpacing(4);
    card_layout->addWidget(diagnostics, 0, Qt::AlignLeft);
    root->addWidget(card);

    auto* finality_card = new QFrame{this};
    finality_card->setObjectName(QStringLiteral("card"));
    auto* finality_layout = new QVBoxLayout{finality_card};
    finality_layout->setContentsMargins(28, 26, 28, 26);
    finality_layout->setSpacing(14);
    auto* finality_title = new QLabel{tr("Finality (BFT)"), finality_card};
    finality_title->setObjectName(QStringLiteral("sectionTitle"));
    finality_layout->addWidget(finality_title);
    finality_layout->addWidget(metricRow(tr("Last finalized height"), m_finalized_metric, finality_card));
    finality_layout->addWidget(metricRow(tr("Validators"), m_validators_metric, finality_card));
    finality_layout->addWidget(metricRow(tr("Fault tolerance"), m_fault_metric, finality_card));
    const QStringList facts{
        tr("Finality is explicit: a height counts only once a BFT certificate commits it."),
        tr("Validator admission is operator-approved; every validator has equal weight 1."),
        tr("f = 1 fault tolerance requires at least 4 validators."),
    };
    for (const QString& fact : facts) {
        auto* bullet = new QLabel{QStringLiteral("\u2022 %1").arg(fact), finality_card};
        bullet->setObjectName(QStringLiteral("bodyText"));
        bullet->setWordWrap(true);
        finality_layout->addWidget(bullet);
    }
    m_finality_hint = new QLabel{finality_card};
    m_finality_hint->setObjectName(QStringLiteral("mutedText"));
    m_finality_hint->setWordWrap(true);
    finality_layout->addWidget(m_finality_hint);
    root->addWidget(finality_card);

    auto* about_card = new QFrame{this};
    about_card->setObjectName(QStringLiteral("card"));
    auto* about_layout = new QVBoxLayout{about_card};
    about_layout->setContentsMargins(28, 24, 28, 24);
    about_layout->setSpacing(10);
    auto* about_title = new QLabel{tr("About CYBOU-DEV"), about_card};
    about_title->setObjectName(QStringLiteral("sectionTitle"));
    about_layout->addWidget(about_title);
    const QStringList dev_facts{
        tr("A development network for testing the CYBOU protocol."),
        tr("Balances have no real-world value; do not treat them as assets."),
        tr("The chain may be reset as the protocol and economics evolve."),
        tr("Onboarding and SystemBalance parameters here are DEV-only."),
    };
    QStringList dev_bullets;
    for (const QString& fact : dev_facts) dev_bullets.append(QStringLiteral("\u2022 %1").arg(fact));
    auto* about_body = new QLabel{dev_bullets.join(QStringLiteral("\n")), about_card};
    about_body->setObjectName(QStringLiteral("bodyText"));
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

    const bool finality_known = status.last_finalized_height >= 0;
    m_finalized_metric->setText(finality_known
        ? QString::number(status.last_finalized_height)
        : tr("Not exposed yet"));
    m_validators_metric->setText(status.validator_count > 0
        ? QString::number(status.validator_count)
        : QStringLiteral("—"));
    if (status.validator_count >= 4) {
        m_fault_metric->setText(QStringLiteral("f = 1"));
    } else if (status.validator_count > 0) {
        m_fault_metric->setText(tr("0 — need 4 validators for f = 1"));
    } else {
        m_fault_metric->setText(QStringLiteral("—"));
    }
    m_finality_hint->setText(finality_known
        ? QString{}
        : tr("Finality data is not exposed by the node yet — these metrics populate once core wires the BFT status feed."));
}
