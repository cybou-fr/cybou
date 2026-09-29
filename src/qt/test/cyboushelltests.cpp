// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <qt/cyboudesktopcontroller.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboutheme.h>
#include <qt/cybouuifixtures.h>

#include <cybou/network_definition.h>
#include <test/cybou_test_helpers.h>
#include <cybou/validator.h>

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTest>
#include <QToolButton>
#include <QDialog>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace {
void AppendUint32LE(std::vector<unsigned char>& out, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

bool WriteNetworkFile(const QString& path, const unsigned char seed_byte)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    const auto keypair = cybou::GenerateValidatorKeyPair(seed);
    if (!keypair) return false;
    const auto genesis = cybou::CreateDevGenesisState(keypair->public_key);
    const auto definition = cybou::CreateDevNetworkDefinition(genesis, cybou::TestPoaFinalizerPublicKey());
    const auto definition_bytes = cybou::SerializeNetworkDefinition(definition);
    const auto state_bytes = cybou::SerializeCybouState(genesis);
    if (!state_bytes) return false;

    std::vector<unsigned char> bytes{'C', 'Y', 'N', '1'};
    AppendUint32LE(bytes, static_cast<uint32_t>(definition_bytes.size()));
    bytes.insert(bytes.end(), definition_bytes.begin(), definition_bytes.end());
    AppendUint32LE(bytes, static_cast<uint32_t>(state_bytes->size()));
    bytes.insert(bytes.end(), state_bytes->begin(), state_bytes->end());

    QFile file{path};
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
        file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size());
}

class ScopedEnvironment final
{
public:
    ScopedEnvironment(const char* name, const QByteArray& value)
        : m_name{name}, m_was_set{qEnvironmentVariableIsSet(name)}, m_previous{qgetenv(name)}
    {
        qputenv(name, value);
    }

    ~ScopedEnvironment()
    {
        if (m_was_set) qputenv(m_name.constData(), m_previous);
        else qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    bool m_was_set;
    QByteArray m_previous;
};

class ScopedUnsetEnvironment final
{
public:
    explicit ScopedUnsetEnvironment(const char* name)
        : m_name{name}, m_was_set{qEnvironmentVariableIsSet(name)}, m_previous{qgetenv(name)}
    {
        qunsetenv(name);
    }

    ~ScopedUnsetEnvironment()
    {
        if (m_was_set) qputenv(m_name.constData(), m_previous);
    }

private:
    QByteArray m_name;
    bool m_was_set;
    QByteArray m_previous;
};
} // namespace

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
    QCOMPARE(window->pageCount(), 7);
    // Backup is not part of the Beta shell.
    for (auto* button : window->findChildren<QToolButton*>()) {
        QVERIFY(!button->text().contains(QStringLiteral("Backup")));
    }
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
    auto* model = window->desktopModel();
    auto* home = window->page(CybouPage::Home);
    QVERIFY(home);

    const auto find_button = [home](const char* id) -> QPushButton* {
        for (auto* button : home->findChildren<QPushButton*>()) {
            if (button->property("cybouId").toString() == QLatin1String{id}) return button;
        }
        return nullptr;
    };
    auto* create = find_button("createIdentity");
    QVERIFY(create);
    QVERIFY(!create->isEnabled());

    // Capabilities come from the desktop model boundary.
    model->setFixtureMode(true); // deterministic recovery words, no core
    CybouCapabilities capabilities;
    capabilities.account_creation = true;
    model->setCapabilities(capabilities);
    QVERIFY(create->isEnabled());

    // Create -> vault password -> recovery words -> confirmation.
    QSignalSpy spy{model, &CybouDesktopModel::createIdentityRequested};
    create->click();
    auto* password = home->findChild<QLineEdit*>(QStringLiteral("vaultPassword"));
    auto* confirm = home->findChild<QLineEdit*>(QStringLiteral("vaultPasswordConfirm"));
    QVERIFY(password && confirm);
    auto* next = find_button("passwordContinue");
    password->setText(QStringLiteral("short"));
    confirm->setText(QStringLiteral("short"));
    QVERIFY(!next->isEnabled());
    password->setText(QStringLiteral("correct horse battery"));
    confirm->setText(QStringLiteral("correct horse battery"));
    QVERIFY(next->isEnabled());
    next->click();
    find_button("wordsContinue")->click();

    // Fixture confirmation asks for words #4, #12 and #21.
    const QStringList expected{QStringLiteral("stone"), QStringLiteral("garden"), QStringLiteral("falcon")};
    auto* submit = find_button("confirmCreate");
    for (int i = 0; i < 3; ++i) {
        auto* input = home->findChild<QLineEdit*>(QStringLiteral("confirmWord%1").arg(i));
        QVERIFY(input);
        input->setText(i == 0 ? QStringLiteral("wrong") : expected.at(i));
    }
    submit->click();
    QCOMPARE(spy.count(), 0); // a wrong word blocks creation
    home->findChild<QLineEdit*>(QStringLiteral("confirmWord0"))->setText(expected.at(0));
    submit->click();
    QCOMPARE(spy.count(), 1);
    QVERIFY(model->identityCreationRequestPending());
}

