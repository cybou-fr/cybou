// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/networkpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouproduct.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
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
#include <QShowEvent>
#include <QTimer>

#include <algorithm>
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

    const int connected = std::count_if(m_peers.begin(), m_peers.end(), [](const auto& p) { return p.connected; });
    painter.drawText(QPointF{16, 56}, tr("%1 connected · %2 known, disconnected (pale)").arg(connected).arg(m_peers.size() - connected));

    // Map drawing rect
    const QRectF available = bounds.adjusted(24, 78, -24, -56);
    const QRectF map_rect = available.adjusted(44, 0, -44, 0);
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
    painter.setPen(QPen{CybouTheme::color(CybouTheme::TEXT_MUTED), 2.0});
    painter.setBrush(CybouTheme::color(CybouTheme::SURFACE));
    painter.drawPolygon(poly);
    painter.drawPolygon(corsica_poly);

    // Inset box for Local Network / LAN
    const QRectF lan_box{24, bounds.bottom() - 48, bounds.width() - 48, 32};
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
            const int count = std::count_if(m_peers.begin(), m_peers.end(), [](const auto& p) { return p.is_lan; });
            center = QPointF{lan_box.left() + 90 + (lan_peer_offset + 0.5) * (lan_box.width() - 110) / std::max(1, count), lan_box.center().y()};
            lan_peer_offset++;
        } else {
            // Plot on schematic France map
            center = QPointF{map_rect.left() + peer.map_coord.x() * map_rect.width(),
                             map_rect.top() + peer.map_coord.y() * map_rect.height()};
        }

        if (!peer.is_lan) {
            auto inside = [&](const QPointF& c) {
                for (const QPointF& d : {QPointF{-18, -18}, QPointF{18, -18}, QPointF{-18, 30}, QPointF{18, 30}})
                    if (!poly.containsPoint(c + d, Qt::OddEvenFill)) return false;
                return true;
            };
            for (int attempt = 0; attempt < 80 && !inside(center); ++attempt)
                center = center * 0.94 + map_rect.center() * 0.06;
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
        const QString peer_tag = QStringLiteral("P%1%2").arg(i + 1).arg(peer.connected ? QString{} : QStringLiteral(" o"));
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


    // Top metrics grid (5 cards across 2 rows)
    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    auto [h_val, h_sub] = MetricTile(grid, 0, 0, tr("Verified height"), this);
    m_metric_height = h_val; m_metric_height_sub = h_sub;

    auto [p_val, p_sub] = MetricTile(grid, 0, 1, tr("Connected peers"), this);
    m_metric_peers = p_val; m_metric_peers_sub = p_sub;

    auto [s_val, s_sub] = MetricTile(grid, 1, 0, tr("Storage capacity (V)"), this);
    m_metric_storage = s_val; m_metric_storage_sub = s_sub;

    auto [pr_val, pr_sub] = MetricTile(grid, 1, 1, tr("Content protection"), this);
    m_metric_protection = pr_val; m_metric_protection_sub = pr_sub;


    for (int col = 0; col < 2; ++col) grid->setColumnStretch(col, 1);
    m_advanced = new QWidget{this};
    m_advanced->setObjectName(QStringLiteral("networkAdvanced"));
    m_advanced_layout = new QVBoxLayout{m_advanced};
    m_advanced_layout->setContentsMargins(0, 0, 0, 0);
    m_advanced_layout->addWidget(m_scope_note);
    m_advanced_layout->addLayout(grid);

    // Middle area: Map (left) + Peer list & details (right)
    auto* middle = new QHBoxLayout;
    middle->setSpacing(16);

    m_map = new SchematicFranceMap{this};
    m_map->setMinimumHeight(320);
    m_map->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    middle->addWidget(m_map, 1);


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
    m_advanced_layout->addWidget(m_table);

    // Peer Details Card
    m_details_card = Card(m_map);
    m_details_card->setObjectName(QStringLiteral("peerDetailsCard"));
    auto* details_card_layout = new QVBoxLayout{m_details_card};
    details_card_layout->setContentsMargins(18, 14, 18, 14);
    details_card_layout->setSpacing(6);
    details_card_layout->addWidget(SectionTitle(tr("Selected Peer Details"), m_details_card));

    auto* details_container = new QWidget{m_details_card};
    details_container->setAutoFillBackground(false);
    m_details_layout = new QVBoxLayout{details_container};
    m_details_layout->setContentsMargins(0, 0, 0, 0);
    m_details_layout->setSpacing(6);
    auto* detail_scroll = new QScrollArea{m_details_card};
    detail_scroll->setWidgetResizable(true);
    detail_scroll->viewport()->setAutoFillBackground(false);
    detail_scroll->setFrameShape(QFrame::NoFrame);
    detail_scroll->setWidget(details_container);
    detail_scroll->setFixedHeight(240);
    details_card_layout->addWidget(detail_scroll);

    m_details_card->setFixedWidth(320);
    m_details_card->move(18, 94);
    m_details_card->hide();
    auto* close = new QPushButton{tr("Close"), m_details_card};
    close->setObjectName(QStringLiteral("secondaryButton"));
    details_card_layout->addWidget(close);
    connect(close, &QPushButton::clicked, this, [this] {
        m_selected_peer_index = -1;
        m_table->clearSelection();
        m_map->setSelectedPeer(-1);
        m_details_card->hide();
    });
    root->addLayout(middle, 1);

    m_advanced_button = new QPushButton{tr("Advanced · Diagnostics"), this};
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
        if (open) m_advanced_scroll->raise();
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
    connect(timer, &QTimer::timeout, this, &NetworkPage::refresh);
    timer->start(15000);

    updateDetails();
    refresh();
}

