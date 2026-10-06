// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_DIAGNOSTICSPAGE_H
#define CYBOU_QT_PAGES_DIAGNOSTICSPAGE_H

#include <QCoreApplication>
#include <QWidget>


class CybouDesktopModel;
class QLabel;
class QVBoxLayout;
class QTimer;

/** Technical panel embedded in Network Advanced. */
class DiagnosticsPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(DiagnosticsPage)

public:
    explicit DiagnosticsPage(CybouDesktopModel* model, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    CybouDesktopModel* const m_model;
    QLabel* m_node{nullptr};
    QLabel* m_network{nullptr};
    QLabel* m_peers{nullptr};
    QLabel* m_height{nullptr};
    QLabel* m_finality{nullptr};
    QLabel* m_fixture{nullptr};
    QVBoxLayout* m_rows{nullptr};
    QVBoxLayout* m_services{nullptr};

    QTimer* m_refresh_timer{nullptr};
    void scheduleRefresh();
    void refresh();
};

#endif // CYBOU_QT_PAGES_DIAGNOSTICSPAGE_H
