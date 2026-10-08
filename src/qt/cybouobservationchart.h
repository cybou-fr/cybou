// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_QT_OBSERVATION_CHART_H
#define CYBOU_QT_OBSERVATION_CHART_H
#include <QWidget>
#include <QCoreApplication>
#include <cybou/observation_history.h>
#include <cybou/network_observation_history.h>
class CybouObservationChart final : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(CybouObservationChart)
public:
    CybouObservationChart(QString title, QString primary, QString secondary, QString unit,
        double scale, QWidget* parent);
    enum class RemoteMetric { STORAGE, TRAFFIC, CPU };
    void setRemoteHistory(const std::vector<cybou::NetworkObservationPoint>& points, RemoteMetric metric);
    void setHistory(const std::vector<cybou::ObservationPoint>& points);
protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    struct ChartPoint {
        uint64_t end_elapsed_ms{0}, cohort{0};
        std::optional<double> primary, secondary;
        QString scope;
    };
    static bool segmentBoundary(const ChartPoint& previous, const ChartPoint& next);
    void setPoints(const std::vector<ChartPoint>& points);
    void selectPoint(int index);
    QString m_primary, m_secondary, m_unit;
    double m_scale;
    std::vector<ChartPoint> m_points;
    bool m_remote{false};
    int m_selected{-1};
};
#endif
