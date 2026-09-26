// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUMAINWINDOW_H
#define BITCOIN_QT_CYBOUMAINWINDOW_H

#include <QMainWindow>

#include <memory>

namespace CybouUi {
class StatusStrip;
}

class CybouDesktopModel;
class CybouDesktopController;
class QButtonGroup;
class QCloseEvent;
class QDialog;
class QMenu;
class QStackedWidget;
class QSystemTrayIcon;

class CybouMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit CybouMainWindow(QWidget* parent = nullptr);
    ~CybouMainWindow() override;

    void startRuntime();
    void showDebugWindow();

    /** Page access used by desktop shell smoke tests. */
    CybouDesktopModel* desktopModel() const { return m_desktop_model; }
    QWidget* pageAt(int index) const;
    int currentPageIndex() const;
    int pageCount() const;

Q_SIGNALS:
    void quitRequested();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    CybouDesktopModel* m_desktop_model;
    std::unique_ptr<CybouDesktopController> m_controller;
    QStackedWidget* m_pages;
    QButtonGroup* m_navigation;
    std::unique_ptr<CybouUi::StatusStrip> m_status_strip;
    QSystemTrayIcon* m_tray_icon{nullptr};
    QMenu* m_tray_menu{nullptr};
    QDialog* m_diagnostics{nullptr};

    void buildShell();
    void buildMenus();
    void buildTrayMenu();
    void applyStyle();
    void showPage(int index);
};

#endif // BITCOIN_QT_CYBOUMAINWINDOW_H