void CybouShellTests::restoreFlowValidatesPhrase()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    model->setFixtureMode(true);
    CybouCapabilities capabilities;
    capabilities.account_creation = true;
    model->setCapabilities(capabilities);
    auto* home = window->page(CybouPage::Home);
    QPushButton* restore{nullptr};
    QPushButton* submit{nullptr};
    for (auto* button : home->findChildren<QPushButton*>()) {
        if (button->property("cybouId").toString() == QLatin1String{"restoreIdentity"}) restore = button;
        if (button->property("cybouId").toString() == QLatin1String{"restoreSubmit"}) submit = button;
    }
    QVERIFY(restore && submit);
    restore->click();
    auto* phrase = home->findChild<QPlainTextEdit*>(QStringLiteral("recoveryPhrase"));
    auto* password = home->findChild<QLineEdit*>(QStringLiteral("restorePassword"));
    auto* confirm = home->findChild<QLineEdit*>(QStringLiteral("restorePasswordConfirm"));
    QVERIFY(phrase && password && confirm);
    phrase->setPlainText(QStringLiteral("one two three"));
    password->setText(QStringLiteral("correct horse battery"));
    confirm->setText(QStringLiteral("correct horse battery"));
    QVERIFY(!submit->isEnabled());
    QStringList words;
    for (int i = 0; i < 24; ++i) words << QStringLiteral("word%1").arg(i);
    phrase->setPlainText(words.join(QLatin1Char{' '}));
    QVERIFY(submit->isEnabled());
    submit->click();
    QCOMPARE(model->status().identity_state, CybouIdentityState::Restoring);
    QVERIFY(phrase->toPlainText().isEmpty());
}

void CybouShellTests::identityPageHidesSecrets()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    auto* identity = window->page(CybouPage::Identity);
    bool found_name = false;
    for (const auto* label : identity->findChildren<QLabel*>()) {
        if (label->text() == QLatin1String{"stan.cybou"}) found_name = true;
        // The recovery phrase is never rendered persistently.
        QVERIFY(!label->text().contains(QLatin1String{"ocean"}));
    }
    QVERIFY(found_name);
    QVERIFY(model->nameLabelProblem(QStringLiteral("abc")).size() > 0);
    QVERIFY(model->nameLabelProblem(QStringLiteral("alice-2")).isEmpty());
}

