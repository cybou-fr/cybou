// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <qt/cyboudesktopcontroller.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboutheme.h>
#include <qt/cybouuifixtures.h>
#include <qt/pages/emailpage.h>
#include <qt/pages/mailcompose.h>
#include <qt/pages/mailreader.h>
#include <qt/pages/storagepage.h>

#include <cybou/network_definition.h>
#include <test/cybou_test_helpers.h>
#include <cybou/validator.h>

#include <QApplication>
#include <QAbstractButton>
#include <QLabel>
#include <QListWidget>
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

void CybouShellTests::mailNavigationAndSearch()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    QVERIFY(mail);
    // Without an Identity, Mail explains the gate.
    QVERIFY(!mail->findChild<QFrame*>(QStringLiteral("identityBanner"))->isHidden());

    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
    QCOMPARE(mail->visibleMessageIds().size(), 5);
    QCOMPARE(model->unreadMailCount(), 4);

    // Search is local and covers sender, subject, body and attachment names.
    auto* search = mail->findChild<QLineEdit*>(QStringLiteral("mailSearch"));
    QCOMPARE(search->placeholderText(), QStringLiteral("Search mail"));
    search->setText(QStringLiteral("contract-signed"));
    QCOMPARE(mail->visibleMessageIds(), QStringList{QStringLiteral("m-contract")});
    search->setText(QStringLiteral("bobby.cybou"));
    QCOMPARE(mail->visibleMessageIds(), QStringList{QStringLiteral("m-dinner")});
    search->clear();

    mail->setView(EmailPage::View::Sent);
    QCOMPARE(mail->visibleMessageIds(), (QStringList{QStringLiteral("m-sent-securing"), QStringLiteral("m-sent-1")}));
    mail->setView(EmailPage::View::Starred);
    QCOMPARE(mail->visibleMessageIds(), QStringList{QStringLiteral("m-welcome")});

    // Opening a message marks it read (local mailbox state).
    mail->setView(EmailPage::View::Inbox);
    mail->openMessage(QStringLiteral("m-project"));
    QCOMPARE(model->unreadMailCount(), 3);
    QCOMPARE(mail->reader()->messageId(), QStringLiteral("m-project"));
    for (const auto* label : mail->reader()->findChildren<QLabel*>()) {
        // Evidence stays in Security Details, not in the reader.
        QVERIFY(!label->text().contains(QLatin1String{"Operation"}));
        QVERIFY(!label->text().contains(QLatin1String{"BFT"}));
    }
}

void CybouShellTests::composeGatesAndSends()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->openCompose();
    auto* send = mail->findChild<QPushButton*>(QStringLiteral("sendButton"));
    auto* to = mail->findChild<QLineEdit*>(QStringLiteral("recipientEdit"));
    auto* body = mail->findChild<QTextEdit*>(QStringLiteral("composeBody"));
    QVERIFY(send && to && body);
    to->setText(QStringLiteral("alice.cybou, bobby.cybou"));
    body->setPlainText(QStringLiteral("hello"));
    QVERIFY(!send->isEnabled()); // one recipient only
    to->setText(QStringLiteral("alice"));
    QVERIFY(!send->isEnabled()); // must be a .cybou name
    to->setText(QStringLiteral("alice.cybou"));
    QVERIFY(send->isEnabled());

    const int before = model->mailItems().size();
    send->click();
    QCOMPARE(model->mailItems().size(), before + 1);
    const auto& sent = model->mailItems().first();
    QCOMPARE(sent.folder, CybouMailFolder::Sent);
    // Finality-first: a new message starts local, never as Sent.
    QCOMPARE(sent.state, CybouContentState::Preparing);
    QCOMPARE(CybouProduct::mailStateText(sent), QStringLiteral("Preparing…"));

    // Attachments stay local until Send, then follow the message lifecycle.
    auto* composer = mail->composer();
    mail->openCompose();
    to->setText(QStringLiteral("alice.cybou"));
    body->setPlainText(QStringLiteral("see attached"));
    CybouAttachmentItem local;
    local.id = QStringLiteral("att-local");
    local.name = QStringLiteral("plan.pdf");
    local.logical_size = 1024;
    composer->addProtectedAttachment(local);
    QCOMPARE(composer->attachments().size(), 1);
    send->click();
    const auto& with_attachment = model->mailItems().first();
    QCOMPARE(with_attachment.attachments.size(), 1);
    QCOMPARE(with_attachment.attachments.first().state, CybouContentState::Preparing);
    model->setMailState(with_attachment.id, CybouContentState::NeedsAttention);
    QCOMPARE(model->mailItems().first().state, CybouContentState::NeedsAttention);
    QSignalSpy retry{model, &CybouDesktopModel::mailSendRequested};
    model->retrySendMail(model->mailItems().first().id);
    QCOMPARE(retry.count(), 1);
    QCOMPARE(model->mailItems().first().state, CybouContentState::Preparing);
    QCOMPARE(CybouProduct::contentStateText(CybouContentState::WaitingForConfirmation, false),
        QStringLiteral("Waiting for network"));

    // Without a connected Mail backend, Send stays disabled and says why.
    CybouCapabilities caps = model->capabilities();
    caps.mail = false;
    model->setCapabilities(caps);
    mail->openCompose();
    to->setText(QStringLiteral("alice.cybou"));
    body->setPlainText(QStringLiteral("hello"));
    QVERIFY(!send->isEnabled());
}

