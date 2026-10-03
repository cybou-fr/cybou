// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/test/cyboushelltests.h>

#include <qt/cybouapplicationbackend.h>
#include <qt/cyboucoreapplicationadapter.h>
#include <qt/cyboudesktopcontroller.h>
#include <qt/cyboufixturebackend.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cybouactivity.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>
#include <qt/cybounotifier.h>
#include <qt/cybouuifixtures.h>
#include <qt/pages/emailpage.h>
#include <qt/pages/identitypage.h>
#include <qt/pages/homepage.h>
#include <qt/pages/mailcompose.h>
#include <qt/pages/mailreader.h>
#include <qt/pages/storagepage.h>
#include <qt/pages/diagnosticspage.h>
#include <qt/pages/networkauthoritypage.h>
#include <QTableWidget>

#include <cybou/network_genesis.h>
#include <cybou/node_runtime.h>
#include <cybou/official_networks.h>
#include <cybou/p2p/session.h>
#include <test/cybou_test_helpers.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_storage_test_network.h>

#include <cybou/recovery_phrase.h>

#include <QApplication>
#include <QAbstractButton>
#include <QMenu>
#include <QMimeData>
#include <QPalette>
#include <QCompleter>
#include <QMenuBar>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSettings>
#include <QStackedWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QTest>
#include <QToolButton>
#include <QDialog>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
#include <array>
#include <cstdint>
#include <vector>

namespace {
/** Initializes a canonical state DB for a different (test PoA key) network. */
bool WriteForeignNetworkState(const QString& directory)
{
    const auto genesis = cybou::CreateDevGenesisState();
    cybou::CybouNodeRuntime runtime{{
        .network_genesis = cybou::CreateTestNetworkGenesis(genesis, cybou::TestPoaFinalizerPublicKey(0x33), cybou::TestNetworkPublicKey(0x33)),
        .data_dir = std::filesystem::path{directory.toStdU16String()} / "cybou_state",
    }};
    return runtime.InitializeGenesis(genesis);
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

namespace {
/**
 * Records commands and never answers on its own, so tests can check that
 * the model issues each command once and changes nothing until the backend
 * reports a result.
 */
class RecordingBackend final : public CybouApplicationBackend
{
public:
    using CybouApplicationBackend::CybouApplicationBackend;
    QStringList commands;
    bool available{true};
    bool open{false};

    bool mailAvailable() const override { return available; }
    bool filesAvailable() const override { return available; }
    void openIdentity() override { open = true; commands << QStringLiteral("open"); }
    void closeIdentity() override { open = false; commands << QStringLiteral("close"); }
    void saveMailDraft(const CybouMailItem& d) override { commands << QStringLiteral("draft:") + d.id; }
    void sendMail(const CybouMailItem& m) override { commands << QStringLiteral("send:") + m.id; }
    void retryMail(const QString& id) override { commands << QStringLiteral("retry:") + id; }
    void setMailRead(const QString& id, bool) override { commands << QStringLiteral("read:") + id; }
    void setMailStarred(const QString& id, bool) override { commands << QStringLiteral("starMail:") + id; }
    void moveMail(const QString& id, CybouMailFolder) override { commands << QStringLiteral("moveMail:") + id; }
    void deleteMail(const QString& id) override { commands << QStringLiteral("deleteMail:") + id; }
    void downloadAttachment(const QString& m, const QString&, const QString&) override { commands << QStringLiteral("attachment:") + m; }
    void saveAttachmentToFiles(const QString& m, const QString&, const QString&) override { commands << QStringLiteral("saveAttachment:") + m; }
    void uploadFile(const QString& id, const QString&, const QString&) override { commands << QStringLiteral("upload:") + id; }
    void downloadFile(const QString& id, const QString&) override { commands << QStringLiteral("download:") + id; }
    void createFolder(const QString& id, const QString&, const QString&) override { commands << QStringLiteral("folder:") + id; }
    void renameFile(const QString& id, const QString&) override { commands << QStringLiteral("rename:") + id; }
    void moveFile(const QString& id, const QString&) override { commands << QStringLiteral("move:") + id; }
    void copyFile(const QString& id, const QString&, const QString&) override { commands << QStringLiteral("copy:") + id; }
    void setFileStarred(const QString& id, bool) override { commands << QStringLiteral("starFile:") + id; }
    void trashFile(const QString& id) override { commands << QStringLiteral("trash:") + id; }
    void restoreFile(const QString& id) override { commands << QStringLiteral("restore:") + id; }
    void deleteFile(const QString& id) override { commands << QStringLiteral("delete:") + id; }

    void setAvailable(bool value)
    {
        available = value;
        Q_EMIT availabilityChanged();
    }
};

CybouFeatureAvailability AllFeatureAvailability()
{
    CybouFeatureAvailability caps;
    caps.account_creation = true;
    caps.payments = true;
    caps.mail = true;
    caps.files = true;
    return caps;
}

CybouFileItem ProtectedFile(const QString& id, const QString& name)
{
    CybouFileItem item;
    item.id = id;
    item.name = name;
    item.state = CybouContentState::Protected;
    return item;
}
} // namespace

CybouShellTests::~CybouShellTests() = default;

std::unique_ptr<CybouMainWindow> CybouShellTests::makeWindow()
{
    // Tests render the light appearance unless a test forces another.
    if (!qEnvironmentVariableIsSet("CYBOU_APPEARANCE")) qputenv("CYBOU_APPEARANCE", "light");
    QSettings{}.setValue(QStringLiteral("desktop/language"), QStringLiteral("en"));
    return std::make_unique<CybouMainWindow>(std::filesystem::path{});
}

void CybouShellTests::mainWindowStarts()
{
    auto window = makeWindow();
    QVERIFY(window);
    QVERIFY(window->centralWidget());
    QVERIFY(window->windowTitle().contains(QStringLiteral("CYBOU")));
    QCOMPARE(window->pageCount(), 8);
    // The Network Authority entry exists only for the genesis-proven authority.
    auto* authority_nav = window->findChild<QAbstractButton*>(
        QStringLiteral("navButton%1").arg(static_cast<int>(CybouPage::NetworkAuthority)));
    QVERIFY(authority_nav);
    QVERIFY(authority_nav->isHidden());
    window->desktopModel()->setNetworkAuthority(CybouNetworkAuthorityStatus{.proven = true, .finalized_height = 5});
    QVERIFY(!authority_nav->isHidden());
    window->desktopModel()->setNetworkAuthority({});
    QVERIFY(authority_nav->isHidden());
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

void CybouShellTests::identityCreateFollowsFeatureAvailability()
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

    // FeatureAvailability come from the desktop model boundary.
    model->setFixtureMode(true); // deterministic recovery words, no core
    CybouFeatureAvailability featureAvailability;
    featureAvailability.account_creation = true;
    model->setFeatureAvailability(featureAvailability);
    QVERIFY(create->isEnabled());
    model->setSyncing(true);
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
    CybouFeatureAvailability featureAvailability;
    featureAvailability.account_creation = true;
    model->setFeatureAvailability(featureAvailability);
    auto* home = window->page(CybouPage::Home);
    QPushButton* restore{nullptr};
    QPushButton* submit{nullptr};
    for (auto* button : home->findChildren<QPushButton*>()) {
        if (button->property("cybouId").toString() == QLatin1String{"restoreIdentity"}) restore = button;
        if (button->property("cybouId").toString() == QLatin1String{"restoreSubmit"}) submit = button;
    }
    QVERIFY(restore && submit);
    restore->click();
    auto* password = home->findChild<QLineEdit*>(QStringLiteral("restorePassword"));
    auto* confirm = home->findChild<QLineEdit*>(QStringLiteral("restorePasswordConfirm"));
    auto* first = home->findChild<QLineEdit*>(QStringLiteral("recoveryWord0"));
    QVERIFY(password && confirm && first);
    password->setText(QStringLiteral("correct horse battery"));
    confirm->setText(QStringLiteral("correct horse battery"));
    // Pasting the whole phrase into the first field fills all 24.
    QStringList words;
    for (int i = 0; i < 23; ++i) words << CybouDesktopModel::recoveryWordList().at(i * 7);
    words << QStringLiteral("notaword");
    first->setText(words.join(QLatin1Char{' '}));
    Q_EMIT first->textEdited(first->text());
    QCOMPARE(home->findChild<QLineEdit*>(QStringLiteral("recoveryWord23"))->text(), QStringLiteral("notaword"));
    QVERIFY(!submit->isEnabled()); // unknown word blocks restore
    home->findChild<QLineEdit*>(QStringLiteral("recoveryWord23"))->setText(QStringLiteral("zoo"));
    QVERIFY(submit->isEnabled());
    submit->click();
    QCOMPARE(model->status().identity_state, CybouIdentityState::Restoring);
    QVERIFY(first->text().isEmpty());
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
    QCOMPARE(mail->visibleMessageIds(), (QStringList{QStringLiteral("m-sent-validated"),
        QStringLiteral("m-sent-securing"), QStringLiteral("m-sent-1")}));
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
    QCOMPARE(sent.state, CybouContentState::Local);
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
    QCOMPARE(with_attachment.attachments.first().state, CybouContentState::Local);
    model->setMailState(with_attachment.id, CybouContentState::NeedsAttention);
    QCOMPARE(model->mailItems().first().state, CybouContentState::NeedsAttention);
    model->requestRetryMail(model->mailItems().first().id);
    QCOMPARE(model->mailItems().first().state, CybouContentState::Local);
    QCOMPARE(CybouProduct::contentWithOperationText(CybouContentState::Local, CybouOperationState::Submitted, false),
        QStringLiteral("Waiting for network"));

    // Without a connected Mail backend, Send stays disabled and says why.
    CybouFeatureAvailability caps = model->featureAvailability();
    caps.mail = false;
    model->setFeatureAvailability(caps);
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
    QAction* save{nullptr};
    for (auto* action : mail->reader()->findChildren<QAction*>()) {
        if (action->objectName() == QLatin1String{"saveToFiles"}) save = action;
    }
    QVERIFY(save);
    QVERIFY(save->isEnabled());
    save->trigger();
    QCOMPARE(model->fileItems().size(), files_before + 1);
    const auto& saved = model->fileItems().last();
    QCOMPARE(saved.name, QStringLiteral("contract-signed.pdf"));
    QCOMPARE(saved.state, CybouContentState::Protected); // reused, not re-uploaded
    // Saving again reuses the same Files item.
    QCOMPARE(model->requestSaveAttachmentToFiles(QStringLiteral("m-contract"), QStringLiteral("c-contract")), saved.id);
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

void CybouShellTests::activityListsRunningAndFailedOperations()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    QVERIFY(CybouActivityOperations(model).isEmpty());
    CybouFileItem uploading;
    uploading.id = QStringLiteral("f1");
    uploading.name = QStringLiteral("report.pdf");
    uploading.state = CybouContentState::Local;
    uploading.operation_state = CybouOperationState::Submitted;
    CybouFileItem failed;
    failed.id = QStringLiteral("f2");
    failed.name = QStringLiteral("photo.jpg");
    failed.state = CybouContentState::NeedsAttention;
    CybouFileItem done;
    done.id = QStringLiteral("f3");
    done.name = QStringLiteral("done.txt");
    done.state = CybouContentState::Protected;
    model.setFileItems({uploading, failed, done});

    // Failed work comes first; settled items never appear.
    const auto operations = CybouActivityOperations(model);
    QCOMPARE(operations.size(), 2);
    QVERIFY(operations.at(0).attention);
    QCOMPARE(operations.at(0).id, QStringLiteral("f2"));
    QCOMPARE(operations.at(1).id, QStringLiteral("f1"));

    CybouActivityButton button{&model};
    QCOMPARE(button.text(), QStringLiteral("1 needs attention"));
    model.setFileItems({done});
    QCOMPARE(button.text(), QStringLiteral("0 in progress"));
    QVERIFY(button.isHidden());
}

void CybouShellTests::contactsComeFromMailAndPayments()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    model.setPrimaryName(QStringLiteral("me.cybou"));
    QSignalSpy changed{&model, &CybouDesktopModel::contactsChanged};
    const auto now = QDateTime::currentDateTime();
    CybouMailItem received;
    received.id = QStringLiteral("m1");
    received.from_name = QStringLiteral("alice.cybou");
    received.to_name = QStringLiteral("me.cybou");
    received.time = now.addDays(-2);
    CybouMailItem sent;
    sent.id = QStringLiteral("m2");
    sent.folder = CybouMailFolder::Sent;
    sent.from_name = QStringLiteral("me.cybou");
    sent.to_name = QStringLiteral("bobby.cybou");
    sent.time = now.addDays(-1);
    CybouMailItem draft;
    draft.id = QStringLiteral("d1");
    draft.draft = true;
    draft.folder = CybouMailFolder::Drafts;
    draft.to_name = QStringLiteral("never.cybou");
    draft.time = now;
    model.setMailItems({received, sent, draft});
    CybouWalletEntry paid;
    paid.id = QStringLiteral("w1");
    paid.kind = CybouWalletEntryKind::Sent;
    paid.counterparty_name = QStringLiteral("carol.cybou");
    paid.time = now;
    model.setWalletEntries({paid});

    // Most recent first; never yourself, never an unsent draft's recipient.
    QVERIFY(changed.count() > 0);
    QStringList names;
    for (const auto& contact : model.contacts()) names << contact.name;
    QCOMPARE(names, (QStringList{QStringLiteral("carol.cybou"), QStringLiteral("bobby.cybou"),
                                 QStringLiteral("alice.cybou")}));
}

void CybouShellTests::walletLocksBalanceIntoSystemBalance()
{
    auto window = makeWindow();
    auto* wallet = window->page(CybouPage::Wallet);
    const auto find_button = [wallet](const char* id) -> QPushButton* {
        for (auto* button : wallet->findChildren<QPushButton*>()) {
            if (button->property("cybouId").toString() == QLatin1String{id}) return button;
        }
        return nullptr;
    };
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    QVERIFY(find_button("walletLock")->isEnabled());

    // Balance -> System Balance is irreversible and never more than available.
    QVERIFY(!model->requestLockToSystemBalance(0));
    QVERIFY(!model->requestLockToSystemBalance(model->status().balance + 1));
    const quint64 balance = model->status().balance;
    const quint64 system = model->status().system_balance;
    QSignalSpy finished{model, &CybouDesktopModel::systemLockFinished};
    QVERIFY(model->requestLockToSystemBalance(100));
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.at(0).at(0).toBool());
    QCOMPARE(model->status().balance, balance - 100);
    QCOMPARE(model->status().system_balance, system + 100);

