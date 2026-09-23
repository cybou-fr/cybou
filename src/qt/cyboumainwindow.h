// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUMAINWINDOW_H
#define BITCOIN_QT_CYBOUMAINWINDOW_H

#include <qt/bitcoingui.h>

class ClientModel;
class CybouDesktopModel;
class QButtonGroup;
class QCloseEvent;
class QStackedWidget;

class CybouMainWindow final : public BitcoinGUI
{
    Q_OBJECT

public:
    CybouMainWindow(interfaces::Node& node, const PlatformStyle* platform_style, const NetworkStyle* network_style, QWidget* parent = nullptr);

    void setClientModel(ClientModel* client_model = nullptr, interfaces::BlockAndHeaderTipInfo* tip_info = nullptr) override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    CybouDesktopModel* m_desktop_model;
    ClientModel* m_client_model{nullptr};
    QStackedWidget* m_pages;
    QButtonGroup* m_navigation;

    void buildShell();
    void buildMenus();
    void buildTrayMenu();
    void applyStyle();
    void showPage(int index);
};

#endif // BITCOIN_QT_CYBOUMAINWINDOW_H
