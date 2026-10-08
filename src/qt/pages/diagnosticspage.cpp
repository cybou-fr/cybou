// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/diagnosticspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouconsoledialog.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <cybou/p2p/session.h>
#include <cybou/storage_economy.h>
#include <cybou/identity_signer.h>

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
#include <QShowEvent>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QSettings>
#include <QScrollBar>
#include <QDateTime>
#include <QToolButton>

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
    value->setWordWrap(true);
    value->setMinimumWidth(0);
    value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(label);
    layout->addWidget(value);
    grid->addWidget(card, row, column);
    return value;
}

void Row(QVBoxLayout* layout, const QString& key, const QString& value, QWidget* parent)
{
    // Keep the rows alive: status ticks must not tear down a scrolled drawer.
    for (auto* existing : parent->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        if (existing->property("diagnosticKey").toString() != key) continue;
        auto* label = existing->findChild<QLabel*>(QStringLiteral("diagnosticValue"));
        if (label->text() != value) label->setText(value);
        return;
    }
    auto* container = new QWidget{parent};
    container->setProperty("diagnosticKey", key);
    auto* row = new QHBoxLayout{container};
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);
    auto* caption = new QLabel{key, container};
    caption->setObjectName(QStringLiteral("rowSub"));
    caption->setFixedWidth(140);
    caption->setWordWrap(true);
    auto* label = new QLabel{value, container};
    label->setObjectName(QStringLiteral("diagnosticValue"));
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setMinimumWidth(0);
    label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    label->setTextFormat(Qt::PlainText);
    row->addWidget(caption, 0, Qt::AlignTop);
    row->addWidget(label, 1);
    layout->addWidget(container);
}

} // namespace