    // Nothing spendable: Send and Add from Balance are unavailable, with guidance.
    model->setBalances(0, 5000);
    QVERIFY(!find_button("walletSend")->isEnabled());
    QVERIFY(!find_button("walletLock")->isEnabled());
    bool guidance = false;
    for (const auto* label : wallet->findChildren<QLabel*>()) {
        if (label->text().contains(QLatin1String{"no spendable CYBOU"})) guidance = true;
    }
    QVERIFY(guidance);
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
    confirm->click(); // review
    QCOMPARE(requested.count(), 0);
    QVERIFY(!wallet->findChild<QLabel*>(QStringLiteral("walletReview"))->isHidden());
    confirm->click(); // confirm and send
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
    // Sorting keeps folders first.
    files->setView(StoragePage::View::MyFiles);
    files->sortBy(1, true); // Size, largest first
    QCOMPARE(files->visibleIds().mid(0, 3), (QStringList{QStringLiteral("f-docs"), QStringLiteral("f-photos"), QStringLiteral("f-archive")}));
    files->sortBy(0, false);
    files->setView(StoragePage::View::Recent);
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
    const QString folder = model->requestCreateFolder(QStringLiteral("Taxes"));
    QVERIFY(files->visibleIds().contains(folder));
    model->requestMoveFile(QStringLiteral("f-photo"), folder);
    QVERIFY(!files->visibleIds().contains(QStringLiteral("f-photo")));
    model->requestRenameFile(QStringLiteral("f-photo"), QStringLiteral("receipt.jpg"));
    QCOMPARE(model->fileItem(QStringLiteral("f-photo"))->name, QStringLiteral("receipt.jpg"));
    model->requestTrashFile(folder);
    QVERIFY(model->fileItem(QStringLiteral("f-photo"))->trashed); // contents follow the folder
    model->requestRestoreFile(folder);
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
    const int before_upload = model->fileItems().size();
    files->uploadFiles({path});
    QCOMPARE(model->fileItems().size(), before_upload + 1);
    const QString uploaded = model->fileItems().last().id;
    QCOMPARE(model->fileItem(uploaded)->name, QStringLiteral("notes.txt"));
    QVERIFY(model->fileItem(uploaded)->available_offline); // the uploading device keeps its copy
    QCOMPARE(model->fileItem(uploaded)->state, CybouContentState::Local);
    QCOMPARE(model->fileItem(uploaded)->logical_size, quint64{5});
}

