// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_NETWORKPAGE_H
#define BITCOIN_QT_PAGES_NETWORKPAGE_H

#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QLabel;

class NetworkPage : public QWidget
{
public:
    NetworkPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_network;
    QLabel* m_status;
    QLabel* m_connections;
    QLabel* m_height;
    QLabel* m_data_directory;
    const std::function<void()> m_diagnostics_requested;

    void refresh();
};

#endif // BITCOIN_QT_PAGES_NETWORKPAGE_H
