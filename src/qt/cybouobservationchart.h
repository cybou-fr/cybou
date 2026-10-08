// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_QT_OBSERVATION_CHART_H
#define CYBOU_QT_OBSERVATION_CHART_H
#include <QWidget>
#include <QCoreApplication>
#include <cybou/observation_history.h>
class CybouObservationChart final : public QWidget {
    Q_DECLARE_TR_FUNCTIONS(CybouObservationChart)
public:
    CybouObservationChart(QString title, QString primary, QString secondary, QString unit,
        double scale, QWidget* parent);
    void setHistory(const std::vector<cybou::ObservationPoint>& points);
protected:
    bool event(QEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    void selectPoint(int index);
    QString m_primary, m_secondary, m_unit;
    double m_scale;
    std::vector<cybou::ObservationPoint> m_points;
    int m_selected{-1};
};
#endif