void CybouShellTests::networkMonitorUsesCoreSnapshot()
{
    CybouDesktopModel model{QStringLiteral("LAB")};
    cybou::NodeDiagnosticsSnapshot snapshot;
    snapshot.network_binding="lab"; snapshot.height=12; snapshot.tip="tip"; snapshot.state_root="root";
    snapshot.peers.push_back({"127.0.0.1:30471",9,"provider"});
    snapshot.operations.push_back({"operation",3,12});
    model.setNetworkDiagnostics(snapshot);
    DiagnosticsPage page{&model,[]{}};
    QPushButton* monitor=nullptr;
    for (auto* button : page.findChildren<QPushButton*>()) if (button->text().contains(QStringLiteral("Network Monitor"))) monitor=button;
    QVERIFY(monitor);
    monitor->click();
    auto* peers=page.findChild<QTableWidget*>(QStringLiteral("networkMonitorPeers"));
    auto* operations=page.findChild<QTableWidget*>(QStringLiteral("networkMonitorOperations"));
    QVERIFY(peers); QVERIFY(operations);
    QCOMPARE(peers->rowCount(),1);
    QCOMPARE(peers->columnCount(),4);
    QCOMPARE(peers->item(0,1)->text(),QStringLiteral("9"));
    QCOMPARE(peers->item(0,2)->text(),QStringLiteral("3"));
    QCOMPARE(peers->item(0,3)->text(),QStringLiteral("provider"));
    QCOMPARE(operations->item(0,1)->text(),QStringLiteral("Finalized"));
    snapshot.peers.clear(); snapshot.operations.clear();
    model.setNetworkDiagnostics(snapshot);
    QCOMPARE(peers->rowCount(),0);
    QCOMPARE(operations->rowCount(),0);
}

void CybouShellTests::authorityDashboardUsesLocalHeightObservation()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    model.setNodeStatus(true, 2, true);
    CybouNetworkAuthorityStatus authority;
    authority.proven = true;
    authority.finalized_height = 10;
    model.setNetworkAuthority(authority);
    NetworkAuthorityPage page{&model};

    auto has_text = [&page](const QString& text) {
        for (const auto* label : page.findChildren<QLabel*>()) if (label->text() == text) return true;
        return false;
    };
    QVERIFY(has_text(QStringLiteral("Not observed yet")));
    QVERIFY(has_text(QStringLiteral("Synced")));
    QVERIFY(has_text(QStringLiteral("Waiting to observe a height change in this view")));

    authority.finalized_height = 11;
    model.setNetworkAuthority(authority);
    QVERIFY(!has_text(QStringLiteral("Not observed yet")));
    const auto labels = page.findChildren<QLabel*>();
    QVERIFY(std::any_of(labels.begin(), labels.end(), [](const QLabel* label) {
        return label->text().contains(QStringLiteral("Height changed "));
    }));
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

    auto* model = window->desktopModel();
    QVERIFY(model);
    auto has_text = [network](const QString& text) {
        for (const auto* label : network->findChildren<QLabel*>()) if (label->text() == text) return true;
        return false;
    };
    QVERIFY(has_text(QStringLiteral("Waiting for a valid Geo database")));
    model->setGeoAdmissionStatus(CybouGeoAdmissionStatus::Ready);
    QVERIFY(has_text(QStringLiteral("Ready")));
    model->setGeoAdmissionStatus(CybouGeoAdmissionStatus::NotRequired);
    QVERIFY(has_text(QStringLiteral("Not required by Lab policy")));
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
    QSignalSpy create_failed{&model, &CybouDesktopModel::identityCreationFailed};
    QSignalSpy create_requested{&model, &CybouDesktopModel::createIdentityRequested};
    model.setSyncing(true);
    QVERIFY(model.featureAvailability().account_creation);
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
        QStringLiteral("Mail fee"),
        // Identity Authority is a network-capability metric, never social trust.
        QStringLiteral("reputation"), QStringLiteral("trust score"), QStringLiteral("trusted user")};
    for (const auto& fixture : {QStringLiteral("empty"), QStringLiteral("active"), QStringLiteral("offline")}) {
        auto window = makeWindow();
        QVERIFY(CybouUiFixtures::apply(*window->desktopModel(), fixture));
        for (int i = 0; i < window->pageCount(); ++i) {
            // Operator-only page, reachable solely by the genesis-proven authority.
            if (i == static_cast<int>(CybouPage::NetworkAuthority)) continue;
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

void CybouShellTests::notificationsOfferUndo()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
    window->show();
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->openMessage(QStringLiteral("m-dinner"));
    QToolButton* trash{nullptr};
    for (auto* button : mail->reader()->findChildren<QToolButton*>()) {
        if (button->toolTip() == QLatin1String{"Move to Trash"}) trash = button;
    }
    QVERIFY(trash);
    trash->click();
    QCOMPARE(model->mailItem(QStringLiteral("m-dinner"))->folder, CybouMailFolder::Trash);
    auto* notifier = window->notifier();
    QVERIFY(notifier->isVisible());
    QCOMPARE(notifier->text(), QStringLiteral("Moved to Trash"));
    QVERIFY(notifier->hasAction());
    notifier->trigger();
    QCOMPARE(model->mailItem(QStringLiteral("m-dinner"))->folder, CybouMailFolder::Inbox);
    QVERIFY(!notifier->isVisible());
    window->close();
}

void CybouShellTests::homeFirstStepsAndQuickActions()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    auto* home = dynamic_cast<HomePage*>(window->page(CybouPage::Home));
    QVERIFY(home->openFirstSteps().isEmpty()); // name, sent mail and files exist
    model->setNames({});
    model->setFileItems({});
    QCOMPARE(home->openFirstSteps(), (QStringList{QStringLiteral("name"), QStringLiteral("files")}));
    QVERIFY(home->onCompose);
    home->onCompose();
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Mail));
}

void CybouShellTests::globalSearchFindsMailAndFiles()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    auto* search = window->globalSearch();
    QVERIFY(search);
    auto* completer = search->completer();
    completer->setCompletionPrefix(QStringLiteral("contract"));
    QStringList hits;
    for (int i = 0; completer->setCurrentRow(i); ++i) hits << completer->currentCompletion();
    QVERIFY(hits.size() >= 1); // the signed-contract mail (subject + attachment)
    completer->setCompletionPrefix(QStringLiteral("budget"));
    QVERIFY(completer->completionCount() >= 1); // budget-2026.xlsx
    // Enter without a suggestion searches Mail.
    search->setText(QStringLiteral("dinner"));
    Q_EMIT search->returnPressed();
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Mail));
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    QCOMPARE(mail->visibleMessageIds(), QStringList{QStringLiteral("m-dinner")});
    QVERIFY(window->menuBar()->isHidden());
}

void CybouShellTests::darkAppearanceResolvesTokens()
{
    ScopedEnvironment appearance{"CYBOU_APPEARANCE", "dark"};
    auto window = makeWindow();
    QVERIFY(CybouTheme::isDark());
    // Tokens resolve to dark values; the sheet has no unresolved tokens.
    QVERIFY(CybouTheme::color(CybouTheme::CANVAS).lightness() < 60);
    QVERIFY(CybouTheme::color(CybouTheme::TEXT_PRIMARY).lightness() > 200);
    const QString sheet = CybouTheme::applicationStyleSheet();
    QVERIFY(!sheet.contains(QStringLiteral("@")));
    QVERIFY(qApp->palette().color(QPalette::Base).lightness() < 60);
    // Live reload keeps state and page.
    QVERIFY(CybouUiFixtures::apply(*window->desktopModel(), QStringLiteral("active")));
    window->showPage(CybouPage::Files);
    window->reloadAppearance();
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Files));
    QCOMPARE(window->pageCount(), 8);
    window.reset();
    qunsetenv("CYBOU_APPEARANCE");
    CybouTheme::setAppearance(CybouTheme::Appearance::Light);
    CybouTheme::applyTo(*qApp);
    QVERIFY(!CybouTheme::isDark());
}

void CybouShellTests::languageSwitchRebuildsShell()
{
    const QString saved_language = QSettings{}.value(QStringLiteral("desktop/language"), QStringLiteral("fr")).toString();
    auto window = makeWindow();
    window->showPage(CybouPage::Files);
    window->setLanguage(QStringLiteral("fr"));
    auto* home = window->findChild<QToolButton*>(QStringLiteral("navButton0"));
    QVERIFY(home);
    QCOMPARE(home->accessibleName(), QStringLiteral("Accueil"));
    window->desktopModel()->setNetworkAuthority(CybouNetworkAuthorityStatus{.proven = true});
    auto* authority_nav = window->findChild<QToolButton*>(QStringLiteral("navButton7"));
    QVERIFY(authority_nav);
    QVERIFY(!authority_nav->isHidden());
    QCOMPARE(authority_nav->accessibleName(), QStringLiteral("Autorité centrale"));
    auto* authority_proof = window->findChild<QLabel*>(QStringLiteral("networkAuthorityProof"));
    QVERIFY(authority_proof);
    QVERIFY(authority_proof->text().contains(QStringLiteral("signataire")));
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Files));

    const auto* old_page = window->page(CybouPage::Files);
    window->setLanguage(QStringLiteral("en"));
    home = window->findChild<QToolButton*>(QStringLiteral("navButton0"));
    QVERIFY(home);
    QCOMPARE(home->accessibleName(), QStringLiteral("Home"));
    authority_nav = window->findChild<QToolButton*>(QStringLiteral("navButton7"));
    QVERIFY(authority_nav);
    QVERIFY(!authority_nav->isHidden());
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Files));
    QVERIFY(window->page(CybouPage::Files) != old_page);

    window->setLanguage(saved_language);
}

