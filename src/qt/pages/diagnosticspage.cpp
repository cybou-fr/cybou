// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/diagnosticspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

using namespace CybouUi;

namespace {

QLabel* Tile(QGridLayout* grid, int row, int column, const QString& caption, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(4);
    auto* label = new QLabel{caption, card};
    label->setObjectName(QStringLiteral("metricCaption"));
    auto* value = new QLabel{card};
    value->setObjectName(QStringLiteral("metric"));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(label);
    layout->addWidget(value);
    grid->addWidget(card, row, column);
    return value;
}

void ClearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->layout()) ClearLayout(item->layout());
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

void Row(QVBoxLayout* layout, const QString& key, const QString& value, QWidget* parent)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("rowSub"));
    k->setFixedWidth(170);
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->setMinimumWidth(0);
    row->addWidget(k, 0, Qt::AlignTop);
    row->addWidget(v, 1);
    layout->addLayout(row);
}

} // namespace

DiagnosticsPage::DiagnosticsPage(CybouDesktopModel* model, std::function<void()> diagnostics_window_requested,
    QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    setMinimumWidth(0);
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(16);
    root->addWidget(MutedText(tr("Technical information about the CYBOU node running inside this app."), this));
    m_fixture = MutedText(tr("Showing fixture data. The CYBOU node is not running."), this);
    m_fixture->setObjectName(QStringLiteral("warningBadge"));
    root->addWidget(m_fixture);

    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    m_node = Tile(grid, 0, 0, tr("Node"), this);
    m_network = Tile(grid, 0, 1, tr("Network"), this);
    m_peers = Tile(grid, 0, 2, tr("Peers"), this);
    m_height = Tile(grid, 1, 0, tr("Finalized height"), this);
    m_finality = Tile(grid, 1, 1, tr("Finality"), this);
    auto* spacer = new QWidget{this};
    grid->addWidget(spacer, 1, 2);
    for (int column = 0; column < 3; ++column) grid->setColumnStretch(column, 1);
    root->addLayout(grid);

    auto* details = Card(this);
    auto* details_layout = new QVBoxLayout{details};
    details_layout->setContentsMargins(22, 18, 22, 18);
    details_layout->setSpacing(8);
    details_layout->addWidget(SectionTitle(tr("Details"), details));
    auto* rows = new QWidget{details};
    m_rows = new QVBoxLayout{rows};
    m_rows->setContentsMargins(0, 0, 0, 0);
    m_rows->setSpacing(8);
    details_layout->addWidget(rows);
    root->addWidget(details);

    auto* services = Card(this);
    auto* services_layout = new QVBoxLayout{services};
    services_layout->setContentsMargins(22, 18, 22, 18);
    services_layout->setSpacing(8);
    services_layout->addWidget(SectionTitle(tr("Services"), services));
    auto* service_rows = new QWidget{services};
    m_services = new QVBoxLayout{service_rows};
    m_services->setContentsMargins(0, 0, 0, 0);
    m_services->setSpacing(8);
    services_layout->addWidget(service_rows);
    root->addWidget(services);

    auto* open = new QPushButton{tr("Open diagnostics window"), this};
    open->setObjectName(QStringLiteral("secondaryButton"));
    connect(open, &QPushButton::clicked, this, [fn = std::move(diagnostics_window_requested)] { if (fn) fn(); });
    root->addWidget(open, 0, Qt::AlignLeft);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(30000);
    refresh();
}

void DiagnosticsPage::refresh()
{
    const auto& status = m_model->status();
    m_fixture->setVisible(m_model->fixtureMode());
    m_node->setText(status.node_running ? tr("Running") : tr("Stopped"));
    m_network->setText(status.network_name);
    m_peers->setText(QString::number(status.peer_count));
    m_height->setText(status.finality_known ? QLocale{}.toString(status.finalized_height) : QStringLiteral("—"));
    m_finality->setText(status.finality_known ? tr("PoA verified") : tr("Waiting"));

    ClearLayout(m_rows);
    QWidget* parent = m_rows->parentWidget();
    Row(m_rows, tr("Connection"), cybouConnectionText(status), parent);
    Row(m_rows, tr("Last sync"), m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("Not yet"), parent);
    if (!status.sync_error.isEmpty()) Row(m_rows, tr("Last error"), status.sync_error, parent);
    Row(m_rows, tr("Network ID"), status.network_id.isEmpty() ? tr("Available after node startup") : status.network_id, parent);
    Row(m_rows, tr("Data directory"), status.data_directory.isEmpty() ? tr("Available after node startup") : status.data_directory, parent);
    Row(m_rows, tr("Finality model"), tr("Single-operator proof of authority (not Byzantine fault tolerant)"), parent);

    ClearLayout(m_services);
    const auto& caps = m_model->capabilities();
    const QPair<QString, bool> services[] = {
        {tr("Identity"), caps.account_creation},
        {tr("Wallet"), caps.payments},
        {tr("Mail"), caps.mail},
        {tr("Files"), caps.files},
    };
    for (const auto& [name, on] : services) {
        Row(m_services, name, on ? tr("Connected") : tr("Not connected yet"), m_services->parentWidget());
    }
}
