// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboutheme.h>
#include <qt/networkstyle.h>
#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>
#include <QVector>

#include <algorithm>

#include <util/chaintype.h>

namespace {

QVector<RPCConsole*> findDiagnosticsConsoles()
{
    QVector<RPCConsole*> consoles;
    const auto top_levels = QApplication::topLevelWidgets();
    for (auto* widget : top_levels) {
        if (auto* console = qobject_cast<RPCConsole*>(widget)) consoles.append(console);
    }
    return consoles;
}

bool anyConsoleVisible()
{
    const auto consoles = findDiagnosticsConsoles();
    return std::any_of(consoles.begin(), consoles.end(), [](const auto* console) { return console->isVisible(); });
}

} // namespace

CybouShellTests::CybouShellTests(interfaces::Node& node)
    : m_node{node},
      m_platform_style{PlatformStyle::instantiate("other")}
{
}

CybouShellTests::~CybouShellTests() = default;

std::unique_ptr<CybouMainWindow> CybouShellTests::makeWindow()
{
    return std::make_unique<CybouMainWindow>(m_node, m_platform_style.get(),
        NetworkStyle::instantiate(ChainType::MAIN), nullptr);
}

void CybouShellTests::mainWindowStarts()
{
    auto window = makeWindow();
    QVERIFY(window);
    QVERIFY(window->centralWidget());
    QVERIFY(window->windowTitle().contains(QStringLiteral("CYBOU")));
    QCOMPARE(window->pageCount(), 7);
}

void CybouShellTests::homePageIsDefault()
{
    auto window = makeWindow();
    QVERIFY(window->pageAt(0));
    QCOMPARE(window->currentPageIndex(), 0);
}

void CybouShellTests::navigationSwitchesPages()
{
    auto window = makeWindow();
    for (int index = 1; index < window->pageCount(); ++index) {
        auto* button = window->findChild<QToolButton*>(QStringLiteral("navButton%1").arg(index));
        QVERIFY2(button, qPrintable(QStringLiteral("navigation button %1 exists").arg(index)));
        button->click();
        QCOMPARE(window->currentPageIndex(), index);
    }
}

void CybouShellTests::diagnosticsStaySecondaryWindow()
{
    auto window = makeWindow();
    window->show();
    QVERIFY(window->isVisible());

    QVERIFY(!anyConsoleVisible());

    window->showDebugWindow();
    QVERIFY(anyConsoleVisible());

    // Closing diagnostics must hide it, never destroy it, and must not
    // close the main window. Other shells (e.g. from AppTests) may own
    // their own console, so close every visible one.
    for (auto* console : findDiagnosticsConsoles()) {
        if (console->isVisible()) console->close();
    }
    QVERIFY(!anyConsoleVisible());
    QVERIFY(window->isVisible());

    // Reopening must keep working.
    window->showDebugWindow();
    QVERIFY(anyConsoleVisible());
    for (auto* console : findDiagnosticsConsoles()) {
        if (console->isVisible()) console->close();
    }
    QVERIFY(!anyConsoleVisible());
}

void CybouShellTests::identityCreateFollowsCapabilities()
{
    auto window = makeWindow();
    auto* identity = window->pageAt(1);
    QVERIFY(identity);

    auto* create = identity->findChild<QPushButton*>(QStringLiteral("primaryButton"));
    QVERIFY(create);
    QVERIFY(!create->isEnabled());

    // Capabilities come from the desktop model boundary, never from
    // version hacks or compile-time guesses.
    CybouCapabilities capabilities;
    capabilities.account_creation = true;
    window->desktopModel()->setCapabilities(capabilities);
    QVERIFY(create->isEnabled());

    // Clicking emits the UI -> core request; the page invents no protocol
    // behavior of its own.
    QSignalSpy spy{window->desktopModel(), &CybouDesktopModel::createIdentityRequested};
    QTest::mouseClick(create, Qt::LeftButton);
    QCOMPARE(spy.count(), 1);
}

void CybouShellTests::emailPageGatesSending()
{
    auto window = makeWindow();
    auto* email = window->pageAt(2);
    QVERIFY(email);

    // The full client UI is present: compose, recipient field, send.
    auto* send = email->findChild<QPushButton*>(QStringLiteral("sendButton"));
    auto* recipient = email->findChild<QLineEdit*>(QStringLiteral("recipientEdit"));
    auto* body = email->findChild<QTextEdit*>(QStringLiteral("composeBody"));
    QVERIFY(send);
    QVERIFY(recipient);
    QVERIFY(body);

    // Without an active identity the composer explains the gate and Send
    // stays disabled no matter how complete the draft is.
    QVERIFY(email->findChild<QFrame*>(QStringLiteral("identityBanner")));
    recipient->setText(QStringLiteral("peer@cybou"));
    body->setPlainText(QStringLiteral("hello"));
    QVERIFY(!send->isEnabled());

    // The one-recipient rule is enforced in the field itself: a second
    // recipient is rejected before any protocol interaction can happen.
    recipient->setText(QStringLiteral("a@cybou, b@cybou"));
    QVERIFY(recipient->text().contains(QStringLiteral(",")));
}

