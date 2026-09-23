// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboumainwindow.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/networkstyle.h>
#include <qt/pages/homepage.h>
#include <qt/pages/identitypage.h>
#include <qt/pages/networkpage.h>
#include <qt/pages/serviceplaceholderpage.h>
#include <qt/pages/settingspage.h>
#include <qt/rpcconsole.h>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QPixmap>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton* NavigationButton(const QString& text, const QIcon& icon, QWidget* parent)
{
    auto* button = new QToolButton{parent};
    button->setText(text);
    button->setIcon(icon);
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
      m_desktop_model{new CybouDesktopModel{"CYBOU-DEV", this}},
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
}

void CybouMainWindow::setClientModel(ClientModel* client_model, interfaces::BlockAndHeaderTipInfo* tip_info)
{
    BitcoinGUI::setClientModel(client_model, tip_info);
    m_desktop_model->setClientModel(client_model);
}

void CybouMainWindow::buildShell()
{
    if (auto* legacy = takeCentralWidget()) {
        legacy->hide();
        if (qobject_cast<RPCConsole*>(legacy)) {
            legacy->setParent(nullptr, Qt::Window);
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
    logo->setPixmap(QPixmap{":/icons/cybou"}.scaled(54, 54, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    auto* brand_text = new QVBoxLayout;
    auto* brand = new QLabel{tr("CYBOU"), sidebar};
    brand->setObjectName("brand");
    auto* network = new QLabel{tr("CYBOU-DEV"), sidebar};
    network->setObjectName("brandCaption");
    brand_text->addWidget(brand);
    brand_text->addWidget(network);
    brand_row->addWidget(logo);
    brand_row->addLayout(brand_text);
    brand_row->addStretch();
    sidebar_layout->addLayout(brand_row);
    sidebar_layout->addSpacing(22);

    const QList<QPair<QString, QStyle::StandardPixmap>> primary_navigation{
        {tr("Home"), QStyle::SP_DirHomeIcon},
        {tr("Identity"), QStyle::SP_FileDialogInfoView},
        {tr("Email"), QStyle::SP_MessageBoxInformation},
        {tr("Storage"), QStyle::SP_DriveHDIcon},
        {tr("Backup"), QStyle::SP_DriveNetIcon},
    };
    for (int index = 0; index < primary_navigation.size(); ++index) {
        const auto& item = primary_navigation.at(index);
        auto* button = NavigationButton(item.first, style()->standardIcon(item.second), sidebar);
        m_navigation->addButton(button, index);
        sidebar_layout->addWidget(button);
    }
    sidebar_layout->addSpacing(12);
    auto* separator = new QFrame{sidebar};
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName("separator");
    sidebar_layout->addWidget(separator);
    sidebar_layout->addSpacing(6);

    auto* network_button = NavigationButton(tr("Network"), style()->standardIcon(QStyle::SP_ComputerIcon), sidebar);
    auto* settings_button = NavigationButton(tr("Settings"), style()->standardIcon(QStyle::SP_FileDialogDetailedView), sidebar);
    m_navigation->addButton(network_button, 5);
    m_navigation->addButton(settings_button, 6);
    sidebar_layout->addWidget(network_button);
    sidebar_layout->addWidget(settings_button);
    sidebar_layout->addStretch();
    auto* sovereignty = new QLabel{tr("Sovereign technology\nfor resilient communication."), sidebar};
    sovereignty->setObjectName("sidebarFootnote");
    sovereignty->setWordWrap(true);
    sidebar_layout->addWidget(sovereignty);

    auto* home = new HomePage{m_desktop_model,
        [this] { showDebugWindow(); },
        [this] {
            m_navigation->button(1)->setChecked(true);
            m_pages->setCurrentIndex(1);
        },
        m_pages};
    auto* identity = new IdentityPage{m_desktop_model, m_pages};
    auto* email = new ServicePlaceholderPage{tr("Email"), tr("Encrypted asynchronous messaging registered as a first-class CYBOU protocol operation."), m_pages};
    auto* storage = new ServicePlaceholderPage{tr("Storage"), tr("Distributed object storage for encrypted content and future attachments."), m_pages};
    auto* backup = new ServicePlaceholderPage{tr("Backup"), tr("Resilient encrypted backup built on the future storage layer."), m_pages};
    auto* network_page = new NetworkPage{m_desktop_model, [this] { showDebugWindow(); }, m_pages};
    auto* settings = new SettingsPage{[this] { optionsClicked(); }, [this] { showDebugWindow(); }, m_pages};
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
    file->addAction(tr("Hide"), this, &BitcoinGUI::toggleHidden);
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

void CybouMainWindow::applyStyle()
{
    setStyleSheet(R"(
        QMainWindow#cybouMainWindow, QWidget#shell, QStackedWidget { background: #f6f8fa; color: #0b1730; }
        QMenuBar { background: #ffffff; border-bottom: 1px solid #e4e9ee; padding: 4px 8px; }
        QMenuBar::item:selected, QMenu::item:selected { background: #dff5f1; color: #087a74; }
        QFrame#sidebar { background: #ffffff; border-right: 1px solid #e4e9ee; }
        QLabel#brand { font-size: 23px; font-weight: 800; letter-spacing: 1px; color: #07162b; }
        QLabel#brandCaption, QLabel#sidebarFootnote, QLabel#mutedText { color: #718096; }
        QLabel#brandCaption { font-size: 11px; font-weight: 700; color: #0c8e86; }
        QLabel#sidebarFootnote { font-size: 12px; line-height: 1.4; }
        QToolButton { border: 0; border-radius: 10px; padding: 9px 12px; text-align: left; color: #42526d; font-size: 14px; }
        QToolButton:hover { background: #eef7f6; color: #087a74; }
        QToolButton:checked { background: #d9f4ef; color: #087a74; font-weight: 700; }
        QFrame#separator { color: #e4e9ee; }
        QFrame#card { background: #ffffff; border: 1px solid #dfe5ea; border-radius: 14px; }
        QLabel#eyebrow { color: #31506e; font-size: 11px; font-weight: 800; letter-spacing: 3px; }
        QLabel#heroTitle { color: #07162b; font-size: 30px; font-weight: 800; }
        QLabel#heroSubtitle { color: #42526d; font-size: 14px; }
        QLabel#pageTitle { color: #07162b; font-size: 28px; font-weight: 800; }
        QLabel#sectionTitle { color: #14233b; font-size: 19px; font-weight: 750; }
        QLabel#cardLabel { color: #42526d; font-size: 12px; font-weight: 700; }
        QLabel#cardTitle { color: #07162b; font-size: 21px; font-weight: 800; }
        QLabel#serviceTitle { color: #07162b; font-size: 16px; font-weight: 750; }
        QLabel#bodyText { color: #31415d; font-size: 14px; }
        QLabel#metric { color: #07162b; font-size: 26px; font-weight: 800; }
        QLabel#statusBadge { background: #d9f4ef; color: #087a74; border-radius: 13px; padding: 6px 12px; font-weight: 700; }
        QLabel#neutralBadge { background: #eef1f5; color: #68768c; border-radius: 11px; padding: 4px 9px; }
        QPushButton { min-height: 34px; border-radius: 8px; padding: 4px 16px; font-weight: 700; }
        QPushButton#primaryButton { background: #087f79; color: white; border: 1px solid #087f79; }
        QPushButton#primaryButton:hover { background: #066b66; }
        QPushButton#secondaryButton { background: #ffffff; color: #176d69; border: 1px solid #cfd9df; }
        QPushButton#secondaryButton:hover { background: #eef8f6; border-color: #9bcfc9; }
        QPushButton:disabled { background: #edf0f3; color: #98a3b3; border-color: #e1e5e9; }
    )");
}
