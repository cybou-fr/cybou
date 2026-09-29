// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboumainwindow.h>

#include <qt/cyboudesktopcontroller.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouuifixtures.h>
#include <qt/cybouui.h>
#include <qt/pages/diagnosticspage.h>
#include <qt/pages/emailpage.h>
#include <qt/pages/homepage.h>
#include <qt/pages/identitypage.h>
#include <qt/pages/onboardingview.h>
#include <qt/pages/settingspage.h>
#include <qt/pages/storagepage.h>
#include <qt/pages/walletpage.h>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidgetAction>

namespace {

/** Below this window width the sidebar collapses to icons. */
constexpr int kCompactSidebarWidth = 1180;
constexpr int kSidebarWidth = 220;
constexpr int kCompactSidebar = 72;

QToolButton* NavigationButton(const QString& text, CybouTheme::NavIcon icon, QWidget* parent)
{
    auto* button = new QToolButton{parent};
    QString label = text;
    button->setText(label.replace(QLatin1Char{'&'}, QStringLiteral("&&")));
    button->setToolTip(text);
    button->setAccessibleName(text);
    button->setIcon(CybouTheme::navIcon(icon));
    button->setIconSize(QSize{22, 22});
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setMinimumHeight(44);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

QString PageTitle(CybouPage page)
{
    switch (page) {
    case CybouPage::Home: return CybouMainWindow::tr("Home");
    case CybouPage::Mail: return CybouMainWindow::tr("Mail");
    case CybouPage::Files: return CybouMainWindow::tr("Files");
    case CybouPage::Wallet: return CybouMainWindow::tr("Wallet");
    case CybouPage::Identity: return CybouMainWindow::tr("Identity & Security");
    case CybouPage::Diagnostics: return CybouMainWindow::tr("Diagnostics");
    case CybouPage::Settings: return CybouMainWindow::tr("Settings");
    }
    return {};
}

void Restyle(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

CybouMainWindow::CybouMainWindow(std::filesystem::path data_directory, QWidget* parent)
    : QMainWindow{parent},
      m_desktop_model{new CybouDesktopModel{QStringLiteral("CYBOU DEV"), this}},
      m_controller{std::make_unique<CybouDesktopController>(m_desktop_model, std::move(data_directory))},
      m_pages{new QStackedWidget{this}},
      m_navigation{new QButtonGroup{this}}
{
    connect(m_controller.get(), &CybouDesktopController::startupFailed, this,
        [this](const QString& reason) {
            QTimer::singleShot(0, this, [this, reason] {
                QMessageBox::critical(this, tr("CYBOU could not start"), reason);
            });
        });

    // Deterministic UI fixtures replace the runtime entirely (no core calls).
    const QString fixture = CybouUiFixtures::requestedFixture();
    if (!fixture.isEmpty() && CybouUiFixtures::apply(*m_desktop_model, fixture)) {
        new CybouUiFixtures::Driver{m_desktop_model, this};
    }

    setObjectName("cybouMainWindow");
    setWindowTitle(tr("CYBOU"));
    setMinimumSize(1040, 720);
    const int screenshot_width = qEnvironmentVariableIntValue("CYBOU_SCREENSHOT_WIDTH");
    const int screenshot_height = qEnvironmentVariableIntValue("CYBOU_SCREENSHOT_HEIGHT");
    resize(screenshot_width > 0 ? screenshot_width : 1280,
        screenshot_height > 0 ? screenshot_height : 860);
    buildShell();
    buildMenus();
    applyStyle();
    buildTrayMenu();

    if (m_desktop_model->fixtureMode()) {
        const QString initial = CybouUiFixtures::initialPage(fixture);
        if (initial == QLatin1String{"mail"}) showPage(CybouPage::Mail);
        else if (initial == QLatin1String{"files"}) showPage(CybouPage::Files);
    }

    const QString shot_dir = qEnvironmentVariable("CYBOU_SCREENSHOT_DIR");
    if (!shot_dir.isEmpty()) {
        const int delay_ms = qEnvironmentVariableIntValue("CYBOU_SCREENSHOT_DELAY_MS");
        QTimer::singleShot(delay_ms > 0 ? delay_ms : 1500, this, [this, shot_dir] { runScreenshotHarness(shot_dir); });
    }
}

CybouMainWindow::~CybouMainWindow() = default;

void CybouMainWindow::startRuntime()
{
    if (m_desktop_model->fixtureMode()) return;
    m_controller->start();
}

void CybouMainWindow::showDebugWindow()
{
    if (!m_diagnostics) {
        m_diagnostics = new QDialog{this, Qt::Window};
        m_diagnostics->setObjectName(QStringLiteral("CYBOUDiagnostics"));
        m_diagnostics->setWindowTitle(tr("CYBOU diagnostics"));
        m_diagnostics->resize(640, 360);
        auto* layout = new QVBoxLayout{m_diagnostics};
        auto* details = new QTextEdit{m_diagnostics};
        details->setObjectName(QStringLiteral("diagnosticsDetails"));
        details->setReadOnly(true);
        layout->addWidget(details);
        const auto update_details = [this, details] {
            const auto& status = m_desktop_model->status();
            details->setPlainText(tr("Network: %1\nNetwork ID: %2\nNode running: %3\nPeers: %4\n"
                                     "Finalized height: %5\nData directory: %6")
                .arg(status.network_name, status.network_id,
                    status.node_running ? tr("yes") : tr("no"))
                .arg(status.peer_count)
                .arg(status.finality_known ? QString::number(status.finalized_height) : tr("unknown"))
                .arg(status.data_directory));
        };
        connect(m_desktop_model, &CybouDesktopModel::statusChanged, m_diagnostics, update_details);
        update_details();
    }
    m_diagnostics->show();
    m_diagnostics->raise();
    m_diagnostics->activateWindow();
}

void CybouMainWindow::showPage(CybouPage page)
{
    const int index = static_cast<int>(page);
    if (auto* button = m_navigation->button(index)) button->setChecked(true);
    m_pages->setCurrentIndex(index);
    refreshHeader();
    if (isMinimized()) showNormal();
}

QWidget* CybouMainWindow::pageAt(int index) const
{
    return index >= 0 && index < m_page_widgets.size() ? m_page_widgets.at(index) : nullptr;
}

int CybouMainWindow::currentPageIndex() const
{
    return m_pages->currentIndex();
}

int CybouMainWindow::pageCount() const
{
    return m_pages->count();
}

void CybouMainWindow::addPage(QWidget* page, bool scrolls)
{
    m_page_widgets.append(page);
    if (!scrolls) {
        m_pages->addWidget(page);
        return;
    }
    // Document-like pages scroll vertically; the window never scrolls
    // horizontally, so pages must fit their width.
    auto* scroll = new QScrollArea{m_pages};
    scroll->setObjectName(QStringLiteral("pageScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    page->setMinimumWidth(0);
    scroll->setWidget(page);
    m_pages->addWidget(scroll);
}

QFrame* CybouMainWindow::buildSidebar(QWidget* parent)
{
    auto* sidebar = new QFrame{parent};
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(kSidebarWidth);
    auto* layout = new QVBoxLayout{sidebar};
    layout->setContentsMargins(14, 20, 14, 18);
    layout->setSpacing(4);

    // Brand + Identity name (the Identity is the center of the app).
    auto* brand_row = new QHBoxLayout;
    brand_row->setSpacing(10);
    auto* logo_tile = new QLabel{sidebar};
    logo_tile->setPixmap(CybouTheme::logoTile({40, 40}, 10, {28, 28}));
    logo_tile->setFixedSize(40, 40);
    auto* brand_text = new QVBoxLayout;
    brand_text->setSpacing(0);
    m_brand_text = new QLabel{tr("CYBOU"), sidebar};
    m_brand_text->setObjectName("brand");
    m_brand_name = new QLabel{sidebar};
    m_brand_name->setObjectName("brandCaption");
    brand_text->addWidget(m_brand_text);
    brand_text->addWidget(m_brand_name);
    brand_row->addWidget(logo_tile);
    brand_row->addLayout(brand_text, 1);
    layout->addLayout(brand_row);
    layout->addSpacing(20);

    const auto add_button = [&](CybouPage page, CybouTheme::NavIcon icon) {
        auto* button = NavigationButton(PageTitle(page), icon, sidebar);
        const int id = static_cast<int>(page);
        button->setObjectName(QStringLiteral("navButton%1").arg(id));
        m_navigation->addButton(button, id);
        layout->addWidget(button);
    };
    add_button(CybouPage::Home, CybouTheme::NavIcon::Home);
    add_button(CybouPage::Mail, CybouTheme::NavIcon::Email);
    add_button(CybouPage::Files, CybouTheme::NavIcon::Storage);
    add_button(CybouPage::Wallet, CybouTheme::NavIcon::Wallet);

    layout->addSpacing(10);
    auto* separator = new QFrame{sidebar};
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName("separator");
    layout->addWidget(separator);
    layout->addSpacing(10);

    add_button(CybouPage::Identity, CybouTheme::NavIcon::Identity);
    add_button(CybouPage::Diagnostics, CybouTheme::NavIcon::Diagnostics);
    add_button(CybouPage::Settings, CybouTheme::NavIcon::Settings);
    layout->addStretch();
    return sidebar;
}

QFrame* CybouMainWindow::buildHeader(QWidget* parent)
{
    auto* header = new QFrame{parent};
    header->setObjectName(QStringLiteral("statusStrip"));
    header->setFixedHeight(56);
    auto* layout = new QHBoxLayout{header};
    layout->setContentsMargins(24, 6, 16, 6);
    layout->setSpacing(10);

    m_header_title = new QLabel{header};
    m_header_title->setObjectName(QStringLiteral("stripValue"));
    m_header_title->setStyleSheet(QStringLiteral("font-size: 16px;"));
    layout->addWidget(m_header_title);
    layout->addStretch();

    m_status_dot = CybouUi::Dot(CybouUi::Tint::Mint, header, 8);
    m_status_text = new QLabel{header};
    m_status_text->setObjectName(QStringLiteral("stripValue"));
    m_status_text->setMinimumWidth(120);
    layout->addWidget(m_status_dot, 0, Qt::AlignVCenter);
    layout->addWidget(m_status_text, 0, Qt::AlignVCenter);
    layout->addSpacing(14);

    m_identity_button = new QToolButton{header};
    m_identity_button->setObjectName(QStringLiteral("identityButton"));
    m_identity_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_identity_button->setPopupMode(QToolButton::InstantPopup);
    m_identity_button->setIconSize({24, 24});
    m_identity_button->setCursor(Qt::PointingHandCursor);
    m_identity_menu = new QMenu{m_identity_button};
    m_identity_menu->setObjectName(QStringLiteral("identityMenu"));
    m_identity_button->setMenu(m_identity_menu);
    layout->addWidget(m_identity_button, 0, Qt::AlignVCenter);

    connect(m_identity_menu, &QMenu::aboutToShow, this, [this] {
        m_identity_menu->clear();
        const auto& status = m_desktop_model->status();
        const bool active = status.identity_state == CybouIdentityState::Active;
        auto* head = new QWidgetAction{m_identity_menu};
        auto* head_widget = new QWidget;
        auto* head_layout = new QVBoxLayout{head_widget};
        head_layout->setContentsMargins(14, 10, 14, 8);
        head_layout->setSpacing(2);
        auto* name = new QLabel{status.primary_name.isEmpty()
            ? (active ? tr("Your CYBOU Identity") : tr("No Identity")) : status.primary_name, head_widget};
        name->setStyleSheet(QStringLiteral("font-weight: 700;"));
        auto* state = new QLabel{active ? tr("Identity active")
            : status.identity_state == CybouIdentityState::Locked ? tr("Vault locked") : tr("Not set up"), head_widget};
        state->setObjectName(QStringLiteral("mutedText"));
        head_layout->addWidget(name);
        head_layout->addWidget(state);
        head->setDefaultWidget(head_widget);
        m_identity_menu->addAction(head);
        m_identity_menu->addSeparator();
        m_identity_menu->addAction(tr("Identity & Security"), this, [this] { showPage(CybouPage::Identity); });
        auto* lock = m_identity_menu->addAction(tr("Lock local vault"), this, [this] {
            m_desktop_model->requestLockVault();
            showPage(CybouPage::Home);
        });
        lock->setEnabled(active);
        m_identity_menu->addAction(tr("Settings"), this, [this] { showPage(CybouPage::Settings); });
    });
    return header;
}

void CybouMainWindow::buildShell()
{
    auto* shell = new QWidget{this};
    shell->setObjectName("shell");
    auto* shell_layout = new QHBoxLayout{shell};
    shell_layout->setContentsMargins(0, 0, 0, 0);
    shell_layout->setSpacing(0);

    m_sidebar = buildSidebar(shell);
    shell_layout->addWidget(m_sidebar);

    auto* main_column = new QWidget{shell};
    auto* main_layout = new QVBoxLayout{main_column};
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);
    main_layout->addWidget(buildHeader(main_column));

    auto* home = new HomePage{m_desktop_model,
        [this] { showPage(CybouPage::Diagnostics); },
        [this] { showPage(CybouPage::Identity); },
        [this] { showPage(CybouPage::Wallet); },
        [this] { showPage(CybouPage::Mail); },
        [this] { showPage(CybouPage::Files); },
        nullptr};
    auto* mail = new EmailPage{m_desktop_model, [this] { showPage(CybouPage::Home); }, nullptr};
    auto* files = new StoragePage{m_desktop_model, [this] { showPage(CybouPage::Home); }, nullptr};
    auto* wallet = new WalletPage{m_desktop_model, nullptr};
    auto* identity = new IdentityPage{m_desktop_model, [this] { showPage(CybouPage::Home); }, nullptr};
    auto* diagnostics = new DiagnosticsPage{m_desktop_model, [this] { showDebugWindow(); }, nullptr};
    auto* settings = new SettingsPage{m_desktop_model, [this] { showPage(CybouPage::Diagnostics); }, nullptr};
    files->onSendByMail = [this, mail](const QString& file_id) {
        const auto attachment = m_desktop_model->attachmentFromFile(file_id);
        if (!attachment) return;
        CybouMailItem draft;
        draft.subject = attachment->name;
        draft.attachments = {*attachment};
        showPage(CybouPage::Mail);
        mail->openCompose(draft);
    };
    addPage(home, true);
    addPage(mail, false);
    addPage(files, false);
    addPage(wallet, true);
    addPage(identity, true);
    addPage(diagnostics, true);
    addPage(settings, true);

    connect(m_navigation, &QButtonGroup::idClicked, this, [this](int id) { showPage(static_cast<CybouPage>(id)); });
    m_navigation->button(0)->setChecked(true);
    m_pages->setMinimumWidth(0);
    main_layout->addWidget(m_pages, 1);
    shell_layout->addWidget(main_column, 1);
    setCentralWidget(shell);

    connect(m_desktop_model, &CybouDesktopModel::statusChanged, this, [this] { refreshHeader(); });
    refreshHeader();
}

void CybouMainWindow::refreshHeader()
{
    const auto& status = m_desktop_model->status();
    m_header_title->setText(PageTitle(static_cast<CybouPage>(m_pages->currentIndex())));

    const QString connection = cybouConnectionText(status);
    const bool healthy = status.sync_error.isEmpty() && status.node_running && status.online;
    m_status_text->setText(healthy ? tr("Online • %1").arg(connection) : connection);
    m_status_dot->setProperty("tint", healthy && !status.syncing ? "mint"
        : !status.sync_error.isEmpty() ? "rose" : "amber");
    Restyle(m_status_dot);

    const bool has_identity = status.identity_state == CybouIdentityState::Active ||
        status.identity_state == CybouIdentityState::Locked || status.identity_state == CybouIdentityState::Syncing;
    const QString label = !status.primary_name.isEmpty() ? status.primary_name
        : has_identity ? CybouProduct::shortId(status.account_id) : tr("No Identity");
    m_identity_button->setText(label);
    m_identity_button->setIcon(QIcon{CybouUi::avatarPixmap(label.left(1), CybouTheme::BRAND_TEAL, 24)});
    m_identity_button->setAccessibleName(tr("Identity menu for %1").arg(label));
    m_brand_name->setText(status.primary_name.isEmpty() ? status.network_name : status.primary_name);
}

void CybouMainWindow::setSidebarCompact(bool compact)
{
    if (m_sidebar_compact == compact) return;
    m_sidebar_compact = compact;
    m_sidebar->setFixedWidth(compact ? kCompactSidebar : kSidebarWidth);
    m_brand_text->setVisible(!compact);
    m_brand_name->setVisible(!compact);
    for (auto* button : m_navigation->buttons()) {
        auto* tool = qobject_cast<QToolButton*>(button);
        tool->setToolButtonStyle(compact ? Qt::ToolButtonIconOnly : Qt::ToolButtonTextBesideIcon);
    }
}

void CybouMainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    setSidebarCompact(event->size().width() < kCompactSidebarWidth);
}

void CybouMainWindow::buildMenus()
{
    menuBar()->clear();
    auto* file = menuBar()->addMenu(tr("File"));
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        file->addAction(tr("Hide CYBOU"), this, &QWidget::hide);
    }
    file->addAction(tr("Quit CYBOU"), this, [this] { Q_EMIT quitRequested(); });

    auto* view = menuBar()->addMenu(tr("View"));
    for (int i = 0; i < m_pages->count(); ++i) {
        const auto page = static_cast<CybouPage>(i);
        auto* action = view->addAction(PageTitle(page), this, [this, page] { showPage(page); });
        if (i < 4) action->setShortcut(QKeySequence{QStringLiteral("Ctrl+%1").arg(i + 1)});
    }

    auto* help = menuBar()->addMenu(tr("Help"));
    help->addAction(tr("Diagnostics window"), this, &CybouMainWindow::showDebugWindow);
    help->addAction(tr("About CYBOU"), this, [this] {
        QMessageBox::about(this, tr("About CYBOU"),
            tr("CYBOU gives you one private Identity for Mail, Files, Names and Wallet."));
    });
    help->addAction(tr("About Qt"), qApp, &QApplication::aboutQt);
}

void CybouMainWindow::buildTrayMenu()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray_menu = new QMenu{this};
    m_tray_menu->addAction(tr("Open CYBOU"), this, [this] {
        showNormal();
        raise();
        activateWindow();
    });
    m_tray_menu->addAction(tr("Diagnostics"), this, [this] {
        showPage(CybouPage::Diagnostics);
        showNormal();
    });
    m_tray_menu->addSeparator();
    m_tray_menu->addAction(tr("Quit CYBOU"), this, [this] { Q_EMIT quitRequested(); });
    m_tray_icon = new QSystemTrayIcon{windowIcon(), this};
    m_tray_icon->setToolTip(tr("CYBOU"));
    m_tray_icon->setContextMenu(m_tray_menu);
    connect(m_tray_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise();
            activateWindow();
        }
    });
    m_tray_icon->show();
}