void CybouShellTests::mailFilesCrossProduct()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    const int files_before = model->fileItems().size();

    // Mail attachment -> Save to Files: a new catalog reference, same content.
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->openMessage(QStringLiteral("m-contract"));
    QPushButton* save{nullptr};
    for (auto* button : mail->reader()->findChildren<QPushButton*>()) {
        if (button->property("cybouId").toString() == QLatin1String{"saveToFiles"}) save = button;
    }
    QVERIFY(save);
    QVERIFY(save->isEnabled());
    save->click();
    QCOMPARE(model->fileItems().size(), files_before + 1);
    const auto& saved = model->fileItems().last();
    QCOMPARE(saved.name, QStringLiteral("contract-signed.pdf"));
    QCOMPARE(saved.state, CybouContentState::Protected); // reused, not re-uploaded
    // Saving again reuses the same Files item.
    QCOMPARE(model->saveAttachmentToFiles(QStringLiteral("m-contract"), QStringLiteral("c-contract")), saved.id);
    QCOMPARE(model->fileItems().size(), files_before + 1);

    // Files -> Send by CYBOU Mail: compose opens with the file attached.
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files->onSendByMail);
    files->onSendByMail(QStringLiteral("f-report"));
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Mail));
    const auto attachments = mail->composer()->attachments();
    QCOMPARE(attachments.size(), 1);
    QCOMPARE(attachments.first().name, QStringLiteral("report.pdf"));
    QCOMPARE(attachments.first().state, CybouContentState::Protected);
    // Unfinished uploads cannot be sent by Mail yet.
    QVERIFY(!model->attachmentFromFile(QStringLiteral("f-archive")).has_value());
}

void CybouShellTests::walletPageShowsBalances()
{
    auto window = makeWindow();
    auto* wallet = window->page(CybouPage::Wallet);
    QVERIFY(wallet);
    const auto find_button = [wallet](const char* id) -> QPushButton* {
        for (auto* button : wallet->findChildren<QPushButton*>()) {
            if (button->property("cybouId").toString() == QLatin1String{id}) return button;
        }
        return nullptr;
    };
    // Without an Identity, Send and Receive are unavailable.
    QVERIFY(!find_button("walletSend")->isEnabled());
    QVERIFY(!find_button("walletReceive")->isEnabled());

    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    bool found_available = false;
    for (const auto* label : wallet->findChildren<QLabel*>()) {
        if (label->text() == cybouAmountText(5820)) found_available = true;
        QVERIFY(!label->text().contains(QLatin1String{"Mail fee"}));
    }
    QVERIFY(found_available);

    // Send to a .cybou name; the fee is shown before sending.
    find_button("walletSend")->click();
    auto* to = wallet->findChild<QLineEdit*>(QStringLiteral("walletTo"));
    auto* amount = wallet->findChild<QLineEdit*>(QStringLiteral("walletAmount"));
    auto* confirm = find_button("walletConfirm");
    to->setText(QStringLiteral("stan.cybou"));
    amount->setText(QStringLiteral("100"));
    QVERIFY(!confirm->isEnabled()); // not to yourself
    to->setText(QStringLiteral("bobby.cybou"));
    amount->setText(QStringLiteral("999999"));
    QVERIFY(!confirm->isEnabled()); // more than available
    amount->setText(QStringLiteral("100"));
    QVERIFY(confirm->isEnabled());
    QSignalSpy requested{model, &CybouDesktopModel::paymentRequested};
    confirm->click();
    QCOMPARE(requested.count(), 1);
    QVERIFY(model->paymentPending());
    model->setPaymentFinished(true);
    QVERIFY(!model->paymentPending());
}

