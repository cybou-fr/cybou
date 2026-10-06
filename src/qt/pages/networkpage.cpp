// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/networkpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouproduct.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

using namespace CybouUi;

namespace {

bool isLanEndpoint(const QString& endpoint)
{
    QString host = endpoint;
    if (host.startsWith(QLatin1Char('['))) {
        const int close_bracket = host.indexOf(QLatin1Char(']'));
        if (close_bracket != -1) {
            host = host.mid(1, close_bracket - 1);
        }
    } else {
        const int colon = host.lastIndexOf(QLatin1Char(':'));
        if (colon != -1) {
            host = host.left(colon);
        }
    }
    host = host.trimmed().toLower();

    if (host == QLatin1String("localhost") || host == QLatin1String("127.0.0.1") ||
        host == QLatin1String("::1") || host.startsWith(QLatin1String("127."))) {
        return true;
    }
    if (host.startsWith(QLatin1String("10."))) return true;
    if (host.startsWith(QLatin1String("192.168."))) return true;
    if (host.startsWith(QLatin1String("169.254."))) return true;
    if (host.startsWith(QLatin1String("172."))) {
        const auto parts = host.split(QLatin1Char('.'));
        if (parts.size() >= 2) {
            bool ok = false;
            const int second = parts[1].toInt(&ok);
            if (ok && second >= 16 && second <= 31) return true;
        }
    }
    if (host.startsWith(QLatin1String("fc")) || host.startsWith(QLatin1String("fd")) ||
        host.startsWith(QLatin1String("fe80"))) {
        return true;
    }
    return false;
}

struct SchematicRegion {
    const char* name;
    QPointF normalized_pt;
};

const SchematicRegion kSchematicRegions[] = {
    {"Île-de-France (Paris)", {0.50, 0.30}},
    {"Hauts-de-France (Lille)", {0.52, 0.12}},
    {"Grand Est (Strasbourg)", {0.80, 0.28}},
    {"Bourgogne-Franche-Comté (Dijon)", {0.66, 0.42}},
    {"Auvergne-Rhône-Alpes (Lyon)", {0.68, 0.60}},
    {"Provence-Alpes-Côte d'Azur (Marseille)", {0.74, 0.80}},
    {"Occitanie (Toulouse)", {0.46, 0.80}},
    {"Nouvelle-Aquitaine (Bordeaux)", {0.32, 0.66}},
    {"Pays de la Loire (Nantes)", {0.28, 0.46}},
    {"Bretagne (Rennes)", {0.18, 0.36}},
    {"Normandie (Rouen)", {0.40, 0.22}},
    {"Centre-Val de Loire (Orléans)", {0.48, 0.46}},
};

constexpr int kRegionCount = sizeof(kSchematicRegions) / sizeof(kSchematicRegions[0]);

QPair<QLabel*, QLabel*> MetricTile(QGridLayout* grid, int row, int column, const QString& caption, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(2);
    auto* title = new QLabel{caption, card};
    title->setObjectName(QStringLiteral("metricCaption"));
    auto* val = new QLabel{card};
    val->setObjectName(QStringLiteral("metric"));
    val->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* sub = new QLabel{card};
    sub->setObjectName(QStringLiteral("cardLabel"));
    sub->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(val);
    layout->addWidget(sub);
    grid->addWidget(card, row, column);
    return {val, sub};
}

void DetailRow(QVBoxLayout* layout, const QString& key, const QString& value, QWidget* parent)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(10);
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("rowSub"));
    k->setFixedWidth(160);
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(k, 0, Qt::AlignTop);
    row->addWidget(v, 1);
    layout->addLayout(row);
}

} // namespace

// ============================================================================
// SchematicFranceMap Implementation
// ============================================================================