void CybouShellTests::appearanceAndLanguageSwitchPreserveMailCompose()
{
    const QString saved_language = QSettings{}.value(QStringLiteral("desktop/language"), QStringLiteral("fr")).toString();
    auto window = makeWindow();
    QVERIFY(CybouUiFixtures::apply(*window->desktopModel(), QStringLiteral("mail")));
    window->showPage(CybouPage::Mail);
    auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->openCompose();
    auto* to = mail->composer()->findChild<QLineEdit*>(QStringLiteral("recipientEdit"));
    auto* subject = mail->composer()->findChild<QLineEdit*>(QStringLiteral("subjectEdit"));
    auto* body = mail->composer()->findChild<QTextEdit*>(QStringLiteral("composeBody"));
    QVERIFY(to && subject && body);
    to->setText(QStringLiteral("alice.cybou"));
    subject->setText(QStringLiteral("Draft survives refresh"));
    body->setPlainText(QStringLiteral("Keep this text and its composer open."));

    window->reloadAppearance();
    mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    QVERIFY(mail->isComposing());
    QCOMPARE(mail->composer()->findChild<QLineEdit*>(QStringLiteral("recipientEdit"))->text(), QStringLiteral("alice.cybou"));
    QCOMPARE(mail->composer()->findChild<QLineEdit*>(QStringLiteral("subjectEdit"))->text(), QStringLiteral("Draft survives refresh"));
    QCOMPARE(mail->composer()->findChild<QTextEdit*>(QStringLiteral("composeBody"))->toPlainText(),
        QStringLiteral("Keep this text and its composer open."));

    window->setLanguage(QStringLiteral("en"));
    mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    QVERIFY(mail->isComposing());
    QCOMPARE(mail->composer()->findChild<QTextEdit*>(QStringLiteral("composeBody"))->toPlainText(),
        QStringLiteral("Keep this text and its composer open."));
    window->setLanguage(saved_language);
}

void CybouShellTests::mailContextMenuAndMoves()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
    window->show();
    auto* mail = dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    const QStringList ids{QStringLiteral("m-dinner"), QStringLiteral("m-alina")};
    std::unique_ptr<QMenu> menu{mail->buildContextMenu(ids, nullptr)};
    QStringList names;
    for (const auto* action : menu->actions()) if (!action->objectName().isEmpty()) names << action->objectName();
    QVERIFY(!names.contains(QStringLiteral("mailReply"))); // multi-select has no Reply
    QVERIFY(names.contains(QStringLiteral("mailArchive")));
    // Mark as read, then drop both on Archive (as a drag onto the folder does).
    menu->findChild<QAction*>(QStringLiteral("mailMarkRead"))->trigger();
    QVERIFY(!model->mailItem(QStringLiteral("m-dinner"))->unread);
    mail->moveMessagesTo(ids, EmailPage::View::Archive);
    QCOMPARE(model->mailItem(QStringLiteral("m-alina"))->folder, CybouMailFolder::Archive);
    window->notifier()->trigger(); // Undo
    QCOMPARE(model->mailItem(QStringLiteral("m-alina"))->folder, CybouMailFolder::Inbox);
    // Sent mail never lands in Inbox.
    mail->moveMessagesTo({QStringLiteral("m-sent-1")}, EmailPage::View::Inbox);
    QCOMPARE(model->mailItem(QStringLiteral("m-sent-1"))->folder, CybouMailFolder::Sent);
}

void CybouShellTests::filesDropIntoFolders()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("files")));
    window->show();
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files->moveFilesTo({QStringLiteral("f-photo"), QStringLiteral("f-report")}, QStringLiteral("f-photos")));
    QCOMPARE(model->fileItem(QStringLiteral("f-report"))->parent_id, QStringLiteral("f-photos"));
    window->notifier()->trigger(); // Undo
    QVERIFY(model->fileItem(QStringLiteral("f-report"))->parent_id.isEmpty());
    // No folder cycles: Documents cannot move into itself or its child.
    const QString child = model->requestCreateFolder(QStringLiteral("Taxes"), QStringLiteral("f-docs"));
    QVERIFY(!files->moveFilesTo({QStringLiteral("f-docs")}, QStringLiteral("f-docs")));
    QVERIFY(!files->moveFilesTo({QStringLiteral("f-docs")}, child));
    // Drag mime round-trip.
    QMimeData data;
    data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("a\nb"));
    QCOMPARE(CybouUi::dragIds(&data, CybouUi::fileIdsMime()), (QStringList{QStringLiteral("a"), QStringLiteral("b")}));
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
    ScopedUnsetEnvironment finalizer_mode{"CYBOU_DEV_FINALIZER"};
    // A stray legacy network file is never read: startup uses compiled DEVNET only.
    {
        QFile stray{directory.filePath(QStringLiteral("network.bin"))};
        QVERIFY(stray.open(QIODevice::WriteOnly));
        stray.write("not a CYBOU network");
    }
    CybouDesktopModel model{QStringLiteral("CYBOU-DEV")};
    CybouDesktopController controller{&model, directory.path().toStdString()};
    QSignalSpy failures{&controller, &CybouDesktopController::startupFailed};
    {
        ScopedEnvironment p2p_host{"CYBOU_DEV_P2P_HOST", "127.0.0.1"};
        ScopedEnvironment bad_port{"CYBOU_DEV_P2P_PORT", "0"};
        controller.start();
    }
    QCOMPARE(failures.count(), 1);
    QVERIFY(!model.status().node_running);

    ScopedEnvironment p2p_host{"CYBOU_DEV_P2P_HOST", "127.0.0.1"};
    ScopedEnvironment p2p_port{"CYBOU_DEV_P2P_PORT", "1"};
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
    ScopedUnsetEnvironment finalizer_mode{"CYBOU_DEV_FINALIZER"};
    QVERIFY(WriteForeignNetworkState(directory.path()));

    CybouDesktopModel model{QStringLiteral("CYBOU-DEV")};
    CybouDesktopController controller{&model, directory.path().toStdString()};
    QSignalSpy failures{&controller, &CybouDesktopController::startupFailed};
    controller.start();
    QCOMPARE(failures.count(), 1);
    const auto reason = failures.takeFirst().at(0).toString();
    QVERIFY2(reason.contains(QStringLiteral("belongs to another network")) ||
        reason.contains(QStringLiteral("compiled DEVNET genesis")) ||
        reason == QStringLiteral("finalized chunk store network ID mismatch"), qPrintable(reason));
    QVERIFY(!model.status().node_running);
}

