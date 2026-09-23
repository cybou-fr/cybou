// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboumainwindow.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/networkstyle.h>
#include <qt/optionsmodel.h>
#include <qt/pages/homepage.h>
#include <qt/pages/identitypage.h>
#include <qt/pages/networkpage.h>
#include <qt/pages/serviceplaceholderpage.h>
#include <qt/pages/settingspage.h>
#include <qt/rpcconsole.h>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton* NavigationButton(const QString& text, CybouTheme::NavIcon icon, QWidget* parent)
{
    auto* button = new QToolButton{parent};
    button->setText(text);
    button->setIcon(CybouTheme::navIcon(icon));
    button->setIconSize(QSize{22, 22});
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(46);
    return button;
}

} // namespace

CybouMainWindow::CybouMainWindow(
    interfaces::Node& node,
    const PlatformStyle* platform_style,
    const NetworkStyle* network_style,
    QWidget* parent)
    : BitcoinGUI{node, platform_style, network_style, parent},
      m_desktop_model{new CybouDesktopModel{QStringLiteral("CYBOU-DEV"), this}},
      m_pages{new QStackedWidget{this}},
      m_navigation{new QButtonGroup{this}}
{
    setObjectName("cybouMainWindow");
    setWindowTitle(tr("CYBOU — Protected communication infrastructure"));
    setMinimumSize(1040, 720);
    resize(1280, 860);
    buildShell();
    buildMenus();
    applyStyle();
    buildTrayMenu();
}

void CybouMainWindow::setClientModel(ClientModel* client_model, interfaces::BlockAndHeaderTipInfo* tip_info)
{
    m_client_model = client_model;
    BitcoinGUI::setClientModel(client_model, tip_info);
    m_desktop_model->setClientModel(client_model);
}

void CybouMainWindow::showPage(int index)
{
    if (auto* button = m_navigation->button(index)) button->setChecked(true);
    m_pages->setCurrentIndex(index);
    showNormalIfMinimized();
    show();
    raise();
    activateWindow();
}

