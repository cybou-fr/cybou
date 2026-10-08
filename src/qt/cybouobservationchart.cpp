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
void CybouObservationChart::setHistory(const std::vector<cybou::ObservationPoint>& points)
{
    const bool follow = m_selected < 0 || m_selected == static_cast<int>(m_points.size()) - 1;
    const auto selected_time = m_selected >= 0 ? m_points[m_selected].end_elapsed_ms : 0;
    const auto first = points.size() > cybou::MAX_OBSERVATION_POINTS ? points.size() - cybou::MAX_OBSERVATION_POINTS : 0;
    m_points.assign(points.begin() + first, points.end());
    int selected = static_cast<int>(m_points.size()) - 1;
    if (!follow && !m_points.empty()) {
        const auto it = std::lower_bound(m_points.begin(), m_points.end(), selected_time,
            [](const auto& point, uint64_t time) { return point.end_elapsed_ms < time; });
        selected = it == m_points.end() ? selected : static_cast<int>(it - m_points.begin());
    }
    setProperty("sampleCount", static_cast<int>(m_points.size()));
    selectPoint(selected);
}
void CybouObservationChart::selectPoint(int index)
{
    m_selected = m_points.empty() ? -1 : std::clamp(index, 0, static_cast<int>(m_points.size()) - 1);
    QString text = tr("Unknown — waiting for a complete interval");
    if (m_selected >= 0) {
        const auto& point = m_points[m_selected];
        text = tr("Interval ending at %1 s · %2: %3 %4 · %5: %6 %4")
            .arg(point.end_elapsed_ms / 1000).arg(m_primary)
            .arg(QLocale{}.toString(point.primary * m_scale, 'f', 1), m_unit, m_secondary,
                 QLocale{}.toString(point.secondary * m_scale, 'f', 1));
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
    if (!m_points.empty()) selectPoint(static_cast<int>((event->position().x() - 58) /
        std::max(1, width() - 70) * m_points.size()));
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
    for (int series = 0; series < 2; ++series) {
        const int y = 15 + series * 20;
        painter.setPen(QPen{series ? secondary : primary, 2, series ? Qt::DashLine : Qt::SolidLine});
        painter.drawLine(10, y, 30, y);
        painter.setPen(muted);
        painter.drawText(38, y + 4, painter.fontMetrics().elidedText(
            (series ? m_secondary : m_primary) + QStringLiteral(" (%1)").arg(m_unit), Qt::ElideRight, std::max(1, width() - 48)));
    }
    painter.setPen(CybouTheme::color(CybouTheme::BORDER)); painter.drawRect(plot);
    if (m_points.empty()) {
        painter.setPen(muted); painter.drawText(plot.adjusted(8, 4, -8, -4), Qt::AlignCenter | Qt::TextWordWrap, accessibleDescription());
    } else {
        double maximum = 1;
        for (const auto& point : m_points) maximum = std::max(maximum, std::max(point.primary, point.secondary) * m_scale);
        maximum *= 1.1;
        painter.setPen(muted);
        painter.drawText(QRectF{0, plot.top() - 6, 52, 18}, Qt::AlignRight, QLocale{}.toString(maximum, 'g', 3));
        painter.drawText(QRectF{0, plot.bottom() - 12, 52, 18}, Qt::AlignRight, QStringLiteral("0"));
        for (int series = 1; series >= 0; --series) {
            QPainterPath path;
            for (size_t i = 0; i < m_points.size(); ++i) {
                const auto& point = m_points[i];
                const QPointF position{plot.left() + (i + 0.5) / m_points.size() * plot.width(),
                    plot.bottom() - (series ? point.secondary : point.primary) * m_scale / maximum * plot.height()};
                if (i == 0) path.moveTo(position); else path.lineTo(position);
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
            tr("%1 min").arg(QLocale{}.toString(m_points.size() / 12.0, 'f', 1)));
        painter.drawText(QRectF{plot.center().x(), plot.bottom() + 5, plot.width() / 2, 18}, Qt::AlignRight, tr("Latest"));
    }
    painter.setPen(muted);
    painter.drawText(QRectF{8, height() - 35.0, width() - 16.0, 32}, Qt::AlignLeft,
        painter.fontMetrics().elidedText(accessibleDescription(), Qt::ElideRight, std::max(1, width() - 16)));
    if (hasFocus()) { painter.setPen(QPen{primary, 2}); painter.drawRect(rect().adjusted(1, 1, -2, -2)); }
}