DiagnosticsPage::DiagnosticsPage(CybouDesktopModel* model, QWidget* parent)
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

    auto* launchers = new QHBoxLayout;
    auto* monitor = IconButton(Glyph::Monitor, this, tr("Open Network Monitor"), IconButtonSize::Toolbar);
    monitor->setObjectName(QStringLiteral("networkMonitorButton"));
    connect(monitor, &QToolButton::clicked, this, [this] {
        if (m_monitor) { m_monitor->show(); m_monitor->raise(); m_monitor->activateWindow(); return; }
        auto* dialog = new QDialog{this};
        m_monitor = dialog;
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->setWindowTitle(tr("CYBOU Network Monitor"));
        dialog->resize(1000, 700);
        auto* layout = new QVBoxLayout{dialog};
        auto* head = new QLabel{dialog}; head->setWordWrap(true);
        head->setTextFormat(Qt::PlainText);
        head->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(head);
        auto* toolbar = new QHBoxLayout;
        auto* pause = new QPushButton{tr("Pause view"), dialog};
        pause->setObjectName(QStringLiteral("networkMonitorPause"));
        pause->setCheckable(true);
        auto* observed = MutedText({}, dialog);
        toolbar->addWidget(observed, 1); toolbar->addWidget(pause);
        layout->addLayout(toolbar);
        auto* tabs = new QTabWidget{dialog};
        tabs->setObjectName(QStringLiteral("networkMonitorTabs"));
        layout->addWidget(tabs, 1);
        auto table = [dialog,tabs](const QStringList& headings, const QString& caption) {
            auto* widget = new QTableWidget{0,static_cast<int>(headings.size()),dialog};
            widget->setHorizontalHeaderLabels(headings);
            widget->setEditTriggers(QAbstractItemView::NoEditTriggers);
            widget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
            widget->setSelectionBehavior(QAbstractItemView::SelectRows);
            tabs->addTab(widget, caption); return widget;
        };
        auto* peers = table({tr("Peer endpoint"),tr("Advertised height"),tr("Advertised delta (unverified)"),tr("StorageId")}, tr("Peers"));
        auto* operations = table({tr("OperationID"),tr("Local assessment"),tr("Finalized height")}, tr("Operations"));
        auto* storage = table({tr("Application object ID"),tr("Content state"),tr("Remote replicas"),tr("Target"),tr("OperationID")}, tr("Own content"));
        peers->setObjectName(QStringLiteral("networkMonitorPeers"));
        operations->setObjectName(QStringLiteral("networkMonitorOperations"));
        storage->setObjectName(QStringLiteral("networkMonitorContent"));
        auto* timer = new QTimer{dialog}; timer->setSingleShot(true); timer->setInterval(150);
        auto refresh = [this,head,peers,operations,storage,pause,observed] {
            const bool active = m_model->status().identity_state == CybouIdentityState::Active;
            if (!active || storage->property("account").toString() != m_model->status().account_id) storage->setRowCount(0);
            storage->setProperty("account",m_model->status().account_id);
            if (pause->isChecked()) return;
            observed->setText(tr("Local observations · Updated %1 · Up to 256 rows per tab").arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))));
            const auto& d = m_model->networkDiagnostics();
            head->setText(tr("%1 | %2 | Height %3 | Safety halt %4\nNetwork binding %5\nTip %6\nState root %7")
                .arg(m_model->status().network_name, m_model->status().online ? tr("Online") : tr("Offline"))
                .arg(d.initialized ? QString::number(d.height) : tr("Unknown"))
                .arg(d.initialized ? (d.safety_halted ? tr("YES") : tr("No")) : tr("Unknown"))
                .arg(QString::fromStdString(d.network_binding),d.tip.empty() ? tr("Unknown") : QString::fromStdString(d.tip),d.state_root.empty() ? tr("Unknown") : QString::fromStdString(d.state_root)));
            for (auto* table : {peers,operations,storage}) {
                table->setProperty("selectedKey",table->currentRow()>=0 ? table->item(table->currentRow(),0)->text() : QString{});
                table->setProperty("scroll",table->verticalScrollBar()->value()); table->setProperty("nextRow",0);
            }
            auto row = [](QTableWidget* table, const QStringList& values) {
                const int r=table->property("nextRow").toInt(); if (r>=256) return;
                if (r>=table->rowCount()) table->insertRow(r);
                for (int c=0;c<values.size();++c) {
                    if (!table->item(r,c)) table->setItem(r,c,new QTableWidgetItem{values[c]});
                    else if (table->item(r,c)->text()!=values[c]) table->item(r,c)->setText(values[c]);
                }
                table->setProperty("nextRow",r+1);
            };
            for (const auto& peer : d.peers) {
                if (peers->property("nextRow").toInt()>=256) break;
                row(peers,{QString::fromStdString(peer.endpoint),
                QString::number(peer.advertised_height),
                !d.initialized ? tr("Unknown") : peer.advertised_height > d.height ? tr("Ahead %1").arg(peer.advertised_height-d.height) : QString::number(d.height-peer.advertised_height),peer.storage_id.empty() ? tr("Unknown") : QString::fromStdString(peer.storage_id)});
            }
            const QStringList states{tr("Unknown"),tr("Local pending"),tr("Accepted remotely"),tr("Finalized"),tr("Rejected"),tr("History unavailable")};
            for (const auto& op : d.operations) {
                if (operations->property("nextRow").toInt()>=256) break;
                row(operations,{QString::fromStdString(op.operation_id),
                op.state < static_cast<unsigned>(states.size()) ? states[op.state] : tr("Unknown"),op.state == 3 ? QString::number(op.finalized_height) : tr("Unknown")});
            }
            for (const auto& file : m_model->fileItems()) {
                if (storage->property("nextRow").toInt()>=256) break;
                if (active && !file.folder)
                row(storage,{file.id,CybouProduct::contentStateText(file.state),file.min_remote_replicas < 0 ? tr("Unknown") : QString::number(file.min_remote_replicas),file.remote_replica_target < 0 ? tr("Unknown") : QString::number(file.remote_replica_target),file.operation_id});
            }
            for (const auto& mail : m_model->mailItems()) {
                if (storage->property("nextRow").toInt()>=256) break;
                if (active && !mail.draft)
                row(storage,{mail.id,CybouProduct::contentStateText(mail.state),mail.min_remote_replicas < 0 ? tr("Unknown") : QString::number(mail.min_remote_replicas),mail.remote_replica_target < 0 ? tr("Unknown") : QString::number(mail.remote_replica_target),mail.operation_id});
            }
            for (auto* table : {peers,operations,storage}) {
                table->setRowCount(table->property("nextRow").toInt()); table->clearSelection(); table->setCurrentItem(nullptr);
                for (int r=0;r<table->rowCount();++r) if (table->item(r,0)->text()==table->property("selectedKey").toString()) { table->setCurrentCell(r,0); break; }
                table->verticalScrollBar()->setValue(table->property("scroll").toInt());
            }
        };
        auto schedule = [timer,storage,this] {
            if (m_model->status().identity_state!=CybouIdentityState::Active || storage->property("account").toString()!=m_model->status().account_id) storage->setRowCount(0);
            if (!timer->isActive()) timer->start();
        };
        connect(timer,&QTimer::timeout,dialog,refresh);
        connect(m_model,&CybouDesktopModel::statusChanged,dialog,schedule);
        connect(m_model,&CybouDesktopModel::filesChanged,dialog,schedule);
        connect(m_model,&CybouDesktopModel::mailChanged,dialog,schedule);
        connect(pause,&QPushButton::toggled,dialog,[refresh,pause](bool paused) { pause->setText(paused ? tr("Resume view") : tr("Pause view")); refresh(); });
        if (qEnvironmentVariableIsEmpty("CYBOU_SCREENSHOT_DIR")) dialog->restoreGeometry(QSettings{}.value(QStringLiteral("networkMonitor/geometry")).toByteArray());
        connect(dialog,&QDialog::finished,dialog,[dialog] { if (qEnvironmentVariableIsEmpty("CYBOU_SCREENSHOT_DIR")) QSettings{}.setValue(QStringLiteral("networkMonitor/geometry"),dialog->saveGeometry()); });
        refresh(); dialog->show();
    });
    launchers->addWidget(monitor);
    launchers->addWidget(MutedText(tr("Network Monitor"), this));

    auto* console_btn = IconButton(Glyph::FileText, this, tr("Open Read-Only Console"), IconButtonSize::Toolbar);
    console_btn->setObjectName(QStringLiteral("readOnlyConsoleButton"));
    console_btn->setCursor(Qt::PointingHandCursor);
    connect(console_btn, &QToolButton::clicked, this, [this] {
        if (m_console) { m_console->show(); m_console->raise(); m_console->activateWindow(); return; }
        auto* console = new CybouConsoleDialog{m_model, this};
        m_console = console;
        console->setAttribute(Qt::WA_DeleteOnClose);
        console->show();
    });
    launchers->addWidget(console_btn);
    launchers->addWidget(MutedText(tr("Console"), this));
    launchers->addStretch();
    root->addLayout(launchers);

    root->addStretch();

    m_refresh_timer = new QTimer{this};
    m_refresh_timer->setSingleShot(true);
    m_refresh_timer->setInterval(200);
    connect(m_refresh_timer, &QTimer::timeout, this, &DiagnosticsPage::refresh);
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { scheduleRefresh(); });
    connect(m_model, &CybouDesktopModel::featureAvailabilityChanged, this, [this] { scheduleRefresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { scheduleRefresh(); });
    ticker->start(30000);
    refresh();
}

void DiagnosticsPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    scheduleRefresh();
}

void DiagnosticsPage::scheduleRefresh()
{
    if (isVisibleTo(window()) && !m_refresh_timer->isActive()) m_refresh_timer->start();
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

    QWidget* parent = m_rows->parentWidget();
    const auto& diagnostics = m_model->networkDiagnostics();
    Row(m_rows, tr("Node type"), tr("Full Node"), parent);
    Row(m_rows, tr("Local storage used / capacity"), QStringLiteral("%1 / %2 bytes").arg(diagnostics.local_storage_used).arg(diagnostics.local_storage_capacity), parent);
    Row(m_rows, tr("Storage held for others / limit"), QStringLiteral("%1 / %2 bytes").arg(diagnostics.storage_used).arg(diagnostics.storage_capacity), parent);
    // Shadow accounting (M4): оценка при полной проверенной доступности; CYBOU не перемещаются.
    const auto shadow = cybou::StorageRentPerDay(cybou::StorageBillingUnits(diagnostics.storage_used), 1);
    Row(m_rows, tr("Estimated storage service value"), shadow ? tr("~%1 CYBOU/day (estimate, not paid)").arg(*shadow) : tr("Unavailable"), parent);
    Row(m_rows, tr("PoA signer active"), diagnostics.poa_signer_active ? tr("Yes") : tr("No"), parent);
    Row(m_rows, tr("Connection"), cybouConnectionText(status), parent);
    const QString geo_status = status.geo_admission == CybouGeoAdmissionStatus::Ready ? tr("Ready")
        : tr("Waiting for a valid Geo database");
    Row(m_rows, tr("Peer admission Geo database"), geo_status, parent);
    Row(m_rows, tr("Last sync"), m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("Not yet"), parent);
    Row(m_rows, tr("Last error"), status.sync_error.isEmpty() ? QStringLiteral("—") : status.sync_error, parent);
    Row(m_rows, tr("Network ID"), status.network_binding.isEmpty() ? tr("Available after node startup") : status.network_binding, parent);
    Row(m_rows, tr("Data directory"), status.data_directory.isEmpty() ? tr("Available after node startup") : status.data_directory, parent);
    Row(m_rows, tr("Finality model"), tr("Single-operator proof of authority (not Byzantine fault tolerant)"), parent);

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