void CybouShellTests::emailPageGatesSending()
{
    auto window = makeWindow();
    auto* email = window->page(CybouPage::Mail);
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
    auto* wallet = window->page(CybouPage::Wallet);
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

void CybouShellTests::filesGateActions()
{
    auto window = makeWindow();
    // Without an identity every Files action stays disabled.
    auto* page = window->page(CybouPage::Files);
    QVERIFY(page);
    const auto buttons = page->findChildren<QPushButton*>();
    QVERIFY(!buttons.isEmpty());
    for (const auto* button : buttons) {
        QVERIFY(!button->isEnabled());
    }
}

void CybouShellTests::networkPageReflectsModel()
{
    auto window = makeWindow();
    auto* network = window->page(CybouPage::Diagnostics);
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
    model->setFinalizedHeight(42);
    QCOMPARE(model->status().finalized_height, quint64{42});
    QVERIFY(model->status().finality_known);
    QVERIFY(status_spy.count() >= 1);

    auto* network = window->page(CybouPage::Diagnostics);
    QVERIFY(network);
    const auto labels = network->findChildren<QLabel*>();
    bool found_height = false;
    bool found_poa = false;
    for (const auto* label : labels) {
        if (label->text() == QLatin1String{"42"}) found_height = true;
        if (label->text().contains(QLatin1String{"PoA verified"})) found_poa = true;
        QVERIFY2(!label->text().contains(QLatin1String{"alidator"}), qPrintable(label->text()));
    }
    QVERIFY(found_height);
    QVERIFY(found_poa);

    const QString sync_error = QStringLiteral("Configured peer belongs to another CYBOU network.");
    model->setSyncError(sync_error);
    QCOMPARE(model->status().sync_error, sync_error);
    bool found_sync_error = false;
    for (const auto* label : network->findChildren<QLabel*>()) {
        if (label->text() == sync_error) found_sync_error = true;
    }
    QVERIFY(found_sync_error);
    model->setSyncError({});
    QVERIFY(model->status().sync_error.isEmpty());

    // Identity lifecycle and balances flow through the same boundary.
    model->setIdentityState(CybouIdentityState::Creating);
    QCOMPARE(model->status().identity_state, CybouIdentityState::Creating);
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("acct-1"), 42);
    QCOMPARE(model->status().identity_state, CybouIdentityState::Active);
    QVERIFY(!model->identityCreationRequestPending());

    model->setBalances(1000, 250);
    QCOMPARE(model->status().balance, quint64{1000});
    QCOMPARE(model->status().system_balance, quint64{250});

    // Unchanged values are a no-op (no extra signal).
    const int before = status_spy.count();
    model->setFinalizedHeight(42);
    model->setBalances(1000, 250);
    QCOMPARE(status_spy.count(), before);
}

void CybouShellTests::productCollectionsDriveModel()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    QSignalSpy mail_spy{&model, &CybouDesktopModel::mailChanged};
    QSignalSpy files_spy{&model, &CybouDesktopModel::filesChanged};

    CybouMailItem mail;
    mail.id = QStringLiteral("m1");
    mail.from_name = QStringLiteral("alice.cybou");
    mail.unread = true;
    model.upsertMailItem(mail);
    QCOMPARE(model.unreadMailCount(), 1);
    mail.unread = false;
    model.upsertMailItem(mail);
    QCOMPARE(model.mailItems().size(), 1);
    QCOMPARE(model.unreadMailCount(), 0);
    QCOMPARE(mail_spy.count(), 2);

    CybouFileItem file;
    file.id = QStringLiteral("opaque-1");
    file.name = QStringLiteral("report.pdf");
    file.state = CybouContentState::Securing;
    model.upsertFileItem(file);
    file.state = CybouContentState::Protected;
    model.upsertFileItem(file);
    QCOMPARE(model.fileItems().size(), 1);
    QCOMPARE(model.fileItems().first().state, CybouContentState::Protected);
    QCOMPARE(files_spy.count(), 2);

    // Finalized content is still Securing; only Protected reads as Sent.
    CybouMailItem sent;
    sent.folder = CybouMailFolder::Sent;
    sent.state = CybouContentState::Securing;
    QVERIFY(CybouProduct::mailStateText(sent) != QStringLiteral("Sent"));
    sent.state = CybouContentState::Protected;
    QCOMPARE(CybouProduct::mailStateText(sent), QStringLiteral("Sent"));

    // Header connection wording.
    CybouDesktopStatus status;
    QCOMPARE(cybouConnectionText(status), QStringLiteral("Offline"));
    status.node_running = true;
    QCOMPARE(cybouConnectionText(status), QStringLiteral("Connecting"));
    status.online = true;
    QCOMPARE(cybouConnectionText(status), QStringLiteral("Synced"));
    status.syncing = true;
    QCOMPARE(cybouConnectionText(status), QStringLiteral("Syncing"));
    status.sync_error = QStringLiteral("x");
    QCOMPARE(cybouConnectionText(status), QStringLiteral("Needs attention"));
}