void CybouShellTests::backendCommandsDriveProjection()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    QVERIFY(backend.open);
    model.setFileItems({ProtectedFile(QStringLiteral("f1"), QStringLiteral("report.pdf"))});
    backend.commands.clear();

    // Persistent Files actions are commands, sent once; the projection waits.
    QSignalSpy files_changed{&model, &CybouDesktopModel::filesChanged};
    model.requestRenameFile(QStringLiteral("f1"), QStringLiteral("renamed.pdf"));
    model.requestMoveFile(QStringLiteral("f1"), QStringLiteral("folder"));
    model.requestTrashFile(QStringLiteral("f1"));
    model.requestFileStarred(QStringLiteral("f1"), true);
    const QString copy = model.requestCopyFile(QStringLiteral("f1"), {});
    const QString folder = model.requestCreateFolder(QStringLiteral("Taxes"));
    QVERIFY(!copy.isEmpty());
    QVERIFY(!folder.isEmpty());
    QCOMPARE(backend.commands, (QStringList{QStringLiteral("rename:f1"), QStringLiteral("move:f1"),
        QStringLiteral("trash:f1"), QStringLiteral("starFile:f1"), QStringLiteral("copy:f1"),
        QStringLiteral("folder:") + folder}));
    QCOMPARE(files_changed.count(), 0);
    QCOMPARE(model.fileItems().size(), 1);
    QCOMPARE(model.fileItem(QStringLiteral("f1"))->name, QStringLiteral("report.pdf"));
    QVERIFY(!model.fileItem(QStringLiteral("f1"))->trashed);
    QVERIFY(!model.fileItem(folder));
    // Unknown items and no-op changes send nothing.
    backend.commands.clear();
    model.requestRenameFile(QStringLiteral("missing"), QStringLiteral("x"));
    model.requestRenameFile(QStringLiteral("f1"), QStringLiteral("report.pdf"));
    QVERIFY(backend.commands.isEmpty());

    // The backend's reply is what changes the projection.
    auto renamed = *model.fileItem(QStringLiteral("f1"));
    renamed.name = QStringLiteral("renamed.pdf");
    Q_EMIT backend.fileItemChanged(renamed);
    QCOMPARE(model.fileItem(QStringLiteral("f1"))->name, QStringLiteral("renamed.pdf"));
    Q_EMIT backend.fileItemsRemoved({QStringLiteral("f1")});
    QVERIFY(model.fileItems().isEmpty());

    // Send: optimistic Preparing in Sent, one command, later states from the backend.
    CybouMailItem message;
    message.to_name = QStringLiteral("alice.cybou");
    message.body = QStringLiteral("hello");
    const QString id = model.requestSendMail(message);
    QVERIFY(!id.isEmpty());
    QCOMPARE(backend.commands.filter(QStringLiteral("send:")), QStringList{QStringLiteral("send:") + id});
    QCOMPARE(model.mailItem(id)->state, CybouContentState::Local);
    QCOMPARE(model.mailItem(id)->folder, CybouMailFolder::Sent);
    Q_EMIT backend.mailStateChanged(id, CybouContentState::Securing);
    QVERIFY(CybouProduct::mailStateText(*model.mailItem(id)) != QStringLiteral("Sent"));
    Q_EMIT backend.mailStateChanged(id, CybouContentState::NeedsAttention);
    backend.commands.clear();
    model.requestRetryMail(id);
    QCOMPARE(backend.commands, QStringList{QStringLiteral("retry:") + id});
    QCOMPARE(model.mailItem(id)->state, CybouContentState::Local);
    Q_EMIT backend.mailStateChanged(id, CybouContentState::Protected);
    QCOMPARE(CybouProduct::mailStateText(*model.mailItem(id)), QStringLiteral("Sent"));

    // Mailbox organization is also a command.
    backend.commands.clear();
    model.requestMoveMail(id, CybouMailFolder::Trash);
    QCOMPARE(backend.commands, QStringList{QStringLiteral("moveMail:") + id});
    QCOMPARE(model.mailItem(id)->folder, CybouMailFolder::Sent);

    // Locking closes the Identity session and drops the projection.
    model.requestLockVault();
    QVERIFY(!backend.open);
    QVERIFY(model.mailItems().isEmpty());
    Q_EMIT backend.mailItemChanged(message); // late replies are not shown while locked
    QVERIFY(model.mailItems().isEmpty());
}

void CybouShellTests::liveFeatureAvailabilityStayHonest()
{
    // No backend: Mail and Files are never claimed, and actions do nothing.
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    model.setFeatureAvailability(AllFeatureAvailability());
    QVERIFY(!model.featureAvailability().mail);
    QVERIFY(!model.featureAvailability().files);
    QVERIFY(model.featureAvailability().payments);
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    QVERIFY(model.requestCreateFolder(QStringLiteral("Taxes")).isEmpty());
    QVERIFY(model.requestSendMail({}).isEmpty());
    QVERIFY(model.fileItems().isEmpty());
    QVERIFY(model.mailItems().isEmpty());

    // A backend enables them only while it reports availability.
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    QVERIFY(backend.open);
    QVERIFY(model.featureAvailability().mail);
    QVERIFY(model.featureAvailability().files);
    backend.setAvailable(false);
    QVERIFY(!model.featureAvailability().files);
    QVERIFY(model.requestCreateFolder(QStringLiteral("Taxes")).isEmpty());
    backend.setAvailable(true);
    QVERIFY(model.featureAvailability().files);
    model.setApplicationBackend(nullptr);
    QVERIFY(!backend.open);
    QVERIFY(!model.featureAvailability().mail);
}

void CybouShellTests::fixtureLifecycleFollowsBackend()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    auto* backend = qobject_cast<CybouFixtureApplicationBackend*>(model.applicationBackend());
    QVERIFY(backend);
    backend->setAutoAdvance(true, 0);

    CybouMailItem message;
    message.to_name = QStringLiteral("alice.cybou");
    message.body = QStringLiteral("hello");
    const QString id = model.requestSendMail(message);
    QCOMPARE(model.mailItem(id)->state, CybouContentState::Local);
    QCOMPARE(model.mailItem(id)->operation_state, CybouOperationState::Preparing);
    using Step = std::pair<CybouContentState, CybouOperationState>;
    QList<Step> seen;
    QObject::connect(&model, &CybouDesktopModel::mailChanged, &model, [&] {
        const auto* item = model.mailItem(id);
        if (!item) return;
        const Step step{item->state, item->operation_state};
        if (seen.isEmpty() || seen.last() != step) seen << step;
    });
    QTRY_COMPARE(model.mailItem(id)->state, CybouContentState::Protected);
    // Operation first (Submitted, then Finalized), then storage: Securing -> Protected.
    QCOMPARE(seen, (QList<Step>{{CybouContentState::Local, CybouOperationState::Submitted},
        {CybouContentState::Securing, CybouOperationState::Finalized},
        {CybouContentState::Protected, CybouOperationState::Finalized}}));
    QCOMPARE(CybouProduct::mailStateText(*model.mailItem(id)), QStringLiteral("Sent"));

    // Upload runs the same lifecycle; retrieval is separate from protection.
    QTemporaryDir dir;
    const QString path = dir.filePath(QStringLiteral("notes.txt"));
    QFile file{path};
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("hello");
    file.close();
    const QString uploaded = model.requestFileUpload(path);
    QCOMPARE(model.fileItem(uploaded)->state, CybouContentState::Local);
    QCOMPARE(model.fileItem(uploaded)->operation_state, CybouOperationState::Preparing);
    QTRY_COMPARE(model.fileItem(uploaded)->state, CybouContentState::Protected);
    QCOMPARE(model.fileItem(uploaded)->operation_state, CybouOperationState::Finalized);
    model.requestFileDownload(QStringLiteral("f-mountain"), dir.filePath(QStringLiteral("m.jpg")));
    QCOMPARE(model.fileItem(QStringLiteral("f-mountain"))->retrieval, CybouRetrievalState::Downloading);
    QCOMPARE(model.fileItem(QStringLiteral("f-mountain"))->state, CybouContentState::Protected);
    // Retrieval ends back at Idle with the content now cached on this computer.
    QTRY_VERIFY(model.fileItem(QStringLiteral("f-mountain"))->available_offline);
    QTRY_COMPARE(model.fileItem(QStringLiteral("f-mountain"))->retrieval, CybouRetrievalState::Idle);
    QCOMPARE(model.fileItem(QStringLiteral("f-mountain"))->state, CybouContentState::Protected);

    // Offline: new work waits for the network and reads "Waiting for network".
    model.setNodeStatus(true, 0, false);
    const QString waiting = model.requestSendMail(message);
    QTest::qWait(20);
    QCOMPARE(model.mailItem(waiting)->state, CybouContentState::Local);
    QCOMPARE(CybouProduct::contentWithOperationText(model.mailItem(waiting)->state,
        model.mailItem(waiting)->operation_state, false), QStringLiteral("Waiting for network"));
}

void CybouShellTests::filesShowLocalAvailability()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("files")));
    window->show();
    window->showPage(CybouPage::Files);
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);
    const auto* report = model->fileItem(QStringLiteral("f-report"));
    const auto* mountain = model->fileItem(QStringLiteral("f-mountain"));
    QCOMPARE(report->state, CybouContentState::Protected);
    QCOMPARE(mountain->state, CybouContentState::Protected);
    QVERIFY(report->available_offline);
    QVERIFY(!mountain->available_offline);
    QCOMPARE(CybouProduct::fileStatusText(*report, true), QStringLiteral("Protected  ·  Available offline"));
    QCOMPARE(CybouProduct::fileStatusText(*mountain, true), QStringLiteral("Protected"));

    const auto label_texts = [files] {
        QStringList texts;
        for (const auto* label : files->findChildren<QLabel*>()) {
            if (label->isVisibleTo(files)) texts << label->text();
        }
        return texts;
    };
    files->showDetails(QStringLiteral("f-report"));
    QTRY_VERIFY(label_texts().contains(QStringLiteral("Available offline")));
    files->showDetails(QStringLiteral("f-mountain"));
    QTRY_VERIFY(label_texts().contains(QStringLiteral("Downloaded when opened")));

    // Content that cannot be fetched right now stays listed and says so.
    model->setFileState(QStringLiteral("f-mountain"), CybouContentState::TemporarilyUnavailable);
    files->openFolder(QStringLiteral("f-photos"));
    QVERIFY(files->visibleIds().contains(QStringLiteral("f-mountain")));
    QCOMPARE(CybouProduct::fileStatusText(*model->fileItem(QStringLiteral("f-mountain")), true),
        QStringLiteral("Temporarily unavailable"));

    // Copy is a backend command that references the same protected content.
    const int before = model->fileItems().size();
    const QString copy = model->requestCopyFile(QStringLiteral("f-report"), {});
    QCOMPARE(model->fileItems().size(), before + 1);
    QCOMPARE(model->fileItem(copy)->state, CybouContentState::Protected);
    QCOMPARE(model->fileItem(copy)->name, QStringLiteral("Copy of report.pdf"));
}