void CybouShellTests::filesGateActions()
{
    auto window = makeWindow();
    // Without an identity Files explains the gate and New is disabled.
    auto* page = window->page(CybouPage::Files);
    QVERIFY(page);
    QPushButton* create{nullptr};
    for (auto* button : page->findChildren<QPushButton*>()) {
        if (button->property("cybouId").toString() == QLatin1String{"filesNew"}) create = button;
    }
    QVERIFY(create);
    QVERIFY(!create->isEnabled());
    QVERIFY(!page->findChild<QFrame*>(QStringLiteral("identityBanner"))->isHidden());
}

void CybouShellTests::filesNavigationAndViews()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("files")));
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);
    // My files: folders first, trashed items hidden.
    const QStringList root{QStringLiteral("f-docs"), QStringLiteral("f-photos"), QStringLiteral("f-archive"),
        QStringLiteral("f-photo"), QStringLiteral("f-report")};
    QCOMPARE(files->visibleIds(), root);
    files->openFolder(QStringLiteral("f-docs"));
    QCOMPARE(files->visibleIds(), (QStringList{QStringLiteral("f-budget"), QStringLiteral("f-notes")}));
    files->setView(StoragePage::View::Starred);
    QCOMPARE(files->visibleIds(), QStringList{QStringLiteral("f-report")});
    files->setView(StoragePage::View::Trash);
    QCOMPARE(files->visibleIds(), QStringList{QStringLiteral("f-old")});
    files->setView(StoragePage::View::Recent);
    QCOMPARE(files->visibleIds().first(), QStringLiteral("f-report"));
    // Grid shows the same items.
    files->setGridMode(true);
    QVERIFY(files->gridMode());
    QCOMPARE(files->findChild<QListWidget*>(QStringLiteral("filesGrid"))->count(), files->visibleIds().size());
    // Search is local over the private catalog.
    auto* search = files->findChild<QLineEdit*>(QStringLiteral("filesSearch"));
    QCOMPARE(search->placeholderText(), QStringLiteral("Search files"));
    search->setText(QStringLiteral("mountain"));
    QCOMPARE(files->visibleIds(), QStringList{QStringLiteral("f-mountain")});
    search->clear();

    // Organization is local catalog state.
    files->setView(StoragePage::View::MyFiles);
    const QString folder = model->createFolder(QStringLiteral("Taxes"));
    QVERIFY(files->visibleIds().contains(folder));
    model->moveFile(QStringLiteral("f-photo"), folder);
    QVERIFY(!files->visibleIds().contains(QStringLiteral("f-photo")));
    model->renameFile(QStringLiteral("f-photo"), QStringLiteral("receipt.jpg"));
    QCOMPARE(model->fileItem(QStringLiteral("f-photo"))->name, QStringLiteral("receipt.jpg"));
    model->trashFile(folder);
    QVERIFY(model->fileItem(QStringLiteral("f-photo"))->trashed); // contents follow the folder
    model->restoreFile(folder);
    QVERIFY(!model->fileItem(QStringLiteral("f-photo"))->trashed);

    // Details drawer shows user-facing status, never chunk/provider data.
    files->showDetails(QStringLiteral("f-report"));
    QCOMPARE(files->detailsId(), QStringLiteral("f-report"));

    // Upload: the item appears immediately as Preparing (finality-first).
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("notes.txt"));
    QFile file{path};
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();
    QSignalSpy uploads{model, &CybouDesktopModel::fileUploadRequested};
    files->uploadFiles({path});
    QCOMPARE(uploads.count(), 1);
    const QString uploaded = uploads.first().at(0).toString();
    QCOMPARE(model->fileItem(uploaded)->state, CybouContentState::Preparing);
    QCOMPARE(model->fileItem(uploaded)->logical_size, quint64{5});
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

