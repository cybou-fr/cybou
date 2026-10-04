// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/diagnosticspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <cybou/p2p/session.h>
#include <cybou/storage_economy.h>
#include <cybou/validation_attestation.h>

#include <QFrame>
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
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

    auto* monitor = new QPushButton{tr("Open Network Monitor"), this};
    monitor->setObjectName(QStringLiteral("networkMonitorButton"));
    connect(monitor, &QPushButton::clicked, this, [this] {
        auto* dialog = new QDialog{this};
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("CYBOU Network Monitor"));
        dialog->resize(1000, 700);
        auto* layout = new QVBoxLayout{dialog};
        auto* head = new QLabel{dialog}; head->setWordWrap(true);
        head->setTextFormat(Qt::PlainText);
        head->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(head);
        auto table = [dialog,layout](const QStringList& headings) {
            auto* widget = new QTableWidget{0,headings.size(),dialog};
            widget->setHorizontalHeaderLabels(headings);
            widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
            widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            layout->addWidget(widget); return widget;
        };
        auto* peers = table({tr("Peer endpoint"),tr("Advertised height"),tr("Lag"),tr("StorageId")});
        auto* operations = table({tr("OperationID"),tr("Local assessment"),tr("Finalized height")});
        auto* storage = table({tr("Application object ID"),tr("Content state"),tr("Remote replicas"),tr("Target"),tr("OperationID")});
        peers->setObjectName(QStringLiteral("networkMonitorPeers"));
        operations->setObjectName(QStringLiteral("networkMonitorOperations"));
        storage->setObjectName(QStringLiteral("networkMonitorContent"));
        auto refresh = [this,head,peers,operations,storage] {
            const auto& d = m_model->networkDiagnostics();
            head->setText(tr("%1 | %2 | Height %3 | Safety halt %4\nNetworkID %5\nTip %6\nState root %7")
                .arg(m_model->status().network_name, m_model->status().online ? tr("Online") : tr("Offline"))
                .arg(d.height).arg(d.safety_halted ? tr("YES") : tr("No"))
                .arg(QString::fromStdString(d.network_binding),QString::fromStdString(d.tip),QString::fromStdString(d.state_root)));
            auto row = [](QTableWidget* table, const QStringList& values) {
                int r=table->rowCount(); table->insertRow(r);
                for (int c=0;c<values.size();++c) table->setItem(r,c,new QTableWidgetItem{values[c]});
            };
            peers->setRowCount(0);
            for (const auto& peer : d.peers) row(peers,{QString::fromStdString(peer.endpoint),
                QString::number(peer.advertised_height),
                QString::number(d.height > peer.advertised_height ? d.height-peer.advertised_height : 0),QString::fromStdString(peer.storage_id)});
            operations->setRowCount(0);
            const QStringList states{tr("Unknown"),tr("Local pending"),tr("Accepted remotely"),tr("Finalized"),tr("Rejected"),tr("History unavailable")};
            for (const auto& op : d.operations) row(operations,{QString::fromStdString(op.operation_id),
                op.state < static_cast<unsigned>(states.size()) ? states[op.state] : tr("Unknown"),QString::number(op.finalized_height)});
            storage->setRowCount(0);
            for (const auto& file : m_model->fileItems()) if (!file.folder && storage->rowCount()<256)
                row(storage,{file.id,CybouProduct::contentStateText(file.state),file.min_remote_replicas < 0 ? tr("Unknown") : QString::number(file.min_remote_replicas),file.remote_replica_target < 0 ? tr("Unknown") : QString::number(file.remote_replica_target),file.operation_id});
            for (const auto& mail : m_model->mailItems()) if (!mail.draft && storage->rowCount()<256)
                row(storage,{mail.id,CybouProduct::contentStateText(mail.state),mail.min_remote_replicas < 0 ? tr("Unknown") : QString::number(mail.min_remote_replicas),mail.remote_replica_target < 0 ? tr("Unknown") : QString::number(mail.remote_replica_target),mail.operation_id});
        };
        connect(m_model,&CybouDesktopModel::statusChanged,dialog,refresh);
        connect(m_model,&CybouDesktopModel::filesChanged,dialog,refresh);
        connect(m_model,&CybouDesktopModel::mailChanged,dialog,refresh);
        refresh(); dialog->show();
    });
    root->addWidget(monitor, 0, Qt::AlignLeft);

    auto* open = new QPushButton{tr("Open diagnostics window"), this};
    open->setObjectName(QStringLiteral("secondaryButton"));
    connect(open, &QPushButton::clicked, this, [fn = std::move(diagnostics_window_requested)] { if (fn) fn(); });
    root->addWidget(open, 0, Qt::AlignLeft);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::featureAvailabilityChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::authorityChanged, this, [this] { refresh(); });
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
    const auto& diagnostics = m_model->networkDiagnostics();
    Row(m_rows, tr("Node type"), tr("Full Node"), parent);
    Row(m_rows, tr("Local storage used / capacity"), QStringLiteral("%1 / %2 bytes").arg(diagnostics.local_storage_used).arg(diagnostics.local_storage_capacity), parent);
    Row(m_rows, tr("Provider obligations / budget"), QStringLiteral("%1 / %2 bytes").arg(diagnostics.storage_used).arg(diagnostics.storage_capacity), parent);
    // Shadow accounting (M4): оценка при полной проверенной доступности; CYBOU не перемещаются.
    const auto shadow = cybou::StorageRentPerDay(cybou::StorageBillingUnits(diagnostics.storage_used), 1);
    Row(m_rows, tr("Shadow provider value"), shadow ? tr("~%1 CYBOU/day (estimate, not paid)").arg(*shadow) : tr("Unavailable"), parent);
    Row(m_rows, tr("PoA signer active"), diagnostics.poa_signer_active ? tr("Yes") : tr("No"), parent);
    Row(m_rows, tr("Connection"), cybouConnectionText(status), parent);
    const QString geo_status = status.geo_admission == CybouGeoAdmissionStatus::Ready ? tr("Ready")
        : tr("Waiting for a valid Geo database");
    Row(m_rows, tr("Peer admission Geo database"), geo_status, parent);
    Row(m_rows, tr("Last sync"), m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("Not yet"), parent);
    if (!status.sync_error.isEmpty()) Row(m_rows, tr("Last error"), status.sync_error, parent);
    Row(m_rows, tr("Network ID"), status.network_binding.isEmpty() ? tr("Available after node startup") : status.network_binding, parent);
    Row(m_rows, tr("Data directory"), status.data_directory.isEmpty() ? tr("Available after node startup") : status.data_directory, parent);
    Row(m_rows, tr("Finality model"), tr("Single-operator proof of authority (not Byzantine fault tolerant)"), parent);
    const auto auth_val = m_model->authority();
    Row(m_rows, tr("Authority"), cybouAuthorityText(auth_val), parent);
    Row(m_rows, tr("Validation eligible"), auth_val > cybou::VALIDATION_AUTHORITY_THRESHOLD ? tr("Yes") : tr("No"), parent);

    ClearLayout(m_services);
    const auto& caps = m_model->featureAvailability();
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