void CybouShellTests::lockHidesPrivateContent()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    auto* completer = window->globalSearch()->completer();
    completer->setCompletionPrefix(QStringLiteral("budget"));
    QVERIFY(completer->completionCount() >= 1);

    model->requestLockVault();
    QCOMPARE(model->status().identity_state, CybouIdentityState::Locked);
    QVERIFY(model->mailItems().isEmpty());
    QVERIFY(model->fileItems().isEmpty());
    completer->setCompletionPrefix(QStringLiteral("budget"));
    QCOMPARE(completer->completionCount(), 0);
    completer->setCompletionPrefix(QStringLiteral("contract"));
    QCOMPARE(completer->completionCount(), 0);
    // Private actions are unavailable while locked.
    QVERIFY(model->requestCreateFolder(QStringLiteral("Taxes")).isEmpty());
    QVERIFY(model->requestSendMail({}).isEmpty());
    // Network status stays visible.
    QVERIFY(model->status().online);

    bool done = false;
    model->requestUnlockIdentityAsync(QStringLiteral("correct horse battery"), [&done](bool ok) { done = ok; });
    QTRY_VERIFY(done);
    QVERIFY(!model->mailItems().isEmpty());
    QVERIFY(model->fileItem(QStringLiteral("f-budget")));
}

void CybouShellTests::restoreFillsInProgressively()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("empty")));
    CybouUiFixtures::Driver driver{&model};
    driver.setStepDelay(150);
    const QString phrase = QStringList(24, QStringLiteral("ocean")).join(QLatin1Char{' '});
    QVERIFY(model.requestRestoreIdentity(phrase, QStringLiteral("correct horse battery")));
    // Identity, Wallet and Names first; Mail and Files keep restoring.
    QTRY_COMPARE(model.restoreProgress().names, CybouRestoreStepState::Done);
    QCOMPARE(model.restoreProgress().identity, CybouRestoreStepState::Done);
    QVERIFY(model.restoreProgress().mail != CybouRestoreStepState::Done);
    // The user opens the desktop while Mail and Files fill in.
    model.setIdentityState(CybouIdentityState::Syncing, model.status().account_id, model.status().creation_height);
    QTRY_COMPARE(model.restoreProgress().files, CybouRestoreStepState::Done);
    QTRY_VERIFY(!model.fileItems().isEmpty());
    QTRY_COMPARE(model.restoreProgress().mail, CybouRestoreStepState::Done);
    QTRY_VERIFY(!model.mailItems().isEmpty());
    QTRY_COMPARE(model.status().identity_state, CybouIdentityState::Active);
}

