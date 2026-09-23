// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboumainwindow.h>
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

void CybouShellTests::placeholdersExposeNoOperations()
{
    auto window = makeWindow();
    for (int index = 2; index <= 4; ++index) {
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
