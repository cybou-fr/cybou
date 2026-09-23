// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_NETWORKPAGE_H
#define BITCOIN_QT_PAGES_NETWORKPAGE_H

#include <QCoreApplication>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QLabel;

/**
 * Network status and BFT finality view.
 *
 *  - Node metrics (status / connections / height) come straight from the
 *    desktop model; the GUI never invents them.
 *  - Finality is explicit: a height counts only once a BFT certificate
 *    commits it. Until core exposes the finality feed the page says so
 *    instead of guessing.
 *  - Validator facts follow the protocol rules: operator-approved
 *    admission, equal weight 1 per validator, and f = 1 requires at
 *    least 4 validators.
 */
class NetworkPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(NetworkPage)

public:
    NetworkPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_status_metric;
    QLabel* m_connections_metric;
    QLabel* m_height_metric;
    QLabel* m_network;
    QLabel* m_network_id;
    QLabel* m_data_directory;
    QLabel* m_finalized_metric;
    QLabel* m_validators_metric;
    QLabel* m_fault_metric;
    QLabel* m_finality_hint;
    const std::function<void()> m_diagnostics_requested;

    void refresh();
};

#endif // BITCOIN_QT_PAGES_NETWORKPAGE_H