SchematicFranceMap::SchematicFranceMap(QWidget* parent)
    : QWidget{parent}
{
    setObjectName(QStringLiteral("schematicFranceMap"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setMouseTracking(true);
}

QSize SchematicFranceMap::sizeHint() const
{
    return QSize{420, 380};
}

QSize SchematicFranceMap::minimumSizeHint() const
{
    return QSize{320, 300};
}

void SchematicFranceMap::setPeers(const QVector<CybouPeerItem>& peers)
{
    m_peers = peers;
    if (m_selected_index >= m_peers.size()) {
        m_selected_index = m_peers.isEmpty() ? -1 : 0;
    }
    update();
}

void SchematicFranceMap::setSelectedPeer(int index)
{
    if (m_selected_index != index) {
        m_selected_index = index;
        update();
    }
}

void SchematicFranceMap::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    const QPointF pos = event->position();
    for (int i = 0; i < m_peer_hit_rects.size(); ++i) {
        if (m_peer_hit_rects[i].contains(pos)) {
            m_selected_index = i;
            update();
            if (on_peer_clicked) on_peer_clicked(i);
            return;
        }
    }
}

void SchematicFranceMap::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect();
    m_peer_hit_rects.resize(m_peers.size());
    for (auto& r : m_peer_hit_rects) r = QRectF{};

    // Card background
    painter.setPen(QPen{CybouTheme::color(CybouTheme::BORDER), 1.0});
    painter.setBrush(CybouTheme::color(CybouTheme::CARD));
    painter.drawRoundedRect(bounds.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);

    // Title & subtitle
    painter.setPen(CybouTheme::color(CybouTheme::TEXT_PRIMARY));
    QFont title_font = painter.font();
    title_font.setBold(true);
    title_font.setPixelSize(13);
    painter.setFont(title_font);
    painter.drawText(QPointF{16, 24}, tr("Observed Mesh Peers • Schematic Map"));

    painter.setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
    QFont sub_font = painter.font();
    sub_font.setBold(false);
    sub_font.setPixelSize(11);
    painter.setFont(sub_font);
    painter.drawText(QPointF{16, 40}, tr("Illustrative regional anchors · Not physical geolocation"));

    // Map drawing rect
    const QRectF map_rect = bounds.adjusted(20, 50, -20, -50);
    if (map_rect.width() < 100 || map_rect.height() < 100) return;

    // Metropolitan France outer polygon
    static const QVector<QPointF> kFranceBorder = {
        {0.50, 0.05}, // Dunkerque / Nord
        {0.62, 0.12}, // Ardennes
        {0.78, 0.20}, // Lorraine
        {0.90, 0.28}, // Strasbourg / Alsace
        {0.84, 0.44}, // Jura
        {0.78, 0.54}, // Alps / Haute-Savoie
        {0.84, 0.78}, // Nice / Côte d'Azur
        {0.70, 0.84}, // Marseille / Toulon
        {0.48, 0.94}, // Perpignan / Pyrénées
        {0.24, 0.88}, // Biarritz / Pays Basque
        {0.22, 0.68}, // Bordeaux / Arcachon
        {0.20, 0.50}, // Vendée / Loire-Atlantique
        {0.05, 0.34}, // Brest / Finistère
        {0.20, 0.28}, // Saint-Malo / Baie du Mont-Saint-Michel
        {0.26, 0.16}, // Cherbourg / Cotentin
        {0.38, 0.18}, // Le Havre / Normandie
        {0.46, 0.08}, // Baie de Somme / Calais
    };

    QPolygonF poly;
    for (const auto& pt : kFranceBorder) {
        poly << QPointF{map_rect.left() + pt.x() * map_rect.width(),
                        map_rect.top() + pt.y() * map_rect.height()};
    }

    // Corsica island
    static const QVector<QPointF> kCorsicaBorder = {
        {0.91, 0.82}, {0.94, 0.80}, {0.96, 0.90}, {0.92, 0.92}
    };
    QPolygonF corsica_poly;
    for (const auto& pt : kCorsicaBorder) {
        corsica_poly << QPointF{map_rect.left() + pt.x() * map_rect.width(),
                                map_rect.top() + pt.y() * map_rect.height()};
    }

    // Paint France mainland & Corsica
    painter.setPen(QPen{CybouTheme::color(CybouTheme::BORDER_MEDIUM), 1.5});
    painter.setBrush(CybouTheme::color(CybouTheme::SURFACE));
    painter.drawPolygon(poly);
    painter.drawPolygon(corsica_poly);

    // Inset box for Local Network / LAN
    const QRectF lan_box{map_rect.left(), map_rect.bottom() - 34, 150, 32};
    painter.setPen(QPen{CybouTheme::color(CybouTheme::BORDER), 1.0});
    painter.setBrush(CybouTheme::color(CybouTheme::SUBTLE));
    painter.drawRoundedRect(lan_box, 6, 6);

    painter.setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
    QFont lan_font = painter.font();
    lan_font.setPixelSize(10);
    lan_font.setBold(true);
    painter.setFont(lan_font);
    painter.drawText(lan_box.adjusted(6, 4, -4, -4), Qt::AlignLeft | Qt::AlignVCenter, tr("LAN / Local"));

    // Empty state if no peers
    if (m_peers.isEmpty()) {
        painter.setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
        QFont empty_font = painter.font();
        empty_font.setPixelSize(12);
        painter.setFont(empty_font);
        painter.drawText(map_rect, Qt::AlignCenter, tr("No peer connections observed"));
        return;
    }

    // Draw peer markers
    int lan_peer_offset = 0;
    for (int i = 0; i < m_peers.size(); ++i) {
        const auto& peer = m_peers[i];
        QPointF center;

        if (peer.is_lan) {
            // Plot inside LAN inset box
            center = QPointF{lan_box.left() + 75 + (lan_peer_offset * 20), lan_box.center().y()};
            lan_peer_offset++;
        } else {
            // Plot on schematic France map
            center = QPointF{map_rect.left() + peer.map_coord.x() * map_rect.width(),
                             map_rect.top() + peer.map_coord.y() * map_rect.height()};
        }

        const bool is_selected = (i == m_selected_index);
        const qreal r = is_selected ? 7.0 : 5.0;

        // Selection halo
        if (is_selected) {
            painter.setPen(QPen{CybouTheme::color(CybouTheme::MINT), 2.0});
            painter.setBrush(CybouTheme::color(CybouTheme::MINT_SOFT));
            painter.drawEllipse(center, 12.0, 12.0);
        }

        // Inner dot
        painter.setPen(QPen{CybouTheme::color(CybouTheme::CARD), 1.5});
        painter.setBrush(CybouTheme::color(peer.is_lan ? CybouTheme::BRAND_BLUE : CybouTheme::BRAND_TEAL));
        painter.drawEllipse(center, r, r);

        // Store hit rect
        m_peer_hit_rects[i] = QRectF{center.x() - 14, center.y() - 14, 28, 28};

        // Short label
        painter.setPen(CybouTheme::color(CybouTheme::TEXT_PRIMARY));
        QFont lbl_font = painter.font();
        lbl_font.setPixelSize(10);
        lbl_font.setBold(is_selected);
        painter.setFont(lbl_font);
        const QString peer_tag = QStringLiteral("P%1").arg(i + 1);
        painter.drawText(QRectF{center.x() - 16, center.y() + (is_selected ? 13 : 9), 32, 14},
                         Qt::AlignCenter, peer_tag);
    }
}