void CybouShellTests::liveMailAndFilesThroughCoreAdapter()
{
    // Real core services on an authority runtime; no fixtures.
    CybouServiceTestFixture fixture;
    auto alice = fixture.CreateIdentity("adapter-alice.vault");
    auto bob = fixture.CreateIdentity("adapter-bob.vault");
    const QString bob_id = QString::fromStdString(bob->GetAccountId()->Value().GetHex());

    const auto open = [&](cybou::CybouIdentityService& identity, const char* dir) {
        auto model = std::make_unique<CybouDesktopModel>(QStringLiteral("CYBOU DEV"));
        auto adapter = std::make_unique<CybouCoreApplicationAdapter>(*fixture.runtime, identity, fixture.directory / dir);
        adapter->setRefreshInterval(20);
        model->setApplicationBackend(adapter.get());
        model->requestApplicationFeatureAvailability(true, true);
        model->setIdentityState(CybouIdentityState::Active,
            QString::fromStdString(identity.GetAccountId()->Value().GetHex()), 1);
        return std::make_pair(std::move(model), std::move(adapter));
    };
    auto [alice_model, alice_adapter] = open(*alice, "desktop-alice");
    // Mail turns on only once the adapter session has opened the core services.
    QTRY_VERIFY(alice_model->featureAvailability().mail);
    QVERIFY(alice_model->featureAvailability().files);

    CybouMailItem message;
    message.to_name = bob_id;
    message.subject = QStringLiteral("Hello");
    message.body = QStringLiteral("Hello Bob, from the live desktop.");
    const QString client_id = alice_model->requestSendMail(message);
    QVERIFY(!client_id.isEmpty());
    // The optimistic item is replaced by the backend's message, keyed by its private ID.
    QTRY_VERIFY(!alice_model->mailItem(client_id) && !alice_model->mailItems().isEmpty());
    const QString sent_id = alice_model->mailItems().first().id;
    QCOMPARE(sent_id.size(), 64);
    QTRY_COMPARE(alice_model->mailItem(sent_id)->operation_state, CybouOperationState::Submitted);
    QCOMPARE(alice_model->mailItem(sent_id)->state, CybouContentState::Local);

    QVERIFY(fixture.runtime->ProduceBlock());
    // Finalized is not Sent: with no storage providers the message keeps Securing.
    QTRY_COMPARE(alice_model->mailItem(sent_id)->state, CybouContentState::Securing);
    QCOMPARE(alice_model->mailItem(sent_id)->folder, CybouMailFolder::Sent);
    QVERIFY(CybouProduct::mailStateText(*alice_model->mailItem(sent_id)) != QStringLiteral("Sent"));
    QTRY_VERIFY(alice_model->mailItem(sent_id)->finalized_height > 0);

    // Bob's desktop discovers it from finalized history.
    auto [bob_model, bob_adapter] = open(*bob, "desktop-bob");
    QTRY_VERIFY(bob_model->mailItem(sent_id) != nullptr);
    const auto* received = bob_model->mailItem(sent_id);
    QCOMPARE(received->subject, QStringLiteral("Hello"));
    QCOMPARE(received->folder, CybouMailFolder::Inbox);
    QVERIFY(received->unread);
    QCOMPARE(bob_model->unreadMailCount(), 1);
    bob_model->requestMailRead(sent_id, true);
    QTRY_COMPARE(bob_model->unreadMailCount(), 0);

    // Attachments: a new local file travels encrypted inside the publication.
    const auto produce_until = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 25 && !done(); ++i) {
            fixture.runtime->ProduceBlock();
            QTest::qWait(60);
        }
        return done();
    };
    QTemporaryDir attach_dir;
    QByteArray contract(300 * 1024, '\0');
    for (int i = 0; i < contract.size(); ++i) contract[i] = static_cast<char>(i * 7 + 3);
    const QString contract_path = attach_dir.filePath(QStringLiteral("contract.pdf"));
    {
        QFile out{contract_path};
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(contract);
    }
    CybouMailItem with_attachment;
    with_attachment.to_name = bob_id;
    with_attachment.subject = QStringLiteral("Contract");
    with_attachment.body = QStringLiteral("Signed copy attached.");
    with_attachment.attachments.append(alice_model->localAttachment(contract_path));
    QVERIFY(!alice_model->requestSendMail(with_attachment).isEmpty());
    const auto bob_contract = [&]() -> const CybouMailItem* {
        for (const auto& item : bob_model->mailItems()) {
            if (item.subject == QStringLiteral("Contract")) return bob_model->mailItem(item.id);
        }
        return nullptr;
    };
    QVERIFY(produce_until([&] { return bob_contract() != nullptr; }));
    QCOMPARE(bob_contract()->attachments.size(), 1);
    const auto received_attachment = bob_contract()->attachments.first();
    QCOMPARE(received_attachment.name, QStringLiteral("contract.pdf"));
    QCOMPARE(received_attachment.logical_size, quint64(contract.size()));
    QCOMPARE(received_attachment.state, CybouContentState::Received); // the sender's durability is not claimed
    QVERIFY(received_attachment.source_path.isEmpty());
    const QString contract_message = bob_contract()->id;
    const QString bob_download = attach_dir.filePath(QStringLiteral("bob-contract.pdf"));
    bob_model->requestAttachmentDownload(contract_message, received_attachment.id, bob_download);
    QTRY_VERIFY(QFile::exists(bob_download));
    {
        QFile in{bob_download};
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), contract);
    }

    // Mail attachment -> Files: a catalog entry referencing the same content.
    QVERIFY(!bob_model->requestSaveAttachmentToFiles(contract_message, received_attachment.id).isEmpty());
    const auto bob_file = [&]() -> const CybouFileItem* {
        for (const auto& item : bob_model->fileItems()) {
            if (item.name == QStringLiteral("contract.pdf")) return bob_model->fileItem(item.id);
        }
        return nullptr;
    };
    QTRY_VERIFY(bob_file() != nullptr);
    QVERIFY(produce_until([&] { return bob_file() && bob_file()->state == CybouContentState::Securing; }));
    QTRY_VERIFY(bob_contract() && !bob_contract()->attachments.first().saved_file_id.isEmpty());
    const QString saved_id = bob_file()->id;
    const QString from_files = attach_dir.filePath(QStringLiteral("from-files.pdf"));
    bob_model->requestFileDownload(saved_id, from_files);
    QTRY_VERIFY(QFile::exists(from_files));
    {
        QFile in{from_files};
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), contract);
    }

    // Files -> Mail: Bob forwards his Files item by reference, without re-upload.
    CybouMailItem forward;
    forward.to_name = QString::fromStdString(alice->GetAccountId()->Value().GetHex());
    forward.subject = QStringLiteral("Fwd: Contract");
    forward.body = QStringLiteral("Here it is.");
    CybouAttachmentItem reference;
    reference.id = QStringLiteral("ref-") + saved_id;
    reference.name = QStringLiteral("contract.pdf");
    reference.logical_size = quint64(contract.size());
    reference.state = CybouContentState::Protected;
    forward.attachments.append(reference);
    QVERIFY(!bob_model->requestSendMail(forward).isEmpty());
    const auto alice_forward = [&]() -> const CybouMailItem* {
        for (const auto& item : alice_model->mailItems()) {
            if (item.subject == QStringLiteral("Fwd: Contract") && item.folder == CybouMailFolder::Inbox) {
                return alice_model->mailItem(item.id);
            }
        }
        return nullptr;
    };
    QVERIFY(produce_until([&] { return alice_forward() != nullptr; }));
    const QString alice_download = attach_dir.filePath(QStringLiteral("alice-forward.pdf"));
    alice_model->requestAttachmentDownload(alice_forward()->id, alice_forward()->attachments.first().id, alice_download);
    QTRY_VERIFY(QFile::exists(alice_download));
    {
        QFile in{alice_download};
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), contract);
    }

    // Drafts persist in the encrypted Application DB across a lock/unlock and
    // are deleted locally; they are never published.
    CybouMailItem draft;
    draft.to_name = QStringLiteral("alice.cybou");
    draft.subject = QStringLiteral("Unfinished");
    draft.body = QStringLiteral("Half a thought");
    const QString draft_id = bob_model->requestSaveMailDraft(draft);
    QVERIFY(!draft_id.isEmpty());
    QVERIFY(bob_model->mailItem(draft_id) && bob_model->mailItem(draft_id)->draft);
    const auto bob_account = QString::fromStdString(bob->GetAccountId()->Value().GetHex());
    bob_model->setIdentityState(CybouIdentityState::Locked, bob_account, 1);
    QVERIFY(!bob_model->mailItem(draft_id));
    bob_model->setIdentityState(CybouIdentityState::Active, bob_account, 1);
    QTRY_VERIFY(bob_model->mailItem(draft_id) != nullptr);
    QCOMPARE(bob_model->mailItem(draft_id)->body, QStringLiteral("Half a thought"));
    QCOMPARE(bob_model->mailItem(draft_id)->folder, CybouMailFolder::Drafts);
    bob_model->requestDeleteMail(draft_id);
    QVERIFY(!bob_model->mailItem(draft_id));
    QTest::qWait(150); // later snapshots must not resurrect it
    QVERIFY(!bob_model->mailItem(draft_id));
    bob_model->setIdentityState(CybouIdentityState::Locked, bob_account, 1);
    bob_model->setIdentityState(CybouIdentityState::Active, bob_account, 1);
    QTRY_VERIFY(bob_model->featureAvailability().mail && !bob_model->mailItems().isEmpty());
    QVERIFY(!bob_model->mailItem(draft_id));

    // An unknown recipient needs attention instead of pretending to send.
    CybouMailItem nobody;
    nobody.to_name = QStringLiteral("nobody-here.cybou");
    nobody.body = QStringLiteral("?");
    const QString failed = alice_model->requestSendMail(nobody);
    QTRY_COMPARE(alice_model->mailItem(failed)->state, CybouContentState::NeedsAttention);

    // Files: folder, upload into it, rename, download exact bytes, trash, delete.
    const auto file_named = [&](const QString& name) -> const CybouFileItem* {
        for (const auto& item : alice_model->fileItems()) {
            if (item.name == name) return alice_model->fileItem(item.id);
        }
        return nullptr;
    };
    const auto state_of = [&](const QString& name) {
        const auto* item = file_named(name);
        return item ? std::optional{item->state} : std::nullopt;
    };
    // Each Files change is one publication; a change made while another is
    // unconfirmed queues behind it, so keep producing blocks until it lands.
    const auto finalize_until = [&](const QString& name, CybouContentState state) {
        for (int i = 0; i < 20 && state_of(name) != std::optional{state}; ++i) {
            fixture.runtime->ProduceBlock();
            QTest::qWait(60);
        }
        return state_of(name) == std::optional{state};
    };
    const QString work_client = alice_model->requestCreateFolder(QStringLiteral("Work"));
    QVERIFY(!work_client.isEmpty());
    QTRY_VERIFY(file_named(QStringLiteral("Work")) != nullptr);
    const QString work_id = file_named(QStringLiteral("Work"))->id;
    QTemporaryDir files_dir;
    QByteArray original(400 * 1024, '\0');
    for (int i = 0; i < original.size(); ++i) original[i] = static_cast<char>(i * 13 + 1);
    const QString source = files_dir.filePath(QStringLiteral("report.bin"));
    {
        QFile out{source};
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(original);
    }
    QVERIFY(!alice_model->requestFileUpload(source, work_client).isEmpty()); // client folder ID resolves
    QTRY_VERIFY(file_named(QStringLiteral("report.bin")) != nullptr);
    QCOMPARE(file_named(QStringLiteral("report.bin"))->parent_id, work_id);
    QVERIFY(file_named(QStringLiteral("report.bin"))->available_offline);
    QVERIFY(finalize_until(QStringLiteral("report.bin"), CybouContentState::Securing));
    QTRY_VERIFY(file_named(QStringLiteral("report.bin")) && file_named(QStringLiteral("report.bin"))->finalized_height > 0);
    QCOMPARE(file_named(QStringLiteral("report.bin"))->logical_size, quint64(original.size()));
    const QString report_id = file_named(QStringLiteral("report.bin"))->id;

    alice_model->requestRenameFile(report_id, QStringLiteral("report-final.bin"));
    QTRY_VERIFY(file_named(QStringLiteral("report-final.bin")) != nullptr);
    QVERIFY(finalize_until(QStringLiteral("report-final.bin"), CybouContentState::Securing));
    QVERIFY(alice_model->fileItem(report_id));

    const QString destination = files_dir.filePath(QStringLiteral("downloaded.bin"));
    alice_model->requestFileDownload(report_id, destination);
    QTRY_VERIFY(QFile::exists(destination));
    {
        QFile in{destination};
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), original);
    }
    QTRY_VERIFY(alice_model->fileItem(report_id) &&
        alice_model->fileItem(report_id)->retrieval == CybouRetrievalState::Idle);
    QVERIFY(alice_model->fileItem(report_id)->available_offline); // every chunk is local

    // Starred is encrypted Identity state: it survives closing the session.
    alice_model->requestFileStarred(report_id, true);
    QTRY_VERIFY(alice_model->fileItem(report_id) && alice_model->fileItem(report_id)->starred);
    QTest::qWait(300); // let the worker store it
    alice_adapter->closeIdentity();
    QTRY_VERIFY(!alice_model->featureAvailability().files);
    QSignalSpy reopened{alice_adapter.get(), &CybouCoreApplicationAdapter::filesSnapshot};
    alice_adapter->openIdentity();
    // Judge only a snapshot of the reopened session, not what the model still shows.
    QTRY_VERIFY(alice_model->featureAvailability().files && reopened.count() > 0);
    const auto fresh = reopened.last().at(0).value<QVector<CybouFileItem>>();
    const auto stored = std::find_if(fresh.begin(), fresh.end(), [&](const CybouFileItem& f) { return f.id == report_id; });
    QVERIFY(stored != fresh.end() && stored->starred);
    QVERIFY(stored->available_offline);

    alice_model->requestTrashFile(work_id); // contents follow the folder
    QTRY_VERIFY(alice_model->fileItem(report_id) && alice_model->fileItem(report_id)->trashed);
    QVERIFY(finalize_until(QStringLiteral("Work"), CybouContentState::Securing));
    QVERIFY(alice_model->fileItem(report_id) && alice_model->fileItem(report_id)->trashed);
    alice_model->requestDeleteFile(work_id);
    QTRY_VERIFY(!alice_model->fileItem(report_id) && !alice_model->fileItem(work_id));
    for (int i = 0; i < 5; ++i) {
        fixture.runtime->ProduceBlock();
        QTest::qWait(60);
    }
    QVERIFY(!alice_model->fileItem(work_id)); // gone from history too

    // Rotation never proceeds before the RecoveryBridge is durable: without
    // storage providers it waits, and the Identity keys stay unchanged.
    alice_model->setIdentityService(alice.get()); // a known vault starts Locked
    alice_model->setIdentityState(CybouIdentityState::Active,
        QString::fromStdString(alice->GetAccountId()->Value().GetHex()), 1);
    QTRY_VERIFY(alice_model->featureAvailability().mail);
    QSignalSpy rotated{alice_model.get(), &CybouDesktopModel::recoveryRotationFinished};
    const auto entropy = cybou::GenerateRecoveryEntropy();
    QVERIFY(entropy.has_value());
    QStringList new_words;
    for (const auto& word : cybou::EncodeRecoveryWords(*entropy)) new_words << QString::fromStdString(word);
    QVERIFY(alice_model->requestRecoveryRootRotation(new_words, QStringLiteral("correct horse battery staple")));
    QVERIFY2(alice_model->recoveryRotationPending(),
        qPrintable(rotated.isEmpty() ? QStringLiteral("?") : rotated.first().at(1).toString()));
    QVERIFY(fixture.runtime->ProduceBlock()); // the bridge itself finalizes
    QTest::qWait(300);
    QCOMPARE(rotated.count(), 0);
    const auto key_epoch = [&] {
        const auto loaded = fixture.runtime->GetStore().LoadState();
        return loaded.state->identities.Find(*alice->GetAccountId())->key_epoch;
    };
    QCOMPARE(key_epoch(), std::uint64_t{0});

    // Locking drops private Mail and the session, and cancels the rotation.
    QObject::connect(alice_model.get(), &CybouDesktopModel::lockVaultRequested, alice_model.get(),
        [desktop_model = alice_model.get()] {
            if (desktop_model->beginVaultLock()) {
                desktop_model->completeVaultLock();
            }
        });
    alice_model->requestLockVault();
    QCOMPARE(rotated.count(), 1);
    QCOMPARE(rotated.first().at(0).value<CybouOperationOutcome>(), CybouOperationOutcome::Failed);
    QVERIFY(!alice_model->recoveryRotationPending());
    QCOMPARE(key_epoch(), std::uint64_t{0});
    QVERIFY(alice_model->mailItems().isEmpty());
    QVERIFY(!alice_model->featureAvailability().mail);
    QVERIFY(alice_model->fileItems().isEmpty());
    alice_model->setApplicationBackend(nullptr);
    bob_model->setApplicationBackend(nullptr);
}




