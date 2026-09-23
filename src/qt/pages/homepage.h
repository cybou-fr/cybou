// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_HOMEPAGE_H
#define BITCOIN_QT_PAGES_HOMEPAGE_H

#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QLabel;

class HomePage : public QWidget
{
public:
    HomePage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
        std::function<void()> identity_requested, std::function<void()> wallet_requested,
        QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_network_name;
    QLabel* m_node_state;
    QLabel* m_peer_count;
    QLabel* m_height;
    QLabel* m_identity_state;
    QLabel* m_balance;
    QLabel* m_system_balance;
    QLabel* m_footer_state;
    const std::function<void()> m_diagnostics_requested;
    const std::function<void()> m_identity_requested;
    const std::function<void()> m_wallet_requested;

    void refresh();
};

#endif // BITCOIN_QT_PAGES_HOMEPAGE_H
