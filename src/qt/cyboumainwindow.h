// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_CYBOUMAINWINDOW_H
#define CYBOU_QT_CYBOUMAINWINDOW_H

#include <QElapsedTimer>
#include <QMainWindow>
#include <QMetaObject>
#include <QTranslator>
#include <QSet>
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
    Network,
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
    void showPage(CybouPage page);
    void showNetworkDiagnostics();
    /** Rebuilds the shell so every page picks up a new appearance. */
    void reloadAppearance();
    void setLanguage(const QString& language);

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
    /** Application-wide input watcher for the inactivity lock. */
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    CybouDesktopModel* m_desktop_model;
    std::unique_ptr<CybouDesktopController> m_controller;
    QStackedWidget* m_pages;
    class CybouActivityButton* m_activity{nullptr};
    QVector<QWidget*> m_page_widgets;
    QButtonGroup* m_navigation;
    QFrame* m_sidebar{nullptr};
    QWidget* m_authority_section{nullptr};
    QLabel* m_administration_label{nullptr};
    QLabel* m_brand_text{nullptr};
    QLabel* m_brand_name{nullptr};
    QLabel* m_header_title{nullptr};
    QLineEdit* m_global_search{nullptr};
    QCompleter* m_search_completer{nullptr};
    QTimer* m_search_index_timer{nullptr};
    QLabel* m_status_dot{nullptr};
    QLabel* m_status_text{nullptr};
    QToolButton* m_identity_button{nullptr};
    QMenu* m_identity_menu{nullptr};
    QSystemTrayIcon* m_tray_icon{nullptr};
    QMenu* m_tray_menu{nullptr};
    QDialog* m_application_loading{nullptr};
    QFrame* m_recovery_banner{nullptr};
    QLabel* m_recovery_text{nullptr};
    bool m_application_loading_dismissed{false};
    void refreshApplicationLoading();
    bool m_sidebar_compact{false};
    CybouUi::Notifier* m_notifier{nullptr};
    /** Notified items ("mail:<id>", "pay:<id>"); primed with what exists at unlock. */
    QSet<QString> m_notified;
    QTranslator m_french_translator;
    QVector<QMetaObject::Connection> m_shell_connections;
    bool m_notify_primed{false};
    bool m_constructed{false};
    QString m_notification_target;
    QElapsedTimer m_last_input;

    void buildShell();
    /** System notifications for new mail/payments/problems, and the inactivity lock. */
    void setupNotificationsAndLock();
    void openNotificationTarget();
    QFrame* buildSidebar(QWidget* parent);
    QFrame* buildHeader(QWidget* parent);
    void buildMenus();
    void buildTrayMenu();
    void rebuildTrayMenu();
    void applyStyle();
    void addPage(QWidget* page, bool scrolls);
    void refreshHeader();
    void queueSearchIndexRebuild();
    void rebuildSearchIndex();
    void updateSearchScope();
    void openSearchResult(const QString& kind, const QString& id);
    void submitSearch(const QString& text);
    void setSidebarCompact(bool compact);
};

#endif // CYBOU_QT_CYBOUMAINWINDOW_H
