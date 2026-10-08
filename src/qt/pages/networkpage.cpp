// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/networkpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouproduct.h>
#include <qt/cybouobservationchart.h>
#include <qt/franceoutline.h>
#include <qt/benchmarkreference.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
#include <QTimeZone>
#include <QFile>
#include <QPushButton>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTableWidget>
#include <QTabWidget>
#include <QShowEvent>
#include <QTimer>

#include <algorithm>
#include <QVBoxLayout>

#include <cmath>
#include <utility>

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

// Stable visual anchors only. No city or regional location is inferred.
const QPointF kSchematicAnchors[] = {
    {0.38, 0.30}, {0.52, 0.17}, {0.68, 0.33}, {0.55, 0.44},
    {0.63, 0.60}, {0.66, 0.77}, {0.45, 0.78}, {0.34, 0.63},
    {0.30, 0.44}, {0.18, 0.33}, {0.42, 0.23}, {0.43, 0.48},
};
constexpr int kAnchorCount = sizeof(kSchematicAnchors) / sizeof(kSchematicAnchors[0]);

QString PeerTag(const QVector<CybouPeerItem>& peers, int index)
{
    int ordinal = 0;
    for (int i = 0; i <= index; ++i) if (peers[i].is_lan == peers[index].is_lan) ++ordinal;
    return QStringLiteral("%1%2").arg(peers[index].is_lan ? QLatin1Char('L') : QLatin1Char('P')).arg(ordinal);
}

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
    val->setWordWrap(true);
    val->setMinimumWidth(0);
    val->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
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
    k->setFixedWidth(110);
    k->setWordWrap(true);
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    v->setMinimumWidth(0);
    v->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    v->setTextFormat(Qt::PlainText);
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
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Observed mesh peers"));
}