void CybouMainWindow::buildShell()
{
    if (auto* legacy = takeCentralWidget()) {
        legacy->hide();
        if (auto* console = qobject_cast<RPCConsole*>(legacy)) {
            // Diagnostics is a secondary window owned by the shell (deleted
            // in ~BitcoinGUI). Closing it must hide it — never destroy it —
            // and must never take CYBOU down with it. It can be reopened
            // via Tools -> Node diagnostics any number of times.
            console->setParent(nullptr, Qt::Window);
            console->setAttribute(Qt::WA_DeleteOnClose, false);
        } else {
            legacy->setParent(this);
        }
    }
    for (auto* toolbar : findChildren<QToolBar*>()) toolbar->hide();
    statusBar()->hide();

    auto* shell = new QWidget{this};
    shell->setObjectName("shell");
    auto* shell_layout = new QHBoxLayout{shell};
    shell_layout->setContentsMargins(0, 0, 0, 0);
    shell_layout->setSpacing(0);

    auto* sidebar = new QFrame{shell};
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(228);
    auto* sidebar_layout = new QVBoxLayout{sidebar};
    sidebar_layout->setContentsMargins(18, 24, 18, 22);
    sidebar_layout->setSpacing(7);

    auto* brand_row = new QHBoxLayout;
    auto* logo = new QLabel{sidebar};
    logo->setPixmap(QPixmap{QStringLiteral(":/icons/cybou")}.scaled(54, 54, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* brand_text = new QVBoxLayout;
    auto* brand = new QLabel{tr("CYBOU"), sidebar};
    brand->setObjectName("brand");
    // Network badge comes from the desktop model so pages and shell agree.
    auto* network = new QLabel{m_desktop_model->status().network_name, sidebar};
    network->setObjectName("brandCaption");
    brand_text->addWidget(brand);
    brand_text->addWidget(network);
    brand_row->addWidget(logo);
    brand_row->addLayout(brand_text);
    brand_row->addStretch();
    sidebar_layout->addLayout(brand_row);
    sidebar_layout->addSpacing(22);

    const QList<QPair<QString, CybouTheme::NavIcon>> primary_navigation{
        {tr("Home"), CybouTheme::NavIcon::Home},
        {tr("Identity"), CybouTheme::NavIcon::Identity},
        {tr("Email"), CybouTheme::NavIcon::Email},
        {tr("Storage"), CybouTheme::NavIcon::Storage},
        {tr("Backup"), CybouTheme::NavIcon::Backup},
    };
    for (int index = 0; index < primary_navigation.size(); ++index) {
        const auto& item = primary_navigation.at(index);
        auto* button = NavigationButton(item.first, item.second, sidebar);
        m_navigation->addButton(button, index);
        sidebar_layout->addWidget(button);
    }
    sidebar_layout->addSpacing(12);
    auto* separator = new QFrame{sidebar};
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName("separator");
    sidebar_layout->addWidget(separator);
    sidebar_layout->addSpacing(6);

    auto* network_button = NavigationButton(tr("Network"), CybouTheme::NavIcon::Network, sidebar);
    auto* settings_button = NavigationButton(tr("Settings"), CybouTheme::NavIcon::Settings, sidebar);
    m_navigation->addButton(network_button, 5);
    m_navigation->addButton(settings_button, 6);
    sidebar_layout->addWidget(network_button);
    sidebar_layout->addWidget(settings_button);
    sidebar_layout->addStretch();
    auto* sovereignty = new QLabel{tr("Sovereign technology\nfor a freer internet."), sidebar};
    sovereignty->setObjectName("sidebarFootnote");
    sovereignty->setWordWrap(true);
    sidebar_layout->addWidget(sovereignty);

    auto* home = new HomePage{m_desktop_model,
        [this] { showDebugWindow(); },
        [this] { showPage(1); },
        m_pages};
    auto* identity = new IdentityPage{m_desktop_model, m_pages};
    auto* email = new ServicePlaceholderPage{tr("Email"), tr("Encrypted asynchronous communication."), CybouTheme::NavIcon::Email, m_pages};
    auto* storage = new ServicePlaceholderPage{tr("Storage"), tr("Encrypted distributed object storage."), CybouTheme::NavIcon::Storage, m_pages};
    auto* backup = new ServicePlaceholderPage{tr("Backup"), tr("Resilient encrypted backup built on CYBOU Storage."), CybouTheme::NavIcon::Backup, m_pages};
    auto* network_page = new NetworkPage{m_desktop_model, [this] { showDebugWindow(); }, m_pages};
    auto* settings = new SettingsPage{m_desktop_model, [this] { optionsClicked(); }, [this] { showDebugWindow(); }, m_pages};
    m_pages->addWidget(home);
    m_pages->addWidget(identity);
    m_pages->addWidget(email);
    m_pages->addWidget(storage);
    m_pages->addWidget(backup);
    m_pages->addWidget(network_page);
    m_pages->addWidget(settings);

    connect(m_navigation, &QButtonGroup::idClicked, m_pages, &QStackedWidget::setCurrentIndex);
    m_navigation->button(0)->setChecked(true);

    shell_layout->addWidget(sidebar);
    shell_layout->addWidget(m_pages, 1);
    setCentralWidget(shell);
}

void CybouMainWindow::buildMenus()
{
    menuBar()->clear();
    auto* file = menuBar()->addMenu(tr("File"));
    file->addAction(tr("Hide CYBOU"), this, &BitcoinGUI::toggleHidden);
    file->addSeparator();
    file->addAction(tr("Quit CYBOU"), this, [this] { Q_EMIT quitRequested(); });

    auto* settings = menuBar()->addMenu(tr("Settings"));
    settings->addAction(tr("Preferences"), this, &BitcoinGUI::optionsClicked);

    auto* tools = menuBar()->addMenu(tr("Tools"));
    tools->addAction(tr("Node diagnostics"), this, &BitcoinGUI::showDebugWindow);
    tools->addAction(tr("Developer console"), this, &BitcoinGUI::showDebugWindowActivateConsole);

    auto* help = menuBar()->addMenu(tr("Help"));
    help->addAction(tr("About CYBOU"), this, &BitcoinGUI::aboutClicked);
    help->addAction(tr("About Qt"), qApp, &QApplication::aboutQt);
}

void CybouMainWindow::buildTrayMenu()
{
#ifndef Q_OS_MACOS
    auto* tray_icon = systemTrayIcon();
    auto* menu = trayContextMenu();
    if (!tray_icon || !menu) return;

    // Replace the inherited (Bitcoin-oriented) tray menu with the CYBOU one.
    // No wallet actions are retained.
    menu->clear();
    menu->addAction(tr("Open CYBOU"), this, [this] { showPage(m_pages->currentIndex()); });
    menu->addAction(tr("Network status"), this, [this] { showPage(5); });
    menu->addSeparator();
    menu->addAction(tr("Quit CYBOU"), this, [this] { Q_EMIT quitRequested(); });
    tray_icon->setContextMenu(menu);
#endif
}

void CybouMainWindow::closeEvent(QCloseEvent* event)
{
#ifdef Q_OS_MACOS
    BitcoinGUI::closeEvent(event);
#else
    auto* options = m_client_model ? m_client_model->getOptionsModel() : nullptr;
    if (!options) {
        // Node not initialized yet: closing the window means quitting.
        Q_EMIT quitRequested();
        event->accept();
        return;
    }
    if (options->getMinimizeOnClose() && hasTrayIcon()) {
        // "Keep running in background": hide CYBOU, node continues, tray remains.
        hide();
        event->ignore();
        return;
    }
    BitcoinGUI::closeEvent(event);
#endif
}

void CybouMainWindow::applyStyle()
{
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        CybouTheme::applyTo(*app);
    }
}
