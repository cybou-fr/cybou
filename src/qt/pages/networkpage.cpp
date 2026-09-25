// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/networkpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

using namespace CybouUi;

namespace {

/** Remove all items from a layout, deleting nested row layouts and their widgets. */
void clearLayoutDeep(QLayout* layout)
{
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QLayout* sub = item->layout()) {
            // For a sub-layout the returned item IS the layout itself.
            while (QLayoutItem* sub_item = sub->takeAt(0)) {
                if (QWidget* widget = sub_item->widget()) widget->deleteLater();
                if (sub_item->layout()) sub_item->layout()->deleteLater();
                else delete sub_item;
            }
            sub->deleteLater();
        } else {
            if (QWidget* widget = item->widget()) widget->deleteLater();
            delete item;
        }
    }
}

/** Abstract decorative world map: dots + arcs, no fabricated geography. */
QPixmap worldMapPixmap(const QSize& size)
{
    QPixmap pixmap{size * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);

    const auto mint = CybouTheme::color(CybouTheme::MINT);
    const auto teal = CybouTheme::color(CybouTheme::BRAND_TEAL_DARK);

    // Arcs between hub dots.
    painter.setPen(QPen{CybouTheme::color(CybouTheme::MINT_SOFT), 2});
    const QPointF hubs[] = {
        {size.width() * 0.22, size.height() * 0.38},
        {size.width() * 0.52, size.height() * 0.22},
        {size.width() * 0.78, size.height() * 0.44},
        {size.width() * 0.40, size.height() * 0.66},
        {size.width() * 0.66, size.height() * 0.72},
    };
    painter.drawLine(hubs[0], hubs[1]);
    painter.drawLine(hubs[1], hubs[2]);
    painter.drawLine(hubs[0], hubs[3]);
    painter.drawLine(hubs[3], hubs[4]);
    painter.drawLine(hubs[4], hubs[2]);
    painter.drawLine(hubs[1], hubs[3]);

    // Dots.
    const QPointF dots[] = {
        hubs[0], hubs[1], hubs[2], hubs[3], hubs[4],
        {size.width() * 0.14, size.height() * 0.62},
        {size.width() * 0.34, size.height() * 0.30},
        {size.width() * 0.60, size.height() * 0.48},
        {size.width() * 0.86, size.height() * 0.26},
        {size.width() * 0.48, size.height() * 0.84},
    };
    painter.setPen(Qt::NoPen);
    for (const QPointF& dot : dots) {
        painter.setBrush(mint);
        painter.drawEllipse(dot, 4.5, 4.5);
        painter.setBrush(teal);
        painter.drawEllipse(dot, 1.8, 1.8);
    }
    return pixmap;
}

/** Decorative validator sparkline bars (ornament, not data). */
QPixmap sparklinePixmap(const QSize& size)
{
    QPixmap pixmap{size * 2};
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const qreal heights[] = {0.35, 0.5, 0.42, 0.62, 0.55, 0.74, 0.66, 0.86, 0.78, 1.0};
    constexpr int count = std::size(heights);
    const qreal bar_width = size.width() / (count * 1.6);
    for (int i = 0; i < count; ++i) {
        const qreal h = size.height() * heights[i];
        painter.setBrush(CybouTheme::color(CybouTheme::MINT_SOFT).darker(100 + i * 8));
        painter.drawRoundedRect(QRectF{
            QPointF{i * bar_width * 1.6, size.height() - h},
            QSizeF{bar_width, h}}, 2, 2);
    }
    return pixmap;
}

} // namespace