void CybouShellTests::fixturesLoadDeterministically()
{
    {
        CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
        QVERIFY(!CybouUiFixtures::apply(model, QStringLiteral("unknown")));
        QVERIFY(!model.fixtureMode());
    }
    for (const auto& name : CybouUiFixtures::names()) {
        CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
        QVERIFY2(CybouUiFixtures::apply(model, name), qPrintable(name));
        QVERIFY(model.fixtureMode());
        const auto state = model.status().identity_state;
        if (name == QLatin1String{"empty"}) {
            QCOMPARE(state, CybouIdentityState::None);
            QVERIFY(model.mailItems().isEmpty());
        } else if (name == QLatin1String{"restoring"}) {
            QCOMPARE(state, CybouIdentityState::Restoring);
            QCOMPARE(model.restoreProgress().identity, CybouRestoreStepState::Done);
        } else {
            QCOMPARE(state, CybouIdentityState::Active);
            QCOMPARE(model.status().primary_name, QStringLiteral("stan.cybou"));
            QVERIFY(model.unreadMailCount() > 0);
            QVERIFY(!model.fileItems().isEmpty());
        }
        if (name == QLatin1String{"offline"}) QVERIFY(!model.status().online);
    }

    // The fixture driver answers UI requests with UI-only transitions.
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("empty")));
    CybouUiFixtures::Driver driver{&model};
    driver.setStepDelay(0);
    model.requestCreateIdentity(QStringLiteral("correct horse battery"));
    QCOMPARE(model.status().identity_state, CybouIdentityState::Creating);
    for (int i = 0; i < 50 && model.status().identity_state != CybouIdentityState::Active; ++i) QTest::qWait(10);
    QCOMPARE(model.status().identity_state, CybouIdentityState::Active);
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

void CybouShellTests::runtimeStartupFailureCanBeRetried()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ScopedEnvironment p2p_host{"CYBOU_DEV_P2P_HOST", "127.0.0.1"};
    ScopedEnvironment p2p_port{"CYBOU_DEV_P2P_PORT", "1"};
    ScopedUnsetEnvironment validator_mode{"CYBOU_DEV_VALIDATOR"};

    CybouDesktopModel model{QStringLiteral("CYBOU-DEV")};
    CybouDesktopController controller{&model, directory.path().toStdString()};
    QSignalSpy failures{&controller, &CybouDesktopController::startupFailed};
    controller.start();
    QCOMPARE(failures.count(), 1);
    QVERIFY(!model.status().node_running);

    QVERIFY(WriteNetworkFile(directory.filePath(QStringLiteral("network.bin")), 0x31));
    controller.start();
    QCOMPARE(failures.count(), 1);
    QVERIFY(model.status().node_running);
}

void CybouShellTests::runtimeRejectsStateFromAnotherNetwork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ScopedEnvironment p2p_host{"CYBOU_DEV_P2P_HOST", "127.0.0.1"};
    ScopedEnvironment p2p_port{"CYBOU_DEV_P2P_PORT", "1"};
    ScopedUnsetEnvironment validator_mode{"CYBOU_DEV_VALIDATOR"};
    QVERIFY(WriteNetworkFile(directory.filePath(QStringLiteral("network.bin")), 0x32));

    {
        CybouDesktopModel model{QStringLiteral("CYBOU-DEV")};
        CybouDesktopController controller{&model, directory.path().toStdString()};
        QSignalSpy failures{&controller, &CybouDesktopController::startupFailed};
        controller.start();
        QCOMPARE(failures.count(), 0);
        QVERIFY(model.status().node_running);
    }

    QVERIFY(WriteNetworkFile(directory.filePath(QStringLiteral("network.bin")), 0x33));
    CybouDesktopModel model{QStringLiteral("CYBOU-DEV")};
    CybouDesktopController controller{&model, directory.path().toStdString()};
    QSignalSpy failures{&controller, &CybouDesktopController::startupFailed};
    controller.start();
    QCOMPARE(failures.count(), 1);
    QVERIFY(failures.takeFirst().at(0).toString().contains(QStringLiteral("belongs to another network")));
    QVERIFY(!model.status().node_running);
}
