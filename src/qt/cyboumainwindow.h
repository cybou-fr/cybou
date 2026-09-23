// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUMAINWINDOW_H
#define BITCOIN_QT_CYBOUMAINWINDOW_H

#include <qt/bitcoingui.h>

class CybouDesktopModel;
class QButtonGroup;
class QStackedWidget;

class CybouMainWindow final : public BitcoinGUI
{
    Q_OBJECT

public:
    CybouMainWindow(interfaces::Node& node, const PlatformStyle* platform_style, const NetworkStyle* network_style, QWidget* parent = nullptr);

    void setClientModel(ClientModel* client_model = nullptr, interfaces::BlockAndHeaderTipInfo* tip_info = nullptr) override;

private:
    CybouDesktopModel* m_desktop_model;
    QStackedWidget* m_pages;
    QButtonGroup* m_navigation;

    void buildShell();
    void buildMenus();
    void applyStyle();
};

#endif // BITCOIN_QT_CYBOUMAINWINDOW_H