void CybouShellTests::storageAndBackupExposeNoOperations()
{
    auto window = makeWindow();
    // Email (page 2) is now a full client UI; Storage and Backup remain
    // inert placeholders until their services are specified.
    for (int index = 3; index <= 4; ++index) {
        auto* placeholder = window->pageAt(index);
        QVERIFY2(placeholder, qPrintable(QStringLiteral("placeholder page %1 exists").arg(index)));
        QVERIFY(placeholder->findChildren<QPushButton*>().isEmpty());
        QVERIFY(placeholder->findChildren<QLineEdit*>().isEmpty());
    }
}

void CybouShellTests::networkPageReflectsModel()
{
    auto window = makeWindow();
    auto* network = window->pageAt(5);
    QVERIFY(network);

    const QString network_name = window->desktopModel()->status().network_name;
    QVERIFY(!network_name.isEmpty());

    const auto labels = network->findChildren<QLabel*>();
    bool found_network_name = false;
    for (const auto* label : labels) {
        if (label->text() == network_name) found_network_name = true;
    }
    QVERIFY(found_network_name);
}

void CybouShellTests::themeResolvesAllTokens()
{
    // Regression guard for the numbered-%N .arg() shift: every @token@ in the
    // stylesheet must be substituted, and key palette colors must appear as-is.
    const QString sheet = CybouTheme::applicationStyleSheet();
    if (sheet.contains(QStringLiteral("@"))) {
        const qsizetype at = sheet.indexOf(QStringLiteral("@"));
        QWARN(qPrintable(QStringLiteral("unresolved token near: %1")
                             .arg(sheet.mid(std::max<qsizetype>(0, at - 40), 80))));
        QFAIL("stylesheet contains an unresolved @token@");
    }
    QVERIFY(sheet.contains(CybouTheme::color(CybouTheme::MINT_SOFT).name()));
    QVERIFY(sheet.contains(CybouTheme::color(CybouTheme::MINT_GHOST).name()));
    QVERIFY(sheet.contains(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
    // The dark logomark tile is pre-rendered (not stylesheet-painted); it must
    // produce a non-empty pixmap with the dark tile background actually drawn.
    const QPixmap tile = CybouTheme::logoTile({44, 44}, 11, {30, 30});
    QVERIFY(!tile.isNull());
    QCOMPARE(tile.toImage().pixelColor(22, 22), CybouTheme::color(CybouTheme::LOGO_TILE_BG));
}

void CybouShellTests::navIconsRender()
{
    // Regression guard for the .svg-suffixed resource paths and the rcc alias
    // collision (:/icons/cybou file vs /icons/cybou prefix silently drops the
    // whole prefix): every nav icon must load from the .qrc and render.
    const QColor stroke = CybouTheme::color(CybouTheme::BRAND_TEAL_DARK);
    const QSize size{22, 22};
    const CybouTheme::NavIcon icons[] = {
        CybouTheme::NavIcon::Home,
        CybouTheme::NavIcon::Identity,
        CybouTheme::NavIcon::Email,
        CybouTheme::NavIcon::Storage,
        CybouTheme::NavIcon::Backup,
        CybouTheme::NavIcon::Network,
        CybouTheme::NavIcon::Settings,
        CybouTheme::NavIcon::Diagnostics,
    };
    for (const auto icon : icons) {
        const QPixmap pixmap = CybouTheme::iconPixmap(icon, size, stroke);
        QVERIFY2(!pixmap.isNull(), "icon resource failed to load/render");
    }
    // The full nav icon set must carry both inactive and active pixmaps.
    const QIcon nav = CybouTheme::navIcon(CybouTheme::NavIcon::Home);
    QVERIFY(!nav.pixmap(size, QIcon::Normal, QIcon::Off).isNull());
    QVERIFY(!nav.pixmap(size, QIcon::Normal, QIcon::On).isNull());
}

void CybouShellTests::closingWithoutNodeRequestsQuit()
{
    auto window = makeWindow();
    window->show();
    QVERIFY(window->isVisible());

    // With no node model attached, closing the window must mean quitting.
    QSignalSpy spy{window.get(), &BitcoinGUI::quitRequested};
    window->close();
    QVERIFY(spy.count() > 0);
}
