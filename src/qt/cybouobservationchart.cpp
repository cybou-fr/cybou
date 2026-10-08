// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#include <qt/cybouobservationchart.h>
#include <qt/cyboutheme.h>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QLocale>
#include <QVariant>
#include <QToolTip>
#include <algorithm>
#include <cmath>
#include <utility>

CybouObservationChart::CybouObservationChart(QString title, QString primary, QString secondary,
    QString unit, double scale, QWidget* parent) : QWidget{parent}, m_primary{std::move(primary)},
    m_secondary{std::move(secondary)}, m_unit{std::move(unit)}, m_scale{scale}
{
    setAccessibleName(title);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setHistory({});
}
void CybouObservationChart::setRemoteHistory(const std::vector<cybou::NetworkObservationPoint>& points, RemoteMetric metric)
{
    m_remote = true;
    std::vector<ChartPoint> converted;
    const auto first = points.size() > cybou::MAX_OBSERVATION_POINTS ? points.size() - cybou::MAX_OBSERVATION_POINTS : 0;
    for (size_t i = first; i < points.size(); ++i) {
        const auto& point = points[i];
        ChartPoint next; next.end_elapsed_ms = point.end_elapsed_ms; next.cohort = point.cohort_revision;
        size_t contributors{0};
        if (metric == RemoteMetric::STORAGE) {
            if (point.storage.capacity_bytes) next.primary = static_cast<double>(*point.storage.capacity_bytes);
            if (point.storage.stored_copy_bytes) next.secondary = static_cast<double>(*point.storage.stored_copy_bytes);
            contributors = point.storage.contributors;
        } else if (metric == RemoteMetric::TRAFFIC) {
            next.primary = point.traffic.received_bytes_per_second; next.secondary = point.traffic.sent_bytes_per_second;
            contributors = point.traffic.contributors;
        } else {
            next.primary = point.cpu.mean_percent; contributors = point.cpu.contributors;
        }
        next.scope = tr("%1 contributors · fresh %2 / %3 · missing %4 · oldest receipt %5 ms · cohort %6")
            .arg(contributors).arg(point.fresh_groups).arg(point.selected_groups).arg(point.missing_groups)
            .arg(point.max_receipt_age_ms).arg(point.cohort_revision);
        if (metric == RemoteMetric::CPU && contributors) next.scope += tr(" · CPU windows %1–%2 ms · ages %3–%4 ms")
            .arg(point.cpu.min_window_ms).arg(point.cpu.max_window_ms).arg(point.cpu.min_age_ms).arg(point.cpu.max_age_ms);
        if (!point.clock_valid) next.scope = tr("Unknown — invalid collection clock");
        converted.push_back(std::move(next));
    }
    setPoints(converted);
}
void CybouObservationChart::setHistory(const std::vector<cybou::ObservationPoint>& points)
{
    m_remote = false;
    std::vector<ChartPoint> converted;
    const auto first = points.size() > cybou::MAX_OBSERVATION_POINTS ? points.size() - cybou::MAX_OBSERVATION_POINTS : 0;
    for (size_t i = first; i < points.size(); ++i) converted.push_back({points[i].end_elapsed_ms, 0,
        static_cast<double>(points[i].primary), static_cast<double>(points[i].secondary), {}});
    setPoints(converted);
}
bool CybouObservationChart::segmentBoundary(const ChartPoint& previous, const ChartPoint& next)
{
    return previous.cohort != next.cohort || next.end_elapsed_ms - previous.end_elapsed_ms >= 10000;
}
void CybouObservationChart::setPoints(const std::vector<ChartPoint>& points)
{
    const bool follow = m_selected < 0 || m_selected == static_cast<int>(m_points.size()) - 1;
    const auto selected_time = m_selected >= 0 ? m_points[m_selected].end_elapsed_ms : 0;
    const auto first = points.size() > cybou::MAX_OBSERVATION_POINTS ? points.size() - cybou::MAX_OBSERVATION_POINTS : 0;
    m_points.assign(points.begin() + first, points.end());
    for (auto& point : m_points) for (auto* value : {&point.primary, &point.secondary})
        if (*value && (!std::isfinite(**value) || **value < 0)) value->reset();
    int selected = static_cast<int>(m_points.size()) - 1;
    if (!follow && !m_points.empty()) {
        const auto it = std::lower_bound(m_points.begin(), m_points.end(), selected_time,
            [](const auto& point, uint64_t time) { return point.end_elapsed_ms < time; });
        selected = it == m_points.end() ? selected : static_cast<int>(it - m_points.begin());
    }
    int segments{0}; bool continuous{false};
    for (size_t i = 0; i < m_points.size(); ++i) {
        const bool known = m_points[i].primary.has_value() || m_points[i].secondary.has_value();
        const bool boundary = i && segmentBoundary(m_points[i-1], m_points[i]);
        if (known && (!continuous || boundary)) ++segments;
        continuous = known;
    }
    setProperty("segmentCount", segments);
    setProperty("sampleCount", static_cast<int>(m_points.size()));
    selectPoint(selected);
}
void CybouObservationChart::selectPoint(int index)
{
    m_selected = m_points.empty() ? -1 : std::clamp(index, 0, static_cast<int>(m_points.size()) - 1);
    QString text = m_remote ? tr("Unknown — no reported samples") : tr("Unknown — waiting for a complete interval");
    if (m_selected >= 0) {
        const auto& point = m_points[m_selected];
        const auto value = [&](const auto& v) { return v ? QLocale{}.toString(*v * m_scale, 'f', 1) : tr("Unknown"); };
        if (m_remote) {
            text = tr("Sample at %1 s · %2: %3 %4").arg(point.end_elapsed_ms / 1000).arg(m_primary, value(point.primary), m_unit);
            if (!m_secondary.isEmpty()) text += tr(" · %1: %2 %3").arg(m_secondary, value(point.secondary), m_unit);
            text += QStringLiteral(" · ") + point.scope;
        } else text = tr("Interval ending at %1 s · %2: %3 %4 · %5: %6 %4")
            .arg(point.end_elapsed_ms / 1000).arg(m_primary).arg(value(point.primary), m_unit, m_secondary, value(point.secondary));
    }
    setProperty("selectedIntervalMs", m_selected < 0 ? qulonglong{0} : m_points[m_selected].end_elapsed_ms);
    setAccessibleDescription(text);
    setToolTip(text);
    update();
}
void CybouObservationChart::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Left) selectPoint(m_selected - 1);
    else if (event->key() == Qt::Key_Right) selectPoint(m_selected + 1);
    else if (event->key() == Qt::Key_Home) selectPoint(0);
    else if (event->key() == Qt::Key_End) selectPoint(static_cast<int>(m_points.size()) - 1);
    else { QWidget::keyPressEvent(event); return; }
    event->accept();
    QToolTip::showText(mapToGlobal(QPoint{width() / 2, height()}), accessibleDescription(), this);
}
bool CybouObservationChart::event(QEvent* event)
{
    const bool result = QWidget::event(event);
    if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut) update();
    return result;
}
void CybouObservationChart::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_points.empty()) {
        const auto start = m_points.front().end_elapsed_ms;
        const auto span = std::max<uint64_t>(5000, m_points.back().end_elapsed_ms - start + 5000);
        const auto ratio = std::clamp((event->position().x() - 58) / std::max(1, width() - 70), 0.0, 1.0);
        const auto time = start + static_cast<uint64_t>(std::max(0.0, ratio * span - 2500.0));
        const auto it = std::lower_bound(m_points.begin(), m_points.end(), time,
            [](const auto& point, uint64_t value) { return point.end_elapsed_ms < value; });
        int index = it == m_points.end() ? static_cast<int>(m_points.size()) - 1 : static_cast<int>(it - m_points.begin());
        if (it != m_points.end() && it != m_points.begin() && time - (it - 1)->end_elapsed_ms <= it->end_elapsed_ms - time) --index;
        selectPoint(index);
    }
}
void CybouObservationChart::paintEvent(QPaintEvent*)
{
    QPainter painter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    const auto primary = CybouTheme::color(CybouTheme::BRAND_TEAL);
    const auto secondary = CybouTheme::color(CybouTheme::BRAND_BLUE);
    const auto muted = CybouTheme::color(CybouTheme::TEXT_MUTED);
    QFont label_font = font(); label_font.setPixelSize(12); painter.setFont(label_font);
    const QRectF plot{58, 58, std::max(1, width() - 70), std::max(1, height() - 118)};
    for (int series = 0; series < (m_secondary.isEmpty() ? 1 : 2); ++series) {
        const int y = 15 + series * 20;
        painter.setPen(QPen{series ? secondary : primary, 2, series ? Qt::DashLine : Qt::SolidLine});
        painter.drawLine(10, y, 30, y);
        painter.setPen(muted);
        painter.drawText(38, y + 4, painter.fontMetrics().elidedText(
            (series ? m_secondary : m_primary) + QStringLiteral(" (%1)").arg(m_unit), Qt::ElideRight, std::max(1, width() - 48)));
    }
    painter.setPen(CybouTheme::color(CybouTheme::BORDER)); painter.drawRect(plot);
    const bool has_values = std::any_of(m_points.begin(), m_points.end(), [](const auto& point) { return point.primary || point.secondary; });
    if (!has_values) {
        painter.setPen(muted); painter.drawText(plot.adjusted(8, 4, -8, -4), Qt::AlignCenter | Qt::TextWordWrap, accessibleDescription());
    } else {
        double maximum = 1;
        for (const auto& point : m_points) maximum = std::max(maximum, std::max(point.primary.value_or(0), point.secondary.value_or(0)) * m_scale);
        maximum *= 1.1;
        painter.setPen(muted);
        painter.drawText(QRectF{0, plot.top() - 6, 52, 18}, Qt::AlignRight, QLocale{}.toString(maximum, 'g', 3));
        painter.drawText(QRectF{0, plot.bottom() - 12, 52, 18}, Qt::AlignRight, QStringLiteral("0"));
        const auto start = m_points.front().end_elapsed_ms;
        const auto span = std::max<uint64_t>(5000, m_points.back().end_elapsed_ms - start + 5000);
        const auto x = [&](uint64_t time) { return plot.left() + (time - start + 2500.0) / span * plot.width(); };
        for (int series = m_secondary.isEmpty() ? 0 : 1; series >= 0; --series) {
            QPainterPath path;
            bool connected{false};
            for (size_t i = 0; i < m_points.size(); ++i) {
                const auto& point = m_points[i];
                const auto value = series ? point.secondary : point.primary;
                if (!value) { connected = false; continue; }
                const bool boundary = i && segmentBoundary(m_points[i-1], point);
                const QPointF position{x(point.end_elapsed_ms), plot.bottom() - *value * m_scale / maximum * plot.height()};
                if (!connected || boundary) path.moveTo(position); else path.lineTo(position);
                connected = true;
                if (m_remote) {
                    painter.setPen(QPen{series ? secondary : primary, 1});
                    painter.drawEllipse(position, 2, 2);
                }
                if (static_cast<int>(i) == m_selected) {
                    painter.setPen(QPen{series ? secondary : primary, 2});
                    painter.drawEllipse(position, series ? 5 : 3, series ? 5 : 3);
                }
            }
            painter.setPen(QPen{series ? secondary : primary, 2, series ? Qt::DashLine : Qt::SolidLine});
            painter.drawPath(path);
        }
        painter.setPen(muted);
        painter.drawText(QRectF{plot.left(), plot.bottom() + 5, plot.width() / 2, 18},
            tr("%1 min").arg(QLocale{}.toString(span / 60000.0, 'f', 1)));
        painter.drawText(QRectF{plot.center().x(), plot.bottom() + 5, plot.width() / 2, 18}, Qt::AlignRight, tr("Latest"));
    }
    painter.setPen(muted);
    painter.drawText(QRectF{8, height() - 35.0, width() - 16.0, 32}, Qt::AlignLeft,
        painter.fontMetrics().elidedText(accessibleDescription(), Qt::ElideRight, std::max(1, width() - 16)));
    if (hasFocus()) { painter.setPen(QPen{primary, 2}); painter.drawRect(rect().adjusted(1, 1, -2, -2)); }
}