QSize SchematicFranceMap::sizeHint() const
{
    return QSize{800, 500};
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

void SchematicFranceMap::setOverview(const QString& network, const QString& summary)
{
    if (m_network == network && m_summary == summary) return;
    m_network = network;
    m_summary = summary;
    update();
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

void SchematicFranceMap::keyPressEvent(QKeyEvent* event)
{
    if (!m_peers.isEmpty() && (event->key() == Qt::Key_Right || event->key() == Qt::Key_Down ||
        event->key() == Qt::Key_Left || event->key() == Qt::Key_Up)) {
        const int step = event->key() == Qt::Key_Left || event->key() == Qt::Key_Up ? -1 : 1;
        const int index = m_selected_index < 0 ? 0 : (m_selected_index + step + m_peers.size()) % m_peers.size();
        setSelectedPeer(index);
        if (on_peer_clicked) on_peer_clicked(index);
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SchematicFranceMap::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    update();
}

void SchematicFranceMap::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

void SchematicFranceMap::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds = rect();
    m_peer_hit_rects.resize(m_peers.size());
    for (auto& r : m_peer_hit_rects) r = QRectF{};

    // Card background
    painter.setPen(QPen{CybouTheme::color(hasFocus() ? CybouTheme::TEXT_PRIMARY : CybouTheme::BORDER),
        hasFocus() ? 2.0 : 1.0});
    painter.setBrush(CybouTheme::color(CybouTheme::CARD));
    painter.drawRoundedRect(bounds.adjusted(1, 1, -1, -1), 12, 12);

    // Title & subtitle
    painter.setPen(CybouTheme::color(CybouTheme::TEXT_PRIMARY));
    QFont title_font = painter.font();
    title_font.setBold(true);
    title_font.setPixelSize(13);
    painter.setFont(title_font);
    painter.drawText(QPointF{16, 24}, tr("NETWORK"));

    painter.setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
    QFont sub_font = painter.font();
    sub_font.setBold(false);
    sub_font.setPixelSize(11);
    painter.setFont(sub_font);
    painter.drawText(QPointF{16, 40}, m_network);

    const int connected = std::count_if(m_peers.begin(), m_peers.end(), [](const auto& p) { return p.connected; });
    painter.drawText(QPointF{16, 56}, m_summary + (m_peers.size() > connected ? tr(" · %1 known, disconnected (pale)").arg(m_peers.size() - connected) : QString{}));

    // Map drawing rect
    const QRectF available = bounds.adjusted(24, 84, -24, -112);
    const QRectF map_rect = available.adjusted(44, 0, -44, 0);
    if (map_rect.width() < 100 || map_rect.height() < 100) return;

    // Compiled public-domain geometry, fitted without stretching its aspect ratio.
    const qreal map_height = std::min(map_rect.height(), map_rect.width() / CybouMap::ASPECT_RATIO);
    const QSizeF map_size{map_height * CybouMap::ASPECT_RATIO, map_height};
    // Use spare horizontal space for the compact reference without covering
    // Corsica. Keep the fitted size/aspect and the same positions in Advanced.
    const qreal left_shift = std::min(qreal{120}, std::max(qreal{0}, (map_rect.width() - map_size.width()) / 2 - 20));
    const QRectF silhouette{map_rect.center() - QPointF{map_size.width() / 2 + left_shift, map_size.height() / 2}, map_size};
    const auto project = [&silhouette](const QPolygonF& ring) {
        QPolygonF result;
        for (const auto& pt : ring)
            result << QPointF{silhouette.left() + pt.x() * silhouette.width(),
                              silhouette.top() + pt.y() * silhouette.height()};
        return result;
    };
    const QPolygonF poly = project(CybouMap::MAINLAND);
    const QPolygonF corsica_poly = project(CybouMap::CORSICA);
    painter.setPen(QPen{CybouTheme::color(CybouTheme::BRAND_TEAL), 1.4});
    painter.setBrush(CybouTheme::color(CybouTheme::MINT_SOFT));
    painter.drawPolygon(poly);
    painter.drawPolygon(corsica_poly);

    // Compact LAN inset. Bound visible markers and keep a selected LAN peer visible.
    const QRectF lan_box{24, bounds.bottom() - 102, std::min(qreal{320}, bounds.width() - 48), 74};
    painter.setPen(QPen{CybouTheme::color(CybouTheme::BORDER), 1.0});
    painter.setBrush(CybouTheme::color(CybouTheme::SUBTLE));
    painter.drawRoundedRect(lan_box, 8, 8);
    painter.setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
    painter.drawText(lan_box.adjusted(12, 6, -12, -40), Qt::AlignLeft | Qt::AlignVCenter, tr("LOCAL NETWORK"));
    QVector<int> visible_lan;
    int lan_count = 0;
    const int lan_limit = std::max(1, static_cast<int>((lan_box.width() - 60) / 52));
    for (int i = 0; i < m_peers.size(); ++i) if (m_peers[i].is_lan) {
        ++lan_count;
        if (visible_lan.size() < lan_limit) visible_lan.append(i);
    }
    if (m_selected_index >= 0 && m_selected_index < m_peers.size() && m_peers[m_selected_index].is_lan &&
        !visible_lan.contains(m_selected_index)) visible_lan.last() = m_selected_index;
    if (!lan_count) painter.drawText(lan_box.adjusted(12, 30, -12, -6), Qt::AlignLeft | Qt::AlignVCenter, tr("No local peers"));
    else if (lan_count > visible_lan.size())
        painter.drawText(lan_box.adjusted(12, 24, -10, -6), Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("+%1").arg(lan_count - visible_lan.size()));

    painter.setPen(CybouTheme::color(CybouTheme::TEXT_MUTED));
    painter.drawText(QPointF{24, bounds.bottom() - 10}, tr("Illustrative positions · Public IP admission: France"));

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
            if (!visible_lan.contains(i)) continue;
            center = QPointF{lan_box.left() + 28 + lan_peer_offset * 52, lan_box.top() + 38};
            ++lan_peer_offset;
        } else {
            center = QPointF{silhouette.left() + peer.map_coord.x() * silhouette.width(),
                             silhouette.top() + peer.map_coord.y() * silhouette.height()};
            const auto inside = [&poly](const QPointF& c) {
                for (const auto& d : {QPointF{-18, -18}, QPointF{18, -18}, QPointF{-18, 30}, QPointF{18, 30}})
                    if (!poly.containsPoint(c + d, Qt::OddEvenFill)) return false;
                return true;
            };
            const QPointF interior{silhouette.left() + 0.48 * silhouette.width(), silhouette.top() + 0.48 * silhouette.height()};
            for (int attempt = 0; attempt < 80 && !inside(center); ++attempt)
                center = center * 0.94 + interior * 0.06;
        }
        const bool is_selected = (i == m_selected_index);
        const qreal r = is_selected ? 10.0 : 8.0;

        // Selection halo
        if (is_selected) {
            painter.setPen(QPen{CybouTheme::color(CybouTheme::MINT), 2.0});
            painter.setBrush(CybouTheme::color(CybouTheme::MINT_SOFT));
            painter.drawEllipse(center, 16.0, 16.0);
        }

        // Inner dot
        painter.setPen(QPen{CybouTheme::color(CybouTheme::CARD), 1.5});
        painter.setBrush(CybouTheme::color(!peer.connected ? CybouTheme::TEXT_MUTED : peer.is_lan ? CybouTheme::BRAND_BLUE : CybouTheme::BRAND_TEAL));
        painter.drawEllipse(center, r, r);

        // Store hit rect
        m_peer_hit_rects[i] = QRectF{center.x() - 18, center.y() - 18, 36, 36};

        // Short label
        painter.setPen(CybouTheme::color(CybouTheme::TEXT_PRIMARY));
        QFont lbl_font = painter.font();
        lbl_font.setPixelSize(12);
        lbl_font.setBold(is_selected);
        painter.setFont(lbl_font);
        const QString peer_tag = PeerTag(m_peers, i) + (peer.connected ? QString{} : QStringLiteral(" o"));
        painter.drawText(QRectF{center.x() - 16, center.y() + (is_selected ? 19 : 13), 32, 14},
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
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(16);

    // Scope and honest provenance note
    m_scope_note = new QLabel{this};
    m_scope_note->setObjectName(QStringLiteral("cardLabel"));
    m_scope_note->setWordWrap(true);
    m_scope_note->setTextInteractionFlags(Qt::TextSelectableByMouse);


    m_advanced = new QWidget{this};
    m_advanced->setObjectName(QStringLiteral("networkAdvanced"));
    m_advanced_layout = new QVBoxLayout{m_advanced};
    m_advanced_layout->setContentsMargins(0, 0, 0, 0);
    m_advanced_tabs = new QTabWidget{m_advanced};
    m_advanced_tabs->setObjectName(QStringLiteral("networkAdvancedTabs"));
    m_advanced_layout->addWidget(m_advanced_tabs);
    const auto section = [this](const QString& title) {
        auto* scroll = new QScrollArea{m_advanced_tabs};
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto* widget = new QWidget{scroll};
        auto* layout = new QVBoxLayout{widget};
        layout->setContentsMargins(10, 12, 10, 12);
        layout->setSpacing(12);
        layout->setAlignment(Qt::AlignTop);
        scroll->setWidget(widget);
        m_advanced_tabs->addTab(scroll, title);
        return std::pair{widget, layout};
    };
    auto [overview, overview_layout] = section(tr("Overview"));
    auto [peers, peer_layout] = section(tr("Peers"));
    m_peer_section = peers;
    m_peer_layout = peer_layout;
    auto [storage, storage_layout] = section(tr("Storage"));
    auto [technical, technical_layout] = section(tr("Technical"));
    m_technical_layout = technical_layout;
    overview_layout->addWidget(m_scope_note);

    // Local mesh/finality observations are separate from storage observations.
    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    auto [h_val, h_sub] = MetricTile(grid, 0, 0, tr("Verified height"), overview);
    m_metric_height = h_val; m_metric_height_sub = h_sub;

    auto [p_val, p_sub] = MetricTile(grid, 0, 1, tr("Connected peers"), overview);
    m_metric_peers = p_val; m_metric_peers_sub = p_sub;

    auto [u_val, u_sub] = MetricTile(grid, 1, 0, tr("Node uptime"), overview);
    m_metric_uptime = u_val; m_metric_uptime_sub = u_sub;
    m_metric_uptime->setObjectName(QStringLiteral("networkNodeUptime"));
    auto [q_val, q_sub] = MetricTile(grid, 1, 1, tr("Local candidate pool"), overview);
    m_metric_queue = q_val; m_metric_queue_sub = q_sub;
    m_metric_queue->setObjectName(QStringLiteral("networkCandidatePool"));
    auto [t_val, t_sub] = MetricTile(grid, 2, 0, tr("Local traffic"), overview);
    m_metric_traffic = t_val; m_metric_traffic_sub = t_sub;
    m_metric_traffic->setObjectName(QStringLiteral("networkTrafficRate"));
    auto [f_val, f_sub] = MetricTile(grid, 2, 1, tr("Observed finalized op/min"), overview);
    m_metric_finalization = f_val; m_metric_finalization_sub = f_sub;
    m_metric_finalization->setObjectName(QStringLiteral("networkFinalizationRate"));
    auto [memory_val, memory_sub] = MetricTile(grid, 3, 0, tr("CYBOU process memory"), overview);
    m_metric_memory = memory_val; m_metric_memory_sub = memory_sub;
    m_metric_memory->setObjectName(QStringLiteral("networkProcessMemory"));
    auto [cpu_val, cpu_sub] = MetricTile(grid, 3, 1, tr("CYBOU process CPU"), overview);
    m_metric_cpu = cpu_val; m_metric_cpu_sub = cpu_sub;
    m_metric_cpu->setObjectName(QStringLiteral("networkProcessCpu"));

    overview_layout->addLayout(grid);
    auto* traffic_history = Card(overview);
    auto* traffic_history_layout = new QVBoxLayout{traffic_history};
    traffic_history_layout->addWidget(SectionTitle(tr("Traffic history"), traffic_history));
    m_traffic_chart = new CybouObservationChart{tr("Traffic history"), tr("Received"), tr("Sent"),
        tr("B/s"), 0.2, traffic_history};
    m_traffic_chart->setObjectName(QStringLiteral("networkTrafficChart"));
    traffic_history_layout->addWidget(m_traffic_chart);
    traffic_history_layout->addWidget(MutedText(tr("Local frames · 5-second intervals · up to 15 minutes · TLS/TCP overhead excluded"), traffic_history));
    overview_layout->addWidget(traffic_history);
    auto* finalization_history = Card(overview);
    auto* finalization_history_layout = new QVBoxLayout{finalization_history};
    finalization_history_layout->addWidget(SectionTitle(tr("Operation observation history"), finalization_history));
    m_finalization_chart = new CybouObservationChart{tr("Operation observation history"), tr("Observed"),
        tr("Locally produced"), QStringLiteral("op/min"), 12.0, finalization_history};
    m_finalization_chart->setObjectName(QStringLiteral("networkFinalizationChart"));
    finalization_history_layout->addWidget(m_finalization_chart);
    finalization_history_layout->addWidget(MutedText(tr("Local arrival observations · 5-second intervals · history imports excluded · not a capacity ceiling"), finalization_history));
    overview_layout->addWidget(finalization_history);
    auto* storage_grid = new QGridLayout;
    storage_grid->setSpacing(14);
    auto [s_val, s_sub] = MetricTile(storage_grid, 0, 0, tr("Storage capacity (V)"), storage);
    m_metric_storage = s_val; m_metric_storage_sub = s_sub;
    m_metric_storage->setObjectName(QStringLiteral("networkStorageUsage"));
    auto [disk_val, disk_sub] = MetricTile(storage_grid, 1, 0, tr("Available disk space"), storage);
    m_metric_disk = disk_val; m_metric_disk_sub = disk_sub;
    m_metric_disk->setObjectName(QStringLiteral("networkDiskAvailable"));

    auto [pr_val, pr_sub] = MetricTile(storage_grid, 0, 1, tr("Content protection"), storage);
    m_metric_protection = pr_val; m_metric_protection_sub = pr_sub;


    for (int col = 0; col < 2; ++col) {
        grid->setColumnStretch(col, 1);
        storage_grid->setColumnStretch(col, 1);
    }
    storage_layout->addLayout(storage_grid);
    storage_layout->addWidget(MutedText(tr("Replica observations concern your Mail and Files. Mesh connections do not prove storage service. Distinct StorageIds do not prove independent machines."), storage));
    m_peer_layout->addWidget(MutedText(tr("Locally observed mesh sessions. Heights are unverified announcements; StorageId proof belongs only to an on-demand storage relationship."), peers));
    auto* benchmark_card = Card(overview);
    auto* benchmark_layout = new QVBoxLayout{benchmark_card};
    benchmark_layout->addWidget(SectionTitle(tr("Benchmark reference"), benchmark_card));
    m_benchmark_reference = new QLabel{benchmark_card};
    m_benchmark_reference->setObjectName(QStringLiteral("networkBenchmarkReference"));
    m_benchmark_reference->setTextFormat(Qt::PlainText);
    m_benchmark_reference->setWordWrap(true);
    m_benchmark_reference->setTextInteractionFlags(Qt::TextSelectableByMouse);
    benchmark_layout->addWidget(m_benchmark_reference);
    overview_layout->addWidget(benchmark_card);

    // Middle area: Map (left) + Peer list & details (right)
    auto* middle = new QHBoxLayout;
    middle->setSpacing(16);

    m_map = new SchematicFranceMap{this};
    m_map->setMinimumHeight(320);
    m_map->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    middle->addWidget(m_map, 1);

    m_benchmark_card = Card(m_map);
    m_benchmark_card->setProperty("cybouId", QStringLiteral("networkBenchmarkCard"));
    m_benchmark_card->setFixedWidth(310);
    auto* compact_layout = new QVBoxLayout{m_benchmark_card};
    compact_layout->setContentsMargins(14, 10, 14, 10);
    compact_layout->setSpacing(5);
    auto* compact_header = new QHBoxLayout;
    compact_header->addWidget(SectionTitle(tr("Benchmark reference"), m_benchmark_card), 1);
    auto* benchmark_details = IconButton(Glyph::Info, m_benchmark_card, tr("Benchmark details"));
    benchmark_details->setObjectName(QStringLiteral("networkBenchmarkDetails"));
    compact_header->addWidget(benchmark_details);
    compact_layout->addLayout(compact_header);
    m_benchmark_summary = new QLabel{m_benchmark_card};
    m_benchmark_summary->setObjectName(QStringLiteral("networkBenchmarkSummary"));
    m_benchmark_summary->setTextFormat(Qt::PlainText);
    m_benchmark_summary->setWordWrap(true);
    compact_layout->addWidget(m_benchmark_summary);
    m_benchmark_scope = MutedText({}, m_benchmark_card);
    m_benchmark_scope->setObjectName(QStringLiteral("networkBenchmarkScope"));
    m_benchmark_scope->setTextFormat(Qt::PlainText);
    compact_layout->addWidget(m_benchmark_scope);
    connect(benchmark_details, &QToolButton::clicked, this, &NetworkPage::showBenchmarkDetails);


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
    m_peer_layout->addWidget(m_table);

    // Peer Details Card
    m_details_card = Card(m_map);
    m_details_card->setObjectName(QStringLiteral("peerDetailsCard"));
    auto* details_card_layout = new QVBoxLayout{m_details_card};
    details_card_layout->setContentsMargins(18, 14, 18, 14);
    details_card_layout->setSpacing(6);
    auto* details_header = new QHBoxLayout;
    m_details_title = SectionTitle(tr("Selected peer"), m_details_card);
    details_header->addWidget(m_details_title, 1);
    auto* close = IconButton(Glyph::Close, m_details_card, tr("Close peer details"));
    close->setProperty("cybouId", QStringLiteral("closePeerDetails"));
    details_header->addWidget(close);
    details_card_layout->addLayout(details_header);

    auto* details_container = new QWidget{m_details_card};
    details_container->setAutoFillBackground(false);
    m_details_layout = new QVBoxLayout{details_container};
    m_details_layout->setContentsMargins(0, 0, 0, 0);
    m_details_layout->setSpacing(6);
    details_card_layout->addWidget(details_container);
    m_details_card->setFixedWidth(320);
    m_details_card->move(18, 94);
    m_details_card->hide();
    connect(close, &QToolButton::clicked, this, [this] {
        m_selected_peer_index = -1;
        m_table->clearSelection();
        m_map->setSelectedPeer(-1);
        m_details_card->hide();
    });
    root->addLayout(middle, 1);

    m_advanced_button = new QPushButton{tr("Advanced"), this};
    m_advanced_button->setIcon(QIcon{glyphPixmap(Glyph::Sliders, {16, 16}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))});
    m_advanced_button->setObjectName(QStringLiteral("networkAdvancedButton"));
    m_advanced_button->setCheckable(true);
    m_advanced_scroll = new QScrollArea{this};
    m_advanced_scroll->setObjectName(QStringLiteral("networkAdvancedDrawer"));
    m_advanced_scroll->setWidgetResizable(true);
    m_advanced_scroll->setFrameShape(QFrame::NoFrame);
    m_advanced_scroll->setWidget(m_advanced);
    connect(m_advanced_button, &QPushButton::toggled, this, [this](bool open) {
        m_advanced->setVisible(open);
        m_advanced_scroll->setVisible(open);
        // One selected-peer surface: move it into the drawer while Advanced is open.
        m_details_card->hide();
        if (open) {
            m_details_card->setParent(m_peer_section);
            m_details_card->setMinimumWidth(0);
            m_details_card->setMaximumWidth(QWIDGETSIZE_MAX);
            m_peer_layout->insertWidget(1, m_details_card);
            if (m_selected_peer_index >= 0) m_advanced_tabs->setCurrentIndex(1);
            m_advanced_scroll->raise();
        } else {
            m_peer_layout->removeWidget(m_details_card);
            m_details_card->setParent(m_map);
            m_details_card->setFixedWidth(320);
            m_details_card->move(18, 94);
        }
        updateDetails();
        m_benchmark_card->setVisible(!open);
        positionOverlays();
        m_advanced_button->raise();
    });
    m_advanced->hide();
    m_advanced_scroll->hide();

    m_refresh_timer = new QTimer{this};
    m_refresh_timer->setSingleShot(true);
    m_refresh_timer->setInterval(150);
    connect(m_refresh_timer, &QTimer::timeout, this, &NetworkPage::refresh);
    // Event connections
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] { onTableSelectionChanged(); });
    m_map->on_peer_clicked = [this](int index) { onMapPeerClicked(index); };
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { scheduleRefresh(); });
    connect(m_model, &CybouDesktopModel::filesChanged, this, [this] { scheduleRefresh(); });
    connect(m_model, &CybouDesktopModel::mailChanged, this, [this] { scheduleRefresh(); });

    auto* timer = new QTimer{this};
    connect(timer, &QTimer::timeout, this, [this] {
        if (isVisibleTo(window())) refresh();
        else m_stale = true;
    });
    timer->start(15000);

    updateDetails();
    refresh();
}

