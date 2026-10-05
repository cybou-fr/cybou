// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_DIAGNOSTICSPAGE_H
#define CYBOU_QT_PAGES_DIAGNOSTICSPAGE_H

#include <QCoreApplication>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QLabel;
class QVBoxLayout;

/**
 * Diagnostics: the only normal page with node and network facts (node
 * state, peers, finalized height, PoA finality, network ID, data
 * directory). Home and product pages never show these.
 */
class DiagnosticsPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(DiagnosticsPage)

public:
    DiagnosticsPage(CybouDesktopModel* model, std::function<void()> diagnostics_window_requested,
        QWidget* parent = nullptr);

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

    void refresh();
};

#endif // CYBOU_QT_PAGES_DIAGNOSTICSPAGE_H