// ============================================================================
// NetworkPage Implementation
// ============================================================================

NetworkPage::NetworkPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    setObjectName(QStringLiteral("networkPage"));
    setMinimumWidth(0);

    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(16);

    // Section header
    root->addWidget(SectionTitle(tr("Network Overview"), this));
    root->addWidget(MutedText(tr("Observed P2P network connections, local consensus state, and storage capacity."), this));

    // Scope and honest provenance note
    m_scope_note = new QLabel{this};
    m_scope_note->setObjectName(QStringLiteral("cardLabel"));
    m_scope_note->setWordWrap(true);
    m_scope_note->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_scope_note);

    // Top metrics grid (5 cards across 2 rows)
    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    auto [c_val, c_sub] = MetricTile(grid, 0, 0, tr("Connectivity"), this);
    m_metric_connectivity = c_val; m_metric_connectivity_sub = c_sub;

    auto [h_val, h_sub] = MetricTile(grid, 0, 1, tr("Verified height"), this);
    m_metric_height = h_val; m_metric_height_sub = h_sub;

    auto [p_val, p_sub] = MetricTile(grid, 0, 2, tr("Connected peers"), this);
    m_metric_peers = p_val; m_metric_peers_sub = p_sub;

    auto [s_val, s_sub] = MetricTile(grid, 1, 0, tr("Storage capacity (V)"), this);
    m_metric_storage = s_val; m_metric_storage_sub = s_sub;

    auto [pr_val, pr_sub] = MetricTile(grid, 1, 1, tr("Content protection"), this);
    m_metric_protection = pr_val; m_metric_protection_sub = pr_sub;

    auto* empty_spacer = new QWidget{this};
    grid->addWidget(empty_spacer, 1, 2);

    for (int col = 0; col < 3; ++col) grid->setColumnStretch(col, 1);
    root->addLayout(grid);

    // Middle area: Map (left) + Peer list & details (right)
    auto* middle = new QHBoxLayout;
    middle->setSpacing(16);

    m_map = new SchematicFranceMap{this};
    middle->addWidget(m_map, 1);

    auto* right_col = new QVBoxLayout;
    right_col->setSpacing(12);

    // Peer Table
    m_table = new QTableWidget{0, 5, this};
    m_table->setObjectName(QStringLiteral("networkPeersTable"));
    m_table->setHorizontalHeaderLabels({
        tr("Endpoint"), tr("Type"), tr("Advertised height"), tr("Lag"), tr("StorageId")
    });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setMinimumHeight(160);
    right_col->addWidget(m_table, 1);

    // Peer Details Card
    m_details_card = Card(this);
    m_details_card->setObjectName(QStringLiteral("peerDetailsCard"));
    auto* details_card_layout = new QVBoxLayout{m_details_card};
    details_card_layout->setContentsMargins(18, 14, 18, 14);
    details_card_layout->setSpacing(6);
    details_card_layout->addWidget(SectionTitle(tr("Selected Peer Details"), m_details_card));

    auto* details_container = new QWidget{m_details_card};
    m_details_layout = new QVBoxLayout{details_container};
    m_details_layout->setContentsMargins(0, 0, 0, 0);
    m_details_layout->setSpacing(6);
    details_card_layout->addWidget(details_container);

    right_col->addWidget(m_details_card, 0);
    middle->addLayout(right_col, 1);
    root->addLayout(middle);

    root->addStretch();

    // Event connections
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { onTableSelectionChanged(); });
    m_map->on_peer_clicked = [this](int index) { onMapPeerClicked(index); };
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::filesChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::mailChanged, this, [this] { refresh(); });

    auto* timer = new QTimer{this};
    connect(timer, &QTimer::timeout, this, &NetworkPage::refresh);
    timer->start(15000);

    refresh();
}

