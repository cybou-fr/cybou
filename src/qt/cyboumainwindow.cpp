// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboumainwindow.h>

#include <qt/cyboudesktopcontroller.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cyboustrip.h>
#include <qt/networkstyle.h>
#include <qt/optionsmodel.h>
#include <qt/pages/backuppage.h>
#include <qt/pages/emailpage.h>
#include <qt/pages/homepage.h>
#include <qt/pages/identitypage.h>
#include <qt/pages/networkpage.h>
#include <qt/pages/settingspage.h>
#include <qt/pages/storagepage.h>
#include <qt/pages/walletpage.h>
#include <qt/rpcconsole.h>

#include <cybou/mail_service.h>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
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
      m_controller{std::make_unique<CybouDesktopController>(m_desktop_model)},
      m_pages{new QStackedWidget{this}},
      m_navigation{new QButtonGroup{this}}
{
    setObjectName("cybouMainWindow");
    setWindowTitle(tr("CYBOU — Protected communication infrastructure"));
    setMinimumSize(1040, 720);
    const int screenshot_width = qEnvironmentVariableIntValue("CYBOU_SCREENSHOT_WIDTH");
    const int screenshot_height = qEnvironmentVariableIntValue("CYBOU_SCREENSHOT_HEIGHT");
    resize(screenshot_width > 0 ? screenshot_width : 1280,
        screenshot_height > 0 ? screenshot_height : 860);
    buildShell();
    buildMenus();
    applyStyle();
    buildTrayMenu();

    // Dev-only screenshot harness: capture every page and quit.
    const auto shot_dir = QProcessEnvironment::systemEnvironment().value(QStringLiteral("CYBOU_SCREENSHOT_DIR"));
    if (!shot_dir.isEmpty()) {
        const auto delay_ms = qEnvironmentVariableIntValue("CYBOU_SCREENSHOT_DELAY_MS");
        QTimer::singleShot(delay_ms > 0 ? delay_ms : 2500, this, [this, shot_dir] {
            QDir{}.mkpath(shot_dir);
            for (int i = 0; i < m_pages->count(); ++i) {
                m_pages->setCurrentIndex(i);
                if (auto* button = m_navigation->button(i)) button->setChecked(true);
                qApp->processEvents();
                QString slug = m_navigation->button(i) ? m_navigation->button(i)->text() : QStringLiteral("page-%1").arg(i);
                slug = slug.toLower();
                slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
                slug = slug.mid(0, 24).replace(QRegularExpression(QStringLiteral("(^-|-$)")), QStringLiteral(""));
                if (slug.isEmpty()) slug = QStringLiteral("page-%1").arg(i);
                this->grab().save(QDir{shot_dir}.filePath(QStringLiteral("%1-%2.png").arg(i).arg(slug)));
                // For the email page also capture the composer surface.
                if (slug == QLatin1String{"email"}) {
                    if (qEnvironmentVariableIsSet("CYBOU_SCREENSHOT_EMAIL_MESSAGE")) {
                        if (auto* email = static_cast<EmailPage*>(m_pages->widget(i))) {
                            email->loadScreenshotFixture();
                            qApp->processEvents();
                            this->grab().save(QDir{shot_dir}.filePath(QStringLiteral("%1-%2-message.png").arg(i).arg(slug)));
                        }
                    }
                    const auto buttons = m_pages->widget(i)->findChildren<QPushButton*>();
                    for (auto* button : buttons) {
                        if (button->text() == tr("Compose")) { button->click(); break; }
                    }
                    qApp->processEvents();
                    this->grab().save(QDir{shot_dir}.filePath(QStringLiteral("%1-%2-compose.png").arg(i).arg(slug)));
                }
            }
            // The diagnostics window is a secondary top-level; capture it too.
            const auto top_levels = qApp->topLevelWidgets();
            for (QWidget* widget : top_levels) {
                if (widget->objectName() != QLatin1String{"RPCConsole"}) continue;
                widget->show();
                qApp->processEvents();
                widget->grab().save(QDir{shot_dir}.filePath(QStringLiteral("8-diagnostics.png")));
                widget->hide();
            }
            qApp->quit();
        });
    }
}

CybouMainWindow::~CybouMainWindow()
{
}