void CybouShellTests::normalUiAvoidsProtocolVocabulary()
{
    // docs/cybou/82-83: protocol machinery stays out of the normal UI.
    const QStringList forbidden{QStringLiteral("MailTx"), QStringLiteral("BFT"), QStringLiteral("alidator"),
        QStringLiteral("ObjectID"), QStringLiteral("Object ID"), QStringLiteral("Merkle"), QStringLiteral("ChunkID"),
        QStringLiteral("RootPublication"), QStringLiteral("nonce"), QStringLiteral("block hash"),
        QStringLiteral("provider"), QStringLiteral("KEM capsule"), QStringLiteral("Backup"), QStringLiteral("shard"),
        QStringLiteral("Mail fee")};
    for (const auto& fixture : {QStringLiteral("empty"), QStringLiteral("active"), QStringLiteral("offline")}) {
        auto window = makeWindow();
        QVERIFY(CybouUiFixtures::apply(*window->desktopModel(), fixture));
        for (int i = 0; i < window->pageCount(); ++i) {
            QStringList texts;
            for (const auto* label : window->pageAt(i)->findChildren<QLabel*>()) texts << label->text();
            for (const auto* button : window->pageAt(i)->findChildren<QAbstractButton*>()) texts << button->text();
            for (const auto& text : texts) {
                for (const auto& word : forbidden) {
                    QVERIFY2(!text.contains(word, Qt::CaseInsensitive),
                        qPrintable(QStringLiteral("page %1 (%2) shows \"%3\"").arg(i).arg(fixture, text)));
                }
            }
        }
    }
}

void CybouShellTests::layoutsFitWithoutHorizontalScroll()
{
    // Supported desktop widths must never force a wider window or a
    // horizontal scrollbar; Mail switches between two and three panes.
    auto window = makeWindow();
    QVERIFY(CybouUiFixtures::apply(*window->desktopModel(), QStringLiteral("active")));
    window->show();
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    for (const QSize size : {QSize{1040, 720}, QSize{1280, 860}, QSize{1600, 900}, QSize{1920, 1080}}) {
        window->resize(size);
        for (int i = 0; i < window->pageCount(); ++i) {
            window->showPage(static_cast<CybouPage>(i));
            QCoreApplication::processEvents();
            QVERIFY2(window->width() <= size.width(),
                qPrintable(QStringLiteral("page %1 widened the window to %2 at %3").arg(i).arg(window->width()).arg(size.width())));
            QVERIFY2(window->centralWidget()->minimumSizeHint().width() <= size.width(),
                qPrintable(QStringLiteral("page %1 needs %2 px at %3").arg(i)
                    .arg(window->centralWidget()->minimumSizeHint().width()).arg(size.width())));
        }
        QCOMPARE(window->sidebarCompact(), window->width() < 1180);
        window->showPage(CybouPage::Mail);
        QCoreApplication::processEvents();
        QCOMPARE(mail->threePane(), window->width() >= 1400);
    }
    window->close();
}

void CybouShellTests::keyboardAndAsyncUnlock()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    window->showPage(CybouPage::Mail);
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->setFocus();
    QTest::keyClick(mail, Qt::Key_N, Qt::ControlModifier);
    QVERIFY(mail->composer()->isVisible());
    mail->openMessage(QStringLiteral("m-project"));
    mail->reader()->setFocus();
    QTest::keyClick(mail->reader(), Qt::Key_R);
    QVERIFY(mail->composer()->isVisible());
    QCOMPARE(mail->findChild<QLineEdit*>(QStringLiteral("recipientEdit"))->text(), QStringLiteral("alice.cybou"));

    // Vault unlock completes asynchronously and never blocks the GUI thread.
    model->requestLockVault();
    QCOMPARE(model->status().identity_state, CybouIdentityState::Locked);
    bool done = false;
    model->requestUnlockIdentityAsync(QStringLiteral("correct horse battery"), [&done](bool ok) { done = ok; });
    QVERIFY(!done);
    for (int i = 0; i < 50 && !done; ++i) QTest::qWait(10);
    QVERIFY(done);
    QCOMPARE(model->status().identity_state, CybouIdentityState::Active);
    window->close();
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