void NetworkPage::selectPeer(int index)
{
    if (index >= 0 && index < m_peers.size()) {
        m_selected_peer_index = index;
        m_table->selectRow(index);
        m_map->setSelectedPeer(index);
        updateDetails();
    }
}

void NetworkPage::onTableSelectionChanged()
{
    const auto selected = m_table->selectedItems();
    if (selected.isEmpty()) {
        m_selected_peer_index = -1;
    } else {
        m_selected_peer_index = selected.first()->row();
    }
    m_map->setSelectedPeer(m_selected_peer_index);
    updateDetails();
}

void NetworkPage::onMapPeerClicked(int index)
{
    if (index >= 0 && index < m_peers.size()) {
        m_selected_peer_index = index;
        m_table->selectRow(index);
        updateDetails();
    }
}

void NetworkPage::refresh()
{
    m_last_update = QDateTime::currentDateTime();
    const auto& status = m_model->status();
    const auto& diag = m_model->networkDiagnostics();

    // 1. Scope note
    const QString update_str = m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("Just now");
    m_scope_note->setText(tr(
        "Source: Local node observations • Sample: Connected peers (%1) • Updated: %2\n"
        "Schematic illustrative map for observed peer connections. Locations are schematic illustrations, "
        "not physical node geolocation or network-wide census.")
        .arg(status.peer_count).arg(update_str));

    // 2. Connectivity tile
    const bool healthy = status.sync_error.isEmpty() && status.node_running && status.online;
    m_metric_connectivity->setText(healthy ? tr("Online") : status.node_running ? tr("Connecting") : tr("Offline"));
    m_metric_connectivity_sub->setText(cybouConnectionText(status));

    // 3. Height tile
    m_metric_height->setText(status.finality_known ? QLocale{}.toString(status.finalized_height) : QStringLiteral("—"));
    m_metric_height_sub->setText(status.finality_known ? tr("Locally verified PoA tip") : tr("Waiting for finality"));

    // 4. Peers tile
    m_metric_peers->setText(QString::number(status.peer_count));
    m_metric_peers_sub->setText(tr("Direct mesh sessions"));

    // 5. Storage tile
    m_metric_storage->setText(QStringLiteral("%1 / %2").arg(
        CybouProduct::sizeText(diag.local_storage_used),
        CybouProduct::sizeText(diag.local_storage_capacity)));
    m_metric_storage_sub->setText(tr("Held for others: %1").arg(CybouProduct::sizeText(diag.storage_used)));

    // 6. Content protection tile
    int protected_count = 0;
    int securing_count = 0;
    for (const auto& f : m_model->fileItems()) {
        if (f.folder) continue;
        if (f.state == CybouContentState::Protected) protected_count++;
        else if (f.state == CybouContentState::Securing) securing_count++;
    }
    for (const auto& m : m_model->mailItems()) {
        if (m.draft) continue;
        if (m.state == CybouContentState::Protected) protected_count++;
        else if (m.state == CybouContentState::Securing) securing_count++;
    }
    m_metric_protection->setText(tr("%1 protected · %2 securing").arg(protected_count).arg(securing_count));
    m_metric_protection_sub->setText(tr("Own encrypted publications"));

    // 7. Process peers
    m_peers.clear();
    for (const auto& p : diag.peers) {
        CybouPeerItem item;
        item.endpoint = QString::fromStdString(p.endpoint);
        item.advertised_height = p.advertised_height;
        item.storage_id = QString::fromStdString(p.storage_id);
        item.is_lan = isLanEndpoint(item.endpoint);

        if (item.is_lan) {
            item.classification = tr("Local Network (LAN)");
            item.region_label = tr("LAN / Loopback");
            item.map_coord = QPointF{0.1, 0.9};
        } else {
            item.classification = tr("France (schematic)");
            const quint32 h = qHash(item.endpoint);
            const int reg_idx = static_cast<int>(h % kRegionCount);
            item.region_label = QString::fromUtf8(kSchematicRegions[reg_idx].name);
            const qreal dx = ((static_cast<int>(h >> 8) % 15) - 7) * 0.008;
            const qreal dy = ((static_cast<int>(h >> 16) % 15) - 7) * 0.008;
            item.map_coord = QPointF{
                qBound(0.08, kSchematicRegions[reg_idx].normalized_pt.x() + dx, 0.90),
                qBound(0.08, kSchematicRegions[reg_idx].normalized_pt.y() + dy, 0.90)
            };
        }
        m_peers.append(item);
    }

    // 8. Update map
    m_map->setPeers(m_peers);

    // 9. Update table
    m_table->setRowCount(m_peers.size());
    for (int r = 0; r < m_peers.size(); ++r) {
        const auto& peer = m_peers[r];
        auto* ep_item = new QTableWidgetItem{peer.endpoint};
        auto* type_item = new QTableWidgetItem{peer.classification};
        auto* height_item = new QTableWidgetItem{QLocale{}.toString(peer.advertised_height)};
        const qint64 lag = (status.finality_known && status.finalized_height > peer.advertised_height)
            ? static_cast<qint64>(status.finalized_height - peer.advertised_height) : 0;
        auto* lag_item = new QTableWidgetItem{lag > 0 ? tr("%1 blocks").arg(lag) : tr("0 (in sync)")};
        auto* sid_item = new QTableWidgetItem{peer.storage_id.isEmpty() ? tr("Pending proof") : peer.storage_id};

        m_table->setItem(r, 0, ep_item);
        m_table->setItem(r, 1, type_item);
        m_table->setItem(r, 2, height_item);
        m_table->setItem(r, 3, lag_item);
        m_table->setItem(r, 4, sid_item);
    }

    if (m_selected_peer_index >= 0 && m_selected_peer_index < m_peers.size()) {
        m_table->selectRow(m_selected_peer_index);
    } else if (!m_peers.isEmpty()) {
        m_selected_peer_index = 0;
        m_table->selectRow(0);
        m_map->setSelectedPeer(0);
    } else {
        m_selected_peer_index = -1;
    }

    updateDetails();
}

