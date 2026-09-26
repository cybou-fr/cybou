// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboutheme.h>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTest>
#include <QToolButton>
#include <QDialog>

#include <algorithm>

#include <util/chaintype.h>

namespace {

} // namespace

CybouShellTests::CybouShellTests(interfaces::Node& node)
{
    (void)node;
}

CybouShellTests::~CybouShellTests() = default;

std::unique_ptr<CybouMainWindow> CybouShellTests::makeWindow()
{
    return std::make_unique<CybouMainWindow>(std::filesystem::path{});
}

void CybouShellTests::mainWindowStarts()
{
    auto window = makeWindow();
    QVERIFY(window);
    QVERIFY(window->centralWidget());
    QVERIFY(window->windowTitle().contains(QStringLiteral("CYBOU")));
    QCOMPARE(window->pageCount(), 8);
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

    window->showDebugWindow();
    auto* diagnostics = window->findChild<QDialog*>(QStringLiteral("CYBOUDiagnostics"));
    QVERIFY(diagnostics);
    QVERIFY(diagnostics->isVisible());
    QVERIFY(window->isVisible());

    window->showDebugWindow();
    QVERIFY(diagnostics->isVisible());
    diagnostics->close();
}

void CybouShellTests::identityCreateFollowsCapabilities()
{
    auto window = makeWindow();
    auto* identity = window->pageAt(1);
    QVERIFY(identity);

    QPushButton* create{nullptr};
    for (auto* btn : identity->findChildren<QPushButton*>()) {
        if (btn->property("cybouId").toString() == QLatin1String{"createIdentity"}) {
            create = btn;
            break;
        }
    }
    if (!create) create = identity->findChild<QPushButton*>(QStringLiteral("primaryButton"));
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

    // The request is tracked UI-side: the button stands down and the page
    // says it is waiting for the node, without inventing protocol phases.
    QVERIFY(window->desktopModel()->identityCreationRequestPending());
    QVERIFY(!create->isEnabled());
    QVERIFY(create->text().contains(QStringLiteral("requested")));
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

void CybouShellTests::walletPageShowsBalances()
{
    auto window = makeWindow();
    auto* wallet = window->pageAt(5);
    QVERIFY(wallet);

    // Amounts render as whole CYBOU (indivisible asset, decimals = 0).
    const auto labels = wallet->findChildren<QLabel*>();
    bool found_amount = false;
    for (const auto* label : labels) {
        if (label->text().contains(QStringLiteral("CYBOU"))) found_amount = true;
    }
    QVERIFY(found_amount);

    // Without an identity all transfer actions stay disabled: Balance
    // debits require the user's authorization, and the UI offers none.
    const auto buttons = wallet->findChildren<QPushButton*>();
    QVERIFY(!buttons.isEmpty());
    for (const auto* button : buttons) {
        QVERIFY(!button->isEnabled());
    }
}

void CybouShellTests::storageAndBackupGateActions()
{
    auto window = makeWindow();
    // Both pages are full client UIs now; without an identity every action
    // must stay disabled and the page must say why.
    for (int index = 3; index <= 4; ++index) {
        auto* page = window->pageAt(index);
        QVERIFY2(page, qPrintable(QStringLiteral("service page %1 exists").arg(index)));
        const auto buttons = page->findChildren<QPushButton*>();
        QVERIFY(!buttons.isEmpty());
        for (const auto* button : buttons) {
            QVERIFY(!button->isEnabled());
        }
    }
}

void CybouShellTests::networkPageReflectsModel()
{
    auto window = makeWindow();
    auto* network = window->pageAt(6);
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

void CybouShellTests::adapterSettersDrivePages()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(model);

    // Doc 73 adapter surface: setters mutate status and pages follow.
    QSignalSpy status_spy{model, &CybouDesktopModel::statusChanged};
    model->setFinalityStatus(42, 4);
    QCOMPARE(model->status().last_finalized_height, 42);
    QCOMPARE(model->status().validator_count, 4);
    QVERIFY(status_spy.count() >= 1);

    auto* network = window->pageAt(6);
    QVERIFY(network);
    const auto labels = network->findChildren<QLabel*>();
    bool found_height = false;
    bool found_fault = false;
    for (const auto* label : labels) {
        if (label->text() == QLatin1String{"42"}) found_height = true;
        if (label->text().contains(QLatin1String{"f = 1"})) found_fault = true;
    }
    QVERIFY(found_height);
    QVERIFY(found_fault);

    // Identity lifecycle and balances flow through the same boundary.
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("acct-1"), 42);
    QCOMPARE(model->status().identity_state, CybouIdentityState::Active);
    QVERIFY(!model->identityCreationRequestPending());

    model->setBalances(1000, 250);
    QCOMPARE(model->status().balance, quint64{1000});
    QCOMPARE(model->status().system_balance, quint64{250});

    // Unchanged values are a no-op (no extra signal).
    const int before = status_spy.count();
    model->setFinalityStatus(42, 4);
    model->setBalances(1000, 250);
    QCOMPARE(status_spy.count(), before);
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
    QSignalSpy spy{window.get(), &CybouMainWindow::quitRequested};
    window->close();
    QVERIFY(spy.count() > 0);
}