void NetworkPage::setDiagnosticsWidget(QWidget* widget) { m_technical_layout->addWidget(widget); }
void NetworkPage::showAdvanced() { findChild<QPushButton*>(QStringLiteral("networkAdvancedButton"))->setChecked(true); }
void NetworkPage::showBenchmarkDetails()
{
    showAdvanced();
    m_advanced_tabs->setCurrentIndex(0);
    static_cast<QScrollArea*>(m_advanced_tabs->widget(0))->ensureWidgetVisible(m_benchmark_reference);
}
void NetworkPage::showTechnicalDetails()
{
    showAdvanced();
    m_advanced_tabs->setCurrentIndex(3);
}

void NetworkPage::selectPeer(int index)
{
    if (index >= 0 && index < m_peers.size()) {
        m_selected_peer_index = index;
        m_table->selectRow(index);
        m_map->setSelectedPeer(index);
        if (m_advanced_button->isChecked()) m_advanced_tabs->setCurrentIndex(1);
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
    selectPeer(index);
}

void NetworkPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionOverlays();
}

void NetworkPage::positionOverlays()
{
    m_advanced_button->adjustSize();
    m_advanced_button->move(width() - m_advanced_button->width() - 18, 16);
    const int panel_width = std::min(560, std::max(300, width() - 36));
    m_advanced_scroll->setGeometry(width() - panel_width - 18, 70, panel_width, std::max(100, height() - 88));
    m_advanced_button->raise();
    m_benchmark_card->adjustSize();
    m_benchmark_card->move(std::max(18, width() - m_benchmark_card->width() - 24),
        std::max(84, height() - m_benchmark_card->height() - 30));
}

void NetworkPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_stale) refresh();
}

void NetworkPage::scheduleRefresh()
{
    m_stale = true;
    if (isVisibleTo(window()) && !m_refresh_timer->isActive()) m_refresh_timer->start();
}

void NetworkPage::refresh()
{
    // Status changes several times a second; a hidden page only notes that it is stale.
    if (!isVisibleTo(window())) {
        m_stale = true;
        return;
    }
    m_refresh_timer->stop();
    m_stale = false;
    m_last_update = QDateTime::currentDateTime();
    const auto& status = m_model->status();
    const auto& diag = m_model->networkDiagnostics();
    static const auto reference = [] {
        QFile file{QStringLiteral(":/evidence/benchmark.json")};
        return file.open(QIODevice::ReadOnly) ? CybouBenchmarkReference::Parse(file.readAll()) : std::nullopt;
    }();
    if (reference && reference->network_binding==QString::fromStdString(diag.network_binding)) {
        const auto& r=*reference;
        m_benchmark_summary->setText(tr("%1 finalized op/min").arg(QLocale{}.toString(r.finalized_per_s*60,'f',1)));
        const auto date = QDateTime::fromString(r.run_id, QStringLiteral("yyyyMMdd-HHmmss")).date().toString(Qt::ISODate);
        m_benchmark_scope->setText(tr("%1 UTC · %2 · %3 operations\n%4 · historical reference")
            .arg(date, r.profile).arg(r.finalized).arg(r.co_located_wsl ? tr("Same-host simulation") : tr("DEVNET cohort")));
        m_benchmark_reference->setText(tr(
            "Finalized throughput: %1 op/min\n"
            "Reference run %2 (UTC) · profile %3 · PASS\n"
            "%4 attempted · %5 submitted · %6 finalized · %7 s\n"
            "Controller window includes reconnect, load and drain. This is a measured cohort, not live network throughput or a capacity limit.\n"
            "Revision %8 · modified source: %9\nLoadgen SHA-256: %10\n"
            "%11 clients · %12 replica target · file size %13\n%14")
            .arg(QLocale{}.toString(r.finalized_per_s*60,'f',1),r.run_id,r.profile)
            .arg(r.attempted).arg(r.submitted).arg(r.finalized)
            .arg(QLocale{}.toString(r.window_s,'f',1),r.revision,r.dirty ? tr("Yes") : tr("No"),r.binary_sha256)
            .arg(r.clients).arg(r.replicas).arg(r.file_size)
            .arg(r.co_located_wsl ? tr("Simulation: Windows and WSL share one physical host. Distinct network addresses do not prove independent remote machines.")
                                 : tr("Observed DEVNET cohort.")));
    } else {
        m_benchmark_summary->setText(tr("Finalized op/min: Unknown"));
        m_benchmark_scope->setText(tr("No accepted reference for this network."));
        m_benchmark_reference->setText(tr(
            "Finalized throughput: Unknown\nNo accepted benchmark reference for this network. Live peer observations do not measure network throughput."));
    }

    // 1. Scope note
    const QString update_str = m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("Unknown");
    m_scope_note->setText(tr(
        "Source: Local node observations • Sample: Connected peers (%1) • Last sync: %2\n"
        "Schematic illustrative map for observed peer connections. Locations are schematic illustrations, "
        "not physical node geolocation or network-wide census.")
        .arg(status.peer_count).arg(update_str));

    // 3. Height tile
    m_metric_height->setText(status.finality_known ? QLocale{}.toString(status.finalized_height) : QStringLiteral("—"));
    m_metric_height_sub->setText(status.finality_known ? tr("Locally verified PoA tip") : tr("Waiting for finality"));

    // 4. Peers tile
    m_metric_peers->setText(QString::number(status.peer_count));
    m_metric_peers_sub->setText(tr("Direct mesh sessions"));

    const bool measured = diag.observed_unix_ms != 0;
    const auto& cpu = diag.process_cpu;
    m_metric_cpu->setText(measured && cpu.interval_percent ?
        tr("%1 %").arg(QLocale{}.toString(*cpu.interval_percent, 'f', 1)) : tr("Unknown"));
    m_metric_cpu_sub->setText(tr("OS online logical processors: %1 · interval: %2 ms · last completed mean: %3 · window: %4 ms / %5 intervals · age: %6 ms")
        .arg(measured && cpu.processors ? QString::number(cpu.processors) : tr("Unknown"))
        .arg(measured && cpu.interval_percent ? QString::number(cpu.interval_ms) : tr("Unknown"))
        .arg(measured && cpu.mean_percent ? tr("%1 %").arg(QLocale{}.toString(*cpu.mean_percent, 'f', 1)) : tr("Unknown"))
        .arg(measured && cpu.mean_percent ? QString::number(cpu.mean_window_ms) : tr("Unknown"))
        .arg(measured && cpu.mean_percent ? QString::number(cpu.mean_intervals) : tr("Unknown"))
        .arg(measured && cpu.mean_percent ? QString::number(cpu.mean_age_ms) : tr("Unknown")));
    m_metric_memory->setText(measured && diag.process_resident_bytes ?
        CybouProduct::sizeText(*diag.process_resident_bytes) : tr("Unknown"));
    m_metric_memory_sub->setText(tr("Instantaneous OS working set / RSS · entire CYBOU process, including GUI and shared pages"));
    m_traffic_chart->setHistory(measured ? diag.traffic.history : std::vector<cybou::ObservationPoint>{});
    m_finalization_chart->setHistory(measured && diag.initialized ? diag.finalization.history : std::vector<cybou::ObservationPoint>{});
    const auto& finalization = diag.finalization.windows.front();
    m_metric_finalization->setText(measured && diag.initialized && finalization.complete && finalization.window_ms ?
        QLocale{}.toString(finalization.observed_operations * 60000.0 / finalization.window_ms, 'f', 1) : tr("Unknown"));
    m_metric_finalization_sub->setText(tr("1-minute local observation · history imports excluded · no global freshness proof"));
    const auto& traffic = diag.traffic;
    m_metric_traffic->setText(measured && traffic.window_ms ? tr("↓ %1 B/s · ↑ %2 B/s")
        .arg(QLocale{}.toString(traffic.window_received_bytes * 1000.0 / traffic.window_ms, 'f', 1),
             QLocale{}.toString(traffic.window_sent_bytes * 1000.0 / traffic.window_ms, 'f', 1)) : tr("Unknown"));
    m_metric_traffic_sub->setText(tr("Local CYBOU frames · 60 complete seconds · excludes TLS/TCP overhead"));
    m_metric_uptime->setText(measured ? tr("%1 s").arg(diag.uptime_ms / 1000) : tr("Unknown"));
    m_metric_uptime_sub->setText(measured ? tr("Local observation: %1 UTC").arg(
        QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(diag.observed_unix_ms), QTimeZone::UTC).toString(QStringLiteral("HH:mm:ss"))) : tr("Unknown"));
    m_metric_queue->setText(measured && diag.initialized ? QString::number(diag.pending_operations) : tr("Unknown"));
    m_metric_queue_sub->setText(measured && diag.initialized ? tr("%1 bytes · volatile, locally validated").arg(diag.pending_operation_bytes) : tr("Unknown"));

    // 5. Storage tile
    m_metric_disk->setText(measured && diag.storage_disk_available ?
        CybouProduct::sizeText(*diag.storage_disk_available) : tr("Unknown"));
    m_metric_disk_sub->setText(tr("OS available bytes on the chunk filesystem · shared with other applications · before admission reserve"));
    m_metric_storage->setText(measured && diag.local_storage_capacity ? QStringLiteral("%1 / %2").arg(
        CybouProduct::sizeText(diag.local_storage_used),
        CybouProduct::sizeText(diag.local_storage_capacity)) : tr("Unknown"));
    m_metric_storage_sub->setText(measured && diag.local_storage_capacity ?
        tr("Stored encrypted bytes: %1 % of V · policy headroom: %2 · admitted provider bytes: %3 / %4")
            .arg(QLocale{}.toString(diag.local_storage_used * 100.0 / diag.local_storage_capacity, 'f', 1))
            .arg(CybouProduct::sizeText(diag.local_storage_capacity - std::min(diag.local_storage_used, diag.local_storage_capacity)))
            .arg(CybouProduct::sizeText(diag.storage_used), CybouProduct::sizeText(diag.storage_capacity)) : tr("Unknown"));

    // 6. Content protection tile
    int total_count = 0;
    int protected_count = 0;
    int securing_count = 0;
    for (const auto& f : m_model->fileItems()) {
        if (f.folder) continue;
        ++total_count;
        if (f.state == CybouContentState::Protected) protected_count++;
        else if (f.state == CybouContentState::Securing) securing_count++;
    }
    for (const auto& m : m_model->mailItems()) {
        if (m.draft) continue;
        ++total_count;
        if (m.state == CybouContentState::Protected) protected_count++;
        else if (m.state == CybouContentState::Securing) securing_count++;
    }
    m_metric_protection->setText(tr("%1/%2 protected").arg(protected_count).arg(total_count));
    m_metric_protection_sub->setText(securing_count ? tr("%1 securing").arg(securing_count) : tr("Own encrypted publications"));

    m_map->setOverview(status.network_name, tr("%1 · %2 connections · %3/%4 protected")
        .arg(cybouConnectionText(status)).arg(status.peer_count).arg(protected_count).arg(total_count));

    // 7. Process peers
    const bool network_changed = m_network_binding != QString::fromStdString(diag.network_binding);
    if (network_changed) {
        m_peers.clear();
        m_selected_peer_index = -1;
        m_network_binding = QString::fromStdString(diag.network_binding);
    }
    const QString selected_endpoint = m_selected_peer_index >= 0 && m_selected_peer_index < m_peers.size()
        ? m_peers[m_selected_peer_index].endpoint : QString{};
    const auto previous_peers = m_peers;
    m_peers.clear();
    for (const auto& p : diag.peers) {
        CybouPeerItem item;
        item.endpoint = QString::fromStdString(p.endpoint);
        item.advertised_height = p.advertised_height;
        item.storage_id = QString::fromStdString(p.storage_id);
        item.is_lan = isLanEndpoint(item.endpoint);

        if (item.is_lan) {
            item.classification = tr("Local Network (LAN)");
            item.map_coord = QPointF{0.1, 0.9};
        } else {
            item.classification = tr("France admission");
            const quint32 h = qHash(item.endpoint);
            const int anchor_idx = static_cast<int>(h % kAnchorCount);
            const qreal dx = ((static_cast<int>(h >> 8) % 15) - 7) * 0.008;
            const qreal dy = ((static_cast<int>(h >> 16) % 15) - 7) * 0.008;
            item.map_coord = QPointF{
                qBound(0.08, kSchematicAnchors[anchor_idx].x() + dx, 0.90),
                qBound(0.08, kSchematicAnchors[anchor_idx].y() + dy, 0.90)
            };
        }
        item.last_seen = QDateTime::currentDateTimeUtc();
        m_peers.append(item);
    }

    // Bounded, session-local observations; no persistent endpoint history. A peer
    // gone for 10 minutes is dropped: old test nodes must not linger as "known".
    const auto forget_before = QDateTime::currentDateTimeUtc().addSecs(-600);
    for (auto old : previous_peers) {
        if (m_peers.size() >= 100) break;
        if (old.last_seen < forget_before) continue;
        if (std::none_of(m_peers.begin(), m_peers.end(), [&](const auto& p) { return p.endpoint == old.endpoint; })) {
            old.connected = false;
            old.storage_id.clear();
            m_peers.append(old);
        }
    }
    // The table and details are rebuilt only when the observed peers change.
    const auto same_peer = [](const CybouPeerItem& a, const CybouPeerItem& b) {
        return a.endpoint == b.endpoint && a.advertised_height == b.advertised_height && a.storage_id == b.storage_id && a.connected == b.connected;
    };
    if (!network_changed && status.finalized_height == m_table_height &&
        std::equal(m_peers.begin(), m_peers.end(), previous_peers.begin(), previous_peers.end(), same_peer)) return;
    m_table_height = status.finalized_height;

    // 8. Update map
    m_map->setPeers(m_peers);

    // 9. Update table
    const QSignalBlocker blocker{m_table};
    m_selected_peer_index = -1;
    for (int i = 0; i < m_peers.size(); ++i) if (m_peers[i].endpoint == selected_endpoint) m_selected_peer_index = i;
    m_table->setRowCount(m_peers.size());
    for (int r = 0; r < m_peers.size(); ++r) {
        const auto& peer = m_peers[r];
        auto* ep_item = new QTableWidgetItem{peer.endpoint};
        auto* type_item = new QTableWidgetItem{peer.classification + QStringLiteral(" · ") + (peer.connected ? tr("Connected") : tr("Known · disconnected"))};
        auto* height_item = new QTableWidgetItem{peer.connected ? QLocale{}.toString(peer.advertised_height) : tr("Unknown")};
        const qint64 lag = (status.finality_known && status.finalized_height > peer.advertised_height)
            ? static_cast<qint64>(status.finalized_height - peer.advertised_height) : 0;
        auto* lag_item = new QTableWidgetItem{!peer.connected || !status.finality_known ? tr("Unknown") :
            peer.advertised_height > status.finalized_height
                ? tr("%1 blocks ahead (unverified)").arg(QLocale{}.toString(peer.advertised_height - status.finalized_height))
                : lag > 0 ? tr("%1 blocks").arg(lag) : tr("0 (same height)")};
        auto* sid_item = new QTableWidgetItem{!peer.connected ? tr("Unknown") : peer.storage_id.isEmpty() ? tr("Pending proof") : peer.storage_id};

        m_table->setItem(r, 0, ep_item);
        m_table->setItem(r, 1, type_item);
        m_table->setItem(r, 2, height_item);
        m_table->setItem(r, 3, lag_item);
        m_table->setItem(r, 4, sid_item);
    }

    if (m_selected_peer_index >= 0 && m_selected_peer_index < m_peers.size()) {
        m_table->selectRow(m_selected_peer_index);
        m_map->setSelectedPeer(m_selected_peer_index);
    } else {
        m_selected_peer_index = -1;
        m_map->setSelectedPeer(-1);
        m_details_card->hide();
    }

    updateDetails();
    positionOverlays();
}