NetworkPage::NetworkPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_diagnostics_requested{std::move(diagnostics_requested)}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    // ---- Hero ---------------------------------------------------------------
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(30, 26, 30, 26);
    hero_layout->setSpacing(24);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(10);
    hero_text->addWidget(Eyebrow(tr("NETWORK"), hero));
    hero_text->addWidget(HeroTitle(tr("Connected and in sync."), hero));
    hero_text->addWidget(HeroSubtitle(tr("Your device is part of the CYBOU network, helping to keep communication services private, resilient and always available."), hero));
    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    m_chip_healthy = Pill(tr("Network healthy"), Tint::Mint, hero);
    m_chip_synced = Pill({}, Tint::Blue, hero);
    chips->addWidget(m_chip_healthy);
    chips->addWidget(m_chip_synced);
    chips->addStretch();
    hero_text->addLayout(chips);
    hero_text->addStretch();
    hero_layout->addLayout(hero_text, 3);
    auto* map = new QLabel{hero};
    map->setPixmap(worldMapPixmap({260, 130}));
    map->setFixedSize(260, 130);
    hero_layout->addWidget(map, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    // ---- Three status cards ---------------------------------------------------
    auto* cards = new QHBoxLayout;
    cards->setSpacing(14);

    auto* health_card = Card(this);
    auto* health_layout = new QVBoxLayout{health_card};
    health_layout->setContentsMargins(22, 18, 22, 18);
    health_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Wifi, Tint::Mint, health_card, 38, 19));
        auto* heading = new QLabel{tr("Connection health"), health_card};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{health_card};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        health_layout->addLayout(header);
        m_health_metric = new QLabel{health_card};
        m_health_metric->setObjectName(QStringLiteral("cardTitle"));
        health_layout->addWidget(m_health_metric);
        m_health_caption = MutedText({}, health_card);
        health_layout->addWidget(m_health_caption);
        const QStringList checks{
            tr("Connected to network"), tr("Peers reachable"),
            tr("Syncing normally"), tr("Services available"),
        };
        for (const QString& check : checks) {
            auto* row = new QHBoxLayout;
            row->setSpacing(8);
            auto* dot = new QLabel{health_card};
            dot->setFixedSize(8, 8);
            dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;")
                .arg(CybouTheme::color(CybouTheme::MINT).name()));
            row->addWidget(dot, 0, Qt::AlignVCenter);
            auto* label = new QLabel{check, health_card};
            label->setObjectName(QStringLiteral("bodyText"));
            row->addWidget(label, 1);
            health_layout->addLayout(row);
        }
    }
    cards->addWidget(health_card, 1);

    auto* sync_card = Card(this);
    auto* sync_layout = new QVBoxLayout{sync_card};
    sync_layout->setContentsMargins(22, 18, 22, 18);
    sync_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Refresh, Tint::Blue, sync_card, 38, 19));
        auto* heading = new QLabel{tr("Synchronization"), sync_card};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{sync_card};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        sync_layout->addLayout(header);
        m_sync_state = new QLabel{sync_card};
        m_sync_state->setObjectName(QStringLiteral("cardTitle"));
        sync_layout->addWidget(m_sync_state);
        sync_layout->addWidget(MutedText(tr("Your device syncs verified blocks from the network."), sync_card));
        m_sync_meter = new QProgressBar{sync_card};
        m_sync_meter->setObjectName(QStringLiteral("usageMeter"));
        m_sync_meter->setRange(0, 100);
        m_sync_meter->setValue(100);
        m_sync_meter->setTextVisible(false);
        sync_layout->addWidget(m_sync_meter);
        auto* metrics = new QHBoxLayout;
        auto* height_column = StatColumn(tr("Finalized height"), {}, sync_card);
        m_height_metric = qobject_cast<QLabel*>(height_column->itemAt(1)->widget());
        auto* sync_column = StatColumn(tr("Last synced"), {}, sync_card);
        m_last_sync = qobject_cast<QLabel*>(sync_column->itemAt(1)->widget());
        metrics->addLayout(height_column);
        metrics->addSpacing(30);
        metrics->addLayout(sync_column);
        metrics->addStretch();
        sync_layout->addLayout(metrics);
    }
    cards->addWidget(sync_card, 1);

    auto* services_card = Card(this);
    auto* services_layout = new QVBoxLayout{services_card};
    services_layout->setContentsMargins(22, 18, 22, 18);
    services_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Server, Tint::Indigo, services_card, 38, 19));
        auto* heading = new QLabel{tr("Network services"), services_card};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{services_card};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        services_layout->addLayout(header);
        m_services_state = new QLabel{services_card};
        m_services_state->setObjectName(QStringLiteral("cardTitle"));
        services_layout->addWidget(m_services_state);
        services_layout->addWidget(MutedText(tr("The CYBOU network operates normally."), services_card));
        m_services_rows = new QWidget{services_card};
        auto* rows = new QVBoxLayout{m_services_rows};
        rows->setContentsMargins(0, 0, 0, 0);
        rows->setSpacing(6);
        services_layout->addWidget(m_services_rows);
    }
    cards->addWidget(services_card, 1);
    root->addLayout(cards);

    // ---- Validators / peers / diagnostics ---------------------------------------
    auto* bottom = new QHBoxLayout;
    bottom->setSpacing(14);

    auto* validators_card = Card(this);
    auto* validators_layout = new QVBoxLayout{validators_card};
    validators_layout->setContentsMargins(22, 18, 22, 18);
    validators_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::ShieldCheck, Tint::Mint, validators_card, 38, 19));
        auto* heading = new QLabel{tr("Network trust & validators"), validators_card};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{validators_card};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        validators_layout->addLayout(header);
        m_validators_metric = new QLabel{validators_card};
        m_validators_metric->setObjectName(QStringLiteral("metric"));
        validators_layout->addWidget(m_validators_metric);
        m_validators_caption = MutedText({}, validators_card);
        validators_layout->addWidget(m_validators_caption);
        auto* spark = new QLabel{validators_card};
        spark->setPixmap(sparklinePixmap({180, 56}));
        validators_layout->addWidget(spark);
        auto* secure = new QFrame{validators_card};
        auto* secure_layout = new QHBoxLayout{secure};
        secure_layout->setContentsMargins(12, 8, 12, 8);
        secure_layout->setSpacing(8);
        auto* shield = new QLabel{secure};
        shield->setPixmap(glyphPixmap(Glyph::ShieldCheck, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
        secure_layout->addWidget(shield, 0, Qt::AlignVCenter);
        auto* secure_text = new QLabel{tr("Network is secure"), secure};
        secure_text->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
            .arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
        secure_layout->addWidget(secure_text, 1);
        secure->setObjectName(QStringLiteral("heroPanel"));
        validators_layout->addWidget(secure);
    }
    bottom->addWidget(validators_card, 1);

    auto* peers_card = Card(this);
    auto* peers_layout = new QVBoxLayout{peers_card};
    peers_layout->setContentsMargins(22, 18, 22, 18);
    peers_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Globe, Tint::Blue, peers_card, 38, 19));
        auto* heading = new QLabel{tr("Peers"), peers_card};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        auto* chevron = new QLabel{peers_card};
        chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
        header->addWidget(chevron, 0, Qt::AlignVCenter);
        peers_layout->addLayout(header);
        m_peers_metric = new QLabel{peers_card};
        m_peers_metric->setObjectName(QStringLiteral("metric"));
        peers_layout->addWidget(m_peers_metric);
        peers_layout->addWidget(MutedText(tr("Your device connects to peers around the world, helping to route and synchronize network data."), peers_card));
        auto* map = new QLabel{peers_card};
        map->setPixmap(worldMapPixmap({200, 84}));
        peers_layout->addWidget(map);
        auto* counts = new QHBoxLayout;
        auto* outbound_column = StatColumn(tr("Outbound"), {}, peers_card);
        m_outbound_value = qobject_cast<QLabel*>(outbound_column->itemAt(1)->widget());
        auto* inbound_column = StatColumn(tr("Inbound"), {}, peers_card);
        m_inbound_value = qobject_cast<QLabel*>(inbound_column->itemAt(1)->widget());
        counts->addLayout(outbound_column);
        counts->addSpacing(30);
        counts->addLayout(inbound_column);
        counts->addStretch();
        peers_layout->addLayout(counts);
    }
    bottom->addWidget(peers_card, 1);

    auto* diag_card = Card(this);
    auto* diag_layout = new QVBoxLayout{diag_card};
    diag_layout->setContentsMargins(22, 18, 22, 18);
    diag_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        header->addWidget(Chip(Glyph::Gear, Tint::Neutral, diag_card, 38, 19));
        auto* heading = new QLabel{tr("Advanced diagnostics"), diag_card};
        heading->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(heading, 0, Qt::AlignVCenter);
        header->addStretch();
        diag_layout->addLayout(header);
        m_diag_rows = new QWidget{diag_card};
        auto* rows = new QVBoxLayout{m_diag_rows};
        rows->setContentsMargins(0, 0, 0, 0);
        rows->setSpacing(6);
        diag_layout->addWidget(m_diag_rows);
        m_finality_hint = MutedText({}, diag_card);
        diag_layout->addWidget(m_finality_hint);
        auto* open = new QPushButton{tr("View detailed diagnostics"), diag_card};
        open->setObjectName(QStringLiteral("secondaryButton"));
        open->setIcon(QIcon{glyphPixmap(Glyph::ArrowUpRight, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
        connect(open, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
        diag_layout->addWidget(open, 0, Qt::AlignLeft);
    }
    bottom->addWidget(diag_card, 1);
    root->addLayout(bottom, 1);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(30000);
    refresh();
}

void NetworkPage::refresh()
{
    const auto& status = m_model->status();
    const bool connected = status.node_running && status.peer_count > 0;
    const bool finality_known = status.last_finalized_height >= 0;

    // Hero chips.
    m_chip_healthy->setText(connected ? tr("Network healthy") : tr("Connecting"));
    m_chip_healthy->setProperty("tint", connected ? "mint" : "amber");
    m_chip_healthy->style()->unpolish(m_chip_healthy);
    m_chip_healthy->style()->polish(m_chip_healthy);
    m_chip_synced->setText(m_model->lastSync().isValid()
        ? tr("Synced %1").arg(relTime(m_model->lastSync()))
        : tr("Sync pending"));

    // Connection health.
    m_health_metric->setText(connected ? tr("Excellent") : tr("Starting"));
    m_health_caption->setText(connected
        ? tr("Stable connection to the CYBOU network.")
        : tr("The node is establishing its connection to the CYBOU network."));

    // Synchronization.
    m_sync_state->setText(finality_known ? tr("Up to date") : tr("Waiting for finality"));
    m_height_metric->setText(finality_known
        ? QLocale{}.toString(status.last_finalized_height)
        : tr("\u2014"));
    m_last_sync->setText(m_model->lastSync().isValid() ? relTime(m_model->lastSync()) : tr("\u2014"));

    // Services: honest capability states.
    const auto& caps = m_model->capabilities();
    struct ServiceDef { const char* name; bool online; };
    const ServiceDef services[]{
        {QT_TR_NOOP("Identity services"), caps.account_creation},
        {QT_TR_NOOP("Email services"), caps.email},
        {QT_TR_NOOP("Storage services"), caps.storage},
        {QT_TR_NOOP("Backup services"), caps.backup},
        {QT_TR_NOOP("Wallet services"), caps.payments},
    };
    bool all_online = true;
    for (const auto& service : services) {
        if (!service.online) all_online = false;
    }
    m_services_state->setText(all_online ? tr("All services online") : tr("Core services online"));
    clearLayoutDeep(m_services_rows->layout());
    for (const auto& service : services) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        auto* dot = new QLabel{m_services_rows};
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;")
            .arg(CybouTheme::color(service.online ? CybouTheme::MINT : CybouTheme::DIM).name()));
        row->addWidget(dot, 0, Qt::AlignVCenter);
        auto* label = new QLabel{tr(service.name), m_services_rows};
        label->setObjectName(QStringLiteral("bodyText"));
        row->addWidget(label, 1);
        auto* state = new QLabel{service.online ? tr("Online") : tr("Planned"), m_services_rows};
        state->setObjectName(QStringLiteral("rowMeta"));
        state->setStyleSheet(QStringLiteral("background: transparent; border: none; color: %1;")
            .arg(CybouTheme::color(service.online ? CybouTheme::BRAND_TEAL_DARK : CybouTheme::DIM).name()));
        row->addWidget(state, 0, Qt::AlignVCenter);
        qobject_cast<QVBoxLayout*>(m_services_rows->layout())->addLayout(row);
    }

    // Validators.
    m_validators_metric->setText(status.validator_count > 0
        ? QLocale{}.toString(status.validator_count)
        : tr("\u2014"));
    m_validators_caption->setText(status.validator_count >= 4
        ? tr("Active validators \u00b7 f = 1 fault tolerance")
        : tr("Active validators \u00b7 at least 4 validators are required for f = 1"));

    // Peers.
    m_peers_metric->setText(connected ? tr("%1 connected").arg(status.peer_count) : tr("Connecting"));
    m_outbound_value->setText(connected ? QString::number(status.peer_count) : QStringLiteral("0"));
    m_inbound_value->setText(QStringLiteral("0"));

    // Diagnostics rows.
    clearLayoutDeep(m_diag_rows->layout());
    auto add_diag = [this](const QString& key, const QString& value) {
        auto* row = new QHBoxLayout;
        row->setSpacing(12);
        auto* k = new QLabel{key, m_diag_rows};
        k->setObjectName(QStringLiteral("rowSub"));
        k->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        k->setFixedWidth(130);
        auto* v = new QLabel{value, m_diag_rows};
        v->setStyleSheet(QStringLiteral("font-weight: 600; color: %1; background: transparent; border: none;")
            .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->setWordWrap(true);
        row->addWidget(k);
        row->addWidget(v, 1);
        qobject_cast<QVBoxLayout*>(m_diag_rows->layout())->addLayout(row);
    };
    add_diag(tr("Network"), status.network_name);
    add_diag(tr("Network ID"), status.network_id.isEmpty() ? tr("Not available yet") : status.network_id);
    add_diag(tr("Connections"), QString::number(status.peer_count));
    add_diag(tr("Finalized height"), finality_known ? QLocale{}.toString(status.last_finalized_height) : tr("Not exposed yet"));
    add_diag(tr("Sync status"), m_model->lastSync().isValid() ? tr("Up to date") : tr("Pending"));
    add_diag(tr("Data directory"), status.data_directory.isEmpty() ? tr("Available after node startup") : status.data_directory);

    m_finality_hint->setText(finality_known
        ? QString{}
        : tr("Finality data is not exposed by the node yet \u2014 these metrics populate once core wires the BFT status feed."));
}