void NetworkPage::updateDetails()
{
    while (QLayoutItem* item = m_details_layout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }

    QWidget* parent = m_details_layout->parentWidget();
    if (m_selected_peer_index < 0 || m_selected_peer_index >= m_peers.size()) {
        auto* empty_lbl = MutedText(tr("Select a peer from the list or map to view connection details."), parent);
        m_details_layout->addWidget(empty_lbl);
        return;
    }

    const auto& peer = m_peers[m_selected_peer_index];
    const auto& status = m_model->status();
    const qint64 lag = (status.finality_known && status.finalized_height > peer.advertised_height)
        ? static_cast<qint64>(status.finalized_height - peer.advertised_height) : 0;

    DetailRow(m_details_layout, tr("Endpoint"), peer.endpoint, parent);
    DetailRow(m_details_layout, tr("Classification"), peer.classification + QStringLiteral(" · ") + peer.region_label, parent);
    DetailRow(m_details_layout, tr("Advertised height"),
              tr("%1 (unverified announcement)").arg(QLocale{}.toString(peer.advertised_height)), parent);
    DetailRow(m_details_layout, tr("Tip delta"),
              lag > 0 ? tr("%1 blocks behind local tip").arg(lag) : tr("In sync with local chain"), parent);
    DetailRow(m_details_layout, tr("StorageId"),
              peer.storage_id.isEmpty() ? tr("Pending proof (no on-demand storage relationship)") : peer.storage_id, parent);
    DetailRow(m_details_layout, tr("Observation"),
              tr("Direct active P2P mesh session · TLS transport pinned"), parent);
}