void NetworkPage::setDiagnosticsWidget(QWidget* widget) { m_advanced_layout->addWidget(widget); }
void NetworkPage::showAdvanced() { findChild<QPushButton*>(QStringLiteral("networkAdvancedButton"))->setChecked(true); }

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

void NetworkPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_advanced_button->adjustSize();
    m_advanced_button->move(width() - m_advanced_button->width() - 18, 16);
    const int panel_width = std::min(560, std::max(300, width() - 36));
    m_advanced_scroll->setGeometry(width() - panel_width - 18, 70, panel_width, std::max(100, height() - 88));
    m_advanced_button->raise();
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

    // 1. Scope note
    const QString update_str = m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("Just now");
    m_scope_note->setText(tr(
        "Source: Local node observations • Sample: Connected peers (%1) • Updated: %2\n"
        "Schematic illustrative map for observed peer connections. Locations are schematic illustrations, "
        "not physical node geolocation or network-wide census.")
        .arg(status.peer_count).arg(update_str));

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

    // Bounded, session-local observations; no persistent endpoint history.
    for (auto old : previous_peers) {
        if (m_peers.size() >= 100) break;
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
        auto* lag_item = new QTableWidgetItem{!peer.connected || !status.finality_known ? tr("Unknown") : lag > 0 ? tr("%1 blocks").arg(lag) : tr("0 (in sync)")};
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

    m_details_card->show();
    m_details_card->raise();
    DetailRow(m_details_layout, tr("Connection"), peer.connected ? tr("Connected") : tr("Known · disconnected"), parent);
    DetailRow(m_details_layout, tr("Endpoint"), peer.endpoint, parent);
    if (!peer.connected) {
        DetailRow(m_details_layout, tr("Observation"), tr("Previously observed in this app session. Current height and reachability unknown."), parent);
        m_details_layout->addStretch();
        m_details_card->adjustSize();
        return;
    }
    DetailRow(m_details_layout, tr("Classification"), peer.classification + QStringLiteral(" · ") + peer.region_label, parent);
    DetailRow(m_details_layout, tr("Advertised height"),
              tr("%1 (unverified announcement)").arg(QLocale{}.toString(peer.advertised_height)), parent);
    DetailRow(m_details_layout, tr("Tip delta"),
              lag > 0 ? tr("%1 blocks behind local tip").arg(lag) : tr("In sync with local chain"), parent);
    DetailRow(m_details_layout, tr("StorageId"),
              peer.storage_id.isEmpty() ? tr("Pending proof (no on-demand storage relationship)") : peer.storage_id, parent);
    DetailRow(m_details_layout, tr("Observation"),
              tr("Direct active P2P mesh session · TLS"), parent);
    m_details_layout->addStretch();
    m_details_card->adjustSize();
}