void CybouShellTests::identityAuthorityIsAnHonestPreview()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    auto* page = window->page(CybouPage::Identity);
    const auto auth_label = [page]() -> QLabel* {
        for (auto* widget : page->findChildren<QLabel*>()) {
            if (widget->property("cybouId").toString() == QLatin1String{"identityAuthorityValue"}) return widget;
        }
        return nullptr;
    };
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    QVERIFY(auth_label());
    QVERIFY(auth_label()->text().contains(QLatin1String{"AUTH"}));

    model->setAuthority(500);
    QVERIFY(auth_label()->text().contains(QStringLiteral("500 AUTH")));
}

void CybouShellTests::rotationKeepsLiveSessionWorking()
{
    // Real core services with two storage providers, so the RecoveryBridge
    // can become durable and the rotation can complete in this session.
    CybouServiceTestFixture fixture;
    ProviderNetwork network{fixture};
    auto alice = fixture.CreateIdentity("rotation-alice.vault");
    const QString account = QString::fromStdString(alice->GetAccountId()->Value().GetHex());
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    CybouCoreApplicationAdapter adapter{*fixture.runtime, *alice, fixture.directory / "desktop"};
    adapter.setRefreshInterval(20);
    adapter.setStorageTransport(&network);
    model.setApplicationBackend(&adapter);
    model.requestApplicationFeatureAvailability(true, true);
    model.setIdentityService(alice.get()); // a known vault starts Locked
    model.setIdentityState(CybouIdentityState::Active, account, 1);
    QTRY_VERIFY(model.featureAvailability().files);

    const auto produce_until = [&](const std::function<bool()>& done) {
        for (int i = 0; i < 40 && !done(); ++i) {
            fixture.runtime->ProduceBlock();
            network.Sync();
            QTest::qWait(60);
        }
        return done();
    };
    const auto file_named = [&](const QString& name) -> const CybouFileItem* {
        for (const auto& item : model.fileItems()) {
            if (item.name == name) return model.fileItem(item.id);
        }
        return nullptr;
    };
    QTemporaryDir dir;
    const auto write = [&](const QString& name, const QByteArray& bytes) {
        const QString path = dir.filePath(name);
        QFile out{path};
        if (out.open(QIODevice::WriteOnly)) out.write(bytes);
        return path;
    };

    // Content published under the original keys.
    const QByteArray before(200 * 1024, 'b');
    QVERIFY(!model.requestFileUpload(write(QStringLiteral("before.bin"), before)).isEmpty());
    QVERIFY(produce_until([&] {
        const auto* file = file_named(QStringLiteral("before.bin"));
        return file && file->state == CybouContentState::Protected;
    }));
    CybouMailItem draft;
    draft.subject = QStringLiteral("Keep me");
    draft.body = QStringLiteral("A draft across rotation");
    const QString draft_id = model.requestSaveMailDraft(draft);
    QVERIFY(!draft_id.isEmpty());

    // Replace the recovery phrase: bridge -> durable -> verified -> IdentityRotate.
    QSignalSpy rotated{&model, &CybouDesktopModel::recoveryRotationFinished};
    const auto entropy = cybou::GenerateRecoveryEntropy();
    QVERIFY(entropy.has_value());
    QStringList words;
    for (const auto& word : cybou::EncodeRecoveryWords(*entropy)) words << QString::fromStdString(word);
    const QString password = QStringLiteral("correct horse battery staple");
    QVERIFY(model.requestRecoveryRootRotation(words, password));
    QVERIFY(produce_until([&] { return rotated.count() > 0; }));
    // Submitted: finality completes the rotation on resume.
    for (int i = 0; i < 5 && rotated.last().at(0).value<CybouOperationOutcome>() == CybouOperationOutcome::Pending; ++i) {
        fixture.runtime->ProduceBlock();
        network.Sync();
        const int seen = rotated.count();
        QVERIFY(model.requestRecoveryRootRotation({}, password, true));
        QTRY_VERIFY(rotated.count() > seen);
    }
    QVERIFY2(rotated.last().at(0).value<CybouOperationOutcome>() == CybouOperationOutcome::Finalized,
        qPrintable(rotated.last().at(1).toString()));
    {
        const auto loaded = fixture.runtime->GetStore().LoadState();
        QCOMPARE(loaded.state->identities.Find(*alice->GetAccountId())->key_epoch, std::uint64_t{1});
    }

    // Same session, new keys: Mail and Files keep working, nothing visible is lost.
    QTRY_VERIFY(model.featureAvailability().files && model.featureAvailability().mail);
    QTRY_VERIFY(file_named(QStringLiteral("before.bin")) != nullptr);
    QTRY_VERIFY(model.mailItem(draft_id) != nullptr);
    QCOMPARE(model.mailItem(draft_id)->body, QStringLiteral("A draft across rotation"));
    const QString downloaded = dir.filePath(QStringLiteral("before-downloaded.bin"));
    model.requestFileDownload(file_named(QStringLiteral("before.bin"))->id, downloaded);
    QTRY_VERIFY(QFile::exists(downloaded));
    {
        QFile in{downloaded};
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), before);
    }
    const QByteArray after(150 * 1024, 'a');
    QVERIFY(!model.requestFileUpload(write(QStringLiteral("after.bin"), after)).isEmpty());
    QVERIFY(produce_until([&] {
        const auto* file = file_named(QStringLiteral("after.bin"));
        return file && file->state == CybouContentState::Protected;
    }));
    CybouMailItem note;
    note.to_name = account;
    note.subject = QStringLiteral("After rotation");
    note.body = QStringLiteral("Still sending");
    QVERIFY(!model.requestSendMail(note).isEmpty());
    const auto note_item = [&]() -> const CybouMailItem* {
        for (const auto& item : model.mailItems()) {
            if (item.subject == QStringLiteral("After rotation") && !item.draft) return model.mailItem(item.id);
        }
        return nullptr;
    };
    QVERIFY(produce_until([&] { return note_item() && note_item()->state == CybouContentState::Protected; }));
    model.setApplicationBackend(nullptr);
}