void CybouMainWindow::runScreenshotHarness(const QString& directory)
{
    // Dev-only QA harness (CYBOU_SCREENSHOT_DIR): captures the named
    // screens for the active fixture and quits. File names are
    // <prefix><screen>.png, e.g. 1280x860-mail-reader.png.
    QDir{}.mkpath(directory);
    const QString prefix = qEnvironmentVariable("CYBOU_SCREENSHOT_PREFIX");
    const QString fixture = CybouUiFixtures::requestedFixture();
    const auto save = [this, directory, prefix](const QString& screen) {
        for (int i = 0; i < 3; ++i) qApp->processEvents();
        grab().save(QDir{directory}.filePath(QStringLiteral("%1%2.png").arg(prefix, screen)));
    };
    auto* home = static_cast<HomePage*>(page(CybouPage::Home));
    auto* mail = static_cast<EmailPage*>(page(CybouPage::Mail));
    auto* files = static_cast<StoragePage*>(page(CybouPage::Files));
    auto* model = m_desktop_model;

    if (fixture == QLatin1String{"empty"} || fixture.isEmpty()) {
        showPage(CybouPage::Home);
        save(QStringLiteral("home-empty"));
        home->onboarding()->showScreen(OnboardingView::Screen::Restore);
        save(QStringLiteral("identity-restore"));
    } else if (fixture == QLatin1String{"restoring"}) {
        showPage(CybouPage::Home);
        save(QStringLiteral("identity-restoring"));
    } else if (fixture == QLatin1String{"offline"}) {
        showPage(CybouPage::Mail);
        mail->setView(EmailPage::View::Sent);
        mail->openMessage(QStringLiteral("m-outgoing"));
        save(QStringLiteral("mail-offline"));
    } else {
        showPage(CybouPage::Home);
        save(QStringLiteral("home-active"));
        showPage(CybouPage::Identity);
        save(QStringLiteral("identity-active"));

        showPage(CybouPage::Mail);
        mail->setView(EmailPage::View::Inbox);
        save(QStringLiteral("mail-inbox"));
        mail->openMessage(QStringLiteral("m-project"));
        save(QStringLiteral("mail-reader"));
        mail->setView(EmailPage::View::Sent);
        mail->openMessage(QStringLiteral("m-sent-securing"));
        save(QStringLiteral("mail-attachment-progress"));
        mail->setView(EmailPage::View::Inbox);
        CybouMailItem draft;
        draft.to_name = QStringLiteral("alice.cybou");
        draft.subject = tr("Project files");
        draft.body = tr("Hello Alice,\n\nHere are the final files.\n\nStan");
        if (const auto attachment = model->attachmentFromFile(QStringLiteral("f-report"))) draft.attachments = {*attachment};
        mail->openCompose(draft);
        save(QStringLiteral("mail-compose"));

        showPage(CybouPage::Files);
        files->setView(StoragePage::View::MyFiles);
        save(QStringLiteral("files-list"));
        files->setGridMode(true);
        save(QStringLiteral("files-grid"));
        files->setGridMode(false);
        files->showDetails(QStringLiteral("f-report"));
        save(QStringLiteral("files-details"));
        files->showDetails({});
        QTemporaryDir temp;
        const QString upload = temp.filePath(QStringLiteral("presentation.pdf"));
        QFile file{upload};
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QByteArray(2 * 1024 * 1024, 'x'));
            file.close();
            const QString id = model->requestFileUpload(upload);
            model->setFileState(id, CybouContentState::Securing, 42);
        }
        save(QStringLiteral("files-upload"));

        showPage(CybouPage::Wallet);
        save(QStringLiteral("wallet"));
        showPage(CybouPage::Diagnostics);
        save(QStringLiteral("diagnostics"));
        showPage(CybouPage::Settings);
        save(QStringLiteral("settings"));

        model->setFileItems({});
        showPage(CybouPage::Files);
        save(QStringLiteral("files-empty"));
    }
    qApp->quit();
}

void CybouMainWindow::closeEvent(QCloseEvent* event)
{
    if (QSettings{}.value(QStringLiteral("desktop/run_in_background"), false).toBool() &&
        m_tray_icon && m_tray_icon->isVisible()) {
        // "Keep running in background": hide CYBOU, node continues, tray remains.
        hide();
        event->ignore();
        return;
    }
    Q_EMIT quitRequested();
    event->accept();
}

void CybouMainWindow::applyStyle()
{
    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        CybouTheme::applyTo(*app);
    }
}
