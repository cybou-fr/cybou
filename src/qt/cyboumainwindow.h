// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUMAINWINDOW_H
#define BITCOIN_QT_CYBOUMAINWINDOW_H

#include <QMainWindow>
#include <QVector>

#include <filesystem>
#include <memory>

class CybouDesktopModel;
class CybouDesktopController;
class QButtonGroup;
class QCloseEvent;
class QDialog;
class QFrame;
class QCompleter;
class QLabel;
class QLineEdit;
class QMenu;
class QResizeEvent;
class QStackedWidget;
class QSystemTrayIcon;
class QToolButton;

namespace CybouUi {
class Notifier;
}

/** Stable page order of the Identity-centric shell. */
enum class CybouPage {
    Home = 0,
    Mail,
    Files,
    Wallet,
    Identity,
    Diagnostics,
    Settings,
    /** Only for the Identity proven from genesis to hold the PoA finalizer key. */
    NetworkAuthority,
};

class CybouMainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit CybouMainWindow(std::filesystem::path data_directory = {}, QWidget* parent = nullptr);
    ~CybouMainWindow() override;

    void startRuntime();
    void showDebugWindow();
    void showPage(CybouPage page);
    /** Rebuilds the shell so every page picks up a new appearance. */
    void reloadAppearance();

    /** Page access used by desktop shell smoke tests. */
    CybouDesktopModel* desktopModel() const { return m_desktop_model; }
    QWidget* pageAt(int index) const;
    QWidget* page(CybouPage page) const { return pageAt(static_cast<int>(page)); }
    int currentPageIndex() const;
    int pageCount() const;
    CybouUi::Notifier* notifier() const { return m_notifier; }
    QLineEdit* globalSearch() const { return m_global_search; }
    bool sidebarCompact() const { return m_sidebar_compact; }

Q_SIGNALS:
    void quitRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    CybouDesktopModel* m_desktop_model;
    std::unique_ptr<CybouDesktopController> m_controller;
    QStackedWidget* m_pages;
    class CybouActivityButton* m_activity{nullptr};
    QVector<QWidget*> m_page_widgets;
    QButtonGroup* m_navigation;
    QFrame* m_sidebar{nullptr};
    QLabel* m_brand_text{nullptr};
    QLabel* m_brand_name{nullptr};
    QLabel* m_header_title{nullptr};
    QLineEdit* m_global_search{nullptr};
    QCompleter* m_search_completer{nullptr};
    QLabel* m_status_dot{nullptr};
    QLabel* m_status_text{nullptr};
    QToolButton* m_identity_button{nullptr};
    QMenu* m_identity_menu{nullptr};
    QSystemTrayIcon* m_tray_icon{nullptr};
    QMenu* m_tray_menu{nullptr};
    QDialog* m_diagnostics{nullptr};
    bool m_sidebar_compact{false};
    CybouUi::Notifier* m_notifier{nullptr};

    void buildShell();
    QFrame* buildSidebar(QWidget* parent);
    QFrame* buildHeader(QWidget* parent);
    void buildMenus();
    void buildTrayMenu();
    void applyStyle();
    void addPage(QWidget* page, bool scrolls);
    void refreshHeader();
    void rebuildSearchIndex();
    void openSearchResult(const QString& kind, const QString& id);
    void submitSearch(const QString& text);
    void setSidebarCompact(bool compact);
    void runScreenshotHarness(const QString& directory);
};

#endif // BITCOIN_QT_CYBOUMAINWINDOW_H