void NetworkPage::updateDetails()
{
    // Rows are nested layouts whose labels belong to the container: deleting only
    // the layout items left every old label alive, piling up thousands of
    // siblings until painting overflowed the stack. Delete the labels themselves.
    QWidget* parent = m_details_layout->parentWidget();
    qDeleteAll(parent->findChildren<QWidget*>(Qt::FindDirectChildrenOnly));
    while (QLayoutItem* item = m_details_layout->takeAt(0)) delete item;

    if (m_selected_peer_index < 0 || m_selected_peer_index >= m_peers.size()) {
        m_details_card->hide();
        auto* empty_lbl = MutedText(tr("Select a peer from the list or map to view connection details."), parent);
        m_details_layout->addWidget(empty_lbl);
        return;
    }

    const auto& peer = m_peers[m_selected_peer_index];
    const auto& status = m_model->status();
    const qint64 lag = (status.finality_known && status.finalized_height > peer.advertised_height)
        ? static_cast<qint64>(status.finalized_height - peer.advertised_height) : 0;

    const bool advanced = m_advanced_button->isChecked();
    m_details_title->setText(tr("Peer %1").arg(PeerTag(m_peers, m_selected_peer_index)));
    m_details_card->show();
    m_details_card->raise();
    DetailRow(m_details_layout, tr("Connection"), peer.connected ? tr("Connected") : tr("Known · disconnected"), parent);
    DetailRow(m_details_layout, tr("Admission"), peer.classification, parent);
    DetailRow(m_details_layout, tr("Map position"), peer.is_lan ? tr("Local inset · illustrative") : tr("Illustrative · not measured"), parent);
    if (advanced) DetailRow(m_details_layout, tr("Endpoint"), peer.endpoint, parent);
    if (!peer.connected) {
        DetailRow(m_details_layout, tr("Observation"), tr("Previously observed in this app session. Current height and reachability unknown."), parent);
    } else {
        DetailRow(m_details_layout, tr("Advertised height"),
                  tr("%1 (unverified announcement)").arg(QLocale{}.toString(peer.advertised_height)), parent);
        if (advanced) {
            DetailRow(m_details_layout, tr("Tip delta"), !status.finality_known ? tr("Unknown") :
                peer.advertised_height > status.finalized_height
                    ? tr("%1 blocks ahead of local tip (unverified)").arg(QLocale{}.toString(peer.advertised_height - status.finalized_height))
                    : lag > 0 ? tr("%1 blocks behind local tip").arg(lag) : tr("Same advertised height · unverified"), parent);
            DetailRow(m_details_layout, tr("StorageId"),
                      peer.storage_id.isEmpty() ? tr("Pending proof (no on-demand storage relationship)") : peer.storage_id, parent);
            DetailRow(m_details_layout, tr("Observation"), tr("Direct active P2P mesh session · TLS"), parent);
        }
    }
    if (!advanced) {
        auto* more = new QPushButton{tr("Advanced"), parent};
        more->setObjectName(QStringLiteral("secondaryButton"));
        connect(more, &QPushButton::clicked, this, [this] { m_advanced_button->setChecked(true); });
        m_details_layout->addWidget(more, 0, Qt::AlignLeft);
        // Floating cards have no parent layout to negotiate their new height.
        // Make the new row widgets participate before measuring the card.
        for (auto* child : parent->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) child->show();
        m_details_layout->activate();
        m_details_card->layout()->activate();
        m_details_card->adjustSize();
    }
}