void CybouMainWindow::setClientModel(ClientModel* client_model, interfaces::BlockAndHeaderTipInfo* tip_info)
{
    m_client_model = client_model;
    BitcoinGUI::setClientModel(client_model, tip_info);
    m_controller->setClientModel(client_model);
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

QWidget* CybouMainWindow::pageAt(int index) const
{
    return m_pages->widget(index);
}

int CybouMainWindow::currentPageIndex() const
{
    return m_pages->currentIndex();
}

int CybouMainWindow::pageCount() const
{
    return m_pages->count();
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
    auto* shell_layout = new QVBoxLayout{shell};
    shell_layout->setContentsMargins(0, 0, 0, 0);
    shell_layout->setSpacing(0);

    // Top status strip: connection, sync, unread, balances (sketch header).
    const auto unread_counter = [this] {
        if (auto* service = m_desktop_model->mailService()) {
            int unread = 0;
            for (const auto& item : service->GetMessages(cybou::MailFolder::INBOX)) {
                if (!item.read) ++unread;
            }
            return unread;
        }
        return 0;
    };
    m_status_strip = std::make_unique<CybouUi::StatusStrip>(m_desktop_model, unread_counter, shell);
    shell_layout->addWidget(m_status_strip->frame());

    auto* body = new QWidget{shell};
    auto* body_layout = new QHBoxLayout{body};
    body_layout->setContentsMargins(0, 0, 0, 0);
    body_layout->setSpacing(0);

    auto* sidebar = new QFrame{body};
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(228);
    auto* sidebar_layout = new QVBoxLayout{sidebar};
    sidebar_layout->setContentsMargins(18, 24, 18, 22);
    sidebar_layout->setSpacing(7);

    auto* brand_row = new QHBoxLayout;
    // The white logomark is transparent-background art: it needs the dark
    // squircle tile (mirrors the site's .logo-tile) to read on the light shell.
    // The tile is pre-rendered into a pixmap (see CybouTheme::logoTile):
    // stylesheet-painted frame tiles do not paint reliably inside styled cards.
    auto* logo_tile = new QLabel{sidebar};
    logo_tile->setPixmap(CybouTheme::logoTile({44, 44}, 11, {30, 30}));
    logo_tile->setFixedSize(44, 44);
    auto* brand_text = new QVBoxLayout;
    auto* brand = new QLabel{tr("CYBOU"), sidebar};
    brand->setObjectName("brand");
    // Network badge comes from the desktop model so pages and shell agree.
    auto* network = new QLabel{m_desktop_model->status().network_name, sidebar};
    network->setObjectName("brandCaption");
    brand_text->addWidget(brand);
    brand_text->addWidget(network);
    brand_row->addWidget(logo_tile);
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
        {tr("Wallet"), CybouTheme::NavIcon::Wallet},
    };
    for (int index = 0; index < primary_navigation.size(); ++index) {
        const auto& item = primary_navigation.at(index);
        auto* button = NavigationButton(item.first, item.second, sidebar);
        button->setObjectName(QStringLiteral("navButton%1").arg(index));
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
    network_button->setObjectName(QStringLiteral("navButton6"));
    settings_button->setObjectName(QStringLiteral("navButton7"));
    m_navigation->addButton(network_button, 6);
    m_navigation->addButton(settings_button, 7);
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
        [this] { showPage(5); },
        m_pages};
    auto* identity = new IdentityPage{m_desktop_model, m_pages};
    auto* email = new EmailPage{m_desktop_model, [this] { showPage(1); }, m_pages};
    auto* storage = new StoragePage{m_desktop_model, m_pages};
    auto* backup = new BackupPage{m_desktop_model, m_pages};
    auto* network_page = new NetworkPage{m_desktop_model, [this] { showDebugWindow(); }, m_pages};
    auto* wallet = new WalletPage{m_desktop_model, m_pages};
    auto* settings = new SettingsPage{m_desktop_model, [this] { optionsClicked(); }, [this] { showDebugWindow(); }, m_pages};
    m_pages->addWidget(home);
    m_pages->addWidget(identity);
    m_pages->addWidget(email);
    m_pages->addWidget(storage);
    m_pages->addWidget(backup);
    m_pages->addWidget(wallet);
    m_pages->addWidget(network_page);
    m_pages->addWidget(settings);

    connect(m_navigation, &QButtonGroup::idClicked, m_pages, &QStackedWidget::setCurrentIndex);
    m_navigation->button(0)->setChecked(true);

    // Pages stack on a scroll surface: the native pages are taller than the
    // window at the default size, and a stacked widget without scrolling
    // squeezes card layouts until labels overlap and clip.
    auto* page_scroll = new QScrollArea{shell};
    page_scroll->setObjectName(QStringLiteral("pageScroll"));
    page_scroll->setWidgetResizable(true);
    page_scroll->setFrameShape(QFrame::NoFrame);

    page_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    page_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_pages->setMinimumWidth(0);
    m_pages->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    page_scroll->setWidget(m_pages);

    body_layout->addWidget(sidebar);
    body_layout->addWidget(page_scroll, 1);
    shell_layout->addWidget(body, 1);
    setCentralWidget(shell);
}

void CybouMainWindow::buildMenus()
{
    menuBar()->clear();
    auto* file = menuBar()->addMenu(tr("File"));
    file->addAction(tr("Hide CYBOU"), this, &BitcoinGUI::toggleHidden);
    // No File->Quit: the node can only be shut down from the tray icon's
    // context menu, so closing the window can never accidentally stop it.

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
