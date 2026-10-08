#include <qt/cybouobservationchart.h>
#include <qt/networkobservationtext.h>
// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/test/cyboushelltests.h>

#include <qt/cybouapplicationbackend.h>
#include <qt/cyboucoreapplicationadapter.h>
#include <qt/cyboudesktopcontroller.h>
#include <qt/cyboufixturebackend.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cybouactivity.h>
#include <qt/cyboumainwindow.h>
#include <qt/cyboutheme.h>
#include <qt/cybouproduct.h>
#include <qt/cybouui.h>
#include <qt/cybounotifier.h>
#include <qt/cybouuifixtures.h>
#include <qt/pages/emailpage.h>
#include <qt/pages/identitypage.h>
#include <qt/pages/onboardingview.h>
#include <qt/pages/homepage.h>
#include <qt/pages/mailcompose.h>
#include <qt/pages/mailreader.h>
#include <qt/pages/storagepage.h>
#include <qt/pages/diagnosticspage.h>
#include <qt/pages/networkpage.h>
#include <qt/benchmarkreference.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <qt/pages/networkauthoritypage.h>
#include <qt/pages/walletpage.h>
#include <qt/cybouconsoledialog.h>
#include <qt/recoveryphrasedialog.h>
#include <QCheckBox>
#include <qt/authorityreview.h>
#include <QMessageBox>
#include <QInputDialog>
#include <QContextMenuEvent>
#include <QComboBox>
#include <QTableWidget>
#include <QTabWidget>
#include <QFile>
#include <QProgressDialog>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QDir>
#include <QSet>

#include <cybou/network_genesis.h>
#include <cybou/node_runtime.h>
#include <cybou/official_networks.h>
#include <cybou/p2p/session.h>
#include <test/cybou_test_helpers.h>
#include <test/cybou_service_test_fixture.h>
#include <test/cybou_storage_test_network.h>

#include <cybou/recovery_phrase.h>

#include <QApplication>
#include <QAccessible>
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
#include <QPointer>
#include <QSignalSpy>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTranslator>
#include <QPlainTextEdit>
#include <QPlainTextEdit>
#include <QTest>
#include <QToolButton>
#include <QDialog>
#include <QDragEnterEvent>
#include <QTemporaryFile>
#include <QUrl>
#include <QDropEvent>
#include <QDialogButtonBox>
#include <QTreeWidget>
#include <QTimer>
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
template <typename T>
T* FindById(QWidget* parent, const QString& id)
{
    for (auto* widget : parent->findChildren<T*>()) {
        if (widget->property("cybouId").toString() == id && widget->isVisibleTo(parent)) return widget;
    }
    return nullptr;
}

class RecordingBackend final : public CybouApplicationBackend
{
public:
    CybouMailItem last_draft;
    CommandProgress refresh_result;
    void refreshProjection(CommandProgress progress) override { commands << QStringLiteral("refresh"); refresh_result = std::move(progress); }
    using CybouApplicationBackend::CybouApplicationBackend;
    QStringList commands;
    bool available{true};
    bool open{false};
    bool delay_load{false};
    QHash<QString, CommandProgress> draft_results;
    QVector<CommandProgress> move_results;
    QVector<CommandProgress> delete_results;
    QVector<CommandProgress> file_results;
    QVector<QPair<QString, QString>> file_moves;
    CommandProgress send_result;
    QString send_draft_id;

    bool mailAvailable() const override { return available; }
    bool filesAvailable() const override { return available; }
    void openIdentity() override { open = true; commands << QStringLiteral("open"); if (!delay_load) Q_EMIT applicationLoadChanged(CybouApplicationLoadState::Ready, 0, 0, {}); }
    void closeIdentity() override { open = false; commands << QStringLiteral("close"); }
    void saveMailDraft(const CybouMailItem& d, CommandProgress progress = {}) override { last_draft = d; commands << QStringLiteral("draft:") + d.id; draft_results.insert(d.id, progress); }
    void sendMail(const CybouMailItem& m, const QString& draft_id = {}, CommandProgress progress = {}) override { commands << QStringLiteral("send:") + m.id; send_result = progress; send_draft_id = draft_id; }
    void retryMail(const QString& id) override { commands << QStringLiteral("retry:") + id; }
    void setMailRead(const QString& id, bool) override { commands << QStringLiteral("read:") + id; }
    void setMailStarred(const QString& id, bool) override { commands << QStringLiteral("starMail:") + id; }
    void moveMail(const QString& id, CybouMailFolder, CommandProgress progress = {}) override { commands << QStringLiteral("moveMail:") + id; move_results.append(progress); }
    void deleteMail(const QString& id, CommandProgress progress = {}) override { commands << QStringLiteral("deleteMail:") + id; delete_results.append(progress); }
    void deleteMailForever(const QStringList& ids, CommandProgress progress = {}) override { commands << QStringLiteral("deleteMailForever:") + ids.join(','); delete_results.append(progress); }
    void downloadAttachment(const QString& m, const QString&, const QString&) override { commands << QStringLiteral("attachment:") + m; }
    void saveAttachmentToFiles(const QString& m, const QString&, const QString&) override { commands << QStringLiteral("saveAttachment:") + m; }
    void uploadFile(const QString& id, const QString&, const QString&) override { commands << QStringLiteral("upload:") + id; }
    void downloadFile(const QString& id, const QString&) override { commands << QStringLiteral("download:") + id; }
    void createFolder(const QString& id, const QString&, const QString&, CommandProgress progress = {}) override { commands << QStringLiteral("folder:") + id; file_results.append(progress); }
    void renameFile(const QString& id, const QString&, CommandProgress progress = {}) override { commands << QStringLiteral("rename:") + id; file_results.append(progress); }
    void moveFile(const QString& id, const QString& parent, CommandProgress progress = {}) override { commands << QStringLiteral("move:") + id; file_moves.append({id,parent}); file_results.append(progress); }
    void copyFile(const QString& id, const QString&, const QString&, CommandProgress progress = {}) override { commands << QStringLiteral("copy:") + id; file_results.append(progress); }
    void setFileStarred(const QString& id, bool) override { commands << QStringLiteral("starFile:") + id; }
    void trashFile(const QString& id, CommandProgress progress = {}) override { commands << QStringLiteral("trash:") + id; file_results.append(progress); }
    void restoreFile(const QString& id, CommandProgress progress = {}) override { commands << QStringLiteral("restore:") + id; file_results.append(progress); }
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
    password->setText(QStringLiteral("too short"));
    confirm->setText(QStringLiteral("too short"));
    // Pasting the whole phrase into the first field fills all 24.
    QStringList words;
    for (int i = 0; i < 23; ++i) words << CybouDesktopModel::recoveryWordList().at(i * 7);
    words << QStringLiteral("notaword");
    first->setText(words.join(QLatin1Char{' '}));
    Q_EMIT first->textEdited(first->text());
    QCOMPARE(home->findChild<QLineEdit*>(QStringLiteral("recoveryWord23"))->text(), QStringLiteral("notaword"));
    QVERIFY(!submit->isEnabled()); // unknown word blocks restore
    auto* blocker = home->findChild<QLabel*>(QStringLiteral("restoreBlocker"));
    QVERIFY(blocker && !blocker->text().isEmpty()); // the disabled button says why
    home->findChild<QLineEdit*>(QStringLiteral("recoveryWord23"))->setText(QStringLiteral("zoo"));
    QVERIFY(!submit->isEnabled()); // a short password blocks restore, and says so
    auto* password_check = home->findChild<QLabel*>(QStringLiteral("restorePasswordCheck"));
    QVERIFY(password_check && password_check->text().contains(QStringLiteral("/ 12")));
    password->setText(QStringLiteral("correct horse battery staple"));
    confirm->setText(QStringLiteral("correct horse battery stapl"));
    QVERIFY(!submit->isEnabled());
    auto* confirm_check = home->findChild<QLabel*>(QStringLiteral("restoreConfirmCheck"));
    QVERIFY(confirm_check && !confirm_check->isHidden());
    confirm->setText(QStringLiteral("correct horse battery staple"));
    QVERIFY(submit->isEnabled());
    QVERIFY(blocker->text().isEmpty());
    submit->click();
    QCOMPARE(model->status().identity_state, CybouIdentityState::Restoring);
    // The words stay until the Identity is back, so a failure can be retried.
    QVERIFY(!first->text().isEmpty());
    Q_EMIT model->identityRestoreFailed(QStringLiteral("Timed out waiting for PoA finality"));
    auto* hint = home->findChild<QLabel*>(QStringLiteral("restoreHint"));
    QVERIFY(hint && hint->text().contains(QStringLiteral("did not confirm in time")));
    QVERIFY(!first->text().isEmpty());
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("aa"), 1);
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
    QCOMPARE(mail->visibleMessageIds(), (QStringList{QStringLiteral("m-sent-submitted"),
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

    to->setText(QStringLiteral("cybou.cybou"));
    QVERIFY(send->isEnabled());
    QVERIFY(!model->nameLabelProblem(QStringLiteral("cybou")).isEmpty());
    QVERIFY(model->recipientNameProblem(QStringLiteral("cybou")).isEmpty());
    to->setText(QStringLiteral("alice.cybou"));

    const int before = model->mailItems().size();
    send->click();
    QCOMPARE(model->mailItems().size(), before + 1);
    const auto& sent = model->mailItems().first();
    QCOMPARE(sent.folder, CybouMailFolder::Sent);
    // Finality-first: a new message starts local, never as Sent.
    QCOMPARE(sent.state, CybouContentState::Local);
    QCOMPARE(CybouProduct::mailStateText(sent), QStringLiteral("Preparing…"));

    QTRY_VERIFY(body->toPlainText().isEmpty());
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

void CybouShellTests::replyUsesCompleteIdentityAddress()
{
    auto window=makeWindow();
    auto* model=window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model,QStringLiteral("mail")));
    CybouMailItem received;
    received.id=QStringLiteral("unnamed-sender");
    received.from_address=QStringLiteral("1822")+QString(56,QLatin1Char{'a'})+QStringLiteral("1930");
    received.from_name=CybouProduct::shortId(received.from_address);
    received.to_name=QStringLiteral("me.cybou");
    received.subject=QStringLiteral("Hello");
    received.body=QStringLiteral("A message from an unnamed Identity");
    model->setMailItems({received});
    auto* mail=dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->reader()->onReply(received.id);
    QCOMPARE(mail->composer()->snapshotForRebuild().to_name,received.from_address);
    QVERIFY(mail->composer()->findChild<QPushButton*>(QStringLiteral("sendButton"))->isEnabled());
    QCOMPARE(received.from_name,QStringLiteral("1822")+QChar{0x2026}+QStringLiteral("1930"));
    received.folder=CybouMailFolder::Archive;
    received.outgoing=true;
    received.to_name=QStringLiteral("recipient display");
    received.to_address=QString(64,QLatin1Char{'b'});
    model->setMailItems({received});
    mail->reader()->onReply(received.id);
    QCOMPARE(mail->composer()->snapshotForRebuild().to_name,received.to_address);
}

void CybouShellTests::composeSelectsProtectedCybouFiles()
{
    auto window=makeWindow();
    auto* model=window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model,QStringLiteral("active")));
    CybouFileItem folder;
    folder.id=QStringLiteral("folder"); folder.name=QStringLiteral("Projects"); folder.folder=true;
    CybouFileItem ready;
    ready.id=QStringLiteral("ready"); ready.name=QStringLiteral("report.pdf"); ready.parent_id=folder.id;
    ready.state=CybouContentState::Protected; ready.logical_size=1024;
    auto waiting=ready; waiting.id=QStringLiteral("waiting"); waiting.name=QStringLiteral("upload.bin");
    waiting.state=CybouContentState::Securing;
    auto trashed=ready; trashed.id=QStringLiteral("trashed"); trashed.trashed=true;
    auto hidden_folder=folder; hidden_folder.id=QStringLiteral("trash-folder"); hidden_folder.trashed=true;
    auto hidden=ready; hidden.id=QStringLiteral("hidden"); hidden.parent_id=hidden_folder.id;
    model->setFileItems({folder,ready,waiting,trashed,hidden_folder,hidden});
    auto* mail=dynamic_cast<EmailPage*>(window->page(CybouPage::Mail));
    mail->openCompose();
    bool checked{false};
    QTimer::singleShot(0,[&] {
        auto* dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (!dialog) return;
        auto* tree=dialog->findChild<QTreeWidget*>(QStringLiteral("cybouFileChoices"));
        auto* buttons=dialog->findChild<QDialogButtonBox*>();
        if (tree && buttons && tree->topLevelItemCount()==2) {
            bool ready_path{false}, waiting_disabled{false};
            for (int i=0;i<tree->topLevelItemCount();++i) {
                auto* row=tree->topLevelItem(i);
                if (row->data(0,Qt::UserRole).toString()==ready.id) {
                    ready_path=row->text(0)==QStringLiteral("Projects/report.pdf");
                    row->setSelected(true);
                } else waiting_disabled=!(row->flags() & Qt::ItemIsEnabled);
            }
            checked=ready_path && waiting_disabled;
            buttons->button(QDialogButtonBox::Ok)->click();
        } else dialog->reject();
    });
    mail->composer()->findChild<QAction*>(QStringLiteral("attachCybouFile"))->trigger();
    QVERIFY(checked);
    QCOMPARE(mail->composer()->attachments().size(),1);
    const auto selected=mail->composer()->attachments().first();
    QCOMPARE(selected.id,QStringLiteral("ref-ready"));
    QVERIFY(selected.source_path.isEmpty());
    QVERIFY(!model->attachmentFromFile(trashed.id));
    model->setFileItems({});
    QCOMPARE(mail->composer()->snapshotForRebuild().attachments.first().id,selected.id);
}

void CybouShellTests::composeDropReusesProtectedFilesAndRejectsFallback()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    auto ready = ProtectedFile(QStringLiteral("ready"), QStringLiteral("Report.pdf"));
    auto pending = ready;
    pending.id = QStringLiteral("pending");
    pending.state = CybouContentState::Securing;
    auto folder = ready;
    folder.id = QStringLiteral("folder");
    folder.folder = true;
    folder.trashed = true;
    auto hidden = ready;
    hidden.id = QStringLiteral("hidden");
    hidden.parent_id = folder.id;
    model.setFileItems({ready, pending, folder, hidden});
    MailCompose composer{&model};
    composer.resize(800, 650);
    composer.show();
    composer.start();
    auto* body = composer.findChild<QTextEdit*>(QStringLiteral("composeBody"));
    auto* hint = composer.findChild<QLabel*>(QStringLiteral("composeDropHint"));
    QVERIFY(body && hint);
    body->setPlainText(QStringLiteral("Keep this body"));
    QTemporaryFile downloaded;
    QVERIFY(downloaded.open());
    QMimeData data;
    data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("ready\nready"));
    data.setUrls({QUrl::fromLocalFile(downloaded.fileName())});
    const auto enter = [](QWidget* target, QMimeData& mime) {
        QDragEnterEvent event{QPoint{10, 10}, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(target, &event);
        return event.isAccepted();
    };
    const auto drop = [](QWidget* target, QMimeData& mime) {
        QDropEvent event{QPointF{10, 10}, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(target, &event);
        return event.isAccepted();
    };
    // Drop directly over the editor: keep the body, attach one reusable reference.
    QVERIFY(enter(body->viewport(), data));
    QVERIFY(drop(body->viewport(), data));
    QCOMPARE(body->toPlainText(), QStringLiteral("Keep this body"));
    QCOMPARE(composer.attachments().size(), 1);
    QCOMPARE(composer.attachments().first().id, QStringLiteral("ref-ready"));
    QVERIFY(composer.attachments().first().source_path.isEmpty());
    QVERIFY(enter(&composer, data));
    QVERIFY(drop(&composer, data));
    QCOMPARE(composer.attachments().size(), 1);
    // All-or-nothing batches and internal MIME precedence: never use the URL fallback.
    for (const auto& ids : {QByteArrayLiteral("ready\npending"), QByteArrayLiteral("hidden"),
             QByteArrayLiteral("missing"), QByteArrayLiteral("folder"), QByteArray{}}) {
        data.setData(CybouUi::fileIdsMime(), ids);
        QVERIFY(!enter(&composer, data));
        QCOMPARE(composer.attachments().size(), 1);
        QVERIFY(!hint->text().isEmpty());
    }
    QVERIFY(!model.attachmentFromFile(hidden.id));
    data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("ready"));
    QVERIFY(enter(body->viewport(), data));
    ready.trashed = true;
    model.upsertFileItem(ready);
    QVERIFY(!drop(body->viewport(), data)); // source changed during drag
    QCOMPARE(composer.attachments().size(), 1);
    QCOMPARE(body->toPlainText(), QStringLiteral("Keep this body"));
    ready.trashed = false;
    model.upsertFileItem(ready);
    QTRY_VERIFY(!backend.draft_results.isEmpty());
    const QString draft_id = backend.draft_results.constBegin().key();
    backend.draft_results.value(draft_id)(CybouCommandState::Committed, {});
    QTRY_COMPARE(composer.findChild<QLabel*>(QStringLiteral("draftSaveStatus"))->text(), QStringLiteral("Draft saved on this computer"));
    QCOMPARE(backend.last_draft.attachments.first().id, QStringLiteral("ref-ready"));
    QVERIFY(backend.last_draft.attachments.first().source_path.isEmpty());
    // Local drops still attach paths; remote-only URLs and oversized batches refuse.
    QMimeData local;
    local.setUrls({QUrl::fromLocalFile(downloaded.fileName())});
    QVERIFY(enter(body->viewport(), local));
    QVERIFY(drop(body->viewport(), local));
    QCOMPARE(composer.attachments().size(), 2);
    QCOMPARE(composer.attachments().last().source_path, downloaded.fileName());
    QMimeData remote;
    remote.setUrls({QUrl{QStringLiteral("https://example.invalid/file.pdf")}});
    QVERIFY(!enter(&composer, remote));
    QVector<CybouFileItem> many{ready};
    QStringList ids;
    for (int i = 0; i < 32; ++i) {
        auto next = ready;
        next.id = QStringLiteral("file-%1").arg(i);
        ids.append(next.id);
        many.append(next);
    }
    model.setFileItems(many);
    data.setData(CybouUi::fileIdsMime(), ids.join(QLatin1Char{'\n'}).toUtf8());
    QVERIFY(!enter(&composer, data));
    QCOMPARE(composer.attachments().size(), 2);
    data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("ready"));
    auto* to = composer.findChild<QLineEdit*>(QStringLiteral("recipientEdit"));
    auto* send = composer.findChild<QPushButton*>(QStringLiteral("sendButton"));
    QVERIFY(to && send);
    to->setText(QString(64, QLatin1Char{'a'}));
    QVERIFY(send->isEnabled());
    send->click();
    QVERIFY(static_cast<bool>(backend.send_result));
    QVERIFY(!enter(&composer, data)); // durable handoff is in progress
    QCOMPARE(composer.attachments().size(), 2);
    backend.send_result(CybouCommandState::Failed, QStringLiteral("Preparation failed"));
    QTRY_VERIFY(send->isEnabled());
    QCOMPARE(body->toPlainText(), QStringLiteral("Keep this body"));
    QVERIFY(enter(body->viewport(), data));
    model.requestLockVault();
    QVERIFY(!drop(body->viewport(), data));
    QVERIFY(!model.attachmentFromFile(ready.id));
    QCOMPARE(composer.attachments().size(), 2);
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
    // Refreshing an open popup must not leave stale row labels painted under the new ones.
    button.click();
    const auto popup = button.findChild<QFrame*>(QStringLiteral("activityPopup"));
    QVERIFY(popup);
    const auto visible_titles = [&] {
        QCoreApplication::processEvents(); // new row widgets are shown on the next event loop pass
        int count{0};
        for (const auto* label : popup->findChildren<QLabel*>(QStringLiteral("rowTitle"))) count += label->isVisible();
        return count;
    };
    QCOMPARE(visible_titles(), 2);
    model.setFileItems({uploading, done});
    QCOMPARE(visible_titles(), 1);
    model.setFileItems({uploading, failed, done});
    QCOMPARE(visible_titles(), 2);
    popup->hide();
    model.setFileItems({done});
    QCOMPARE(button.text(), QStringLiteral("0 in progress"));
    QVERIFY(button.isHidden());
}

void CybouShellTests::commonTasksTrackFilesAndPreservePopup()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    const auto file = ProtectedFile(QStringLiteral("file"), QStringLiteral("Private.pdf"));
    model.setFileItems({file});
    CybouMailItem draft;
    draft.id = QStringLiteral("draft-common");
    model.requestSaveMailDraft(draft);
    int completed{0};
    model.requestRenameFile(file.id, QStringLiteral("Renamed.pdf"), [&](bool ok, const QString&) { QVERIFY(ok); ++completed; });
    QCOMPARE(model.applicationTasks().size(), 2);
    QCOMPARE(model.applicationTasks().last().scope, CybouTaskScope::Files);
    const QString key = model.applicationTasks().last().id;
    QCOMPARE(model.applicationTasks().last().state, CybouCommandState::Queued);
    CybouActivityButton button{&model};
    button.show();
    button.click();
    auto* popup = button.findChild<QFrame*>(QStringLiteral("activityPopup"));
    QVERIFY(popup);
    const auto findRow = [popup](const QString& key) -> QWidget* {
        for (auto* row : popup->findChildren<QWidget*>(QStringLiteral("activityTaskRow")))
            if (row->property("taskKey").toString() == key) return row;
        return nullptr;
    };
    auto* retained = findRow(key);
    QVERIFY(retained);
    auto* open = retained->findChild<QPushButton*>(QStringLiteral("taskOpen"));
    open->setFocus();
    backend.file_results.first()(CybouCommandState::Running, {});
    QTRY_COMPARE(model.applicationTasks().last().state, CybouCommandState::Running);
    QCOMPARE(findRow(key), retained);
    QVERIFY(open->hasFocus());
    QVERIFY(retained->findChild<QLabel*>(QStringLiteral("rowSub"))->text().contains(QStringLiteral("Saving changes on this computer")));
    backend.file_results.first()(CybouCommandState::Committed, {});
    backend.file_results.first()(CybouCommandState::Failed, QStringLiteral("Late duplicate"));
    QTRY_COMPARE(completed, 1);
    QCOMPARE(model.fileItem(file.id)->name, file.name); // acknowledgment is not a finalized catalog projection
    const auto after_save = CybouActivityOperations(model);
    QVERIFY(std::none_of(after_save.begin(), after_save.end(), [&](const auto& op) { return op.key == key; }));
    for (int i = 0; i < 104; ++i) model.requestMoveFile(file.id, {});
    QCoreApplication::processEvents();
    auto* scroll = popup->findChild<QScrollArea*>(QStringLiteral("activityTaskScroll"));
    QVERIFY(scroll);
    QCOMPARE(popup->findChildren<QWidget*>(QStringLiteral("activityTaskRow")).size(), 100);
    QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
    auto* overflow = popup->findChild<QLabel*>(QStringLiteral("taskOverflow"));
    QVERIFY(overflow && overflow->isVisible());
    QVERIFY(overflow->text().contains(QStringLiteral("100 of 105")));
    scroll->verticalScrollBar()->setValue(100);
    const auto late = backend.file_results.last();
    backend.file_results.last()(CybouCommandState::Running, {});
    QCoreApplication::processEvents();
    QCOMPARE(scroll->verticalScrollBar()->value(), 100);
    const QString failed_key = model.applicationTasks().last().id;
    backend.file_results.last()(CybouCommandState::Failed, QStringLiteral("<b>Local save failed</b>"));
    QTRY_COMPARE(model.applicationTasks().last().state, CybouCommandState::Failed);
    auto* failure = findRow(failed_key);
    QVERIFY(failure);
    auto* error_label = failure->findChild<QLabel*>(QStringLiteral("rowSub"));
    QCOMPARE(error_label->textFormat(), Qt::PlainText);
    QCOMPARE(error_label->text(), QStringLiteral("<b>Local save failed</b>"));
    QVERIFY(failure->findChild<QPushButton*>(QStringLiteral("taskRetry"))->isHidden());
    QString opened;
    button.onOpenFile = [&](const QString& id) { opened = id; };
    failure->findChild<QPushButton*>(QStringLiteral("taskOpen"))->click();
    QCOMPARE(opened, file.id);
    QVERIFY(!popup->isVisible());
    model.requestLockVault();
    QVERIFY(model.applicationTasks().isEmpty());
    QVERIFY(!popup->isVisible());
    for (auto* label : popup->findChildren<QLabel*>(QStringLiteral("rowTitle"))) QVERIFY(label->text().isEmpty());
    late(CybouCommandState::Failed, QStringLiteral("Private stale error"));
    QCoreApplication::processEvents();
    QVERIFY(model.applicationTasks().isEmpty());

    // Bound terminal history while retaining a queued command across other completions.
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    QVector<CybouFileItem> files;
    for (int i = 0; i < 40; ++i) files.append(ProtectedFile(QString::number(i), QStringLiteral("File.pdf")));
    model.setFileItems(files);
    model.requestSaveMailDraft(draft);
    for (const auto& item : files) {
        model.requestRenameFile(item.id, QStringLiteral("Renamed.pdf"));
        backend.file_results.last()(item.id == QStringLiteral("7") ? CybouCommandState::Failed : CybouCommandState::Committed,
            item.id == QStringLiteral("7") ? QStringLiteral("Old failure") : QString{});
    }
    QCoreApplication::processEvents();
    QCOMPARE(button.text(), QStringLiteral("1 in progress")); // evicting the last failure updates the panel too
    QCOMPARE(model.applicationTasks().size(), 33); // 32 terminal plus one queued draft
    QCOMPARE(model.applicationTasks().first().state, CybouCommandState::Queued);
    model.requestLockVault();
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
    QTRY_VERIFY(changed.count() > 0);
    QStringList names;
    for (const auto& contact : model.contacts()) names << contact.name;
    QCOMPARE(names, (QStringList{QStringLiteral("carol.cybou"), QStringLiteral("bobby.cybou"),
                                 QStringLiteral("alice.cybou")}));

    const QString own = QString(64, QLatin1Char{'a'});
    const QString peer = QStringLiteral("1822") + QString(56, QLatin1Char{'b'}) + QStringLiteral("1930");
    model.setIdentityState(CybouIdentityState::Active, own, 1);
    CybouMailItem unnamed;
    unnamed.id = QStringLiteral("unnamed");
    unnamed.from_address = peer;
    unnamed.from_name = CybouProduct::shortId(peer);
    unnamed.to_address = own;
    unnamed.to_name = CybouProduct::shortId(own);
    unnamed.time = now.addSecs(1);
    model.setMailItems({received, sent, draft, unnamed});
    QTRY_COMPARE(model.contacts().first().name, peer);
    QCOMPARE(model.contacts().first().display_name, CybouProduct::shortId(peer));
    for (const auto& contact : model.contacts()) QVERIFY(contact.name != own);
    MailCompose composer{&model};
    CybouMailItem note;
    note.to_name = model.contacts().first().name;
    composer.start(note);
    QCOMPARE(composer.findChild<QLineEdit*>(QStringLiteral("recipientEdit"))->text(), CybouProduct::shortId(peer));
    QCOMPARE(composer.snapshotForRebuild().to_name, peer);
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

void CybouShellTests::remoteObservationChartsBreakAtGapsAndCohorts()
{
    CybouObservationChart chart{QStringLiteral("Reported traffic"), QStringLiteral("Received"), QStringLiteral("Sent"), QStringLiteral("B/s"), 1, nullptr};
    std::vector<cybou::NetworkObservationPoint> points(5);
    for (size_t n = 0; n < points.size(); ++n) {
        points[n].end_elapsed_ms = (n + 1) * 5000; points[n].cohort_revision = n < 2 ? 1 : 2;
        points[n].traffic.received_bytes_per_second = 0; points[n].traffic.sent_bytes_per_second = 10;
    }
    points[3].traffic = {}; points[4].end_elapsed_ms = 40000;
    chart.setRemoteHistory(points, CybouObservationChart::RemoteMetric::TRAFFIC);
    QCOMPARE(chart.property("sampleCount").toInt(), 5);
    QCOMPARE(chart.property("segmentCount").toInt(), 3);
    chart.resize(780, 260); chart.show(); chart.setFocus(); QTest::keyClick(&chart, Qt::Key_Home);
    QVERIFY(chart.accessibleDescription().contains(QStringLiteral("Received: ") + QLocale{}.toString(0.0, 'f', 1)));
    QTest::keyClick(&chart, Qt::Key_Right); QTest::keyClick(&chart, Qt::Key_Right); QTest::keyClick(&chart, Qt::Key_Right);
    QVERIFY(chart.accessibleDescription().contains(QStringLiteral("Unknown")));
    QVERIFY(!chart.grab().isNull());
    if (const auto path = qEnvironmentVariable("CYBOU_CHART_CAPTURE"); !path.isEmpty()) QVERIFY(chart.grab().save(path));
    chart.setRemoteHistory({}, CybouObservationChart::RemoteMetric::TRAFFIC);
    QCOMPARE(chart.property("sampleCount").toInt(), 0);
    QVERIFY(chart.accessibleDescription().contains(QStringLiteral("Unknown")));
}

void CybouShellTests::networkObservationCardsKeepPartialScope()
{
    cybou::NodeDiagnosticsSnapshot d;
    auto unknown = cybouNetworkObservationText(d);
    QVERIFY(unknown.capacity.contains(QStringLiteral("Unknown")));
    auto observation = std::make_shared<cybou::NetworkObservationSnapshot>();
    observation->network_binding.fill(8);
    d.network_binding = cybou::Hash256{observation->network_binding}.GetHex();
    observation->remote.selected_remote_groups = 3;
    observation->remote.fresh_remote_groups = 2;
    observation->remote.missing_remote_groups = 1;
    observation->remote.storage.contributors = 2;
    observation->remote.storage.capacity_bytes = 100;
    observation->remote.storage.stored_copy_bytes = 0;
    observation->remote.storage.utilization_percent = 0;
    observation->remote.traffic.contributors = 1;
    observation->remote.traffic.received_bytes_per_second = 0;
    observation->remote.traffic.sent_bytes_per_second = 12;
    observation->remote.reports.push_back({false, 1200, 20});
    d.network_observation = observation;
    const auto text = cybouNetworkObservationText(d);
    QVERIFY(cybouNetworkObservationText(d, observation->captured_at + std::chrono::seconds{89}).capacity.contains(QStringLiteral("Unknown")));
    QVERIFY(cybouNetworkObservationText(d, observation->captured_at - std::chrono::seconds{1}).capacity.contains(QStringLiteral("Unknown")));
    QVERIFY(text.capacity_title.contains(QStringLiteral("2 reporting groups")));
    QVERIFY(text.storage_detail.contains(QStringLiteral("Stored copies: 0")));
    QVERIFY(text.coverage.contains(QStringLiteral("2 / 3")));
    QVERIFY(text.coverage.contains(QStringLiteral("missing: 1")));
    QVERIFY(text.cpu.contains(QStringLiteral("Unknown")));
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    model.setNetworkDiagnostics(d);
    NetworkPage page{&model}; page.show(); page.showTechnicalDetails();
    auto* capacity = page.findChild<QLabel*>(QStringLiteral("networkObservedCapacity"));
    auto* coverage = page.findChild<QLabel*>(QStringLiteral("networkObservedCoverage"));
    QVERIFY(capacity && coverage);
    QTRY_COMPARE(capacity->text(), text.capacity);
    QVERIFY(coverage->text().contains(QStringLiteral("2 / 3")));
    d.network_observation.reset(); model.setNetworkDiagnostics(d);
    QTRY_VERIFY(capacity->text().contains(QStringLiteral("Unknown")));
    d.network_observation = observation; d.network_binding = "other-network";
    QVERIFY(cybouNetworkObservationText(d).capacity.contains(QStringLiteral("Unknown")));
    observation->remote.clock_valid = false; d.network_binding = cybou::Hash256{observation->network_binding}.GetHex();
    QVERIFY(cybouNetworkObservationText(d).capacity.contains(QStringLiteral("Unknown")));
    QTranslator translator;
    QVERIFY(translator.load(QStringLiteral(":/i18n/cybou_fr.qm")));
    const bool translated = qApp->installTranslator(&translator);
    const auto french = cybouNetworkObservationText(d);
    qApp->removeTranslator(&translator);
    QVERIFY(translated);
    QVERIFY(french.capacity.contains(QStringLiteral("Inconnu")));
    QVERIFY(french.capacity_title.contains(QStringLiteral("Capacité de stockage observée")));
}

void CybouShellTests::networkMonitorUsesCoreSnapshot()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    cybou::NodeDiagnosticsSnapshot snapshot;
    snapshot.network_binding="devnet-fixture"; snapshot.height=12; snapshot.tip="tip"; snapshot.state_root="root";
    snapshot.initialized=true;
    snapshot.peers.push_back({"127.0.0.1:30471",9,"provider"});
    snapshot.operations.push_back({"operation",3,12});
    model.setNetworkDiagnostics(snapshot);
    DiagnosticsPage page{&model};
    auto* monitor=page.findChild<QToolButton*>(QStringLiteral("networkMonitorButton"));
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
    peers->setCurrentCell(0,0);
    auto* retained = peers->item(0,0);
    snapshot.peers[0].advertised_height = 13;
    model.setNetworkDiagnostics(snapshot);
    QTRY_COMPARE(peers->item(0,2)->text(),QStringLiteral("Ahead 1"));
    QCOMPARE(peers->item(0,0),retained);
    QCOMPARE(peers->currentRow(),0);
    monitor->click();
    QCOMPARE(page.findChildren<QTableWidget*>(QStringLiteral("networkMonitorPeers")).size(),1);
    auto* pause = page.findChild<QPushButton*>(QStringLiteral("networkMonitorPause"));
    QVERIFY(pause);
    pause->click();
    snapshot.peers[0].advertised_height = 14;
    model.setNetworkDiagnostics(snapshot);
    QTest::qWait(200);
    QCOMPARE(peers->item(0,1)->text(),QStringLiteral("13"));
    model.setIdentityState(CybouIdentityState::Active,QStringLiteral("own"),1);
    CybouFileItem file; file.id=QStringLiteral("private-file");file.name=QStringLiteral("private.txt");
    model.setFileItems({file});
    pause->click();
    auto* content = page.findChild<QTableWidget*>(QStringLiteral("networkMonitorContent"));
    QTRY_COMPARE(content->rowCount(),1);
    pause->click();
    model.setIdentityState(CybouIdentityState::Locked);
    QCOMPARE(content->rowCount(),0);
    pause->click();
    snapshot.peers.clear(); snapshot.operations.clear();
    model.setNetworkDiagnostics(snapshot);
    QTRY_COMPARE(peers->rowCount(),0);
    QTRY_COMPARE(operations->rowCount(),0);
}

void CybouShellTests::authorityDashboardUsesLocalHeightObservation()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    model.setNodeStatus(true, 2, true);
    model.setIdentityState(CybouIdentityState::Active,QStringLiteral("authority"),1);
    CybouNetworkAuthorityStatus authority;
    authority.proven = true;
    authority.signer_enabled = true;
    authority.finalizer = CybouFinalizerState::Finalizing;
    authority.finalized_height = 10;
    authority.candidates = 3;
    model.setNetworkAuthority(authority);
    NetworkAuthorityPage page{&model};

    auto has_text = [&page](const QString& text) {
        for (const auto* label : page.findChildren<QLabel*>()) if (label->text() == text) return true;
        return false;
    };
    auto button = [&page](const char* id) {
        for (auto* candidate : page.findChildren<QPushButton*>()) {
            if (candidate->property("cybouId").toString() == QLatin1String{id}) return candidate;
        }
        return static_cast<QPushButton*>(nullptr);
    };
    QVERIFY(has_text(QStringLiteral("None in this view")));
    QVERIFY(has_text(QStringLiteral("Synced")));
    QVERIFY(has_text(QStringLiteral("Finalizing")));

    authority.finalized_height = 11;
    model.setNetworkAuthority(authority);
    QVERIFY(!has_text(QStringLiteral("None in this view")));
    QVERIFY(has_text(QStringLiteral("0 s ago")));

    // Pause asks the controller to stop the loop; one block on demand only while paused.
    auto* pause = button("authorityPause");
    auto* finalize_now = button("authorityFinalizeNow");
    QVERIFY(pause);
    QVERIFY(finalize_now);
    QVERIFY(finalize_now->isHidden());
    QSignalSpy paused{&model, &CybouDesktopModel::finalizationPauseRequested};
    QTimer::singleShot(0,&page,[&page] {
        auto* review=page.findChild<QMessageBox*>(QStringLiteral("authorityPauseReview"));
        if (review) review->findChild<QPushButton*>(QStringLiteral("authorityConfirmPause"))->click();
    });
    pause->click();
    QCOMPARE(paused.count(), 1);
    QCOMPARE(paused.at(0).at(0).toBool(), true);
    authority.finalizer = CybouFinalizerState::Paused;
    model.setNetworkAuthority(authority);
    QVERIFY(!finalize_now->isHidden());
    QCOMPARE(pause->text(), QStringLiteral("Resume finalization"));
    QSignalSpy finalize{&model, &CybouDesktopModel::finalizeNowRequested};
    finalize_now->click();
    QCOMPARE(finalize.count(), 1);

    authority.finalizer = CybouFinalizerState::SafetyHalt;
    authority.signer_enabled = false;
    model.setNetworkAuthority(authority);
    QVERIFY(pause->isEnabled() == false);
}

void CybouShellTests::networkPageReflectsModel()
{
    auto window = makeWindow();
    window->showNetworkDiagnostics();
    auto* network = window->page(CybouPage::Network);
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
    QTRY_VERIFY(has_text(QStringLiteral("Ready")));

    auto* uptime = network->findChild<QLabel*>(QStringLiteral("networkNodeUptime"));
    auto* pool = network->findChild<QLabel*>(QStringLiteral("networkCandidatePool"));
    QVERIFY(uptime && pool);
    QCOMPARE(pool->text(), QStringLiteral("Unknown"));
    cybou::NodeDiagnosticsSnapshot observed;
    observed.observed_unix_ms = 1791460800000ULL;
    observed.uptime_ms = 120000;
    observed.process_resident_bytes = 1048576;
    observed.local_storage_used = 1024;
    observed.local_storage_capacity = 4096;
    observed.storage_disk_available = 1048576;
    observed.process_cpu = {.interval_percent = 12.5, .mean_percent = 10.0, .processors = 4,
        .interval_ms = 3000, .mean_window_ms = 60000, .mean_intervals = 20, .mean_age_ms = 2000};
    observed.initialized = true;
    observed.pending_operations = 3;
    observed.pending_operation_bytes = 700;
    observed.finalization.history = {{5000, 1, 1}, {10000, 3, 2}};
    observed.finalization.windows[0] = {.window_ms = 60000, .observed_operations = 5, .complete = true};
    observed.traffic = {.received_bytes = 6000, .sent_bytes = 12000,
        .window_received_bytes = 6000, .window_sent_bytes = 12000, .window_ms = 60000};
    observed.traffic.history = {{5000, 50, 100}, {10000, 150, 200}};
    observed.storage_transfers.put = {.received_bytes = 60, .sent_bytes = 120,
        .window_received_bytes = 60, .window_sent_bytes = 120, .window_ms = 60000};
    observed.storage_transfers.get = {.received_bytes = 180, .sent_bytes = 240,
        .window_received_bytes = 180, .window_sent_bytes = 240, .window_ms = 60000};
    model->setNetworkDiagnostics(observed);
    QTRY_COMPARE(uptime->text(), QStringLiteral("120 s"));
    auto* memory = network->findChild<QLabel*>(QStringLiteral("networkProcessMemory"));
    QVERIFY(memory);
    QTRY_COMPARE(memory->text(), CybouProduct::sizeText(1048576));
    auto* disk = network->findChild<QLabel*>(QStringLiteral("networkDiskAvailable"));
    QVERIFY(disk);
    QTRY_COMPARE(disk->text(), CybouProduct::sizeText(1048576));
    auto* cpu = network->findChild<QLabel*>(QStringLiteral("networkProcessCpu"));
    QVERIFY(cpu);
    QTRY_VERIFY(cpu->text().contains(QLocale{}.toString(12.5, 'f', 1)));
    observed.process_resident_bytes.reset();
    observed.storage_disk_available.reset();
    model->setNetworkDiagnostics(observed);
    QTRY_COMPARE(memory->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(disk->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(pool->text(), QStringLiteral("3"));
    auto* finalized_rate = network->findChild<QLabel*>(QStringLiteral("networkFinalizationRate"));
    QVERIFY(finalized_rate);
    QTRY_COMPARE(finalized_rate->text(), QLocale{}.toString(5.0, 'f', 1));
    auto* traffic = network->findChild<QLabel*>(QStringLiteral("networkTrafficRate"));
    QVERIFY(traffic);
    QTRY_VERIFY(traffic->text().contains(QLocale{}.toString(100.0, 'f', 1)));
    auto* put_payload = network->findChild<QLabel*>(QStringLiteral("networkPutPayloadRate"));
    auto* get_payload = network->findChild<QLabel*>(QStringLiteral("networkGetPayloadRate"));
    QVERIFY(put_payload && get_payload);
    QTRY_VERIFY(put_payload->text().contains(QLocale{}.toString(2.0, 'f', 1)));
    QTRY_VERIFY(get_payload->text().contains(QLocale{}.toString(3.0, 'f', 1)));
    observed.storage_transfers.put.window_ms = 0;
    observed.storage_transfers.get.window_ms = 0;
    model->setNetworkDiagnostics(observed);
    QTRY_COMPARE(put_payload->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(get_payload->text(), QStringLiteral("Unknown"));
    auto* traffic_chart = network->findChild<QWidget*>(QStringLiteral("networkTrafficChart"));
    auto* operation_chart = network->findChild<QWidget*>(QStringLiteral("networkFinalizationChart"));
    QVERIFY(traffic_chart && operation_chart);
    QTRY_COMPARE(traffic_chart->property("sampleCount").toInt(), 2);
    QTRY_COMPARE(operation_chart->property("sampleCount").toInt(), 2);
    static_cast<NetworkPage*>(network)->showBenchmarkDetails();
    window->show();
    QTest::keyClick(traffic_chart, Qt::Key_Home);
    QCOMPARE(traffic_chart->property("selectedIntervalMs").toULongLong(), 5000ULL);
    QVERIFY(traffic_chart->accessibleDescription().contains(QLocale{}.toString(10.0, 'f', 1)));
    QTest::keyClick(traffic_chart, Qt::Key_Right);
    QCOMPARE(traffic_chart->property("selectedIntervalMs").toULongLong(), 10000ULL);
    observed.traffic.history.clear();
    for (uint64_t i = 0; i < 200; ++i) observed.traffic.history.push_back({(i + 1) * 5000, i * 50, (i % 7) * 60});
    model->setNetworkDiagnostics(observed);
    QTRY_COMPARE(traffic_chart->property("sampleCount").toInt(), 180);
    QTest::keyClick(traffic_chart, Qt::Key_Home);
    QCOMPARE(traffic_chart->property("selectedIntervalMs").toULongLong(), 105000ULL);
    QTest::keyClick(traffic_chart, Qt::Key_End);
    QCOMPARE(traffic_chart->property("selectedIntervalMs").toULongLong(), 1000000ULL);
    QDir{}.mkpath(QStringLiteral("artifacts/network-charts-20261008"));
    QVERIFY(traffic_chart->grab().save(QStringLiteral("artifacts/network-charts-20261008/traffic-chart.png")));
    QVERIFY(operation_chart->grab().save(QStringLiteral("artifacts/network-charts-20261008/operations-chart.png")));
    observed.initialized = false;
    model->setNetworkDiagnostics(observed);
    QTRY_COMPARE(pool->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(finalized_rate->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(operation_chart->property("sampleCount").toInt(), 0);
    model->setNetworkDiagnostics({});
    QTRY_COMPARE(uptime->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(traffic->text(), QStringLiteral("Unknown"));
    QTRY_COMPARE(traffic_chart->property("sampleCount").toInt(), 0);
    QTRY_COMPARE(cpu->text(), QStringLiteral("Unknown"));
}

void CybouShellTests::adapterSettersDrivePages()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(model);

    window->showNetworkDiagnostics();
    // Doc 73 adapter surface: setters mutate status and pages follow.
    QSignalSpy status_spy{model, &CybouDesktopModel::statusChanged};
    model->setFinalizedHeight(42);
    QCOMPARE(model->status().finalized_height, quint64{42});
    QVERIFY(model->status().finality_known);
    QVERIFY(status_spy.count() >= 1);

    auto* network = window->page(CybouPage::Network);
    QVERIFY(network);
    QTest::qWait(250);
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
    QTest::qWait(250);
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

void CybouShellTests::mailFilesKeyboardScopesInBothLanguages()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    for (const auto& language : {QStringLiteral("en"), QStringLiteral("fr")}) {
        window->setLanguage(language);
        window->showPage(CybouPage::Files);
        auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
        RecordingBackend backend;
        const auto file_commands = [&backend] {
            QStringList result;
            for (const auto& command : backend.commands) {
                if (command.startsWith(QStringLiteral("rename:")) || command.startsWith(QStringLiteral("trash:")) ||
                    command.startsWith(QStringLiteral("delete:")) || command.startsWith(QStringLiteral("restore:"))) result.append(command);
            }
            return result; // Projection refresh and session lifecycle are independent.
        };
        model->setApplicationBackend(&backend);
        model->setFeatureAvailability(AllFeatureAvailability());
        model->setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
        auto file = ProtectedFile(QStringLiteral("keyboard-file"), QStringLiteral("keyboard.txt"));
        model->setFileItems({file});
        files->setView(StoragePage::View::MyFiles);
        backend.commands.clear(); // Exclude session open/close from mutation assertions.
        auto* table = files->findChild<QTreeWidget*>(QStringLiteral("filesTable"));
        auto* grid = files->findChild<QListWidget*>(QStringLiteral("filesGrid"));
        auto* nav = files->findChild<QListWidget*>(QStringLiteral("folderList"));
        QVERIFY(table && grid && nav);
        QCOMPARE(table->accessibleName(), language == QLatin1String("fr") ? QStringLiteral("Fichiers") : QStringLiteral("Files"));
        for (bool tiles : {false, true}) {
            files->setGridMode(tiles);
            if (tiles) grid->setCurrentRow(0); else table->setCurrentItem(table->topLevelItem(0));
            // Retained selection must not make navigation a destructive shortcut target.
            nav->setFocus();
            QTRY_VERIFY(nav->hasFocus());
            bool unexpected_dialog = false;
            QTimer::singleShot(0, window.get(), [&] {
                if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                    unexpected_dialog = true;
                    dialog->reject();
                }
            });
            QTest::keyClick(nav, Qt::Key_F2);
            QApplication::processEvents();
            QVERIFY(!unexpected_dialog);
            QTest::keyClick(nav, Qt::Key_Delete);
            QVERIFY2(file_commands().isEmpty(), qPrintable(backend.commands.join(QLatin1Char(','))));
            // Header text editing also leaves the selected file alone.
            auto* search = window->globalSearch();
            search->setFocus();
            search->setText(QStringLiteral("keyboard"));
            search->setCursorPosition(0);
            QTest::keyClick(search, Qt::Key_Delete);
            QCOMPARE(search->text(), QStringLiteral("eyboard"));
            QVERIFY2(file_commands().isEmpty(), qPrintable(backend.commands.join(QLatin1Char(','))));
            // Dismiss completion before returning to the catalog, as a user does.
            QTest::keyClick(search, Qt::Key_Escape);
            search->clear();
            QApplication::processEvents();
            QCOMPARE(files->visibleIds(), QStringList{file.id});
            QWidget* view = tiles ? static_cast<QWidget*>(grid) : table;
            QVERIFY(view->isVisible());
            window->activateWindow();
            view->setFocus();
            QTRY_VERIFY(view->hasFocus());
            bool renamed = false;
            QTimer::singleShot(0, window.get(), [&] {
                if (auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget())) {
                    renamed = true;
                    dialog->setTextValue(QStringLiteral("renamed.txt"));
                    dialog->accept();
                }
            });
            QTest::keyClick(view, Qt::Key_F2);
            QApplication::processEvents();
            QVERIFY(renamed);
            QCOMPARE(file_commands(), QStringList{QStringLiteral("rename:keyboard-file")});
            backend.file_results.last()(CybouCommandState::Committed, {});
            backend.commands.clear();
            window->activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(window.get()));
            view->setFocus();
            QTRY_VERIFY(view->hasFocus());
            QTest::keyClick(view, Qt::Key_Delete);
            QCOMPARE(file_commands(), QStringList{QStringLiteral("trash:keyboard-file")});
            backend.file_results.last()(CybouCommandState::Failed, QStringLiteral("Save failed"));
            QVERIFY(!model->fileItem(file.id)->trashed);
            backend.commands.clear();
        }
        file.trashed = true;
        model->setFileItems({file});
        files->setView(StoragePage::View::Trash);
        files->setGridMode(false);
        table->setCurrentItem(table->topLevelItem(0));
        table->setFocus();
        QTRY_VERIFY(table->hasFocus());
        bool trash_rename = false;
        QTimer::singleShot(0, window.get(), [&] {
            if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
                trash_rename = true;
                dialog->reject();
            }
        });
        QTest::keyClick(table, Qt::Key_F2);
        QApplication::processEvents();
        QVERIFY(!trash_rename);
        QTest::keyClick(table, Qt::Key_Delete);
        QVERIFY2(file_commands().isEmpty(), qPrintable(backend.commands.join(QLatin1Char(','))));
        model->setApplicationBackend(nullptr);
        QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
        window->showPage(CybouPage::Mail);
        auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
        mail->setFocus();
        QTest::keyClick(mail, Qt::Key_Slash);
        QTRY_VERIFY(window->globalSearch()->hasFocus());
        window->globalSearch()->clear();
        mail->setFocus();
        QTest::keyClick(mail, Qt::Key_N, Qt::ControlModifier);
        QVERIFY(mail->isComposing());
        auto* body = mail->composer()->findChild<QTextEdit*>(QStringLiteral("composeBody"));
        QVERIFY(body);
        body->setFocus();
        QTest::keyClicks(body, "crf/");
        QCOMPARE(body->toPlainText(), QStringLiteral("crf/"));
        QTest::keyClick(body, Qt::Key_Home);
        QTest::keyClick(body, Qt::Key_Delete);
        QCOMPARE(body->toPlainText(), QStringLiteral("rf/"));
        QVERIFY(mail->isComposing());
    }
    window->setLanguage(QStringLiteral("en"));
    window->close();
}

void CybouShellTests::mailFilesTabReachabilityAndFocus()
{
    ScopedEnvironment appearance{"CYBOU_APPEARANCE", "light"};
    auto window = makeWindow();
    window->resize(1280, 860);
    window->show();
    window->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(window.get()));
    const QString shots = qEnvironmentVariable("CYBOU_TEST_FOCUS_SCREENSHOT_DIR");
    if (!shots.isEmpty()) QVERIFY(QDir{}.mkpath(shots));
    bool cycles_complete = true;
    bool captures_saved = true;
    const auto walk = [&](QWidget* start, bool reverse, const QString& prefix) {
        QSet<QWidget*> visited;
        QApplication::processEvents(); // Complete page replacement/layout before native focus.
        window->activateWindow();
        if (!QTest::qWaitForWindowActive(window.get())) { cycles_complete = false; return visited; }
        start->setFocus(Qt::TabFocusReason);
        QApplication::processEvents();
        for (int i = 0; i < 256; ++i) {
            QWidget* focused = QApplication::focusWidget();
            if (!focused || (i > 0 && focused == start)) break;
            visited.insert(focused);
            if (!shots.isEmpty() && !reverse &&
                (focused->objectName() == QLatin1String("sendButton") ||
                 focused->property("cybouId").toString() == QLatin1String("securityDetails") ||
                 focused->objectName() == QLatin1String("filesSelectionTrash"))) {
                const QString semantic_id = focused->property("cybouId").toString();
                const QString id = semantic_id.isEmpty() ? focused->objectName() : semantic_id;
                const QString path = shots + QLatin1Char('/') + prefix + QLatin1Char('-') + id;
                captures_saved &= focused->grab().save(path + QStringLiteral(".png"));
                captures_saved &= focused->parentWidget()->grab().save(path + QStringLiteral("-context.png"));
            }
            QTest::keyClick(focused, Qt::Key_Tab, reverse ? Qt::ShiftModifier : Qt::NoModifier);
            QApplication::processEvents();
        }
        cycles_complete &= QApplication::focusWidget() == start;
        if (visited.isEmpty()) qWarning("Tab walk started without native focus");
        return visited;
    };
    for (const auto& theme : {QByteArray{"light"}, QByteArray{"dark"}}) {
        qputenv("CYBOU_APPEARANCE", theme);
        window->reloadAppearance();
        for (const auto& language : {QStringLiteral("en"), QStringLiteral("fr")}) {
            window->setLanguage(language);
            auto* model = window->desktopModel();
            QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("mail")));
            window->showPage(CybouPage::Mail);
            auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
            mail->openCompose();
            auto* to = mail->composer()->findChild<QLineEdit*>(QStringLiteral("recipientEdit"));
            auto* body = mail->composer()->findChild<QTextEdit*>(QStringLiteral("composeBody"));
            QVERIFY(to && body);
            to->setText(QStringLiteral("alice.cybou"));
            body->setPlainText(QStringLiteral("Keyboard acceptance"));
            CybouAttachmentItem attachment;
            attachment.id = QStringLiteral("tab-attachment");
            attachment.name = QStringLiteral("report.pdf");
            mail->composer()->addProtectedAttachment(attachment);
            const QString prefix = language + QLatin1Char('-') + QString::fromLatin1(theme);
            const auto check = [&](QWidget* page, QWidget* start) {
                const auto forward = walk(start, false, prefix);
                const auto backward = walk(start, true, prefix);
                for (auto* button : page->findChildren<QAbstractButton*>()) {
                    if (!button->isVisible() || !button->isEnabled()) continue;
                    if (!forward.contains(button) || !backward.contains(button)) return button;
                    if (button->text().isEmpty() && button->accessibleName().isEmpty()) return button;
                }
                return static_cast<QAbstractButton*>(nullptr);
            };
            auto* missed = check(mail->composer(), to);
            QVERIFY2(!missed, missed ? qPrintable(missed->objectName() + missed->accessibleName()) : "");
            const auto fields = walk(to, false, prefix);
            QVERIFY(fields.contains(body));
            QVERIFY(fields.contains(mail->composer()->findChild<QLineEdit*>(QStringLiteral("subjectEdit"))));
            // Leave the draft through its existing saved-intent path, then inspect a reader.
            mail->openMessage(QStringLiteral("m-project"));
            auto* security = FindById<QPushButton>(mail->reader(), "securityDetails");
            QVERIFY(security);
            missed = check(mail->reader(), security);
            QVERIFY2(!missed, missed ? qPrintable(missed->objectName() + missed->accessibleName()) : "");
            window->showPage(CybouPage::Files);
            auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
            files->setView(StoragePage::View::Recent);
            auto* table = files->findChild<QTreeWidget*>(QStringLiteral("filesTable"));
            auto* grid = files->findChild<QListWidget*>(QStringLiteral("filesGrid"));
            QVERIFY(table && grid && table->topLevelItemCount() > 0);
            for (bool tiles : {false, true}) {
                files->setGridMode(tiles);
                if (tiles) grid->setCurrentRow(0); else table->setCurrentItem(table->topLevelItem(0));
                missed = check(files, tiles ? static_cast<QWidget*>(grid) : table);
                QVERIFY2(!missed, missed ? qPrintable(missed->objectName() + missed->accessibleName()) : "");
            }
        }
    }
    QVERIFY(cycles_complete);
    QVERIFY(captures_saved);
    window->setLanguage(QStringLiteral("en"));
    window->close();
}

void CybouShellTests::identityWalletNetworkKeyboardNavigation()
{
    ScopedEnvironment appearance{"CYBOU_APPEARANCE", "light"};
    auto window = makeWindow();
    window->resize(1280, 860);
    window->show();
    const auto focus = [&](QWidget* widget) {
        QApplication::processEvents();
        window->activateWindow();
        if (!QTest::qWaitForWindowActive(window.get())) return false;
        QApplication::setActiveWindow(window.get());
        widget->setFocus(Qt::TabFocusReason);
        QApplication::processEvents();
        return widget->hasFocus();
    };
    const QString shots = qEnvironmentVariable("CYBOU_TEST_FOCUS_SCREENSHOT_DIR");
    if (!shots.isEmpty()) QVERIFY(QDir{}.mkpath(shots));
    for (const auto& theme : {QByteArray{"light"}, QByteArray{"dark"}}) {
        qputenv("CYBOU_APPEARANCE", theme);
        window->reloadAppearance();
        for (const auto& language : {QStringLiteral("en"), QStringLiteral("fr")}) {
            window->setLanguage(language);
            auto* model = window->desktopModel();
            QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
            for (const auto page_id : {CybouPage::Identity, CybouPage::Wallet, CybouPage::Network}) {
                window->showPage(page_id);
                QWidget* page = window->page(page_id);
                QApplication::processEvents();
                auto buttons = page->findChildren<QAbstractButton*>();
                QWidget* start = nullptr;
                for (auto* button : buttons) {
                    if (button->isVisible() && button->isEnabled()) { start = button; break; }
                }
                QVERIFY(start);
                for (bool reverse : {false, true}) {
                    QVERIFY(focus(start));
                    QSet<QWidget*> visited;
                    for (int i = 0; i < 256; ++i) {
                        auto* current = QApplication::focusWidget();
                        QVERIFY(current);
                        if (i > 0 && current == start) break;
                        visited.insert(current);
                        QTest::keyClick(current, Qt::Key_Tab, reverse ? Qt::ShiftModifier : Qt::NoModifier);
                        QApplication::processEvents();
                    }
                    QCOMPARE(QApplication::focusWidget(), start);
                    for (auto* button : buttons) {
                        if (!button->isVisible() || !button->isEnabled()) continue;
                        QVERIFY2(visited.contains(button), qPrintable(button->objectName() + button->text()));
                        QVERIFY(!button->text().isEmpty() || !button->accessibleName().isEmpty());
                    }
                    for (auto* row : page->findChildren<QFrame*>(QStringLiteral("walletActivityRow"))) {
                        QVERIFY(visited.contains(row));
                        QVERIFY(!row->accessibleName().isEmpty());
                    }
                    if (page_id == CybouPage::Network) {
                        QVERIFY(visited.contains(static_cast<NetworkPage*>(page)->mapWidget()));
                    }
                }
            }
            window->showPage(CybouPage::Network);
            auto* network = static_cast<NetworkPage*>(window->page(CybouPage::Network));
            cybou::NodeDiagnosticsSnapshot snap;
            snap.peers = {{"127.0.0.1:29461", 12, ""}, {"192.168.1.50:29461", 10, ""}};
            model->setNetworkDiagnostics(snap);
            QTRY_COMPARE(network->peerCount(), 2);
            network->selectPeer(0);
            QVERIFY(focus(network->mapWidget()));
            QTest::keyClick(network->mapWidget(), Qt::Key_Right);
            QCOMPARE(network->selectedPeerIndex(), 1);
            QTest::keyClick(network->mapWidget(), Qt::Key_Left);
            QCOMPARE(network->selectedPeerIndex(), 0);
            const QString prefix = shots + QLatin1Char('/') + language + QLatin1Char('-') + QString::fromLatin1(theme);
            if (!shots.isEmpty()) QVERIFY(network->mapWidget()->grab().save(prefix + QStringLiteral("-map.png")));

            window->showPage(CybouPage::Wallet);
            auto* wallet = window->page(CybouPage::Wallet);
            QFrame* entry_row = nullptr;
            for (auto* row : wallet->findChildren<QFrame*>(QStringLiteral("walletActivityRow"))) {
                if (!row->property("walletEntryId").toString().isEmpty()) { entry_row = row; break; }
            }
            QVERIFY(entry_row && focus(entry_row));
            if (!shots.isEmpty()) QVERIFY(entry_row->grab().save(prefix + QStringLiteral("-wallet-row.png")));
            const auto drive_dialog = [&](const QString& name, bool amount_dialog) {
                bool seen = false;
                bool valid = false;
                QTimer driver;
                driver.setInterval(10);
                QObject::connect(&driver, &QTimer::timeout, window.get(), [&] {
                    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    if (!dialog || dialog->objectName() != name) return;
                    driver.stop();
                    seen = true;
                    dialog->activateWindow();
                    QApplication::setActiveWindow(dialog);
                    if (amount_dialog) {
                        auto* amount = dialog->findChild<QLineEdit*>(QStringLiteral("walletLockAmount"));
                        auto* confirm = dialog->findChild<QPushButton*>(QStringLiteral("primaryButton"));
                        valid = amount && confirm && !amount->accessibleName().isEmpty() && !confirm->isEnabled();
                        if (amount) { amount->setFocus(); QTest::keyClicks(amount, "1"); }
                        valid &= confirm && confirm->isEnabled();
                    } else {
                        valid = true;
                    }
                    if (auto* child = dialog->focusWidget()) QTest::keyClick(child, Qt::Key_Escape);
                    valid &= !dialog->isVisible();
                    if (dialog->isVisible()) dialog->reject();
                });
                QTimer timeout;
                timeout.setSingleShot(true);
                QObject::connect(&timeout, &QTimer::timeout, window.get(), [] {
                    if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
                });
                driver.start(); timeout.start(3000);
                QTest::keyClick(QApplication::focusWidget(), Qt::Key_Space);
                return seen && valid;
            };
            QVERIFY(drive_dialog(QStringLiteral("walletEntryDetails"), false));
            QCOMPARE(window->focusWidget(), entry_row);
            auto* lock = FindById<QPushButton>(wallet, QStringLiteral("walletLock"));
            QVERIFY(lock && focus(lock));
            const auto balance = model->status().balance;
            const auto system = model->status().system_balance;
            QSignalSpy submitted{model, &CybouDesktopModel::systemLockFinished};
            QVERIFY(drive_dialog(QStringLiteral("walletLockDialog"), true));
            QCOMPARE(submitted.count(), 0);
            QCOMPARE(model->status().balance, balance);
            QCOMPARE(model->status().system_balance, system);
            QCOMPARE(window->focusWidget(), lock);

            auto entries = model->walletEntries();
            QVERIFY(!entries.isEmpty());
            entries = {entries.first(), entries.first()};
            entries[0].id = language + QString::fromLatin1(theme) + QStringLiteral("keyboard-fee-a");
            entries[1].id = language + QString::fromLatin1(theme) + QStringLiteral("keyboard-fee-b");
            for (auto& e : entries) { e.kind = CybouWalletEntryKind::NetworkServiceFee; e.operation_state = CybouOperationState::Finalized; e.operation_id.clear(); }
            model->setWalletEntries(entries);
            QFrame* group = nullptr;
            for (auto* row : wallet->findChildren<QFrame*>(QStringLiteral("walletActivityRow"))) {
                if (!row->property("walletFeeGroup").toStringList().isEmpty()) group = row;
            }
            QVERIFY(group && focus(group));
            QTest::keyClick(group, Qt::Key_Return);
            QTRY_VERIFY(window->focusWidget() && window->focusWidget()->property("walletEntryId").toString() == entries.first().id);
        }
    }
    window->setLanguage(QStringLiteral("en"));
    window->close();
}

void CybouShellTests::recoveryAndConsoleKeyboardNavigation()
{
    ScopedEnvironment appearance{"CYBOU_APPEARANCE", "light"};
    auto window = makeWindow();
    window->show();
    const auto activate = [](QWidget* dialog, QWidget* target) {
        QApplication::processEvents();
        dialog->activateWindow();
        if (!QTest::qWaitForWindowActive(dialog)) return false;
        QApplication::setActiveWindow(dialog);
        target->setFocus();
        QApplication::processEvents();
        return target->hasFocus();
    };
    for (const auto& theme : {QByteArray{"light"}, QByteArray{"dark"}}) {
        qputenv("CYBOU_APPEARANCE", theme);
        window->reloadAppearance();
        for (const auto& language : {QStringLiteral("en"), QStringLiteral("fr")}) {
            window->setLanguage(language);
            auto* model = window->desktopModel();
            QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
            window->showPage(CybouPage::Identity);
            auto* page = window->page(CybouPage::Identity);
            auto* reveal = FindById<QPushButton>(page, QStringLiteral("recoveryOptions"));
            QVERIFY(reveal && activate(window.get(), reveal));
            QTest::keyClick(reveal, Qt::Key_Space);
            QDialog* gate = nullptr;
            QTRY_VERIFY((gate = page->findChild<QDialog*>(QStringLiteral("revealRecoveryDialog"))) != nullptr);
            auto* password = gate->findChild<QLineEdit*>(QStringLiteral("revealPassword"));
            auto* ack = gate->findChild<QCheckBox*>(QStringLiteral("revealAcknowledge"));
            auto* accept = gate->findChild<QPushButton*>(QStringLiteral("primaryButton"));
            QVERIFY(password && ack && accept && !password->accessibleName().isEmpty());
            QVERIFY(!accept->isEnabled());
            QVERIFY(activate(gate, password));
            QTest::keyClicks(password, "synthetic-password");
            QVERIFY(!accept->isEnabled());
            ack->setFocus(); QTest::keyClick(ack, Qt::Key_Space);
            QVERIFY(accept->isEnabled());
            QTest::keyClick(ack, Qt::Key_Escape);
            QTRY_VERIFY(!page->findChild<QDialog*>(QStringLiteral("revealRecoveryDialog")));
            QCOMPARE(window->focusWidget(), reveal);

            auto* replace = FindById<QPushButton>(page, QStringLiteral("replaceRecoveryPhrase"));
            QVERIFY(replace && activate(window.get(), replace));
            bool review_seen = false;
            bool safe_default = false;
            QTimer review_driver;
            review_driver.setInterval(10);
            QObject::connect(&review_driver, &QTimer::timeout, window.get(), [&] {
                auto* review = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (!review) return;
                review_driver.stop(); review_seen = true;
                safe_default = review->defaultButton() == review->button(QMessageBox::No);
                QApplication::setActiveWindow(review);
                QTest::keyClick(review, Qt::Key_Return);
            });
            QTimer watchdog;
            watchdog.setSingleShot(true);
            QObject::connect(&watchdog, &QTimer::timeout, window.get(), [] {
                if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
            });
            review_driver.start(); watchdog.start(3000);
            QTest::keyClick(replace, Qt::Key_Space);
            QVERIFY(review_seen && safe_default);
            QCOMPARE(window->focusWidget(), replace);

            QStringList words;
            for (int i = 0; i < 24; ++i) words << QStringLiteral("syntheticword");
            RecoveryPhraseDialog phrase{RecoveryPhraseDialog::Mode::Rotate, words, window.get()};
            phrase.show();
            auto* first = phrase.findChild<QLineEdit*>(QStringLiteral("recoveryConfirmFirst"));
            auto* second = phrase.findChild<QLineEdit*>(QStringLiteral("recoveryConfirmSecond"));
            auto* confirm = phrase.findChild<QPushButton*>(QStringLiteral("primaryButton"));
            QVERIFY(first && second && confirm);
            QVERIFY(!first->accessibleName().isEmpty() && first->accessibleName() != second->accessibleName());
            QVERIFY(activate(&phrase, first));
            QTest::keyClicks(first, "wrong"); QTest::keyClicks(second, "syntheticword");
            QVERIFY(!confirm->isEnabled());
            first->setText(QStringLiteral("syntheticword"));
            QVERIFY(confirm->isEnabled());
            QTest::keyClick(first, Qt::Key_Tab);
            QCOMPARE(QApplication::focusWidget(), second);
            QTest::keyClick(second, Qt::Key_Escape);
            QCOMPARE(phrase.result(), int(QDialog::Rejected));
            QVERIFY(phrase.findChild<QTextEdit*>(QStringLiteral("recoveryWords"))->toPlainText().isEmpty());

            CybouConsoleDialog console{model, window.get()};
            console.show();
            auto* input = console.findChild<QLineEdit*>(QStringLiteral("consoleInput"));
            auto* output = console.findChild<QPlainTextEdit*>(QStringLiteral("consoleOutput"));
            auto* run = console.findChild<QToolButton*>(QStringLiteral("consoleRun"));
            QVERIFY(input && output && run && !input->accessibleName().isEmpty());
            QVERIFY(activate(&console, input));
            QTest::keyClick(input, Qt::Key_Tab);
            QCOMPARE(QApplication::focusWidget(), run);
            QTest::keyClick(run, Qt::Key_Tab, Qt::ShiftModifier);
            QCOMPARE(QApplication::focusWidget(), input);
            input->setText(QStringLiteral("status"));
            QTest::keyClick(input, Qt::Key_Return);
            QCOMPARE(console.historyCount(), 1);
            QTest::keyClick(input, Qt::Key_Up);
            QCOMPARE(input->text(), QStringLiteral("status"));
            QTest::keyClick(input, Qt::Key_Down);
            QVERIFY(input->text().isEmpty());
            QTest::keyClick(input, Qt::Key_F, Qt::ControlModifier);
            auto* find = console.findChild<QLineEdit*>(QStringLiteral("consoleSearch"));
            QCOMPARE(QApplication::focusWidget(), find);
            QTest::keyClick(find, Qt::Key_Escape);
            QVERIFY(console.isVisible()); QCOMPARE(QApplication::focusWidget(), input);
            QTest::keyClick(input, Qt::Key_L, Qt::ControlModifier);
            QVERIFY(console.outputText().isEmpty()); QCOMPARE(console.historyCount(), 0);
            QTest::keyClick(input, Qt::Key_Escape);
            QVERIFY(!console.isVisible());
        }
    }
    window->setLanguage(QStringLiteral("en"));
    window->close();
}

void CybouShellTests::mailFilesKeyboardMenusAndDialogs()
{
    auto window = makeWindow();
    window->resize(1280, 860);
    window->show();
    const auto focus = [&](QWidget* widget) {
        QApplication::processEvents();
        window->activateWindow();
        if (!QTest::qWaitForWindowActive(window.get())) return false;
        QApplication::setActiveWindow(window.get());
        widget->setFocus();
        QApplication::processEvents();
        return widget->hasFocus();
    };
    const auto activate_dialog = [](QDialog* dialog) {
        dialog->activateWindow();
        if (!QTest::qWaitForWindowActive(dialog)) return false;
        // Nested synthetic key delivery can precede QWidget WindowActivate.
        QApplication::setActiveWindow(dialog);
        if (auto* target = dialog->focusWidget()) target->setFocus();
        QApplication::processEvents();
        return QApplication::focusWidget() && QApplication::focusWidget()->window() == dialog;
    };
    const auto choose = [](QMenu* menu, QAction* target) {
        if (!target || !target->isEnabled()) return false;
        QTest::keyClick(menu, Qt::Key_Home);
        for (int i = 0; i < 40 && menu->activeAction() != target; ++i) QTest::keyClick(menu, Qt::Key_Down);
        if (menu->activeAction() != target) return false;
        QTest::keyClick(menu, Qt::Key_Return);
        return true;
    };
    for (const auto& language : {QStringLiteral("en"), QStringLiteral("fr")}) {
        window->setLanguage(language);
        auto* model = window->desktopModel();
        QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
        window->showPage(CybouPage::Mail);
        auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
        mail->setView(EmailPage::View::Inbox); // Each language starts with the list, not a retained composer.
        auto* list = mail->findChild<QListWidget*>(QStringLiteral("messageList"));
        QVERIFY(list && list->count() > 1);
        list->setCurrentRow(1);
        const QString current_id = list->currentItem()->data(Qt::UserRole).toString();
        const QString other_id = list->item(0)->data(Qt::UserRole).toString();
        QVERIFY(focus(list));
        const QPoint wrong = list->visualItemRect(list->item(0)).center();
        QContextMenuEvent mail_menu{QContextMenuEvent::Keyboard, wrong, list->viewport()->mapToGlobal(wrong)};
        QApplication::sendEvent(list->viewport(), &mail_menu);
        QMenu* popup = nullptr;
        QTRY_VERIFY((popup = qobject_cast<QMenu*>(QApplication::activePopupWidget())) != nullptr);
        QCOMPARE(list->selectedItems().size(), 1);
        QCOMPARE(list->selectedItems().first()->data(Qt::UserRole).toString(), current_id);
        QTest::keyClick(popup, Qt::Key_Escape);
        QTRY_VERIFY(QApplication::activePopupWidget() == nullptr);
        QCOMPARE(window->focusWidget(), list);
        // Preserve an existing multi-selection when its current item owns the menu.
        list->item(0)->setSelected(true);
        QApplication::sendEvent(list, &mail_menu);
        QTRY_VERIFY((popup = qobject_cast<QMenu*>(QApplication::activePopupWidget())) != nullptr);
        QCOMPARE(list->selectedItems().size(), 2);
        QVERIFY(choose(popup, popup->findChild<QAction*>(QStringLiteral("mailArchive"))));
        QTRY_COMPARE(model->mailItem(current_id)->folder, CybouMailFolder::Archive);
        QCOMPARE(model->mailItem(other_id)->folder, CybouMailFolder::Archive);

        // A menu button opens with Space; Escape from its picker retains compose text.
        mail->openCompose();
        auto* body = mail->composer()->findChild<QTextEdit*>(QStringLiteral("composeBody"));
        body->setPlainText(QStringLiteral("Keep this draft"));
        auto* attach = FindById<QPushButton>(mail->composer(), QStringLiteral("attachFile"));
        QVERIFY(attach && focus(attach));
        bool picker_seen = false;
        bool picker_named = false;
        bool source_chosen = false;
        int picker_phase = 0;
        QTimer picker_driver;
        picker_driver.setInterval(10);
        QTimer picker_modal;
        picker_modal.setInterval(10);
        QObject::connect(&picker_driver, &QTimer::timeout, window.get(), [&] {
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) {
                picker_driver.stop();
                picker_phase = 1;
                source_chosen = choose(menu, menu->findChild<QAction*>(QStringLiteral("attachCybouFile")));
            }
        });
        QObject::connect(&picker_modal, &QTimer::timeout, window.get(), [&] {
            if (picker_phase != 1) return;
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto* tree = dialog->findChild<QTreeWidget*>(QStringLiteral("cybouFileChoices"));
            picker_seen = tree;
            picker_named = tree && tree->accessibleName() == MailCompose::tr("Attach from CYBOU Files");
            picker_modal.stop();
            if (activate_dialog(dialog)) {
                QWidget* target = dialog->focusWidget();
                QTest::keyClick(target ? target : dialog, Qt::Key_Escape);
            } else dialog->reject();
        });
        QTimer picker_timeout;
        picker_timeout.setSingleShot(true);
        QObject::connect(&picker_timeout, &QTimer::timeout, window.get(), [&] {
            if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
            if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) menu->close();
        });
        picker_timeout.start(3000);
        picker_driver.start();
        picker_modal.start();
        QTest::keyClick(attach, Qt::Key_Space);
        QTRY_VERIFY(picker_seen);
        picker_driver.stop();
        picker_modal.stop();
        picker_timeout.stop();
        QVERIFY(source_chosen && picker_seen && picker_named);
        QCOMPARE(body->toPlainText(), QStringLiteral("Keep this draft"));
        QVERIFY(mail->composer()->attachments().isEmpty());
        QCOMPARE(window->focusWidget(), attach);

        window->showPage(CybouPage::Files);
        auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
        RecordingBackend backend;
        model->setApplicationBackend(&backend);
        model->setFeatureAvailability(AllFeatureAvailability());
        model->setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
        auto file = ProtectedFile(QStringLiteral("menu-file"), QStringLiteral("A.txt"));
        auto other = ProtectedFile(QStringLiteral("menu-other"), QStringLiteral("B.txt"));
        CybouFileItem folder;
        folder.id = QStringLiteral("destination"); folder.name = QStringLiteral("Projects"); folder.folder = true;
        model->setFileItems({file, other, folder});
        files->setView(StoragePage::View::Recent);
        auto* table = files->findChild<QTreeWidget*>(QStringLiteral("filesTable"));
        auto* grid = files->findChild<QListWidget*>(QStringLiteral("filesGrid"));
        for (bool tiles : {false, true}) {
            files->setGridMode(tiles);
            QWidget* view = tiles ? static_cast<QWidget*>(grid) : table;
            // Current keyboard item may be outside a retained selection.
            if (tiles) {
                grid->clearSelection();
                grid->item(0)->setSelected(true);
                grid->setCurrentRow(1, QItemSelectionModel::NoUpdate);
            } else {
                table->clearSelection();
                table->topLevelItem(0)->setSelected(true);
                table->setCurrentItem(table->topLevelItem(1), 0, QItemSelectionModel::NoUpdate);
            }
            const QString expected = tiles ? grid->currentItem()->data(Qt::UserRole + 1).toString()
                : table->currentItem()->data(0, Qt::UserRole + 1).toString();
            QVERIFY(focus(view));
            for (bool move : {false, true}) {
                int phase = 0;
                bool menu_chosen = false;
                bool named = !move;
                bool timed_out = false;
                QTimer driver;
                driver.setInterval(10);
                QTimer modal_driver;
                modal_driver.setInterval(10);
                QTimer timeout;
                timeout.setSingleShot(true);
                QObject::connect(&timeout, &QTimer::timeout, window.get(), [&] {
                    timed_out = true;
                    if (auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) dialog->reject();
                    if (auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget())) menu->close();
                });
                QObject::connect(&driver, &QTimer::timeout, window.get(), [&] {
                    auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
                    if (!menu) return;
                    QAction* action = nullptr;
                    for (auto* candidate : menu->actions())
                        if (candidate->text() == StoragePage::tr(move ? "Move" : "Rename")) action = candidate;
                    driver.stop();
                    phase = 1;
                    menu_chosen = choose(menu, action);
                });
                QObject::connect(&modal_driver, &QTimer::timeout, window.get(), [&] {
                    if (phase != 1) return;
                    auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                    if (!dialog) return;
                    phase = 2;
                    modal_driver.stop();
                    if (!activate_dialog(dialog)) { dialog->reject(); named = false; return; }
                    if (move) {
                        auto* combo = dialog->findChild<QComboBox*>();
                        auto* buttons = dialog->findChild<QDialogButtonBox*>();
                        named = combo && combo->accessibleName() == StoragePage::tr("Move to");
                        if (combo && buttons) {
                            combo->setFocus();
                            QTest::keyClick(combo, Qt::Key_End);
                            buttons->button(QDialogButtonBox::Ok)->setFocus();
                            QTest::keyClick(buttons->button(QDialogButtonBox::Ok), Qt::Key_Return);
                        } else dialog->reject();
                    } else {
                        QWidget* target = dialog->focusWidget();
                        QTest::keyClick(target ? target : dialog, Qt::Key_Escape);
                    }
                });
                const int before = backend.file_moves.size();
                driver.start();
                modal_driver.start();
                timeout.start(3000);
                // Coordinates deliberately point away from the current item.
                QContextMenuEvent request{QContextMenuEvent::Keyboard, QPoint{1, 1}, view->mapToGlobal(QPoint{1, 1})};
                QApplication::sendEvent(view, &request);
                driver.stop();
                modal_driver.stop();
                timeout.stop();
                QVERIFY2(menu_chosen && phase == 2 && named && !timed_out,
                    qPrintable(QStringLiteral("language=%1 grid=%2 move=%3 chosen=%4 phase=%5 named=%6 timeout=%7")
                        .arg(language).arg(tiles).arg(move).arg(menu_chosen).arg(phase).arg(named).arg(timed_out)));
                QCOMPARE(backend.file_moves.size(), before + (move ? 1 : 0));
                if (!move) QVERIFY(!backend.commands.contains(QStringLiteral("rename:") + expected));
                if (move) {
                    QCOMPARE(backend.file_moves.last(), qMakePair(expected, folder.id));
                    backend.file_results.last()(CybouCommandState::Failed, QStringLiteral("Save failed"));
                    QVERIFY(model->fileItem(expected)->parent_id.isEmpty());
                }
                QCOMPARE(window->focusWidget(), view);
                QVERIFY(focus(view));
            }
        }
        model->setApplicationBackend(nullptr);
    }
    window->setLanguage(QStringLiteral("en"));
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
    QTRY_COMPARE(notifier->text(), QStringLiteral("Moved to Trash"));
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
    auto* authority_nav = window->findChild<QToolButton*>(
        QStringLiteral("navButton%1").arg(static_cast<int>(CybouPage::NetworkAuthority)));
    QVERIFY(authority_nav);
    QVERIFY(!authority_nav->isHidden());
    QCOMPARE(authority_nav->accessibleName(), QStringLiteral("Autorité centrale"));
    QLabel* authority_state = nullptr;
    for (auto* label : window->findChildren<QLabel*>()) {
        if (label->property("cybouId").toString() == QStringLiteral("authorityFinalizerState")) authority_state = label;
    }
    QVERIFY(authority_state);
    QCOMPARE(authority_state->text(), QStringLiteral("Signataire indisponible"));
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Files));

    const auto* old_page = window->page(CybouPage::Files);
    window->setLanguage(QStringLiteral("en"));
    home = window->findChild<QToolButton*>(QStringLiteral("navButton0"));
    QVERIFY(home);
    QCOMPARE(home->accessibleName(), QStringLiteral("Home"));
    authority_nav = window->findChild<QToolButton*>(
        QStringLiteral("navButton%1").arg(static_cast<int>(CybouPage::NetworkAuthority)));
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
    QTRY_VERIFY(window->notifier()->findChild<QPushButton*>(QStringLiteral("notifierAction"))->isVisible());
    window->notifier()->trigger(); // Undo after durable acknowledgement
    QTRY_COMPARE(model->mailItem(QStringLiteral("m-alina"))->folder, CybouMailFolder::Inbox);
    // Sent mail never lands in Inbox.
    mail->moveMessagesTo({QStringLiteral("m-sent-1")}, EmailPage::View::Inbox);
    QCOMPARE(model->mailItem(QStringLiteral("m-sent-1"))->folder, CybouMailFolder::Sent);
    // Exercise the actual folder viewport event route, rather than calling the
    // move helper. The OS mouse-drag/DPI path still needs separate acceptance.
    auto* folders = mail->findChild<QListWidget*>(QStringLiteral("folderList"));
    QVERIFY(folders);
    QMimeData data;
    data.setData(CybouUi::mailIdsMime(), QByteArrayLiteral("m-dinner"));
    for (const auto view : {EmailPage::View::Trash, EmailPage::View::Archive}) {
        const QPoint pos = folders->visualItemRect(folders->item(static_cast<int>(view))).center();
        QDragEnterEvent enter{pos, Qt::MoveAction, &data, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(folders->viewport(), &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent drop{QPointF{pos}, Qt::MoveAction, &data, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(folders->viewport(), &drop);
        QVERIFY(drop.isAccepted());
        QTRY_COMPARE(model->mailItem(QStringLiteral("m-dinner"))->folder,
            view == EmailPage::View::Trash ? CybouMailFolder::Trash : CybouMailFolder::Archive);
    }
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
    QTRY_VERIFY(window->notifier()->findChild<QPushButton*>(QStringLiteral("notifierAction"))->isVisible());
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

void CybouShellTests::filesDropTargetsRespectIdentityAndCatalog()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    auto file = ProtectedFile(QStringLiteral("file"), QStringLiteral("Report.pdf"));
    auto folder = ProtectedFile(QStringLiteral("folder"), QStringLiteral("Folder"));
    folder.folder = true;
    auto child = folder;
    child.id = QStringLiteral("child");
    child.name = QStringLiteral("Child");
    child.parent_id = folder.id;
    model.setFileItems({file, folder, child});
    StoragePage page{&model};
    page.resize(1200, 800);
    page.show();
    QCoreApplication::processEvents();
    qInfo("Qt drop-event routing: device pixel ratio %.2f; synthetic events, not physical mouse acceptance", page.devicePixelRatioF());
    auto* table = page.findChild<QTreeWidget*>(QStringLiteral("filesTable"));
    auto* grid = page.findChild<QListWidget*>(QStringLiteral("filesGrid"));
    auto* nav = page.findChild<QListWidget*>(QStringLiteral("folderList"));
    QVERIFY(table && grid && nav);
    QTemporaryFile downloaded;
    QVERIFY(downloaded.open());
    QMimeData data;
    data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("file\nfile"));
    data.setUrls({QUrl::fromLocalFile(downloaded.fileName())});
    // These are delivered Qt events, not physical OS mouse-drag acceptance.
    const auto enter = [](QWidget* target, const QPoint& pos, QMimeData& mime) {
        QDragEnterEvent event{pos, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(target, &event);
        return event.isAccepted();
    };
    const auto drop = [](QWidget* target, const QPoint& pos, QMimeData& mime) {
        QDropEvent event{QPointF{pos}, Qt::CopyAction | Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier};
        QApplication::sendEvent(target, &event);
        return event.isAccepted();
    };
    const auto position = [&](bool tiles, const QString& id) -> QPoint {
        if (tiles) {
            for (int i = 0; i < grid->count(); ++i)
                if (grid->item(i)->data(Qt::UserRole + 1).toString() == id) return grid->visualItemRect(grid->item(i)).center();
        } else {
            for (int i = 0; i < table->topLevelItemCount(); ++i)
                if (table->topLevelItem(i)->data(0, Qt::UserRole + 1).toString() == id)
                    return table->visualItemRect(table->topLevelItem(i)).center();
        }
        return {-1, -1};
    };
    for (const bool tiles : {false, true}) {
        page.setGridMode(tiles);
        page.openFolder({});
        QCoreApplication::processEvents();
        QWidget* viewport = tiles ? grid->viewport() : table->viewport();
        const QPoint folder_pos = position(tiles, folder.id);
        QVERIFY(folder_pos.x() >= 0);
        backend.commands.clear();
        const auto move_count = backend.file_moves.size();
        QVERIFY(enter(viewport, folder_pos, data));
        QVERIFY(drop(viewport, folder_pos, data));
        QCOMPARE(backend.file_moves.size(), move_count + 1); // duplicate IDs are one command
        QCOMPARE(backend.file_moves.last(), qMakePair(file.id, folder.id));
        QVERIFY(backend.commands.filter(QStringLiteral("upload:")).isEmpty());
        QMimeData external;
        external.setUrls({QUrl::fromLocalFile(downloaded.fileName())});
        QVERIFY(enter(viewport, folder_pos, external));
        QVERIFY(drop(viewport, folder_pos, external));
        QTRY_COMPARE(backend.commands.filter(QStringLiteral("upload:")).size(), 1);
        page.openFolder(folder.id);
        QCoreApplication::processEvents();
        const QPoint child_pos = position(tiles, child.id);
        QVERIFY(child_pos.x() >= 0);
        data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("folder"));
        backend.commands.clear();
        QVERIFY(!enter(viewport, child_pos, data)); // cycle, despite a valid downloaded URL
        QVERIFY(!drop(viewport, child_pos, data));
        QVERIFY(backend.commands.isEmpty());
        data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("missing"));
        QVERIFY(!enter(viewport, child_pos, data));
        data.setData(CybouUi::fileIdsMime(), QByteArray{});
        QVERIFY(!enter(viewport, child_pos, data));
        data.setData(CybouUi::fileIdsMime(), QByteArrayLiteral("file"));
        QVERIFY(enter(&page, QPoint{5, 5}, data));
        QVERIFY(drop(&page, QPoint{5, 5}, data)); // blank page uses current folder
        QCOMPARE(backend.file_moves.last(), qMakePair(file.id, folder.id));
    }
    const QPoint trash_pos = nav->visualItemRect(nav->item(static_cast<int>(StoragePage::View::Trash))).center();
    QVERIFY(enter(nav->viewport(), trash_pos, data));
    QVERIFY(drop(nav->viewport(), trash_pos, data));
    QCOMPARE(backend.commands.last(), QStringLiteral("trash:file"));
    QVERIFY(!page.moveFilesTo({file.id}, QStringLiteral("missing")));
    QVERIFY(!page.moveFilesTo({file.id}, file.id));
    QMimeData remote;
    remote.setUrls({QUrl{QStringLiteral("https://example.invalid/report.pdf")}});
    QVERIFY(!enter(&page, QPoint{5, 5}, remote));
    // An accepted drag must be revalidated when Identity locks before the drop.
    QVERIFY(enter(nav->viewport(), trash_pos, data));
    backend.commands.clear();
    model.requestLockVault();
    const auto after_lock = backend.commands;
    QVERIFY(!drop(nav->viewport(), trash_pos, data));
    QVERIFY(!enter(&page, QPoint{5, 5}, data));
    QVERIFY(!page.moveFilesTo({file.id}, {}));
    QCOMPARE(backend.commands, after_lock);
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

void CybouShellTests::runtimeRetiresStateFromAnotherNetwork()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ScopedEnvironment p2p_host{"CYBOU_DEV_P2P_HOST", "127.0.0.1"};
    ScopedEnvironment p2p_port{"CYBOU_DEV_P2P_PORT", "1"};
    ScopedUnsetEnvironment finalizer_mode{"CYBOU_DEV_FINALIZER"};
    QVERIFY(WriteForeignNetworkState(directory.path()));
    // Unrelated files in the data directory are never moved.
    QFile unrelated{directory.filePath(QStringLiteral("unrelated.vhdx"))};
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    unrelated.write("keep");
    unrelated.close();

    CybouDesktopModel model{QStringLiteral("CYBOU-DEV")};
    CybouDesktopController controller{&model, directory.path().toStdString()};
    QSignalSpy failures{&controller, &CybouDesktopController::startupFailed};
    controller.start();
    // Cutover: the foreign network-bound data is moved aside and the node starts clean.
    QCOMPARE(failures.count(), 0);
    QVERIFY(model.status().node_running);
    const QDir retired{directory.filePath(QStringLiteral("retired"))};
    const auto runs = retired.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCOMPARE(runs.size(), 1);
    const QDir run{retired.filePath(runs.first())};
    QVERIFY(run.exists(QStringLiteral("cybou_state")));
    QVERIFY(!run.exists(QStringLiteral("unrelated.vhdx")));
    QVERIFY(QFile::exists(directory.filePath(QStringLiteral("unrelated.vhdx"))));
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
    // Unknown items send nothing; apparent no-ops must reach the worker because
    // an earlier queued change may not yet be reflected in this projection.
    backend.commands.clear();
    model.requestRenameFile(QStringLiteral("missing"), QStringLiteral("x"));
    QVERIFY(backend.commands.isEmpty());
    model.requestRenameFile(QStringLiteral("f1"), QStringLiteral("report.pdf"));
    QCOMPARE(backend.commands, QStringList{QStringLiteral("rename:f1")});

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

void CybouShellTests::activityRefreshPreservesRows()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    window->showPage(CybouPage::Home);
    window->show();
    auto* home = static_cast<HomePage*>(window->page(CybouPage::Home));
    const auto now = QDateTime::currentDateTime();
    QVector<CybouActivityItem> items{{CybouActivityKind::FileUploaded, QStringLiteral("Original"), {}, now, QStringLiteral("event-a")},
        {CybouActivityKind::MailReceived, QStringLiteral("Message"), {}, now.addSecs(-30), QStringLiteral("event-b")}};
    model->setActivity(items);
    QPointer<QFrame> row;
    QTRY_VERIFY((row = FindById<QFrame>(home, QStringLiteral("event-a"))) != nullptr);
    QSignalSpy activity_changed{model, &CybouDesktopModel::activityChanged};
    model->setActivity(items);
    QCOMPARE(activity_changed.count(), 0);
    items[0].title = QStringLiteral("Updated");
    items[0].subtitle = QStringLiteral("Details arrived");
    items.prepend({CybouActivityKind::PaymentReceived, QStringLiteral("New payment"), {}, now.addSecs(1), QStringLiteral("event-c")});
    model->setActivity(items);
    QCOMPARE(FindById<QFrame>(home, QStringLiteral("event-a")), row.data());
    QCOMPARE(row->findChild<QLabel*>(QStringLiteral("rowTitle"))->text(), QStringLiteral("Updated"));
    QVERIFY(row->findChild<QLabel*>(QStringLiteral("rowSub"))->isVisible());
    auto* refresh = FindById<QToolButton>(home, QStringLiteral("activityRefresh"));
    QVERIFY(refresh && refresh->isEnabled());
    refresh->click();
    QVERIFY(model->applicationRefreshing());
    QVERIFY(!refresh->isEnabled());
    QTRY_VERIFY(model->lastApplicationRefresh().isValid());
    QCOMPARE(FindById<QFrame>(home, QStringLiteral("event-a")), row.data());
    for (int i = 0; i < 10; ++i) items.append({CybouActivityKind::FileUploaded,
        QStringLiteral("Another file %1").arg(i), {}, now.addSecs(-60 - i), QStringLiteral("extra-%1").arg(i)});
    model->setActivity(items);
    window->resize(1040, 720);
    auto* scroll = home->findChild<QScrollArea*>(QStringLiteral("homeScroll"));
    QVERIFY(scroll);
    QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
    QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
    model->requestLockVault();
    QVERIFY(model->activity().isEmpty());
    QVERIFY(!model->lastApplicationRefresh().isValid());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(row.isNull());
}

void CybouShellTests::localRefreshRejectsStaleReplies()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    QVERIFY(model.requestApplicationRefresh());
    const auto first = backend.refresh_result;
    QVERIFY(!model.requestApplicationRefresh());
    QCOMPARE(backend.commands.count(QStringLiteral("refresh")), 1);
    first(CybouCommandState::Failed, QStringLiteral("snapshot failed"));
    QTRY_VERIFY(!model.applicationRefreshing());
    QVERIFY(!model.lastApplicationRefresh().isValid());
    QCOMPARE(model.applicationRefreshError(), QStringLiteral("snapshot failed"));
    QVERIFY(model.requestApplicationRefresh());
    const auto second = backend.refresh_result;
    first(CybouCommandState::Committed, {});
    QCoreApplication::processEvents();
    QVERIFY(model.applicationRefreshing());
    QVERIFY(!model.lastApplicationRefresh().isValid());
    second(CybouCommandState::Committed, {});
    QTRY_VERIFY(model.lastApplicationRefresh().isValid());
    bool interrupted = false;
    QVERIFY(model.requestApplicationRefresh([&](bool ok, const QString&) { interrupted = !ok; }));
    const auto abandoned = backend.refresh_result;
    backend.available = false;
    Q_EMIT backend.availabilityChanged();
    QVERIFY(interrupted);
    QVERIFY(!model.applicationRefreshing());
    abandoned(CybouCommandState::Committed, {});
    QCoreApplication::processEvents();
    QVERIFY(!model.applicationRefreshError().isEmpty());
    backend.available = true;
    Q_EMIT backend.availabilityChanged();
    QVERIFY(model.requestApplicationRefresh());
    const auto late = backend.refresh_result;
    model.requestLockVault();
    late(CybouCommandState::Committed, {});
    QCoreApplication::processEvents();
    QVERIFY(!model.applicationRefreshing());
    QVERIFY(!model.lastApplicationRefresh().isValid());
}

void CybouShellTests::localMailCommandsWaitForCommit()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    CybouMailItem mail;
    mail.id = QStringLiteral("mail-1");
    model.setMailItems({mail});
    int completed = 0;
    model.requestMoveMail(mail.id, CybouMailFolder::Archive, [&](bool ok, const QString&) { QVERIFY(!ok); ++completed; });
    QCOMPARE(completed, 0);
    QCOMPARE(model.applicationTasks().last().state, CybouCommandState::Queued);
    backend.move_results.last()(CybouCommandState::Running, {});
    QTRY_COMPARE(model.applicationTasks().last().state, CybouCommandState::Running);
    QCOMPARE(completed, 0);
    backend.move_results.last()(CybouCommandState::Failed, QStringLiteral("DB failed"));
    QTRY_COMPARE(completed, 1);
    QCOMPARE(model.mailItem(mail.id)->folder, CybouMailFolder::Inbox);
    MailReader reader{&model};
    reader.showMessage(mail.id);
    reader.show();
    int closed = 0;
    reader.onBack = [&] { ++closed; };
    auto* archive = FindById<QToolButton>(&reader, QStringLiteral("readerArchive"));
    QVERIFY(archive);
    archive->click();
    QCOMPARE(closed, 0);
    QVERIFY(!archive->isEnabled());
    backend.move_results.last()(CybouCommandState::Failed, QStringLiteral("DB failed"));
    QTRY_VERIFY(archive->isEnabled());
    QCOMPARE(closed, 0);
    archive->click();
    backend.move_results.last()(CybouCommandState::Committed, {});
    QTRY_COMPARE(closed, 1);
    QCOMPARE(model.mailItem(mail.id)->folder, CybouMailFolder::Archive);
    // An Undo-like request to the currently displayed folder still queues:
    // an earlier move could already be waiting on the worker.
    const auto command_count = backend.move_results.size();
    model.requestMoveMail(mail.id, CybouMailFolder::Archive);
    QCOMPARE(backend.move_results.size(), command_count + 1);
    const auto late = backend.move_results.last();
    model.requestLockVault();
    late(CybouCommandState::Committed, {});
    QCoreApplication::processEvents();
    QVERIFY(model.applicationTasks().isEmpty());
    QVERIFY(model.mailItems().isEmpty());
    // A result listener can lock synchronously while the task signal is emitted.
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    model.setMailItems({mail});
    bool stale_done = false;
    model.requestMoveMail(mail.id, CybouMailFolder::Archive, [&](bool, const QString&) { stale_done = true; });
    connect(&model, &CybouDesktopModel::applicationTasksChanged, &model, [&] {
        if (!model.applicationTasks().isEmpty() && model.applicationTasks().last().state == CybouCommandState::Committed)
            model.requestLockVault();
    });
    backend.move_results.last()(CybouCommandState::Committed, {});
    QTRY_VERIFY(model.mailItems().isEmpty());
    QVERIFY(!stale_done);
}

void CybouShellTests::mailDeletionWaitsForDurableCommit()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    CybouMailItem mail;
    mail.id = QStringLiteral("mail-delete");
    mail.folder = CybouMailFolder::Trash;
    mail.body = QStringLiteral("Retain on database failure");
    CybouMailItem draft;
    draft.id = QStringLiteral("draft-delete");
    draft.draft = true;
    draft.folder = CybouMailFolder::Drafts;
    draft.body = QStringLiteral("Keep this draft");
    model.setMailItems({mail, draft});
    int completed = 0;
    model.requestDeleteMailForever({mail.id}, [&](bool ok, const QString&) { QVERIFY(!ok); ++completed; });
    QVERIFY(model.mailItem(mail.id));
    QCOMPARE(completed, 0);
    backend.delete_results.last()(CybouCommandState::Running, {});
    QTRY_COMPARE(model.applicationTasks().last().state, CybouCommandState::Running);
    QVERIFY(model.mailItem(mail.id));
    backend.delete_results.last()(CybouCommandState::Failed, QStringLiteral("DB failed"));
    QTRY_COMPARE(completed, 1);
    QCOMPARE(model.mailItem(mail.id)->body, mail.body);
    model.requestDeleteMailForever({mail.id});
    backend.delete_results.last()(CybouCommandState::Committed, {});
    QTRY_VERIFY(!model.mailItem(mail.id));

    auto other = mail;
    other.id = QStringLiteral("mail-retained");
    model.setMailItems({mail, other, draft});
    EmailPage page{&model, [] {}};
    page.setView(EmailPage::View::Trash);
    page.openMessage(mail.id);
    QSignalSpy notices{&model, &CybouDesktopModel::notificationRequested};
    QTimer::singleShot(0, &page, [&] {
        auto* question = page.findChild<QMessageBox*>();
        QVERIFY(question);
        question->button(QMessageBox::Yes)->click();
    });
    page.deleteForever({mail.id, other.id});
    QVERIFY(model.mailItem(mail.id) && model.mailItem(other.id));
    QVERIFY(page.isDetailOpen());
    QCOMPARE(notices.last().first().toString(), QStringLiteral("Deleting messages…"));
    backend.delete_results.at(backend.delete_results.size()-2)(CybouCommandState::Committed, {});
    backend.delete_results.last()(CybouCommandState::Failed, QStringLiteral("DB failed"));
    QTRY_VERIFY(notices.last().first().toString().contains(QStringLiteral("1 messages deleted; 1 could not be deleted")));
    QVERIFY(!model.mailItem(mail.id));
    QVERIFY(model.mailItem(other.id));

    MailCompose composer{&model};
    composer.start(draft);
    int closed = 0;
    composer.onClosed = [&] { ++closed; };
    auto* body = composer.findChild<QTextEdit*>(QStringLiteral("composeBody"));
    auto* discard = FindById<QPushButton>(&composer, QStringLiteral("composeDiscard"));
    QVERIFY(body && discard);
    discard->click();
    QCOMPARE(closed, 0);
    QVERIFY(!body->isEnabled());
    QVERIFY(model.mailItem(draft.id));
    backend.delete_results.last()(CybouCommandState::Failed, QStringLiteral("Delete failed"));
    QTRY_VERIFY(body->isEnabled());
    QCOMPARE(closed, 0);
    QCOMPARE(body->toPlainText(), draft.body);
    // An in-flight autosave reply must not clear or re-enable a discard.
    body->setPlainText(QStringLiteral("Newest unsaved text"));
    QTRY_VERIFY_WITH_TIMEOUT(backend.draft_results.contains(draft.id), 2000);
    discard->click();
    backend.draft_results.value(draft.id)(CybouCommandState::Committed, {});
    QCoreApplication::processEvents();
    QCOMPARE(closed, 0);
    QVERIFY(!body->isEnabled());
    QCOMPARE(body->toPlainText(), QStringLiteral("Newest unsaved text"));
    // A shell rebuild can follow the same pending deletion by task identity.
    MailCompose rebuilt{&model};
    rebuilt.start(composer.snapshotForRebuild());
    int rebuilt_closed = 0;
    rebuilt.onClosed = [&] { ++rebuilt_closed; };
    QVERIFY(!rebuilt.findChild<QTextEdit*>(QStringLiteral("composeBody"))->isEnabled());
    backend.delete_results.last()(CybouCommandState::Committed, {});
    QTRY_COMPARE(closed, 1);
    QTRY_COMPARE(rebuilt_closed, 1);
    QVERIFY(body->toPlainText().isEmpty());
    QVERIFY(!model.mailItem(draft.id));
    model.setMailItems({mail});
    bool stale = false;
    model.requestDeleteMailForever({mail.id}, [&](bool, const QString&) { stale = true; });
    const auto late = backend.delete_results.last();
    model.requestLockVault();
    late(CybouCommandState::Committed, {});
    QCoreApplication::processEvents();
    QVERIFY(!stale);
    QVERIFY(model.mailItems().isEmpty());

    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    model.setMailItems({mail});
    bool reentrant_done = false;
    model.requestDeleteMailForever({mail.id}, [&](bool, const QString&) { reentrant_done = true; });
    connect(&model, &CybouDesktopModel::mailChanged, &model, [&] {
        if (!model.mailItem(mail.id)) model.requestLockVault();
    });
    backend.delete_results.last()(CybouCommandState::Committed, {});
    QTRY_VERIFY(model.mailItems().isEmpty());
    QVERIFY(!reentrant_done);
}

void CybouShellTests::fileChangesAcknowledgeAndOrderUndo()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    auto first = ProtectedFile(QStringLiteral("first"), QStringLiteral("First.pdf"));
    auto second = ProtectedFile(QStringLiteral("second"), QStringLiteral("Second.pdf"));
    auto folder = ProtectedFile(QStringLiteral("folder"), QStringLiteral("Folder"));
    folder.folder = true;
    model.setFileItems({first, second, folder});
    StoragePage page{&model};
    std::function<void()> undo;
    QString notice;
    connect(&model, &CybouDesktopModel::notificationRequested, &model,
        [&](const QString& text, const QString&, std::function<void()> action) { notice = text; undo = std::move(action); });
    QVERIFY(page.moveFilesTo({first.id,second.id},QStringLiteral("folder")));
    QCOMPARE(notice,QStringLiteral("Saving file changes…"));
    QVERIFY(!undo);
    QCOMPARE(model.fileItem(first.id)->parent_id,QString{});
    backend.file_results[0](CybouCommandState::Committed,{});
    backend.file_results[1](CybouCommandState::Failed,QStringLiteral("DB failed"));
    QTRY_VERIFY(static_cast<bool>(undo));
    QCOMPARE(notice,QStringLiteral("1 changes saved; 1 failed. Try again."));
    // The forward intent is saved, but its projection has not arrived yet.
    undo();
    QCOMPARE(backend.file_moves.size(),3);
    QCOMPARE(backend.file_moves.last(),qMakePair(first.id,QString{}));
    const auto retained_undo=undo;
    int completed=0;
    model.requestRenameFile(first.id,QStringLiteral("Renamed.pdf"),[&](bool ok,const QString&) { QVERIFY(!ok); ++completed; });
    backend.file_results.last()(CybouCommandState::Running,{});
    QCoreApplication::processEvents();
    QCOMPARE(completed,0);
    backend.file_results.last()(CybouCommandState::Failed,QStringLiteral("DB failed"));
    QTRY_COMPARE(completed,1);
    QCOMPARE(model.fileItem(first.id)->name,first.name);
    bool stale=false;
    model.requestRestoreFile(first.id,[&](bool,const QString&) { stale=true; });
    const auto late=backend.file_results.last();
    model.requestLockVault();
    late(CybouCommandState::Committed,{});
    model.setIdentityState(CybouIdentityState::Active,QStringLiteral("acct"),1);
    model.setFileItems({first,second});
    QCoreApplication::processEvents();
    QVERIFY(!stale);
    const auto count=backend.file_moves.size();
    retained_undo();
    QCOMPARE(backend.file_moves.size(),count);
    model.requestLockVault();
    bool unavailable = false;
    model.requestMoveFile(first.id, {}, [&](bool ok, const QString& error) {
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
        unavailable = true;
    });
    QTRY_VERIFY(unavailable);
}

void CybouShellTests::composerKeepsTextOnSaveAndSendFailure()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    RecordingBackend backend;
    model.setApplicationBackend(&backend);
    model.setFeatureAvailability(AllFeatureAvailability());
    model.setIdentityState(CybouIdentityState::Active, QStringLiteral("acct"), 1);
    MailCompose composer{&model};
    composer.start();
    auto* to = composer.findChild<QLineEdit*>(QStringLiteral("recipientEdit"));
    auto* body = composer.findChild<QTextEdit*>(QStringLiteral("composeBody"));
    auto* close = FindById<QToolButton>(&composer, QStringLiteral("keepDraftAndClose"));
    QVERIFY(to && body && close);
    int closed = 0;
    composer.onClosed = [&] { ++closed; };
    to->setText(QStringLiteral("alice.cybou"));
    body->setPlainText(QStringLiteral("Never lose this text"));
    QTRY_COMPARE_WITH_TIMEOUT(backend.draft_results.size(), 1, 2000); // autosave
    const QString draft_id = backend.draft_results.constBegin().key();
    close->click(); // waits for the in-flight save, does not clear
    QCOMPARE(closed, 0);
    backend.draft_results.value(draft_id)(CybouCommandState::Failed, QStringLiteral("Save failed"));
    QTRY_VERIFY(body->isEnabled());
    QCOMPARE(body->toPlainText(), QStringLiteral("Never lose this text"));
    QCOMPARE(closed, 0);
    close->click();
    backend.draft_results.value(draft_id)(CybouCommandState::Committed, {});
    QTRY_COMPARE(closed, 1);
    QVERIFY(body->toPlainText().isEmpty());

    composer.start();
    to->setText(QStringLiteral("alice.cybou"));
    body->setPlainText(QStringLiteral("Keep on send failure"));
    composer.findChild<QPushButton*>(QStringLiteral("sendButton"))->click();
    QVERIFY(backend.send_result);
    QVERIFY(!backend.send_draft_id.isEmpty());
    QVERIFY(!backend.commands.contains(QStringLiteral("deleteMail:") + backend.send_draft_id));
    backend.send_result(CybouCommandState::Failed, QStringLiteral("Recipient unavailable"));
    QTRY_VERIFY(body->isEnabled());
    QCOMPARE(body->toPlainText(), QStringLiteral("Keep on send failure"));
    QVERIFY(model.applicationTasks().last().state == CybouCommandState::Failed);
    // A rebuilt shell follows the model's pending handoff, not a dead widget callback.
    auto first = std::make_unique<MailCompose>(&model);
    first->start();
    first->findChild<QLineEdit*>(QStringLiteral("recipientEdit"))->setText(QStringLiteral("alice.cybou"));
    first->findChild<QTextEdit*>(QStringLiteral("composeBody"))->setPlainText(QStringLiteral("Survive rebuild while sending"));
    first->findChild<QPushButton*>(QStringLiteral("sendButton"))->click();
    const auto preserved = first->snapshotForRebuild();
    first.reset();
    MailCompose rebuilt{&model};
    rebuilt.start(preserved);
    QVERIFY(!rebuilt.findChild<QTextEdit*>(QStringLiteral("composeBody"))->isEnabled());
    int accepted = 0;
    rebuilt.onSent = [&](const QString&) { ++accepted; };
    backend.send_result(CybouCommandState::Committed, {});
    QTRY_COMPARE(accepted, 1);
    QVERIFY(rebuilt.findChild<QTextEdit*>(QStringLiteral("composeBody"))->toPlainText().isEmpty());

}

void CybouShellTests::folderImportIsCancellableAndPreservesStructure()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    model.setFileItems({});
    StoragePage page{&model};
    page.resize(1040, 720);
    page.show();
    QTemporaryDir source;
    const QString root = source.filePath(QStringLiteral("Import"));
    QVERIFY(QDir{}.mkpath(root + QStringLiteral("/Nested/Empty")));
    for (const auto& name : {QStringLiteral("alpha.txt"), QStringLiteral(".hidden"), QStringLiteral("Nested/beta.txt")}) {
        QFile file{root + QStringLiteral("/") + name};
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("hello"), qint64{5});
    }
    QSignalSpy notifications{&model, &CybouDesktopModel::notificationRequested};
    const auto cancel = [&] {
        auto* dialog = page.findChild<QProgressDialog*>();
        QVERIFY(dialog);
        auto* button = dialog->findChild<QPushButton*>();
        QVERIFY(button);
        button->click();
    };
    page.uploadFiles({root});
    QVERIFY(model.fileItems().isEmpty()); // Discovery never stages synchronously.
    auto* progress = page.findChild<QProgressDialog*>();
    QVERIFY(progress && progress->isVisible());
    const QString capture = qEnvironmentVariable("CYBOU_IMPORT_SCREENSHOT");
    if (!capture.isEmpty()) QVERIFY(progress->grab().save(capture));
    cancel();
    QTest::qWait(150);
    QVERIFY(model.fileItems().isEmpty());
    QVERIFY(notifications.last().first().toString().contains(QStringLiteral("0 items")));

    QStringList too_many;
    for (int i{0}; i < 10001; ++i) too_many.append(root);
    page.uploadFiles(too_many);
    QVERIFY(model.fileItems().isEmpty());
    QVERIFY(notifications.last().first().toString().contains(QStringLiteral("10,000")));

    page.uploadFiles({root});
    // A second import does not start another tree while the first owns the task.
    page.uploadFiles({root});
    QTRY_COMPARE(model.fileItems().size(), 6);
    QTRY_VERIFY(page.findChild<QProgressDialog*>() == nullptr);
    const auto named = [&](const QString& name) -> const CybouFileItem* {
        for (const auto& file : model.fileItems()) if (file.name == name) return model.fileItem(file.id);
        return nullptr;
    };
    QVERIFY(named(QStringLiteral("Import")));
    QVERIFY(named(QStringLiteral("Nested")));
    QVERIFY(named(QStringLiteral("Empty")));
    QCOMPARE(named(QStringLiteral("Nested"))->parent_id, named(QStringLiteral("Import"))->id);
    QCOMPARE(named(QStringLiteral("Empty"))->parent_id, named(QStringLiteral("Nested"))->id);
    QCOMPARE(named(QStringLiteral("beta.txt"))->parent_id, named(QStringLiteral("Nested"))->id);
    QVERIFY(named(QStringLiteral(".hidden"))); // Do not silently omit hidden content.
    QCOMPARE(named(QStringLiteral("alpha.txt"))->logical_size, quint64{5});

    const int baseline = model.fileItems().size();
    // A discovery error rejects the whole plan before any folder is created.
    page.uploadFiles({root, source.filePath(QStringLiteral("missing"))});
    QTRY_VERIFY(page.findChild<QProgressDialog*>() == nullptr);
    QCOMPARE(model.fileItems().size(), baseline);
    QVERIFY(notifications.last().first().toString().contains(QStringLiteral("No items")));

    bool cancelled{false};
    auto stop = QObject::connect(&model, &CybouDesktopModel::filesChanged, &page, [&] {
        if (!cancelled && model.fileItems().size() > baseline) { cancelled = true; cancel(); }
    });
    page.uploadFiles({root});
    QTRY_VERIFY(cancelled);
    QTest::qWait(150);
    QCOMPARE(model.fileItems().size(), baseline + 1); // The accepted root stays; descendants are stopped.
    QVERIFY(notifications.last().first().toString().contains(QStringLiteral("1 items")));
    QObject::disconnect(stop);

    bool locked{false};
    auto lock = QObject::connect(&model, &CybouDesktopModel::filesChanged, &page, [&] {
        if (!locked && model.fileItems().size() > baseline + 1) {
            locked = true;
            model.setIdentityState(CybouIdentityState::Locked, model.status().account_id, 1);
        }
    });
    page.uploadFiles({root});
    QTRY_VERIFY(locked);
    QTRY_VERIFY(page.findChild<QProgressDialog*>() == nullptr);
    QTest::qWait(150);
    QVERIFY(model.fileItems().isEmpty());
    QObject::disconnect(lock);

    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    model.setFileItems({});
    auto* temporary_page = new StoragePage{&model};
    temporary_page->uploadFiles({root});
    delete temporary_page; // No GUI-thread join and no late staging.
    QTest::qWait(150);
    QVERIFY(model.fileItems().isEmpty());
}

void CybouShellTests::largeFileCatalogUpdatesInPlace()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    const int requested = qEnvironmentVariableIntValue("CYBOU_TEST_CATALOG_SIZE");
    const int count = requested > 0 ? std::clamp(requested, 2000, 10000) : 2000;
    QVector<CybouFileItem> catalog;
    for (int i{0}; i < count; ++i) {
        CybouFileItem file;
        file.id = QString::number(i);
        file.name = QStringLiteral("Document %1.pdf").arg(i, 4, 10, QLatin1Char('0'));
        file.state = CybouContentState::Securing;
        file.operation_state = CybouOperationState::Finalized;
        file.min_remote_replicas = 1;
        file.remote_replica_target = 2;
        catalog.append(file);
    }
    model.setFileItems(catalog);
    QElapsedTimer load;
    load.start();
    StoragePage page{&model};
    const qint64 load_ms = load.elapsed();
    page.resize(1040, 720);
    page.show();
    auto* table = page.findChild<QTreeWidget*>(QStringLiteral("filesTable"));
    QVERIFY(table);
    auto* row = table->topLevelItem(1000);
    QVERIFY(!table->itemWidget(row, 3));
    table->setCurrentItem(row);
    QVector<qint64> times;
    int steps{0};
    bool finished{false};
    QTimer ticks;
    ticks.setInterval(0);
    QObject::connect(&ticks, &QTimer::timeout, &page, [&] {
        QElapsedTimer update;
        update.start();
        catalog[1000].min_remote_replicas = steps % 2;
        model.setFileItems(catalog);
        times.append(update.elapsed());
        if (++steps == 10) { ticks.stop(); finished = true; }
    });
    ticks.start();
    QTRY_VERIFY_WITH_TIMEOUT(finished, 10000);
    QCOMPARE(table->topLevelItemCount(), count);
    QCOMPARE(table->topLevelItem(1000), row);
    QVERIFY(!table->itemWidget(row, 3));
    QCOMPARE(table->currentItem(), row);
    QVERIFY(row->isSelected());
    QCOMPARE(page.findChildren<QLabel*>(QStringLiteral("stateChip")).size(), 0);
    QVERIFY(!row->data(3, Qt::AccessibleTextRole).toString().isEmpty());
    std::sort(times.begin(), times.end());
    qInfo("Synthetic %d-file catalog: initial construction %lld ms; 10 one-file updates median %lld ms, max %lld ms (not a live storage benchmark)",
        count, static_cast<long long>(load_ms), static_cast<long long>(times[times.size() / 2]), static_cast<long long>(times.last()));
}

void CybouShellTests::mailReaderKeepsContextAndClearsOnLock()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    CybouMailItem message;
    message.id = QStringLiteral("reader-one");
    message.subject = QStringLiteral("<b>literal subject</b>");
    message.body = QStringLiteral("<b>literal body</b>\n") + QStringLiteral("A long readable line.\n").repeated(200);
    message.from_name = QStringLiteral("alice.cybou");
    message.to_name = QStringLiteral("stan.cybou");
    message.state = CybouContentState::Received;
    message.operation_state = CybouOperationState::Finalized;
    CybouAttachmentItem attachment;
    attachment.id = QStringLiteral("attachment-one");
    attachment.name = QStringLiteral("report.pdf");
    attachment.state = CybouContentState::Received;
    message.attachments.append(attachment);
    auto unrelated = message;
    unrelated.id = QStringLiteral("reader-two");
    unrelated.body = QStringLiteral("Short second message");
    unrelated.attachments.clear();
    model.setMailItems({message, unrelated});
    MailReader reader{&model};
    reader.resize(680, 720);
    reader.showMessage(message.id);
    reader.show();
    auto* scroll = reader.findChild<QScrollArea*>(QStringLiteral("readerScroll"));
    auto* body = reader.findChild<QLabel*>(QStringLiteral("readerBody"));
    QVERIFY(scroll && body);
    QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 100);
    QPointer<QPushButton> download;
    QTRY_VERIFY((download = FindById<QPushButton>(&reader, QStringLiteral("downloadAttachment"))) != nullptr);
    body->setSelection(0, 3);
    QCOMPARE(body->selectedText(), QStringLiteral("<b>"));
    scroll->verticalScrollBar()->setValue(100);
    unrelated.subject = QStringLiteral("Unrelated update");
    model.setMailItems({message, unrelated});
    QCOMPARE(FindById<QPushButton>(&reader, QStringLiteral("downloadAttachment")), download.data());
    QCOMPARE(body->selectedText(), QStringLiteral("<b>"));
    QCOMPARE(scroll->verticalScrollBar()->value(), 100);
    message.starred = true;
    model.setMailItems({message, unrelated});
    QCOMPARE(FindById<QPushButton>(&reader, QStringLiteral("downloadAttachment")), download.data());
    QCOMPARE(body->selectedText(), QStringLiteral("<b>"));
    message.attachments[0].retrieval = CybouRetrievalState::Downloading;
    model.setMailItems({message, unrelated});
    QTRY_VERIFY(download.isNull());
    QTRY_VERIFY(FindById<QPushButton>(&reader, QStringLiteral("downloadAttachment")) != nullptr);
    QVERIFY(!FindById<QPushButton>(&reader, QStringLiteral("downloadAttachment"))->isEnabled());
    QCOMPARE(body->selectedText(), QStringLiteral("<b>"));
    QCOMPARE(scroll->verticalScrollBar()->value(), 100);
    reader.showMessage(unrelated.id);
    QTRY_COMPARE(scroll->verticalScrollBar()->value(), 0);
    QVERIFY(body->selectedText().isEmpty());
    reader.showMessage(message.id);
    bool details_survived{false};
    QTimer::singleShot(0, &reader, [&] {
        auto* dialog = reader.findChild<QDialog*>(QStringLiteral("mailSecurityDetails"));
        unrelated.starred = true;
        model.setMailItems({message, unrelated});
        details_survived = dialog && dialog->isVisible();
        model.setIdentityState(CybouIdentityState::Locked, model.status().account_id, 1);
    });
    reader.showSecurityDetails();
    QVERIFY(details_survived);
    QVERIFY(reader.messageId().isEmpty());
    QVERIFY(body->text().isEmpty());
    QTRY_VERIFY(FindById<QPushButton>(&reader, QStringLiteral("downloadAttachment")) == nullptr);
}

void CybouShellTests::largeMailboxPresentationProfile()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    const int requested = qEnvironmentVariableIntValue("CYBOU_TEST_MAILBOX_SIZE");
    const int count = requested > 0 ? std::clamp(requested, 2000, 10000) : 2000;
    const auto now = QDateTime::currentDateTime();
    QVector<CybouMailItem> mail;
    mail.reserve(count);
    for (int i = 0; i < count; ++i) {
        CybouMailItem message;
        message.id = QStringLiteral("mail-%1").arg(i);
        message.subject = QStringLiteral("Mailbox profile subject %1").arg(i);
        message.preview = QStringLiteral("A locally indexed encrypted message preview.");
        message.from_name = QStringLiteral("alice.cybou");
        message.to_name = QStringLiteral("bob.cybou");
        message.time = now.addSecs(-i * 60);
        message.state = CybouContentState::Received;
        message.unread = i % 3 == 0;
        message.starred = i % 7 == 0;
        if (i % 5 == 0) message.attachments.append(CybouAttachmentItem{});
        mail.append(message);
    }
    model.setMailItems(mail);
    QElapsedTimer timer;
    timer.start();
    EmailPage page{&model, {}};
    const qint64 construction = timer.elapsed();
    page.resize(1040, 720);
    page.show();
    QCoreApplication::processEvents();
    auto* list = page.findChild<QListWidget*>(QStringLiteral("messageList"));
    QVERIFY(list);
    const auto first = list->viewport()->grab();
    QVERIFY(!first.isNull());
    const qint64 first_render = timer.elapsed();
    QCOMPARE(list->count(), count);
    auto* retained = list->item(1000);
    list->setCurrentItem(retained);
    list->scrollToItem(retained, QAbstractItemView::PositionAtTop);
    QVector<qint64> scroll_times, update_times;
    for (int i = 0; i < 10; ++i) {
        timer.restart();
        list->verticalScrollBar()->setValue(list->verticalScrollBar()->maximum() * i / 9);
        QVERIFY(!list->viewport()->grab().isNull()); // synchronous completed Qt render, not monitor presentation
        scroll_times.append(timer.elapsed());
    }
    list->scrollToItem(retained, QAbstractItemView::PositionAtTop);
    const int anchor = list->verticalScrollBar()->value();
    for (int i = 0; i < 10; ++i) {
        timer.restart();
        mail[1000].preview = QStringLiteral("Updated preview %1").arg(i);
        model.setMailItems(mail);
        QVERIFY(!list->viewport()->grab().isNull());
        update_times.append(timer.elapsed());
        QCOMPARE(list->item(1000), retained);
        QCOMPARE(list->currentItem(), retained);
        QVERIFY(retained->isSelected());
        QCOMPARE(list->verticalScrollBar()->value(), anchor);
    }
    std::sort(scroll_times.begin(), scroll_times.end());
    std::sort(update_times.begin(), update_times.end());
    qInfo("Synthetic %d-message mailbox (%s, DPR %.2f): construction %lld ms; first completed Qt viewport render %lld ms; 10 scroll renders median %lld/max %lld ms; 10 one-message update+render median %lld/max %lld ms; row widgets %lld (no network/storage I/O or monitor presentation timing)",
        count, qPrintable(QGuiApplication::platformName()), list->devicePixelRatioF(),
        static_cast<long long>(construction), static_cast<long long>(first_render),
        static_cast<long long>(scroll_times[5]), static_cast<long long>(scroll_times.last()),
        static_cast<long long>(update_times[5]), static_cast<long long>(update_times.last()),
        static_cast<long long>(page.findChildren<QWidget*>(QStringLiteral("mailRow")).size()));
    QCOMPARE(page.findChildren<QWidget*>(QStringLiteral("mailRow")).size(), 0);
    QVERIFY(!list->itemWidget(retained));
    const QString screenshot = qEnvironmentVariable("CYBOU_TEST_MAILBOX_SCREENSHOT");
    if (!screenshot.isEmpty()) QVERIFY(page.grab().save(screenshot));
    page.setSearchText(QStringLiteral("subject 1000"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0), retained);
    model.requestLockVault();
    QCOMPARE(list->count(), 0);
}

void CybouShellTests::mailDelegateExposesSemanticStatus()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    QVector<CybouMailItem> messages;
    for (int i = 0; i < 6; ++i) {
        CybouMailItem message;
        message.id = QString::number(i);
        message.from_name = QStringLiteral("alice.cybou");
        message.to_name = QStringLiteral("bob.cybou");
        message.subject = QStringLiteral("<b>literal subject</b>");
        message.preview = QStringLiteral("A plain preview");
        message.time = QDateTime::currentDateTime().addSecs(-i * 60);
        message.state = CybouContentState::Received;
        messages.append(message);
    }
    messages[0].unread = messages[0].starred = true;
    messages[0].attachments.append(CybouAttachmentItem{});
    messages[1].outgoing = true;
    messages[1].state = CybouContentState::Protected;
    messages[2].outgoing = true;
    messages[2].state = CybouContentState::Local;
    messages[2].operation_state = CybouOperationState::Submitted;
    messages[3].state = CybouContentState::Securing;
    messages[4].state = CybouContentState::NeedsAttention;
    messages[5].below_support_rate = true;
    model.setMailItems(messages);
    EmailPage page{&model, {}};
    page.resize(1040, 720);
    page.show();
    auto* list = page.findChild<QListWidget*>(QStringLiteral("messageList"));
    QVERIFY(list);
    QCoreApplication::processEvents();
    QVERIFY(!list->viewport()->grab().isNull());
    const QString first = list->item(0)->data(Qt::AccessibleTextRole).toString();
    QVERIFY(first.contains(QStringLiteral("<b>literal subject</b>")));
    QVERIFY(first.contains(CybouUi::shortTime(messages[0].time)));
    QVERIFY(first.contains(QStringLiteral("unread")) && first.contains(QStringLiteral("Starred")) && first.contains(QStringLiteral("Has attachments")));
    QVERIFY(list->item(0)->toolTip().contains(QStringLiteral("&lt;b&gt;literal subject&lt;/b&gt;")));
    QCOMPARE(list->item(0)->data(Qt::AccessibleDescriptionRole).toString(), messages[0].preview);
    QVERIFY(list->item(1)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("To: bob.cybou")));
    QVERIFY(list->item(1)->toolTip().contains(QStringLiteral("Protected: encrypted here")));
    QVERIFY(list->item(2)->data(Qt::AccessibleTextRole).toString().contains(CybouProduct::contentWithOperationText(
        messages[2].state, messages[2].operation_state, model.status().online)));
    QVERIFY(list->item(3)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Securing")));
    QVERIFY(list->item(4)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Needs attention")));
    QVERIFY(list->item(5)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Below support rate")));
    for (int i = 0; i < list->count(); ++i) QVERIFY(!list->itemWidget(list->item(i)));
    const QString screenshot = qEnvironmentVariable("CYBOU_TEST_MAILBOX_SCREENSHOT");
    if (!screenshot.isEmpty()) QVERIFY(page.grab().save(screenshot));
    messages[0].draft = true;
    messages[0].folder = CybouMailFolder::Drafts;
    model.setMailItems(messages);
    page.setView(EmailPage::View::Drafts);
    QCOMPARE(list->count(), 1);
    QVERIFY(list->item(0)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Draft")));
    model.requestLockVault();
    QCOMPARE(list->count(), 0);
}

void CybouShellTests::mailRowsRetainContextAndReplacement()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    QVector<CybouMailItem> mail;
    for (int i{0}; i < 100; ++i) {
        CybouMailItem message;
        message.id = QStringLiteral("mail-%1").arg(i);
        message.subject = QStringLiteral("Subject %1").arg(i);
        message.from_name = QStringLiteral("alice.cybou");
        message.state = CybouContentState::Received;
        message.operation_state = CybouOperationState::Finalized;
        message.folder = CybouMailFolder::Inbox;
        message.time = QDateTime::currentDateTime().addSecs(-i * 60);
        mail.append(message);
    }
    mail[51].unread = true;
    model.setMailItems(mail);
    EmailPage page{&model, {}};
    page.resize(1040, 720);
    page.show();
    auto* list = page.findChild<QListWidget*>(QStringLiteral("messageList"));
    QVERIFY(list);
    auto* folders = page.findChild<QListWidget*>(QStringLiteral("folderList"));
    QVERIFY(folders);
    QPointer<QWidget> inbox_target{folders->itemWidget(folders->item(0))};
    QVERIFY(inbox_target);
    QTRY_VERIFY(list->verticalScrollBar()->maximum() > 0);
    auto* selected = list->item(50);
    auto* other = list->item(51);
    QVERIFY(!list->itemWidget(other));
    list->setCurrentItem(selected);
    other->setSelected(true);
    list->scrollToItem(selected, QAbstractItemView::PositionAtTop);
    const int scroll = list->verticalScrollBar()->value();
    page.openMessage(QStringLiteral("mail-50"));
    mail[50].starred = true;
    model.setMailItems(mail);
    QVERIFY(!list->itemWidget(selected));
    QVERIFY(selected->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("Starred")));
    QCOMPARE(list->item(50), selected);
    QVERIFY(!list->itemWidget(other));
    QCOMPARE(folders->itemWidget(folders->item(0)), inbox_target.data());
    QVERIFY(selected->isSelected() && other->isSelected());
    QCOMPARE(list->currentItem(), selected);
    QCOMPARE(list->verticalScrollBar()->value(), scroll);
    QCOMPARE(page.reader()->messageId(), QStringLiteral("mail-50"));

    Q_EMIT model.applicationBackend()->mailItemReplaced(QStringLiteral("mail-50"), QStringLiteral("permanent-mail"));
    QCOMPARE(list->item(50), selected);
    QCOMPARE(selected->data(Qt::UserRole).toString(), QStringLiteral("permanent-mail"));
    QCOMPARE(page.reader()->messageId(), QStringLiteral("permanent-mail"));
    QVERIFY(!model.mailItem(QStringLiteral("mail-50")));
    QVERIFY(model.mailItem(QStringLiteral("permanent-mail")));
    QVERIFY(selected->isSelected() && other->isSelected());
    mail[50].id = QStringLiteral("permanent-mail");
    CybouMailItem incoming = mail.first();
    incoming.id = QStringLiteral("new-mail");
    incoming.time = incoming.time.addSecs(60);
    mail.append(incoming);
    model.setMailItems(mail);
    QCOMPARE(list->item(51), selected);
    QCOMPARE(list->itemAt(QPoint{1, 0}), selected);
    QCOMPARE(list->currentItem(), selected);
    QVERIFY(!list->itemWidget(other));
    mail.removeAt(51);
    model.setMailItems(mail);
    QVERIFY(inbox_target->findChild<QLabel*>(QStringLiteral("folderCount"))->isHidden());
    QCOMPARE(list->selectedItems().size(), 1);
    QVERIFY(selected->isSelected());
    page.setSearchText(QStringLiteral("Subject 50"));
    QCOMPARE(list->count(), 1);
    QCOMPARE(list->item(0), selected);
    page.setSearchText(QStringLiteral("no-match"));
    QCOMPARE(list->count(), 0);
    page.setSearchText({});
    QVERIFY(list->selectedItems().isEmpty());
    model.setIdentityState(CybouIdentityState::Locked, model.status().account_id, 1);
    QCOMPARE(list->count(), 0);
}

void CybouShellTests::fileRowsRetainInteractionAcrossUpdates()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    QVERIFY(CybouUiFixtures::apply(model, QStringLiteral("active")));
    QVector<CybouFileItem> items;
    for (int i{0}; i < 120; ++i) {
        CybouFileItem file;
        file.id = QStringLiteral("stable-%1").arg(i);
        file.name = QStringLiteral("File %1.txt").arg(i, 3, 10, QLatin1Char('0'));
        file.logical_size = 100;
        file.state = CybouContentState::Securing;
        file.operation_state = CybouOperationState::Finalized;
        file.min_remote_replicas = 1;
        file.remote_replica_target = 2;
        items.append(file);
    }
    model.setFileItems(items);
    StoragePage page{&model};
    page.resize(1040, 720);
    page.show();
    auto* table = page.findChild<QTreeWidget*>(QStringLiteral("filesTable"));
    auto* grid = page.findChild<QListWidget*>(QStringLiteral("filesGrid"));
    QVERIFY(table && grid);
    QTRY_VERIFY(table->verticalScrollBar()->maximum() > 0);
    auto* selected = table->topLevelItem(50);
    auto* other = table->topLevelItem(51);
    auto* tile = grid->item(50);
    const QString selected_id = selected->data(0, Qt::UserRole + 1).toString();
    table->setCurrentItem(selected);
    other->setSelected(true);
    table->scrollToItem(selected, QAbstractItemView::PositionAtTop);
    const int scroll = table->verticalScrollBar()->value();
    const QString other_status = other->data(3, Qt::AccessibleTextRole).toString();
    QVERIFY(!table->itemWidget(selected, 3));
    QVERIFY(!other_status.isEmpty());
    // A changed progress caption updates the existing row/chip; other rows stay untouched.
    items[50].progress_percent = 61;
    items[50].min_remote_replicas = 0;
    items[52].name = QStringLiteral("File 052.pdf");
    model.setFileItems(items);
    QCOMPARE(table->topLevelItem(50), selected);
    QCOMPARE(grid->item(50), tile);
    QVERIFY(!table->itemWidget(selected, 3));
    QCOMPARE(other->data(3, Qt::AccessibleTextRole).toString(), other_status);
    QCOMPARE(table->currentItem(), selected);
    QVERIFY(selected->isSelected() && other->isSelected());
    QCOMPARE(table->verticalScrollBar()->value(), scroll);
    QVERIFY(selected->data(3, Qt::AccessibleTextRole).toString().contains(QStringLiteral("0 of 2")));
    QCOMPARE(selected->toolTip(3), selected->text(3));
    auto* accessible = QAccessible::queryAccessibleInterface(table);
    QVERIFY(accessible && accessible->tableInterface());
    auto* status_cell = accessible->tableInterface()->cellAt(50, 3);
    QVERIFY(status_cell);
    QCOMPARE(status_cell->text(QAccessible::Name), selected->data(3, Qt::AccessibleTextRole).toString());

    page.setGridMode(true);
    QVERIFY(tile->isSelected() && grid->item(51)->isSelected());
    QCOMPARE(grid->currentItem(), tile);
    page.setGridMode(false);
    QCOMPARE(table->currentItem(), selected);
    QVERIFY(selected->isSelected() && other->isSelected());

    // Insert above the viewport: retained rows and the visible anchor stay in place.
    CybouFileItem added = items.first();
    added.id = QStringLiteral("inserted");
    added.name = QStringLiteral("AAA.txt");
    items.append(added);
    model.setFileItems(items);
    QCOMPARE(table->topLevelItem(51), selected);
    QCOMPARE(grid->item(51), tile);
    QCOMPARE(table->currentItem(), selected);
    QCOMPARE(table->itemAt(QPoint{1, 0}), selected);
    QVERIFY(selected->isSelected() && other->isSelected());

    page.sortBy(0, true);
    QVERIFY(selected->isSelected() && other->isSelected());
    QCOMPARE(table->currentItem(), selected);
    QCOMPARE(grid->item(grid->row(tile)), tile);
    QCOMPARE(table->topLevelItem(0)->text(0), QStringLiteral("File 119.txt"));
    // A real rename changes ordering but preserves object/current/selection.
    items[50].name = QStringLiteral("ZZZ-renamed.txt");
    model.setFileItems(items);
    QCOMPARE(table->topLevelItem(0), selected);
    QCOMPARE(grid->item(0), tile);
    QCOMPARE(selected->text(0), QStringLiteral("ZZZ-renamed.txt"));
    QCOMPARE(table->currentItem(), selected);
    QVERIFY(selected->isSelected());
    items[50].state = CybouContentState::Protected;
    model.setFileItems(items);
    QCOMPARE(table->topLevelItem(0), selected);
    QVERIFY(!table->itemWidget(selected, 3));
    QVERIFY(selected->data(3, Qt::AccessibleTextRole).toString().contains(QStringLiteral("Protected")));

    items.removeAt(51); // Remove another selected file without affecting this one.
    model.setFileItems(items);
    QVERIFY(selected->isSelected());
    QCOMPARE(table->selectedItems().size(), 1);
    QCOMPARE(table->currentItem(), selected);
    QVERIFY(!page.visibleIds().contains(QStringLiteral("stable-51")));
    // Filtering removes hidden selections rather than keeping invisible action targets.
    page.setSearchText(QStringLiteral("ZZZ"));
    QCOMPARE(table->topLevelItemCount(), 1);
    QCOMPARE(table->topLevelItem(0), selected);
    page.setSearchText(QStringLiteral("no-match"));
    QCOMPARE(table->topLevelItemCount(), 0);
    QCOMPARE(grid->count(), 0);
    page.setSearchText({});
    QVERIFY(page.visibleIds().contains(selected_id));
    QVERIFY(table->selectedItems().isEmpty());
    model.setIdentityState(CybouIdentityState::Locked, model.status().account_id, 1);
    QCOMPARE(table->topLevelItemCount(), 0);
    QCOMPARE(grid->count(), 0);
    QVERIFY(page.currentFolder().isEmpty());
    QVERIFY(page.detailsId().isEmpty());
}

void CybouShellTests::fileAdvancedSurvivesRefresh()
{
    auto window = makeWindow();
    QVERIFY(CybouUiFixtures::apply(*window->desktopModel(), QStringLiteral("files")));
    window->showPage(CybouPage::Files);
    window->show();
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    const auto item = std::find_if(window->desktopModel()->fileItems().begin(), window->desktopModel()->fileItems().end(),
        [](const auto& f) { return !f.folder && !f.trashed; });
    QVERIFY(item != window->desktopModel()->fileItems().end());
    const QString id = item->id;
    files->showDetails(id);
    QToolButton* advanced = nullptr;
    QTRY_VERIFY((advanced = FindById<QToolButton>(files, QStringLiteral("fileAdvanced"))) != nullptr);
    advanced->click();
    QVERIFY(advanced->isChecked());
    auto updated = window->desktopModel()->fileItems();
    for (auto& file : updated) if (file.id == id) file.min_remote_replicas = 1;
    window->desktopModel()->setFileItems(updated);
    QCoreApplication::processEvents();
    auto* refreshed = FindById<QToolButton>(files, QStringLiteral("fileAdvanced"));
    QVERIFY(refreshed && refreshed->isChecked());
    QSignalSpy changed{window->desktopModel(), &CybouDesktopModel::filesChanged};
    window->desktopModel()->setFileItems(updated);
    QCOMPARE(changed.size(), 0);
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

void CybouShellTests::filesExplainScopedProtectionObservations()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("files")));
    window->show();
    window->showPage(CybouPage::Files);
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);
    auto item = *model->fileItem(QStringLiteral("f-report"));
    item.state = CybouContentState::Securing;
    item.available_offline = false;
    item.min_remote_replicas = -1;
    item.remote_replica_target = -1;
    const auto texts = [files] {
        QStringList rows;
        for (const auto* label : files->findChildren<QLabel*>()) if (label->isVisibleTo(files)) rows.append(label->text());
        return rows.join(QLatin1Char{'\n'});
    };
    model->upsertFileItem(item);
    files->showDetails(item.id, true);
    QTRY_VERIFY(texts().contains(QStringLiteral("Unknown — reading saved records does not check copies")));
    QVERIFY(texts().contains(QStringLiteral("Remote copy count is unknown")));
    QVERIFY(!texts().contains(QStringLiteral("Measured copies: -1")));
    QVERIFY(texts().contains(QStringLiteral("No verified local copy is available and remote protection is incomplete.")));
    for (const int copies : {0, 1, 2}) {
        item.min_remote_replicas = copies;
        item.remote_replica_target = 2;
        item.protection_observed_at = QDateTime::currentDateTimeUtc();
        item.protection_observation_scope = QStringLiteral("Local audit of part of this publication");
        item.protection_reason = copies < 2 ? QStringLiteral("Observed storage refusal") : QString{};
        model->upsertFileItem(item);
        QTRY_VERIFY(texts().contains(QStringLiteral("Encrypted copies: %1 of 2").arg(copies)));
        QVERIFY(texts().contains(item.protection_observation_scope));
        QVERIFY(!texts().contains(QStringLiteral("Replicating to network")));
        if (copies < 2) QVERIFY(texts().contains(item.protection_reason));
    }
    item.protection_observed_at = QDateTime::currentDateTimeUtc().addDays(-2);
    item.protection_reason = QStringLiteral("Local storage progress could not be saved. Check local storage.");
    item.available_offline = true;
    model->upsertFileItem(item);
    QTRY_VERIFY(texts().contains(QStringLiteral("Older than 24 hours")));
    QVERIFY(texts().contains(item.protection_reason));
    QVERIFY(!texts().contains(QStringLiteral("No verified local copy is available")));
    auto* download = FindById<QPushButton>(files, QStringLiteral("fileDownload"));
    QVERIFY(download && download->isEnabled());
    QCOMPARE(item.min_remote_replicas, 2); // observation age does not invent replica loss
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
    QTRY_COMPARE(alice_model->applicationLoadState(), CybouApplicationLoadState::Ready);
    QVERIFY(alice_model->mailItems().isEmpty());
    QVERIFY(alice_model->fileItems().isEmpty());
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
    QCOMPARE(received->from_address,QString::fromStdString(alice->GetAccountId()->Value().GetHex()));
    QCOMPARE(received->to_address,bob_id);
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
    const QString saved_client = bob_model->requestSaveAttachmentToFiles(contract_message, received_attachment.id);
    QVERIFY(!saved_client.isEmpty());
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
    QCOMPARE(saved_client, saved_id);
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
    bool forward_saved = false;
    const QString forward_draft = bob_model->requestSaveMailDraft(forward, [&](bool ok, const QString& error) {
        QVERIFY2(ok, qPrintable(error));
        forward_saved = true;
    });
    QTRY_VERIFY(forward_saved);
    bob_model->setIdentityState(CybouIdentityState::Locked, bob_id, 1);
    bob_model->setIdentityState(CybouIdentityState::Active, bob_id, 1);
    QTRY_VERIFY(bob_model->mailItem(forward_draft));
    const auto restored_forward = *bob_model->mailItem(forward_draft);
    QCOMPARE(restored_forward.attachments.size(), 1);
    QCOMPARE(restored_forward.attachments.first().id, reference.id);
    QVERIFY(restored_forward.attachments.first().source_path.isEmpty());
    QVERIFY(!bob_model->requestSendMail(restored_forward).isEmpty());
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

    // Self-mail enters Inbox and keeps a usable attachment, including reused Files content.
    CybouMailItem self_mail;
    self_mail.to_name = bob_id;
    self_mail.subject = QStringLiteral("Note to myself");
    self_mail.body = QStringLiteral("Keep this attachment.");
    self_mail.attachments.append(reference);
    QVERIFY(!bob_model->requestSendMail(self_mail).isEmpty());
    const auto self_received = [&]() -> const CybouMailItem* {
        for (const auto& item : bob_model->mailItems())
            if (item.subject == self_mail.subject && item.folder == CybouMailFolder::Inbox)
                return bob_model->mailItem(item.id);
        return nullptr;
    };
    QVERIFY(produce_until([&] { return self_received() != nullptr; }));
    QCOMPARE(self_received()->body, self_mail.body);
    QCOMPARE(self_received()->attachments.size(), 1);
    const QString self_download = attach_dir.filePath(QStringLiteral("self-contract.pdf"));
    bob_model->requestAttachmentDownload(self_received()->id, self_received()->attachments.first().id, self_download);
    QTRY_VERIFY(QFile::exists(self_download));
    {
        QFile in{self_download};
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), contract);
    }

    // Removing the source before preparation refuses reuse and retains the
    // acknowledged draft instead of silently substituting downloaded plaintext.
    auto missing_source_draft = forward;
    missing_source_draft.subject = QStringLiteral("Source removed");
    bool missing_source_saved = false;
    const QString missing_source_id = bob_model->requestSaveMailDraft(missing_source_draft,
        [&](bool ok, const QString& error) { QVERIFY2(ok, qPrintable(error)); missing_source_saved = true; });
    QTRY_VERIFY(missing_source_saved);
    bob_model->requestDeleteFile(saved_id);
    QTRY_VERIFY(!bob_model->fileItem(saved_id));
    bool source_refused = false;
    const QString failed_reuse = bob_model->requestSendMail(*bob_model->mailItem(missing_source_id),
        [&](bool ok, const QString& error) { QVERIFY(!ok); QVERIFY(!error.isEmpty()); source_refused = true; });
    QVERIFY(!failed_reuse.isEmpty());
    QTRY_VERIFY(source_refused);
    QVERIFY(bob_model->mailItem(missing_source_id));
    QCOMPARE(bob_model->mailItem(missing_source_id)->body, forward.body);
    QCOMPARE(bob_model->mailItem(missing_source_id)->attachments.first().id, reference.id);

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
    QTRY_VERIFY(!bob_model->mailItem(draft_id));
    QTest::qWait(150); // later snapshots must not resurrect it
    QVERIFY(!bob_model->mailItem(draft_id));
    bob_model->requestMoveMail(sent_id, CybouMailFolder::Trash);
    QTRY_VERIFY(bob_model->mailItem(sent_id) && bob_model->mailItem(sent_id)->folder == CybouMailFolder::Trash);
    bool deletion_committed = false;
    bob_model->requestDeleteMailForever({sent_id}, [&](bool ok, const QString& error) {
        QVERIFY2(ok, qPrintable(error));
        deletion_committed = true;
    });
    QVERIFY(bob_model->mailItem(sent_id));
    QTRY_VERIFY(deletion_committed);
    QVERIFY(!bob_model->mailItem(sent_id));
    bob_model->setIdentityState(CybouIdentityState::Locked, bob_account, 1);
    bob_model->setIdentityState(CybouIdentityState::Active, bob_account, 1);
    QTRY_VERIFY(bob_model->featureAvailability().mail && !bob_model->mailItems().isEmpty());
    QVERIFY(!bob_model->mailItem(draft_id));

    QVERIFY(!bob_model->mailItem(sent_id));

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
    QCOMPARE(work_client, work_id);
    QTemporaryDir files_dir;
    QByteArray original(400 * 1024, '\0');
    for (int i = 0; i < original.size(); ++i) original[i] = static_cast<char>(i * 13 + 1);
    const QString source = files_dir.filePath(QStringLiteral("report.bin"));
    {
        QFile out{source};
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write(original);
    }
    const QString report_client = alice_model->requestFileUpload(source, work_client);
    QVERIFY(!report_client.isEmpty());
    QTRY_VERIFY(file_named(QStringLiteral("report.bin")) != nullptr);
    QCOMPARE(file_named(QStringLiteral("report.bin"))->parent_id, work_id);
    QVERIFY(file_named(QStringLiteral("report.bin"))->available_offline);
    StoragePage live_files{alice_model.get()};
    live_files.show();
    live_files.showDetails(report_client);
    QToolButton* live_advanced{nullptr};
    QTRY_VERIFY((live_advanced = FindById<QToolButton>(&live_files, QStringLiteral("fileAdvanced"))) != nullptr);
    live_advanced->click();
    QVERIFY(live_advanced->isChecked());
    QVERIFY(finalize_until(QStringLiteral("report.bin"), CybouContentState::Securing));
    QCOMPARE(live_files.detailsId(), report_client);
    QTRY_VERIFY(FindById<QToolButton>(&live_files, QStringLiteral("fileAdvanced")) != nullptr);
    QVERIFY(FindById<QToolButton>(&live_files, QStringLiteral("fileAdvanced"))->isChecked());
    QTRY_VERIFY(file_named(QStringLiteral("report.bin")) && file_named(QStringLiteral("report.bin"))->finalized_height > 0);
    QCOMPARE(file_named(QStringLiteral("report.bin"))->logical_size, quint64(original.size()));
    const QString report_id = file_named(QStringLiteral("report.bin"))->id;
    QCOMPARE(report_client, report_id);

    alice_model->requestRenameFile(report_id, QStringLiteral("report-final.bin"));
    QTRY_VERIFY(file_named(QStringLiteral("report-final.bin")) != nullptr);
    QVERIFY(finalize_until(QStringLiteral("report-final.bin"), CybouContentState::Securing));
    QVERIFY(alice_model->fileItem(report_id));

    QTRY_VERIFY(alice_model->fileItem(report_id)->protection_observed_at.isValid());
    QCOMPARE(alice_model->fileItem(report_id)->min_remote_replicas, 0);
    QCOMPARE(alice_model->fileItem(report_id)->protection_observation_scope, QStringLiteral("Local placement attempt"));
    QVERIFY(!alice_model->fileItem(report_id)->protection_reason.isEmpty());

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

    // Undo must queue after an acknowledged forward intent even before finality.
    const QString previous_operation = alice_model->fileItem(report_id)->operation_id;
    int changes_saved = 0;
    const auto saved_change = [&](bool ok, const QString& error) {
        QVERIFY2(ok, qPrintable(error));
        ++changes_saved;
    };
    alice_model->requestMoveFile(report_id, {}, saved_change);
    alice_model->requestMoveFile(report_id, work_id, saved_change);
    QTRY_COMPARE(changes_saved, 2);
    QVERIFY(produce_until([&] {
        const auto* item = alice_model->fileItem(report_id);
        return item && item->parent_id == work_id && item->operation_id != previous_operation &&
            item->operation_state == CybouOperationState::Finalized;
    }));
    const QString before_trash = alice_model->fileItem(report_id)->operation_id;
    alice_model->requestTrashFile(report_id, saved_change);
    alice_model->requestMoveFile(report_id, work_id, saved_change);
    QTRY_COMPARE(changes_saved, 4);
    QVERIFY(produce_until([&] {
        const auto* item = alice_model->fileItem(report_id);
        return item && !item->trashed && item->parent_id == work_id && item->operation_id != before_trash &&
            item->operation_state == CybouOperationState::Finalized;
    }));

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

void CybouShellTests::searchScopeAndIncrementalIndex()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    auto* search = window->globalSearch();
    QVERIFY(search);

    // Home offers both products; Mail and Files suggestions stay in context.
    window->showPage(CybouPage::Home);
    QVERIFY(search->placeholderText().contains(QStringLiteral("mail"), Qt::CaseInsensitive));

    // Switch to Files: placeholder & tooltip adapt to current Files view.
    window->showPage(CybouPage::Files);
    QVERIFY(search->placeholderText().contains(QStringLiteral("Files"), Qt::CaseInsensitive) ||
            search->placeholderText().contains(QStringLiteral("folder"), Qt::CaseInsensitive));
    QVERIFY(search->toolTip().contains(QStringLiteral("Files"), Qt::CaseInsensitive));
    auto* completer = search->completer();
    for (int i = 0; i < completer->model()->rowCount(); ++i)
        QCOMPARE(completer->model()->index(i, 0).data(Qt::UserRole + 1).toString(), QStringLiteral("file"));
    window->showPage(CybouPage::Mail);
    for (int i = 0; i < completer->model()->rowCount(); ++i)
        QCOMPARE(completer->model()->index(i, 0).data(Qt::UserRole + 1).toString(), QStringLiteral("mail"));

    // Wallet shows its title; Ctrl+K exposes global search without leaving it.
    window->showPage(CybouPage::Wallet);
    QVERIFY(search->isHidden());
    auto* title = window->findChild<QLabel*>(QStringLiteral("headerPageTitle"));
    QVERIFY(title && !title->isHidden());
    window->show();
    window->activateWindow();
    QTest::qWait(50);
    QTest::keyClick(window.get(), Qt::Key_K, Qt::ControlModifier);
    QTRY_VERIFY(!search->isHidden());
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Wallet));
    QTest::keyClick(search, Qt::Key_Escape);
    QVERIFY(search->isHidden());
    QVERIFY(!title->isHidden());

    // Bounded completer:
    QVERIFY(completer);
    auto* comp_model = completer->model();
    QVERIFY(comp_model);
    QVERIFY(comp_model->rowCount() > 0);
    QVERIFY(comp_model->rowCount() <= 300);

    // Enter while on Files page searches Files and stays on Files page.
    window->showPage(CybouPage::Files);
    search->setText(QStringLiteral("budget"));
    auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
    QCOMPARE(files->searchText(), QStringLiteral("budget"));
    QVERIFY(files->findChild<QLineEdit*>(QStringLiteral("filesSearch"))->isHidden());
    search->clear();
    QCOMPARE(files->searchText(), QString{});
    search->setText(QStringLiteral("budget"));
    Q_EMIT search->returnPressed();
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Files));
    window->showPage(CybouPage::Mail);
    search->setText(QStringLiteral("dinner"));
    auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    QCOMPARE(mail->searchText(), QStringLiteral("dinner"));
    QVERIFY(mail->findChild<QLineEdit*>(QStringLiteral("mailSearch"))->isHidden());
    window->showPage(CybouPage::Files);
    QCOMPARE(search->text(), QStringLiteral("budget"));
    files->openFolder(QStringLiteral("f-docs"));
    QVERIFY(search->text().isEmpty()); // Folder navigation clears the same query.
    window->showPage(CybouPage::Mail);
    QCOMPARE(search->text(), QStringLiteral("dinner"));
    model->setIdentityState(CybouIdentityState::Locked, QStringLiteral("locked"));
    QVERIFY(search->text().isEmpty());
}

void CybouShellTests::walletAndAuthorityPreserveRowsWithoutChurn()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    window->showPage(CybouPage::Wallet);
    window->show();

    auto* wallet = dynamic_cast<WalletPage*>(window->page(CybouPage::Wallet));
    QVERIFY(wallet);

    QPointer<QWidget> wallet_row;
    for (auto* w : wallet->findChildren<QWidget*>()) {
        if (w->property("walletEntryId").toString() == QStringLiteral("w1")) {
            wallet_row = w;
            break;
        }
    }
    QVERIFY(!wallet_row.isNull());

    // Emitting mailChanged or filesChanged should NOT cause wallet activity rebuild.
    Q_EMIT model->mailChanged();
    Q_EMIT model->filesChanged();
    QCoreApplication::processEvents();
    QVERIFY(!wallet_row.isNull());

    // Updating wallet entries in model preserves existing row widget pointer.
    auto entries = model->walletEntries();
    QVERIFY(!entries.isEmpty());
    entries[0].counterparty_name = QStringLiteral("updated.cybou");
    model->setWalletEntries(entries);
    QCoreApplication::processEvents();
    QVERIFY(!wallet_row.isNull());

    // Switch to NetworkAuthorityPage.
    CybouNetworkAuthorityStatus authority;
    authority.proven = true;
    authority.signer_enabled = true;
    authority.finalizer = CybouFinalizerState::Finalizing;
    authority.finalized_height = 100;
    model->setNetworkAuthority(authority);
    window->showPage(CybouPage::NetworkAuthority);
    auto* auth = dynamic_cast<NetworkAuthorityPage*>(window->page(CybouPage::NetworkAuthority));
    QVERIFY(auth);

    QLabel* spendable_label = nullptr;
    for (auto* l : auth->findChildren<QLabel*>()) {
        if (l->text().contains(QStringLiteral("Spendable"), Qt::CaseInsensitive) ||
            l->text().contains(QStringLiteral("disponible"), Qt::CaseInsensitive)) {
            spendable_label = l;
            break;
        }
    }
    QVERIFY(spendable_label != nullptr);
    QPointer<QLabel> guard{spendable_label};

    // Re-triggering authority changed signal (refresh) preserves permanent widgets without deletion.
    model->setNetworkAuthority(authority);
    QCoreApplication::processEvents();
    QVERIFY(!guard.isNull());
}

void CybouShellTests::relativeTimeLocalization()
{
    auto window = makeWindow();
    window->setLanguage(QStringLiteral("fr"));
    const auto now = QDateTime::currentDateTime();
    QCOMPARE(CybouUi::relTime(now, now), QStringLiteral("à l'instant"));
    QCOMPARE(CybouUi::relTime(now.addSecs(-120), now), QStringLiteral("il y a 2 min"));
    QCOMPARE(CybouUi::relTime(now.addSecs(-7200), now), QStringLiteral("il y a 2 h"));
    QCOMPARE(CybouUi::relTime(now.addDays(-3), now), QStringLiteral("il y a 3 j"));

    window->setLanguage(QStringLiteral("en"));
    QCOMPARE(CybouUi::relTime(now, now), QStringLiteral("just now"));
    QCOMPARE(CybouUi::relTime(now.addSecs(-120), now), QStringLiteral("2 min ago"));
    QCOMPARE(CybouUi::relTime(now.addSecs(-7200), now), QStringLiteral("2 h ago"));
    QCOMPARE(CybouUi::relTime(now.addDays(-3), now), QStringLiteral("3 d ago"));
}

void CybouShellTests::appearanceSwitchPreservesFullContext()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));

    // 1. Setup Mail context: Archive folder, search "dinner", open message "m-dinner" in reader
    window->showPage(CybouPage::Mail);
    auto* mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    QVERIFY(mail);
    mail->setView(EmailPage::View::Archive);
    mail->setSearchText(QStringLiteral("dinner"));
    mail->openMessage(QStringLiteral("m-dinner"));
    QCOMPARE(mail->view(), EmailPage::View::Archive);
    QCOMPARE(mail->searchText(), QStringLiteral("dinner"));
    QCOMPARE(mail->currentMessageId(), QStringLiteral("m-dinner"));

    // 2. Setup Files context: open folder "f-docs", open details for "f-budget", enable Advanced
    window->showPage(CybouPage::Files);
    auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);
    files->openFolder(QStringLiteral("f-docs"));
    files->setSearchText(QStringLiteral("budget"));
    files->showDetails(QStringLiteral("f-budget"), true);
    QCOMPARE(files->currentFolder(), QStringLiteral("f-docs"));
    QCOMPARE(files->searchText(), QStringLiteral("budget"));
    QCOMPARE(files->detailsId(), QStringLiteral("f-budget"));
    QVERIFY(files->isDetailsVisible());
    QVERIFY(files->isDetailsAdvanced());

    // 3. The shell and current Files filter now share one query.
    auto* search = window->globalSearch();
    QVERIFY(search);
    search->setText(QStringLiteral("budget"));

    // 4. Trigger appearance reload (rebuilds entire shell)
    window->reloadAppearance();

    // Verify current page is still Files
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Files));

    // Verify global search text preserved
    search = window->globalSearch();
    QVERIFY(search);
    QCOMPARE(search->text(), QStringLiteral("budget"));

    // Verify Files state preserved
    files = static_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);
    QCOMPARE(files->currentFolder(), QStringLiteral("f-docs"));
    QCOMPARE(files->searchText(), QStringLiteral("budget"));
    QCOMPARE(files->detailsId(), QStringLiteral("f-budget"));
    QVERIFY(files->isDetailsVisible());
    QVERIFY(files->isDetailsAdvanced());

    // Verify Mail state preserved
    mail = static_cast<EmailPage*>(window->page(CybouPage::Mail));
    QVERIFY(mail);
    QCOMPARE(mail->view(), EmailPage::View::Archive);
    QCOMPARE(mail->searchText(), QStringLiteral("dinner"));
    QCOMPARE(mail->currentMessageId(), QStringLiteral("m-dinner"));
    QVERIFY(mail->isDetailOpen());
}

void CybouShellTests::filesProtectionAndOfflineDownload()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("files")));
    window->showPage(CybouPage::Files);
    window->show();
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);

    // f-archive is Securing (state != Protected), but available_offline is true.
    const auto* archive = model->fileItem(QStringLiteral("f-archive"));
    QVERIFY(archive != nullptr);
    QCOMPARE(archive->state, CybouContentState::Securing);
    QVERIFY(archive->available_offline);
    QCOMPARE(archive->min_remote_replicas, 1);
    QCOMPARE(archive->remote_replica_target, 2);

    files->showDetails(QStringLiteral("f-archive"));
    QVERIFY(files->isDetailsVisible());

    // Download button MUST be enabled because file is available locally on this computer
    QPushButton* dl = nullptr;
    for (auto* btn : files->findChildren<QPushButton*>()) {
        if (btn->property("cybouId").toString() == QLatin1String("fileDownload")) {
            dl = btn;
            break;
        }
    }
    QVERIFY(dl != nullptr);
    QVERIFY(dl->isEnabled());

    // Missing copies alone do not establish that replication is currently running.
    bool found_unmet_target = false;
    for (const auto* label : files->findChildren<QLabel*>()) {
        QVERIFY(!label->text().contains(QStringLiteral("Replicating to network")));
        if (label->text().contains(QStringLiteral("Remote copy target not reached"))) found_unmet_target = true;
    }
    QVERIFY(found_unmet_target);

    // Responsive test: at 1040x720 window size with details panel open, Size column remains visible
    window->resize(1040, 720);
    QCoreApplication::processEvents();
    auto* table = files->findChild<QTreeWidget*>(QStringLiteral("filesTable"));
    QVERIFY(table != nullptr);
    // Column 1 is SizeColumn (Name=0, Size=1, Modified=2, Status=3)
    QVERIFY(!table->isColumnHidden(1));
}

void CybouShellTests::walletForecastAndTransferReview()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    window->showPage(CybouPage::Wallet);
    window->show();
    auto* wallet = dynamic_cast<WalletPage*>(window->page(CybouPage::Wallet));
    QVERIFY(wallet);

    // 1. Forecast text must scope to standard fees and note storage rent exclusion
    auto* hint = wallet->findChild<QLabel*>(QStringLiteral("walletSystemHint"));
    QVERIFY(hint != nullptr);
    QVERIFY(hint->text().contains(QStringLiteral("standard fees"), Qt::CaseInsensitive) ||
            hint->text().contains(QStringLiteral("frais standard"), Qt::CaseInsensitive));
    QVERIFY(hint->text().contains(QStringLiteral("storage rent"), Qt::CaseInsensitive) ||
            hint->text().contains(QStringLiteral("loyer de stockage"), Qt::CaseInsensitive));

    // 2. Transfer review must explicitly identify Available Balance and irreversible finality
    wallet->openSend(QStringLiteral("bobby.cybou"));
    auto* amount = wallet->findChild<QLineEdit*>(QStringLiteral("walletAmount"));
    QVERIFY(amount != nullptr);
    amount->setText(QStringLiteral("50"));

    // Click Review
    QPushButton* confirm_btn = nullptr;
    for (auto* btn : wallet->findChildren<QPushButton*>()) {
        if (btn->property("cybouId").toString() == QLatin1String("walletConfirm")) {
            confirm_btn = btn;
            break;
        }
    }
    QVERIFY(confirm_btn != nullptr);
    QVERIFY(confirm_btn->isEnabled());
    confirm_btn->click();

    // Check review text
    auto* review_label = wallet->findChild<QLabel*>(QStringLiteral("walletReview"));
    QVERIFY(review_label != nullptr);
    QVERIFY(review_label->isVisible());
    QVERIFY(review_label->text().contains(QStringLiteral("Available Balance"), Qt::CaseInsensitive) ||
            review_label->text().contains(QStringLiteral("solde disponible"), Qt::CaseInsensitive));
    QVERIFY(review_label->text().contains(QStringLiteral("cannot be reversed"), Qt::CaseInsensitive) ||
            review_label->text().contains(QStringLiteral("irréversibles"), Qt::CaseInsensitive) ||
            review_label->text().contains(QStringLiteral("final"), Qt::CaseInsensitive));
}

void CybouShellTests::assuranceAndRestoreResponsiveness()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("empty")));
    window->showPage(CybouPage::Home);
    window->show();
    auto* home = dynamic_cast<HomePage*>(window->page(CybouPage::Home));
    QVERIFY(home);

    auto* onboarding = dynamic_cast<OnboardingView*>(home->findChild<QWidget*>(QStringLiteral("onboarding")));
    QVERIFY(onboarding != nullptr);

    // 1. Check Restore welcome button has no protocol jargon ("mnemonic")
    QPushButton* restore_btn = nullptr;
    for (auto* btn : onboarding->findChildren<QPushButton*>()) {
        if (btn->property("cybouId").toString() == QLatin1String("restoreIdentity")) {
            restore_btn = btn;
            break;
        }
    }
    QVERIFY(restore_btn != nullptr);
    QVERIFY(!restore_btn->text().contains(QStringLiteral("mnemonic"), Qt::CaseInsensitive));
    QVERIFY(restore_btn->text().contains(QStringLiteral("recovery phrase"), Qt::CaseInsensitive) ||
            restore_btn->text().contains(QStringLiteral("phrase de récupération"), Qt::CaseInsensitive));

    // 2. Open restore screen and test Enter key navigation through the 24 words to password fields
    restore_btn->click();
    QCOMPARE(onboarding->screen(), OnboardingView::Screen::Restore);

    auto* word0 = onboarding->findChild<QLineEdit*>(QStringLiteral("recoveryWord0"));
    auto* word23 = onboarding->findChild<QLineEdit*>(QStringLiteral("recoveryWord23"));
    auto* password = onboarding->findChild<QLineEdit*>(QStringLiteral("restorePassword"));
    auto* confirm = onboarding->findChild<QLineEdit*>(QStringLiteral("restorePasswordConfirm"));
    QVERIFY(word0 != nullptr);
    QVERIFY(word23 != nullptr);
    QVERIFY(password != nullptr);
    QVERIFY(confirm != nullptr);

    // Enter on word 23 must advance focus to restorePassword
    word23->setFocus();
    QTest::keyClick(word23, Qt::Key_Return);
    QCOMPARE(window->focusWidget(), password);

    // Enter on restorePassword must advance focus to restorePasswordConfirm
    QTest::keyClick(password, Qt::Key_Return);
    QCOMPARE(window->focusWidget(), confirm);

    // 3. StoragePage encryption copy: no "recovery capsules", clear recovery phrase copy
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("files")));
    window->showPage(CybouPage::Files);
    auto* files = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(files);
    files->showDetails(QStringLiteral("f-report"));
    QVERIFY(files->isDetailsVisible());

    bool found_clean_encryption = false;
    for (const auto* label : files->findChildren<QLabel*>()) {
        const QString text = label->text();
        QVERIFY(!text.contains(QStringLiteral("recovery capsules"), Qt::CaseInsensitive));
        if (text.contains(QStringLiteral("recovery phrase"), Qt::CaseInsensitive) ||
            text.contains(QStringLiteral("phrase de récupération"), Qt::CaseInsensitive)) {
            found_clean_encryption = true;
        }
    }
    QVERIFY(found_clean_encryption);

    // 4. IdentityPage recovery copy: "Configured in vault" instead of vague "Secured"
    window->showPage(CybouPage::Identity);
    auto* identity = dynamic_cast<IdentityPage*>(window->page(CybouPage::Identity));
    QVERIFY(identity);
    bool found_vault_configured = false;
    for (const auto* label : identity->findChildren<QLabel*>()) {
        const QString text = label->text();
        if (text.contains(QStringLiteral("Configured in vault"), Qt::CaseInsensitive) ||
            text.contains(QStringLiteral("dans le coffre-fort"), Qt::CaseInsensitive)) {
            found_vault_configured = true;
        }
    }
    QVERIFY(found_vault_configured);
}

void CybouShellTests::networkPageAndSchematicFranceMap()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));

    // 1. Navigate to NetworkPage
    window->showPage(CybouPage::Network);
    QCOMPARE(window->currentPageIndex(), static_cast<int>(CybouPage::Network));

    auto* net_page = dynamic_cast<NetworkPage*>(window->page(CybouPage::Network));
    QVERIFY(net_page != nullptr);
    QVERIFY(net_page->mapWidget() != nullptr);
    QVERIFY(net_page->tableWidget() != nullptr);
    QVERIFY(net_page->detailsWidget() != nullptr);

    // 2. Feed structured network diagnostics with known France and LAN peers
    cybou::NodeDiagnosticsSnapshot snap;
    snap.height = 1500;
    snap.storage_used = 1024 * 1024 * 50;
    snap.storage_capacity = 1024 * 1024 * 100;
    snap.local_storage_used = 1024 * 1024 * 150;
    snap.local_storage_capacity = 1024 * 1024 * 300;
    snap.peers = {
        {"51.255.46.58:29461", 1500, "c7b20e0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d"},
        {"127.0.0.1:29462", 1498, "d8a30e0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d"},
        {"192.168.1.50:29461", 1495, ""}
    };
    model->setNetworkDiagnostics(snap);
    model->setNodeStatus(true, 3, true);
    model->setFinalizedHeight(1500);

    // Verify peer count & table rows
    QTRY_COMPARE(net_page->peerCount(), 3);
    auto* table = net_page->tableWidget();
    QCOMPARE(table->rowCount(), 3);

    // Check peer classifications
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("51.255.46.58:29461"));
    QVERIFY(table->item(0, 1)->text().contains(QStringLiteral("France"), Qt::CaseInsensitive));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("1,500"));
    QVERIFY(table->item(0, 3)->text().contains(QStringLiteral("0"), Qt::CaseInsensitive));

    // Peer 1 is loopback LAN
    QCOMPARE(table->item(1, 0)->text(), QStringLiteral("127.0.0.1:29462"));
    QVERIFY(table->item(1, 1)->text().contains(QStringLiteral("LAN"), Qt::CaseInsensitive));
    QCOMPARE(table->item(1, 2)->text(), QStringLiteral("1,498"));
    QVERIFY(table->item(1, 3)->text().contains(QStringLiteral("2 blocks"), Qt::CaseInsensitive));

    // Peer 2 is RFC1918 LAN with pending StorageId proof
    QCOMPARE(table->item(2, 0)->text(), QStringLiteral("192.168.1.50:29461"));
    QVERIFY(table->item(2, 1)->text().contains(QStringLiteral("LAN"), Qt::CaseInsensitive));
    QVERIFY(table->item(2, 4)->text().contains(QStringLiteral("Pending"), Qt::CaseInsensitive));

    // 3. Selection synchronization: select peer 0
    net_page->selectPeer(0);
    QCOMPARE(net_page->selectedPeerIndex(), 0);
    QCOMPARE(net_page->mapWidget()->selectedPeer(), 0);

    // Normal peer cards show illustrative admission, without endpoint/key/city detail.
    QVERIFY(net_page->detailsWidget()->parentWidget() == net_page->mapWidget());
    QTRY_VERIFY(net_page->detailsWidget()->height() > 160);
    for (const auto* label : net_page->detailsWidget()->findChildren<QLabel*>()) {
        QVERIFY(!label->text().contains(QStringLiteral("51.255.46.58")));
        QVERIFY(!label->text().contains(QStringLiteral("c7b20e010203")));
        QVERIFY(!label->text().contains(QStringLiteral("Paris")));
    }
    net_page->showAdvanced();
    auto* advanced = net_page->findChild<QWidget*>(QStringLiteral("networkAdvanced"));
    QVERIFY(advanced->isAncestorOf(net_page->detailsWidget()));
    QCOMPARE(net_page->selectedPeerIndex(), 0);
    QCOMPARE(net_page->mapWidget()->selectedPeer(), 0);
    // Advanced retains full un-truncated StorageId evidence.
    bool found_full_sid = false;
    for (const auto* label : net_page->detailsWidget()->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("c7b20e0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d"))) {
            found_full_sid = true;
        }
    }
    QVERIFY(found_full_sid);

    // An ahead announcement is unverified, never evidence of synchronization.
    snap.peers[0].advertised_height = 1600;
    model->setNetworkDiagnostics(snap);
    QTRY_VERIFY(table->item(0, 3)->text().contains(QStringLiteral("ahead")));
    net_page->selectPeer(0);
    bool found_ahead = false;
    for (const auto* label : net_page->detailsWidget()->findChildren<QLabel*>())
        if (label->text().contains(QStringLiteral("100 blocks ahead of local tip (unverified)"))) found_ahead = true;
    QVERIFY(found_ahead);
    snap.peers[0].advertised_height = 1500;
    model->setNetworkDiagnostics(snap);
    QTRY_VERIFY(table->item(0, 3)->text().contains(QStringLiteral("same height")));

    // 4. Select peer 1 via map click
    net_page->mapWidget()->on_peer_clicked(1);
    QCOMPARE(net_page->selectedPeerIndex(), 1);
    QCOMPARE(table->currentRow(), 1);
    bool found_lag_in_details = false;
    for (const auto* label : net_page->detailsWidget()->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("2 blocks behind"), Qt::CaseInsensitive)) {
            found_lag_in_details = true;
        }
    }
    QVERIFY(found_lag_in_details);

    auto* toggle = net_page->findChild<QPushButton*>(QStringLiteral("networkAdvancedButton"));
    toggle->setChecked(false);
    QVERIFY(net_page->detailsWidget()->parentWidget() == net_page->mapWidget());
    QCOMPARE(net_page->selectedPeerIndex(), 1);
    QVERIFY(!net_page->detailsWidget()->isHidden());

    // Rebuilding the details must replace their labels, not pile them up:
    // leaked rows once reached thousands of siblings and overflowed painting.
    const auto detail_labels = net_page->detailsWidget()->findChildren<QLabel*>().size();
    for (int i = 0; i < 50; ++i) net_page->selectPeer(i % 3);
    net_page->selectPeer(1);
    QCOMPARE(net_page->detailsWidget()->findChildren<QLabel*>().size(), detail_labels);

    // A lost session remains a bounded known observation, never a current height.
    snap.peers.clear();
    model->setNetworkDiagnostics(snap);
    model->setNodeStatus(false, 0, false);
    QCOMPARE(net_page->peerCount(), 3);
    QCOMPARE(table->rowCount(), 3);
    QTRY_VERIFY(table->item(0, 1)->text().contains(QStringLiteral("disconnected")));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Unknown"));
    QCOMPARE(table->item(0, 3)->text(), QStringLiteral("Unknown"));
    QVERIFY(advanced->isHidden());
    net_page->showAdvanced();
    QVERIFY(!advanced->isHidden());
    snap.network_binding = "different-network";
    model->setNetworkDiagnostics(snap);
    QTRY_COMPARE(net_page->peerCount(), 0);
    QVERIFY(net_page->detailsWidget()->isHidden());

}

void CybouShellTests::authorityExplorerAndEvidenceWorkspace()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    model->setIdentityState(CybouIdentityState::Active,QStringLiteral("authority"),1);
    QVERIFY(model);

    // 1. Setup authorized state with candidate operations and diagnostic history
    CybouNetworkAuthorityStatus authority;
    authority.proven = true;
    authority.signer_enabled = true;
    authority.finalizer = CybouFinalizerState::Finalizing;
    authority.finalized_height = 42;
    authority.safety_journal_status = QStringLiteral("Observed durable signing journal status");
    authority.next_settlement_period = 3;
    authority.next_settlement_due_utc = 86400;
    for (int i = 0; i < 12; ++i) {
        authority.candidate_ids.append(QStringLiteral("cand_%1_%2").arg(i).arg(QString(60, QLatin1Char{'c'})));
    }
    authority.candidates = authority.candidate_ids.size();
    model->setNetworkAuthority(authority);

    cybou::NodeDiagnosticsSnapshot diag;
    diag.initialized = true;
    diag.height = 42;
    diag.tip = std::string(64, 't');
    for (int i = 0; i < 5; ++i) {
        cybou::OperationDiagnostics op;
        op.operation_id = std::string("op_") + std::to_string(i) + "_" + std::string(59, 'f');
        op.finalized_height = 40 + i;
        op.state = static_cast<std::uint32_t>(cybou::OperationStatusKind::FINALIZED);
        diag.operations.push_back(op);
    }
    model->setNetworkDiagnostics(diag);

    auto* page = dynamic_cast<NetworkAuthorityPage*>(window->page(CybouPage::NetworkAuthority));
    QVERIFY(page);

    // Total = 12 candidates + 5 finalized ops + 1 block tip = 18 items
    QCOMPARE(page->explorerItemCount(), 18);
    diag.operations.push_back({"rejected-local",static_cast<unsigned>(cybou::OperationStatusKind::REJECTED_KNOWN),0});
    model->setNetworkDiagnostics(diag);
    QCOMPARE(page->explorerItemCount(),18);
    QVERIFY(!page->explorerDetailWidget()->findChild<QLabel*>()->text().contains(QStringLiteral("rejected-local")));
    QVERIFY(!window->findChild<QWidget*>(QStringLiteral("authorityAdministrationSection"))->isHidden());

    QCOMPARE(page->explorerPage(), 1);
    QCOMPARE(page->explorerTable()->rowCount(), 10);

    // 2. Pagination forward
    auto* next_btn = page->findChild<QPushButton*>(QStringLiteral("authorityExplorerNext"));
    QVERIFY(next_btn);
    next_btn->click();
    QCOMPARE(page->explorerPage(), 2);
    QCOMPARE(page->explorerTable()->rowCount(), 8);

    // 3. Selection reveals un-truncated identifier
    page->selectExplorerItem(0);
    QCOMPARE(page->explorerPage(), 1);
    bool found_full_id = false;
    for (const auto* label : page->explorerDetailWidget()->findChildren<QLabel*>()) {
        if (label->text() == authority.candidate_ids.first()) {
            found_full_id = true;
        }
    }
    QVERIFY(found_full_id);

    // 4. Filtering
    page->setExplorerFilter(QStringLiteral("cand_1_"));
    QVERIFY(page->explorerItemCount() >= 1);
    page->setExplorerFilter({});
    QCOMPARE(page->explorerItemCount(), 18);

    // 5. Evidence labels and authorized notice
    bool found_safety_journal = false;
    bool found_settlement_readiness = false;
    bool found_auth_notice = false;
    for (const auto* label : page->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("Observed durable signing journal status"), Qt::CaseInsensitive)) {
            found_safety_journal = true;
        }
        if (label->text().contains(QStringLiteral("actual payout entries from local off-chain"), Qt::CaseInsensitive)) {
            found_settlement_readiness = true;
        }
        if (label->text().contains(QStringLiteral("Genesis-authorized PoA signing key is active"), Qt::CaseInsensitive)) {
            found_auth_notice = true;
        }
    }
    QVERIFY(found_safety_journal);
    QVERIFY(found_settlement_readiness);
    QVERIFY(found_auth_notice);

    // 6. Idle chain reassurance & read-only guard when signer is unavailable
    authority.candidate_ids.clear();
    authority.candidates = 0;
    authority.signer_enabled = false;
    model->setNetworkAuthority(authority);

    bool found_idle_reassurance = false;
    bool found_read_only_notice = false;
    for (const auto* label : page->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("Idle chain: Pool is empty"), Qt::CaseInsensitive)) {
            found_idle_reassurance = true;
        }
        if (label->text().contains(QStringLiteral("Read-only console"), Qt::CaseInsensitive)) {
            found_read_only_notice = true;
        }
    }
    QVERIFY(found_idle_reassurance);
    QVERIFY(found_read_only_notice);

    auto find_button = [page](const char* id) -> QPushButton* {
        for (auto* btn : page->findChildren<QPushButton*>()) {
            if (btn->property("cybouId").toString() == QLatin1String{id}) return btn;
        }
        return nullptr;
    };
    auto* pause = find_button("authorityPause");
    auto* settle = find_button("authoritySettleStorage");
    QVERIFY(pause);
    QVERIFY(settle);
    QVERIFY(!pause->isEnabled());
    QVERIFY(!settle->isEnabled());
}

void CybouShellTests::ownContentInspectorAndBoundedConsole()
{
    auto window = makeWindow();
    window->show();
    auto* model = window->desktopModel();
    QVERIFY(model);

    // 1. Setup active Identity and own file with known size and content root ID
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("acct-1"), 100);
    model->setPrimaryName(QStringLiteral("alice.cybou"));

    CybouFileItem file1;
    file1.id = QStringLiteral("f_own_1");
    file1.name = QStringLiteral("contract.pdf");
    file1.logical_size = 1500000; // ~1.43 MiB -> ceil(1500000 / 524288) = 3 chunks
    file1.state = CybouContentState::Protected;
    file1.available_offline = true;
    file1.min_remote_replicas = 2;
    file1.remote_replica_target = 2;
    file1.content_root_id = QStringLiteral("d41d8cd98f00b204e9800998ecf8427e00000000000000000000000000000001");
    model->setFileItems({file1});

    // 2. StoragePage inspection in Advanced section
    window->showPage(CybouPage::Files);
    auto* storage = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(storage);
    storage->showDetails(file1.id, true);
    QVERIFY(storage->isDetailsVisible());
    QVERIFY(storage->isDetailsAdvanced());

    bool found_chunk_count = false;
    bool found_integrity_evidence = false;
    bool found_auth_reference = false;
    for (const auto* label : storage->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("3 chunks (512 KiB unit)"))) {
            found_chunk_count = true;
        }
        if (label->text().contains(QStringLiteral("BLAKE3 Merkle tree"), Qt::CaseInsensitive)) {
            found_integrity_evidence = true;
        }
        if (label->text().contains(QStringLiteral("RootPublication"), Qt::CaseInsensitive)) {
            found_auth_reference = true;
        }
    }
    QVERIFY(found_chunk_count);
    QVERIFY(found_integrity_evidence);
    QVERIFY(found_auth_reference);

    auto* inspect_btn = storage->findChild<QPushButton*>(QStringLiteral("inspectChunkTreeButton"));
    QVERIFY(inspect_btn);

    // 3. Diagnostics page button verification
    window->showNetworkDiagnostics();
    auto* diag_page = window->page(CybouPage::Network);
    QVERIFY(diag_page);
    auto* console_open_btn = diag_page->findChild<QToolButton*>(QStringLiteral("readOnlyConsoleButton"));
    QVERIFY(console_open_btn);

    // 4. Bounded read-only console command execution
    CybouConsoleDialog console{model};

    // help command
    console.executeCommand(QStringLiteral("help"));
    QVERIFY(console.outputText().contains(QStringLiteral("Available read-only commands")));

    // status command
    console.executeCommand(QStringLiteral("status"));
    QVERIFY(console.outputText().contains(model->status().network_name));
    QVERIFY(console.outputText().contains(QStringLiteral("alice.cybou")));

    // storage summary command
    console.executeCommand(QStringLiteral("storage"));
    QVERIFY(console.outputText().contains(QStringLiteral("Own Storage Summary")));
    QVERIFY(console.outputText().contains(QStringLiteral("1 items")));

    // files list command
    console.executeCommand(QStringLiteral("files"));
    QVERIFY(console.outputText().contains(QStringLiteral("contract.pdf")));

    // single file info command
    console.executeCommand(QStringLiteral("file f_own_1"));
    QVERIFY(console.outputText().contains(file1.content_root_id));
    QVERIFY(console.outputText().contains(QStringLiteral("Billing units: 3")));

    // Only real semantic evidence may be reported, never invented chunk digests.
    console.clearOutput();
    console.executeCommand(QStringLiteral("chunks f_own_1"));
    QVERIFY(console.outputText().contains(file1.content_root_id));
    QVERIFY(console.outputText().contains(QStringLiteral("No integrity check was performed")));
    QVERIFY(!console.outputText().contains(QStringLiteral("Verified")));
    QVERIFY(!console.outputText().contains(QStringLiteral("Leaf:")));

    // Real own-content chunk diagnostics when evidence is available
    CybouFileChunkDiagnostics test_diag;
    test_diag.available = true;
    test_diag.file_id = QStringLiteral("f_own_1");
    test_diag.file_name = QStringLiteral("contract.pdf");
    test_diag.root_chunk_id = file1.content_root_id;
    test_diag.chunk_count = 3;
    test_diag.local_count = 3;
    test_diag.verified_count = 3;
    test_diag.chunks = {
        {QStringLiteral("chunk_a"), true, true},
        {QStringLiteral("chunk_b"), true, true},
        {QStringLiteral("chunk_c"), true, true}
    };
    test_diag.retrieval_diagnosis = QStringLiteral("All chunks locally present and verified");
    model->setFixtureChunkDiagnostics(QStringLiteral("f_own_1"), test_diag);

    console.clearOutput();
    console.executeCommand(QStringLiteral("chunks f_own_1"));
    QVERIFY(console.outputText().contains(QStringLiteral("Chunk Tree & Integrity Diagnostics")));
    QVERIFY(console.outputText().contains(QStringLiteral("Verified (BLAKE3-256): 3")));
    QVERIFY(console.outputText().contains(QStringLiteral("All chunks locally present and verified")));
    QVERIFY(console.outputText().contains(QStringLiteral("chunk_a")));
    QVERIFY(console.outputText().contains(QStringLiteral("Verified (BLAKE3-256)")));

    // Blockchain block inspection
    CybouBlockExplorerInfo binfo;
    binfo.found = true;
    binfo.height = 42;
    binfo.block_id = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000042");
    binfo.parent_block_id = QStringLiteral("0000000000000000000000000000000000000000000000000000000000000041");
    binfo.state_root = QStringLiteral("1111111111111111111111111111111111111111111111111111111111111111");
    binfo.has_poa_certificate = true;
    binfo.operation_count = 1;
    binfo.operation_ids = {QStringLiteral("op_hash_42")};
    model->setFixtureBlock(QStringLiteral("42"), binfo);

    console.clearOutput();
    console.executeCommand(QStringLiteral("block 42"));
    QVERIFY(console.outputText().contains(QStringLiteral("Block Height 42")));
    QVERIFY(console.outputText().contains(QStringLiteral("Verified PoA signature")));
    QVERIFY(console.outputText().contains(QStringLiteral("op_hash_42")));

    // Operation lookup
    CybouOperationExplorerInfo opinfo;
    opinfo.found = true;
    opinfo.operation_id = QStringLiteral("op_hash_42");
    opinfo.state = QStringLiteral("Finalized");
    opinfo.height = 42;
    opinfo.index = 0;
    opinfo.kind = QStringLiteral("Payment");
    model->setFixtureOperation(QStringLiteral("op_hash_42"), opinfo);

    console.clearOutput();
    console.executeCommand(QStringLiteral("op op_hash_42"));
    QVERIFY(console.outputText().contains(QStringLiteral("Operation op_hash_42")));
    QVERIFY(console.outputText().contains(QStringLiteral("Finalized")));
    QVERIFY(console.outputText().contains(QStringLiteral("Payment")));

    // Blockchain history
    CybouHistoryItem hitem;
    hitem.height = 42;
    hitem.block_id = binfo.block_id;
    hitem.state_root = binfo.state_root;
    hitem.operation_count = 1;
    model->setFixtureHistory({hitem});

    console.clearOutput();
    console.executeCommand(QStringLiteral("history 1"));
    QVERIFY(console.outputText().contains(QStringLiteral("Blockchain History (Page 1)")));
    QVERIFY(console.outputText().contains(QStringLiteral("Height 42")));

    // peers and jobs commands
    console.executeCommand(QStringLiteral("peers"));
    console.executeCommand(QStringLiteral("jobs"));

    // 5. Restriction against shell / mutation commands
    console.executeCommand(QStringLiteral("rm -rf /"));
    QVERIFY(console.outputText().contains(QStringLiteral("Error: Command 'rm' is not recognized or not permitted")));

    console.executeCommand(QStringLiteral("exec drop table;"));
    QVERIFY(console.outputText().contains(QStringLiteral("Error: Command 'exec' is not recognized or not permitted")));

    QVERIFY(console.historyCount() >= 4);

    // 6. Private session history and output cleanup on lock
    model->setIdentityState(CybouIdentityState::None);
    QCOMPARE(console.historyCount(), 0);
    QVERIFY(console.outputText().contains(QStringLiteral("Vault locked. Private session history and output cleared.")));
}

void CybouShellTests::consoleTranslationsPermissionsAndBounds()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(!window->findChild<QDialog*>(QStringLiteral("CYBOUDiagnostics")));
    window->showNetworkDiagnostics();
    QVERIFY(!window->page(CybouPage::Network)->findChild<QPushButton*>(QStringLiteral("diagnosticsWindowButton")));
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("own-account"), 1);
    model->setPrimaryName(QStringLiteral("cybou.cybou")); // A name never grants signing authority.
    cybou::NodeDiagnosticsSnapshot snapshot;
    snapshot.initialized = true;
    snapshot.height = 42;
    snapshot.network_binding = "network-binding";
    snapshot.tip = "verified-tip";
    snapshot.state_root = "verified-state-root";
    snapshot.local_storage_used = 1234;
    snapshot.local_storage_capacity = 16106127360ULL;
    snapshot.storage_used = 456;
    snapshot.storage_capacity = 10737418240ULL;
    snapshot.operations.push_back({"operation-1", 1, 42});
    model->setNetworkDiagnostics(snapshot);
    CybouFileItem file;
    file.id = QStringLiteral("own-file"); file.name = QStringLiteral("own-file.txt");
    file.logical_size = 123; file.min_remote_replicas = -1; file.remote_replica_target = -1;
    model->setFileItems({file});
    CybouConsoleDialog console{model};
    console.executeCommand(QStringLiteral("help"));
    QVERIFY(!console.outputText().contains(QStringLiteral("authority [")));
    console.executeCommand(QStringLiteral("authority totals"));
    QVERIFY(console.outputText().contains(QStringLiteral("not recognized or not permitted")));
    console.clearOutput();
    console.executeCommand(QStringLiteral("file own-file"));
    QVERIFY(console.outputText().contains(QStringLiteral("Remote Replicas: — of —")));
    console.executeCommand(QStringLiteral("status extra"));
    QVERIFY(console.outputText().contains(QStringLiteral("Usage: status")));
    console.executeCommand(QStringLiteral("network"));
    QVERIFY(console.outputText().contains(QStringLiteral("verified-state-root")));
    console.executeCommand(QStringLiteral("operations"));
    QVERIFY(console.outputText().contains(QStringLiteral("operation-1")));

    console.clearOutput();
    console.executeCommand(QStringLiteral("health"));
    QVERIFY(console.outputText().contains(QStringLiteral("Node uptime: Unknown")));
    console.clearOutput(); console.executeCommand(QStringLiteral("metrics"));
    QVERIFY(console.outputText().contains(QStringLiteral("resident memory: Unknown bytes")));
    QVERIFY(console.outputText().contains(QStringLiteral("Local process CPU: Unknown")));
    console.clearOutput(); console.executeCommand(QStringLiteral("capacity"));
    QVERIFY(console.outputText().contains(QStringLiteral("OS available disk: Unknown bytes")));
    snapshot.observed_unix_ms = 1791460800000ULL;
    snapshot.uptime_ms = 120000;
    snapshot.process_resident_bytes = 1048576;
    snapshot.storage_disk_available = 1048576;
    snapshot.process_cpu = {.interval_percent = 12.5, .mean_percent = 10.0, .processors = 4,
        .interval_ms = 3000, .mean_window_ms = 60000, .mean_intervals = 20, .mean_age_ms = 2000};
    snapshot.pending_operations = 3;
    snapshot.pending_operation_bytes = 700;
    snapshot.finalization.windows[0] = {.window_ms = 60000, .observed_operations = 5,
        .local_produced_operations = 3, .history_operations = 1000, .complete = true};
    snapshot.traffic = {.received_bytes = 6000, .sent_bytes = 12000,
        .window_received_bytes = 6000, .window_sent_bytes = 12000, .window_ms = 60000};
    snapshot.storage_transfers.put = {.received_bytes = 60, .sent_bytes = 120,
        .window_received_bytes = 60, .window_sent_bytes = 120, .window_ms = 60000};
    model->setNetworkDiagnostics(snapshot);
    console.clearOutput();
    console.executeCommand(QStringLiteral("health"));
    QVERIFY(console.outputText().contains(QStringLiteral("Node uptime: 120 s")));
    QVERIFY(console.outputText().contains(QStringLiteral("3 operations / 700 bytes")));
    console.executeCommand(QStringLiteral("health extra"));
    QVERIFY(console.outputText().contains(QStringLiteral("Usage: health")));
    console.clearOutput(); console.executeCommand(QStringLiteral("metrics"));
    QVERIFY(console.outputText().contains(QStringLiteral("Received since runtime start: 6000 bytes")));
    QVERIFY(console.outputText().contains(QStringLiteral("Local PUT payload")));
    QVERIFY(console.outputText().contains(QStringLiteral("provider receipt verified")));
    QVERIFY(console.outputText().contains(QStringLiteral("↓ ") + QLocale{}.toString(1.0, 'f', 1)));
    QVERIFY(console.outputText().contains(QStringLiteral("resident memory: 1048576 bytes")));
    QVERIFY(console.outputText().contains(QStringLiteral("Local process CPU: ") + QLocale{}.toString(12.5, 'f', 1)));
    QVERIFY(console.outputText().contains(QStringLiteral("60000 ms / 20 intervals; age 2000 ms")));
    console.executeCommand(QStringLiteral("capacity"));
    QVERIFY(console.outputText().contains(QStringLiteral("OS available disk: 1048576 bytes")));
    QVERIFY(console.outputText().contains(QStringLiteral("Policy headroom: 16106126126 bytes")));
    console.executeCommand(QStringLiteral("capacity extra"));
    QVERIFY(console.outputText().contains(QStringLiteral("Usage: capacity")));
    QVERIFY(console.outputText().contains(QStringLiteral("1 min: observed ") + QLocale{}.toString(5.0, 'f', 1)));
    QVERIFY(console.outputText().contains(QStringLiteral("history 1000 operations")));
    QVERIFY(console.outputText().contains(QStringLiteral("5 min: observed Unknown")));
    QVERIFY(console.outputText().contains(QStringLiteral("Received rate: ") + QLocale{}.toString(100.0, 'f', 1)));

    CybouNetworkAuthorityStatus authority;
    authority.proven = true; authority.finalizer = CybouFinalizerState::Finalizing;
    authority.candidates = 2; authority.candidate_ids = {QStringLiteral("candidate-1"), QStringLiteral("candidate-2")};
    authority.candidate_wait_seconds = {15, 5};
    authority.oldest_candidate_age_seconds = 15;
    authority.safety_journal_status = QStringLiteral("Fail-closed durable append-only journal active.");
    authority.settlement_due = true;
    authority.next_settlement_period = 3;
    authority.preview_payouts_count = 4;
    authority.preview_payouts_amount = 80000;
    authority.total_balance = 12345; authority.finalized_height = 42;
    model->setNetworkAuthority(authority);

    console.clearOutput(); console.executeCommand(QStringLiteral("help"));
    QVERIFY(console.outputText().contains(QStringLiteral("authority [status|candidates|totals|settle]")));

    console.executeCommand(QStringLiteral("authority status"));
    QVERIFY(console.outputText().contains(QStringLiteral("Local finalizer: Finalizing")));
    QVERIFY(console.outputText().contains(QStringLiteral("queue age: 15 s"), Qt::CaseInsensitive));
    QVERIFY(console.outputText().contains(QStringLiteral("Fail-closed durable append-only journal active.")));
    QVERIFY(console.outputText().contains(QStringLiteral("Due now (period 3, 4 payouts, ") + cybouAmountText(80000) + QLatin1Char{')'}));

    console.executeCommand(QStringLiteral("authority candidates"));
    QVERIFY(console.outputText().contains(QStringLiteral("candidate-1 (waiting 15 s)")));
    QVERIFY(console.outputText().contains(QStringLiteral("candidate-2 (waiting 5 s)")));

    console.executeCommand(QStringLiteral("authority totals"));
    QVERIFY(console.outputText().contains(QStringLiteral("12345 CYBOU")));

    // The console is read-only: control commands point to the Central Authority page.
    for (const auto& command : {QStringLiteral("authority pause"), QStringLiteral("authority resume"),
                                QStringLiteral("authority finalize")}) {
        console.clearOutput();
        console.executeCommand(command);
        QVERIFY(console.outputText().contains(QStringLiteral("read-only")));
    }

    // Settlement is previewed, never submitted, from the console.
    console.clearOutput();
    console.executeCommand(QStringLiteral("authority settle"));
    QVERIFY(console.outputText().contains(QStringLiteral("Storage settlement preview for period 3")));
    QVERIFY(console.outputText().contains(QStringLiteral("Central Authority page")));
    console.clearOutput();
    console.executeCommand(QStringLiteral("authority settle confirm"));
    QVERIFY(!console.outputText().contains(QStringLiteral("submitted")));

    // Revoking authority clears output and history
    model->setNetworkAuthority({});
    QVERIFY(!console.outputText().contains(QStringLiteral("candidate-1")));
    QCOMPARE(console.historyCount(), 0);

    // Reproduce the user's French locale: formerly blank multiline translations.
    QTranslator translator;
    QVERIFY(translator.load(QStringLiteral(":/i18n/cybou_fr.qm")));
    qApp->installTranslator(&translator);
    for (const auto& cmd : {QStringLiteral("help"), QStringLiteral("status"), QStringLiteral("storage")}) {
        console.clearOutput(); console.executeCommand(cmd);
        const QString result = console.outputText().section(QLatin1Char('\n'), 1).trimmed();
        QVERIFY2(!result.isEmpty(), qPrintable(cmd));
    }
    console.clearOutput(); console.executeCommand(QStringLiteral("help"));
    QVERIFY(console.outputText().contains(QStringLiteral("Commandes disponibles")));
    console.executeCommand(QStringLiteral("storage"));
    QVERIFY(console.outputText().contains(QStringLiteral("1234 / 16106127360")));
    console.clearOutput(); console.executeCommand(QStringLiteral("health"));
    QVERIFY(console.outputText().contains(QStringLiteral("Temps de fonctionnement : 120 s")));
    QVERIFY(console.outputText().contains(QStringLiteral("3 opérations / 700 octets")));
    console.clearOutput(); console.executeCommand(QStringLiteral("metrics"));
    QVERIFY(console.outputText().contains(QStringLiteral("Reçu depuis le démarrage : 6000 octets")));
    QVERIFY(console.outputText().contains(QStringLiteral("Données PUT locales")));
    QVERIFY(console.outputText().contains(QStringLiteral("réception distante inconnue")));
    QVERIFY(console.outputText().contains(QStringLiteral("Mémoire résidente du processus CYBOU local : 1048576 octets")));
    QVERIFY(console.outputText().contains(QStringLiteral("CPU du processus local : ") + QLocale{}.toString(12.5, 'f', 1)));
    console.executeCommand(QStringLiteral("capacity"));
    QVERIFY(console.outputText().contains(QStringLiteral("Disque disponible selon le système : 1048576 octets")));
    QVERIFY(console.outputText().contains(QStringLiteral("historique 1000 opérations")));
    qApp->removeTranslator(&translator);

    QVector<CybouFileItem> many;
    for (int i = 0; i < 250; ++i) { auto row = file; row.id = QString::number(i); row.name = QStringLiteral("row-%1").arg(i); many.push_back(row); }
    model->setFileItems(many);
    console.clearOutput(); console.executeCommand(QStringLiteral("files"));
    QVERIFY(console.outputText().contains(QStringLiteral("row-99")));
    QVERIFY(!console.outputText().contains(QStringLiteral("row-100")));
    QVERIFY(console.outputText().contains(QStringLiteral("limited to 100 rows")));
    for (int i = 0; i < 120; ++i) console.executeCommand(QStringLiteral("files %1").arg(i));
    QCOMPARE(console.historyCount(), 100);
    QVERIFY(console.findChild<QPlainTextEdit*>(QStringLiteral("consoleOutput"))->blockCount() <= 500);
    auto* input = console.findChild<QLineEdit*>(QStringLiteral("consoleInput"));
    input->setText(QStringLiteral("private search"));
    model->setIdentityState(CybouIdentityState::Locked);
    QVERIFY(input->text().isEmpty());
    QCOMPARE(console.historyCount(), 0);
    console.executeCommand(QStringLiteral("files"));
    QVERIFY(console.outputText().contains(QStringLiteral("Unlock your Identity")));
    QVERIFY(!console.outputText().contains(QStringLiteral("row-99")));
    console.clearOutput(); console.executeCommand(QStringLiteral("help"));
    QVERIFY(!console.outputText().contains(QStringLiteral("files [filter]")));
    console.clearOutput(); console.executeCommand(QStringLiteral("health"));
    QVERIFY(console.outputText().contains(QStringLiteral("Node uptime: 120 s")));
    snapshot.local_storage_used = snapshot.local_storage_capacity + 1;
    snapshot.storage_used = snapshot.storage_capacity + 1;
    snapshot.storage_disk_available = 0;
    model->setNetworkDiagnostics(snapshot);
    console.clearOutput(); console.executeCommand(QStringLiteral("capacity"));
    QVERIFY(console.outputText().contains(QStringLiteral("Policy headroom: 0 bytes")));
    QVERIFY(console.outputText().contains(QStringLiteral("budget headroom: 0 bytes")));
    QVERIFY(console.outputText().contains(QStringLiteral("OS available disk: 0 bytes")));
    console.executeCommand(QStringLiteral("status"));
    model->setPeerCount(7); // Routine ticks must not clear output while locked.
    QVERIFY(console.outputText().contains(QStringLiteral("> status")));
    console.executeCommand(QStringLiteral("clear"));
    QVERIFY(console.outputText().isEmpty());
    QCOMPARE(console.historyCount(), 0);
}

void CybouShellTests::assuranceLifecycleAndRecoveryGates()
{
    auto window = makeWindow();
    window->show();
    auto* model = window->desktopModel();
    QVERIFY(model);

    // 1. Setup active identity
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("acct-assurance-1"), 50);
    model->setPrimaryName(QStringLiteral("alice.cybou"));

    // 2. Home page hero badge reflects session state "Active", not unqualified "Protected"
    window->showPage(CybouPage::Home);
    auto* home = dynamic_cast<HomePage*>(window->page(CybouPage::Home));
    QVERIFY(home);
    bool found_active_pill = false;
    for (const auto* pill : home->findChildren<QLabel*>()) {
        if (pill->text() == QStringLiteral("Active")) {
            found_active_pill = true;
            break;
        }
    }
    QVERIFY(found_active_pill);

    // 3. Identity page displays honest recovery guidance and scoped post-quantum claim
    window->showPage(CybouPage::Identity);
    auto* id_page = dynamic_cast<IdentityPage*>(window->page(CybouPage::Identity));
    QVERIFY(id_page);
    bool found_recovery_guide = false;
    bool found_pq_scope = false;
    for (const auto* label : id_page->findChildren<QLabel*>()) {
        if (label->text().contains(QStringLiteral("Phrase presence in this vault is not a substitute for a tested restore"))) {
            found_recovery_guide = true;
        }
        if (label->text().contains(QStringLiteral("Hybrid ML-KEM-768 with X25519 for messaging and storage capsules"))) {
            found_pq_scope = true;
        }
    }
    QVERIFY(found_recovery_guide);
    QVERIFY(found_pq_scope);

    // 4. StoragePage: 4 Assurance Pillars in Advanced file details
    CybouFileItem active_file;
    active_file.id = QStringLiteral("f_assure_1");
    active_file.name = QStringLiteral("whitepaper.pdf");
    active_file.logical_size = 1048576; // 1 MiB -> 2 chunks
    active_file.state = CybouContentState::Protected;
    active_file.available_offline = true;
    active_file.min_remote_replicas = 2;
    active_file.remote_replica_target = 2;
    active_file.content_root_id = QStringLiteral("c0ffee0000000000000000000000000000000000000000000000000000000001");

    CybouFileItem trashed_file;
    trashed_file.id = QStringLiteral("f_assure_trash");
    trashed_file.name = QStringLiteral("old_draft.txt");
    trashed_file.logical_size = 12000;
    trashed_file.trashed = true;
    trashed_file.state = CybouContentState::Protected;

    model->setFileItems({active_file, trashed_file});

    window->showPage(CybouPage::Files);
    auto* storage = dynamic_cast<StoragePage*>(window->page(CybouPage::Files));
    QVERIFY(storage);

    // Check active file assurance pillars
    storage->showDetails(active_file.id, true);
    QVERIFY(storage->isDetailsVisible());
    QVERIFY(storage->isDetailsAdvanced());

    bool found_confidentiality = false;
    bool found_integrity = false;
    bool found_availability_scope = false;
    bool found_recovery = false;

    for (const auto* label : storage->findChildren<QLabel*>()) {
        const QString txt = label->text();
        if (txt.contains(QStringLiteral("Hybrid post-quantum encryption before upload (ML-KEM-768 + X25519)"))) {
            found_confidentiality = true;
        }
        if (txt.contains(QStringLiteral("Content-addressed BLAKE3 Merkle tree"))) {
            found_integrity = true;
        }
        if (txt.contains(QStringLiteral("Measured copies: 2 of 2 target")) &&
            txt.contains(QStringLiteral("does not prove independent physical host failure domains"))) {
            found_availability_scope = true;
        }
        if (txt.contains(QStringLiteral("Recoverable on any node using your account recovery phrase via owner self-capsule"))) {
            found_recovery = true;
        }
    }
    QVERIFY(found_confidentiality);
    QVERIFY(found_integrity);
    QVERIFY(found_availability_scope);
    QVERIFY(found_recovery);

    // 5. Trashed file: 5-phase Deletion Lifecycle card
    storage->showDetails(trashed_file.id, false);
    QVERIFY(storage->isDetailsVisible());

    auto* trash_card = storage->findChild<QFrame*>(QStringLiteral("trashLifecycleCard"));
    QVERIFY(trash_card);

    bool found_lifecycle_title = false;
    bool found_phase1 = false;
    bool found_phase2 = false;
    bool found_phase3 = false;
    bool found_phase4_unconfirmed = false;
    bool found_phase5_retained = false;

    for (const auto* label : trash_card->findChildren<QLabel*>()) {
        const QString txt = label->text();
        if (txt.contains(QStringLiteral("Deletion lifecycle"))) {
            found_lifecycle_title = true;
        }
        if (txt.contains(QStringLiteral("Phase 1: Local catalog removal (immediate)"))) {
            found_phase1 = true;
        }
        if (txt.contains(QStringLiteral("Phase 2: Finalized publication revocation"))) {
            found_phase2 = true;
        }
        if (txt.contains(QStringLiteral("Phase 3: Storage lease closure"))) {
            found_phase3 = true;
        }
        if (txt.contains(QStringLiteral("Phase 4: Provider chunk purge (remote acknowledgements unconfirmed)"))) {
            found_phase4_unconfirmed = true;
        }
        if (txt.contains(QStringLiteral("Phase 5: Retained copies"))) {
            found_phase5_retained = true;
        }
    }
    QVERIFY(found_lifecycle_title);
    QVERIFY(found_phase1);
    QVERIFY(found_phase2);
    QVERIFY(found_phase3);
    QVERIFY(found_phase4_unconfirmed);
    QVERIFY(found_phase5_retained);

    // Action buttons in trashed view
    QPushButton* restore_btn{nullptr};
    QPushButton* forever_btn{nullptr};
    for (auto* btn : storage->findChildren<QPushButton*>()) {
        if (btn->property("cybouId") == QLatin1String{"fileRestore"}) restore_btn = btn;
        if (btn->property("cybouId") == QLatin1String{"fileDeleteForever"}) forever_btn = btn;
    }
    QVERIFY(restore_btn);
    QVERIFY(forever_btn);
}




void CybouShellTests::applicationLoadingWaitsForProjection()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    RecordingBackend backend;
    backend.delay_load = true;
    model->setApplicationBackend(&backend);
    model->setIdentityState(CybouIdentityState::Active, QStringLiteral("loading-account"));
    auto* dialog = window->findChild<QDialog*>(QStringLiteral("applicationLoadingDialog"));
    QVERIFY(dialog);
    QVERIFY(dialog->isVisible());
    QVERIFY(dialog->isModal());
    auto* progress = dialog->findChild<QProgressBar*>(QStringLiteral("applicationLoadingProgress"));
    QVERIFY(progress);
    QCOMPARE(progress->maximum(), 0);
    Q_EMIT backend.applicationLoadChanged(CybouApplicationLoadState::Loading, 25, 100, {});
    QCOMPARE(progress->value(), 25);
    auto* background = dialog->findChild<QPushButton*>(QStringLiteral("applicationLoadingLocalButton"));
    QVERIFY(background && background->isVisible());
    background->click();
    QVERIFY(dialog->isHidden());
    auto* banner = window->findChild<QFrame*>(QStringLiteral("backgroundRecoveryBanner"));
    QVERIFY(banner && !banner->isHidden());
    Q_EMIT backend.applicationLoadChanged(CybouApplicationLoadState::Loading, 50, 100, {});
    QVERIFY(dialog->isHidden());
    window->reloadAppearance();
    banner = window->centralWidget()->findChild<QFrame*>(QStringLiteral("backgroundRecoveryBanner"));
    QVERIFY(banner && !banner->isHidden());
    window->centralWidget()->findChild<QToolButton*>(QStringLiteral("backgroundRecoveryDetails"))->click();
    QVERIFY(dialog->isVisible());
    QVERIFY(banner->isHidden());
    Q_EMIT backend.applicationLoadChanged(CybouApplicationLoadState::Loading, 100, 100, {});
    QCOMPARE(progress->value(), 99); // Scan progress is not proof the semantic view was applied.
    QVERIFY(dialog->isVisible());
    dialog->reject();
    QVERIFY(dialog->isVisible());
    background->click();
    QVERIFY(dialog->isHidden());
    Q_EMIT backend.applicationLoadChanged(CybouApplicationLoadState::Failed, 0, 0, QStringLiteral("Load failed"));
    QVERIFY(dialog->isVisible()); // Background mode must never suppress a failure.
    QCOMPARE(dialog->findChild<QLabel*>(QStringLiteral("applicationLoadingStage"))->text(), QStringLiteral("Load failed"));
    Q_EMIT backend.mailSnapshot({});
    Q_EMIT backend.filesSnapshot({});
    Q_EMIT backend.applicationLoadChanged(CybouApplicationLoadState::Ready, 0, 0, {});
    QVERIFY(dialog->isHidden()); // An empty Identity is a valid prepared view, including offline.
    QVERIFY(banner->isHidden());
    model->setIdentityState(CybouIdentityState::Locked, QStringLiteral("loading-account"));
    Q_EMIT backend.applicationLoadChanged(CybouApplicationLoadState::Loading, 0, 100, {});
    QCOMPARE(model->applicationLoadState(), CybouApplicationLoadState::Closed);
    QVERIFY(dialog->isHidden());
    model->setApplicationBackend(nullptr);
}

void CybouShellTests::filesSelectionToolbarFollowsView()
{
    auto window = makeWindow();
    auto* model = window->desktopModel();
    QVERIFY(CybouUiFixtures::apply(*model, QStringLiteral("active")));
    window->showPage(CybouPage::Files);
    auto* files = static_cast<StoragePage*>(window->page(CybouPage::Files));
    auto* table = files->findChild<QTreeWidget*>(QStringLiteral("filesTable"));
    auto* bar = files->findChild<QFrame*>(QStringLiteral("selectionBar"));
    auto* star = files->findChild<QToolButton*>(QStringLiteral("filesSelectionStar"));
    auto* trash = files->findChild<QToolButton*>(QStringLiteral("filesSelectionTrash"));
    auto* restore = files->findChild<QToolButton*>(QStringLiteral("filesSelectionRestore"));
    QVERIFY(table && bar && star && trash && restore);
    table->clearSelection();
    QVERIFY(bar->isHidden());
    QVERIFY(table->topLevelItemCount() > 0);
    table->topLevelItem(0)->setSelected(true);
    QVERIFY(!bar->isHidden());
    QVERIFY(!star->isHidden() && !trash->isHidden() && restore->isHidden());
    QVERIFY(!star->accessibleName().isEmpty());
    QCOMPARE(star->focusPolicy(), Qt::StrongFocus);
    files->setGridMode(true);
    QVERIFY(!bar->isHidden()); // Selection carries into grid mode.
    files->setView(StoragePage::View::Trash);
    files->setGridMode(false);
    QVERIFY(table->topLevelItemCount() > 0);
    table->topLevelItem(0)->setSelected(true);
    QVERIFY(!bar->isHidden());
    QVERIFY(star->isHidden() && trash->isHidden() && !restore->isHidden());
    model->setIdentityState(CybouIdentityState::Locked, QStringLiteral("locked"));
    QVERIFY(bar->isHidden());
}

void CybouShellTests::networkReferenceAndAdvancedScopes()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    NetworkPage page{&model};
    page.resize(1040, 720);
    page.show();
    auto* summary = page.findChild<QLabel*>(QStringLiteral("networkBenchmarkSummary"));
    auto* scope = page.findChild<QLabel*>(QStringLiteral("networkBenchmarkScope"));
    auto* tabs = page.findChild<QTabWidget*>(QStringLiteral("networkAdvancedTabs"));
    QVERIFY(summary && scope && tabs);
    QCOMPARE(tabs->count(), 4);
    QTRY_VERIFY(summary->text().contains(QStringLiteral("Unknown")));
    QFile evidence{QStringLiteral(":/evidence/benchmark.json")};
    QVERIFY(evidence.open(QIODevice::ReadOnly));
    const auto reference = CybouBenchmarkReference::Parse(evidence.readAll());
    cybou::NodeDiagnosticsSnapshot snapshot;
    if (reference) snapshot.network_binding = reference->network_binding.toStdString();
    snapshot.peers = {{"127.0.0.1:29461", 100, ""}};
    model.setNetworkDiagnostics(snapshot);
    QTRY_COMPARE(page.peerCount(), 1);
    if (reference) {
        QTRY_VERIFY(summary->text().contains(QLocale{}.toString(reference->finalized_per_s*60,'f',1)));
        QVERIFY(scope->text().contains(QStringLiteral("historical reference")));
        if (reference->co_located_wsl) QVERIFY(scope->text().contains(QStringLiteral("Same-host simulation")));
    }
    QVERIFY(summary->isVisibleTo(&page));
    page.selectPeer(0);
    page.showAdvanced();
    QCOMPARE(tabs->currentIndex(), 1);
    QCOMPARE(page.selectedPeerIndex(), 0);
    QVERIFY(!summary->isVisibleTo(&page));
    page.showBenchmarkDetails();
    QCOMPARE(tabs->currentIndex(), 0);
    QCOMPARE(page.selectedPeerIndex(), 0);
    page.showTechnicalDetails();
    QCOMPARE(tabs->currentIndex(), 3);
    page.findChild<QPushButton*>(QStringLiteral("networkAdvancedButton"))->setChecked(false);
    QVERIFY(summary->isVisibleTo(&page));
    QCOMPARE(page.selectedPeerIndex(), 0);
    QVERIFY(page.detailsWidget()->parentWidget() == page.mapWidget());
    snapshot.network_binding = "different-network";
    model.setNetworkDiagnostics(snapshot);
    QTRY_VERIFY(summary->text().contains(QStringLiteral("Unknown")));
    QTRY_COMPARE(page.selectedPeerIndex(), -1);
}

void CybouShellTests::networkRefreshCoalescesStatusBurst()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    NetworkPage page{&model};
    page.resize(1040, 720);
    page.show();
    QTest::qWait(250);
    QLabel* height = nullptr;
    for (auto* caption : page.findChildren<QLabel*>()) {
        if (caption->text() == QStringLiteral("Verified height"))
            height = caption->parentWidget()->findChild<QLabel*>(QStringLiteral("metric"));
    }
    QVERIFY(height);
    const auto old_height = height->text();
    const auto label_count = page.findChildren<QLabel*>().size();
    for (int i = 1; i <= 1000; ++i) model.setFinalizedHeight(i);
    QCOMPARE(height->text(), old_height); // No synchronous rebuild during a burst.
    QTRY_COMPARE(height->text(), QLocale{}.toString(quint64{1000}));
    QCOMPARE(page.findChildren<QLabel*>().size(), label_count);
    const auto* drawer = page.findChild<QScrollArea*>(QStringLiteral("networkAdvancedDrawer"));
    QVERIFY(drawer && drawer->isHidden());
    QVERIFY(page.mapWidget()->height() >= page.height() - 4);
    page.showAdvanced();
    QTest::qWait(350);
    QVERIFY(!drawer->isHidden());
    QVERIFY(page.mapWidget()->height() >= page.height() - 4);
    const auto visible_labels = page.findChildren<QLabel*>().size();
    for (int i = 1001; i <= 2000; ++i) model.setFinalizedHeight(i);
    QTRY_COMPARE(height->text(), QLocale{}.toString(quint64{2000}));
    QCOMPARE(page.findChildren<QLabel*>().size(), visible_labels);
}

void CybouShellTests::headerSaysWhenTheNetworkStopsConfirming()
{
    CybouDesktopModel model{QStringLiteral("CYBOU DEV")};
    model.setNodeStatus(true, 3, true);
    model.setSyncing(false);
    model.setFinalizedHeight(10);
    const auto start = QDateTime::currentDateTimeUtc();

    // An idle network produces no blocks: no waiting work, no warning.
    model.checkFinalityStall(start.addSecs(600));
    QCOMPARE(model.status().finality_stall_minutes, 0);

    // Submitted work and no new block for over two minutes: the header says so.
    model.setOperationStatus({QStringLiteral("op-1"), CybouOperationState::Submitted});
    model.checkFinalityStall(start.addSecs(1));
    QCOMPARE(model.status().finality_stall_minutes, 0);
    model.checkFinalityStall(start.addSecs(131));
    QCOMPARE(model.status().finality_stall_minutes, 2);
    QVERIFY(cybouConnectionText(model.status()).contains(QStringLiteral("not confirming")));

    // A new block clears it.
    model.setFinalizedHeight(11);
    QCOMPARE(model.status().finality_stall_minutes, 0);
    QVERIFY(!cybouConnectionText(model.status()).contains(QStringLiteral("not confirming")));

    // Syncing shows how far there is to go from what peers announce.
    cybou::NodeDiagnosticsSnapshot snap;
    snap.peers = {{"51.255.46.58:29461", 1011, ""}};
    model.setNetworkDiagnostics(snap);
    model.setSyncing(true);
    const auto text = cybouConnectionText(model.status());
    QVERIFY2(text.contains(QStringLiteral("1%")) && text.contains(QStringLiteral("blocks left")), qPrintable(text));

    // One peer announcing an absurd height cannot drive the estimate: the median is used.
    snap.peers = {{"51.255.46.58:29461", 1011, ""}, {"51.255.46.58:29462", 1011, ""}, {"203.0.113.9:29461", 9000000000, ""}};
    model.setNetworkDiagnostics(snap);
    QCOMPARE(model.status().sync_target_height, quint64{1011});
}

void CybouShellTests::benchmarkReferenceRequiresAcceptedEvidence()
{
    QJsonObject o{{"result","PASS"},{"run_id","20261007-120000"},{"network_binding",QString(64,'a')},
        {"profile","files"},{"attempted_operations",5},{"submitted_operations",3},{"finalized_operations",3},
        {"measurement_window_s",10},{"finalized_ops_per_s",0.3},{"replicas",2},{"file_size","64KiB"},
        {"clients",QJsonObject{{"test-client",QJsonObject{}}}},{"co_located_wsl",true},
        {"provenance",QJsonObject{{"revision",QString(40,'b')},{"windows_loadgen_sha256",QString(64,'c')},{"dirty",true}}},
        {"checks",QJsonArray{QJsonArray{"restore",true,"3/3"}}}};
    const auto parse=[](const QJsonObject& object) { return CybouBenchmarkReference::Parse(QJsonDocument{object}.toJson()); };
    const auto accepted=parse(o);
    QVERIFY(accepted);
    QCOMPARE(accepted->finalized,quint64{3});
    QCOMPARE(accepted->finalized_per_s,0.3);
    QVERIFY(accepted->co_located_wsl);
    for (const auto& [key,value] : std::vector<std::pair<QString,QJsonValue>>{
        {"result","FAIL"},{"finalized_operations",2},{"attempted_operations",1},
        {"measurement_window_s",0},{"finalized_ops_per_s",10},{"checks",QJsonArray{}},
        {"checks",QJsonArray{QJsonArray{"restore",false,"failed"}}},{"provenance",QJsonObject{}},
        {"run_id","20269999-120000"},{"network_binding","unknown"},{"replicas",0},
        {"clients",QJsonObject{}},{"file_size","unknown"}}) {
        auto bad=o; bad.insert(key,value); QVERIFY2(!parse(bad),qPrintable(key));
    }
    QVERIFY(!CybouBenchmarkReference::Parse("{}"));
    QVERIFY(!CybouBenchmarkReference::Parse("{\"operations\":3,\"operations_per_s\":1}"));
    QVERIFY(!CybouBenchmarkReference::Parse(QByteArray(1024*1024+1,' ')));
}

void CybouShellTests::consoleCompletionAndSearchClearOnLock()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    model.setIdentityState(CybouIdentityState::Active,QStringLiteral("own"),1);
    CybouFileItem file; file.id=QStringLiteral("private-id");file.name=QStringLiteral("private.txt");
    model.setFileItems({file});
    CybouConsoleDialog console{&model};
    console.show(); console.activateWindow();
    auto* input=console.findChild<QLineEdit*>(QStringLiteral("consoleInput"));
    auto* find=console.findChild<QLineEdit*>(QStringLiteral("consoleSearch"));
    auto* completion=console.findChild<QCompleter*>();
    QVERIFY(input); QVERIFY(find); QVERIFY(completion);
    input->setFocus(); input->setText(QStringLiteral("file "));
    QTest::keyClick(input,Qt::Key_Space,Qt::ControlModifier);
    QCOMPARE(completion->completionCount(),2);
    QCOMPARE(console.historyCount(),0); // Suggestions never dispatch a command.
    completion->popup()->setCurrentIndex(completion->completionModel()->index(0,0));
    QTest::keyClick(completion->popup(),Qt::Key_Return);
    QCOMPARE(input->text(),QStringLiteral("file private-id"));
    QCOMPARE(console.historyCount(),0);
    completion->popup()->hide();
    console.executeCommand(QStringLiteral("file private-id"));
    console.executeCommand(QStringLiteral("file private-id"));
    input->setFocus();
    QTest::keyClick(input,Qt::Key_F,Qt::ControlModifier);
    QVERIFY(find->isVisible());
    find->setText(QStringLiteral("private-id"));
    QVERIFY(console.findChild<QPlainTextEdit*>(QStringLiteral("consoleOutput"))->textCursor().hasSelection());
    model.setIdentityState(CybouIdentityState::Locked);
    QCOMPARE(console.historyCount(),0);
    QVERIFY(find->text().isEmpty()); QVERIFY(input->text().isEmpty());
    QCOMPARE(completion->model()->rowCount(),0);
    QVERIFY(!console.outputText().contains(QStringLiteral("private-id")));
    input->setText(QStringLiteral("file "));
    QTest::keyClick(input,Qt::Key_Space,Qt::ControlModifier);
    QCOMPARE(completion->completionCount(),0);
    console.executeCommand(QStringLiteral("status"));
    input->setFocus(); QTest::keyClick(input,Qt::Key_L,Qt::ControlModifier);
    QVERIFY(console.outputText().isEmpty()); QCOMPARE(console.historyCount(),0);
    const auto saved_geometry = QSettings{}.value(QStringLiteral("console/geometry"));
    console.resize(740,600);
    console.close();
    QVERIFY(!QSettings{}.value(QStringLiteral("console/geometry")).toByteArray().isEmpty());
    CybouConsoleDialog reopened{&model};
    QCOMPARE(reopened.size(),QSize(740,600));
    QSettings{}.setValue(QStringLiteral("console/geometry"),saved_geometry);
}

void CybouShellTests::authorityReviewsRejectStaleSessions()
{
    CybouDesktopModel model{QStringLiteral("DEVNET")};
    model.setIdentityState(CybouIdentityState::Active,QStringLiteral("authority"),1);
    CybouNetworkAuthorityStatus authority;
    authority.proven=true; authority.signer_enabled=true; authority.finalizer=CybouFinalizerState::Finalizing;
    authority.settlement_due=true; authority.next_settlement_period=3;
    authority.next_settlement_start_utc=86400; authority.next_settlement_due_utc=172800;
    authority.storage_escrow=1000;
    model.setNetworkAuthority(authority);
    QWidget host;
    cybou::Hash256 publication{1};
    cybou::Hash256 account{2};
    const std::vector<cybou::StorageSettlementEntry> entries{{publication,cybou::AccountId{account},500}};
    bool preview_inspected=false;
    QTimer::singleShot(0,&host,[&] {
        auto* review=host.findChild<QMessageBox*>(QStringLiteral("authoritySettlementReview"));
        if (!review) return;
        preview_inspected=review->text().contains(QStringLiteral("500")) && review->text().contains(QStringLiteral("Payout accounts: 1")) && review->detailedText().contains(QString::fromStdString(account.GetHex()));
        QCOMPARE(review->defaultButton(),qobject_cast<QPushButton*>(review->button(QMessageBox::Cancel)));
        QTest::keyClick(review, Qt::Key_Escape);
    });
    QVERIFY(!ReviewStorageSettlement(&model,3,86400,172800,entries,&host));
    QVERIFY(preview_inspected);
    QTimer::singleShot(0,&host,[&] {
        host.findChild<QMessageBox*>(QStringLiteral("authoritySettlementReview"))->findChild<QPushButton*>(QStringLiteral("authorityConfirmSettlement"))->click();
    });
    QVERIFY(ReviewStorageSettlement(&model,3,86400,172800,entries,&host));
    QTimer::singleShot(0,&host,[&] { model.setIdentityState(CybouIdentityState::Locked); });
    QVERIFY(!ReviewStorageSettlement(&model,3,86400,172800,entries,&host));
    QSignalSpy pauses{&model,&CybouDesktopModel::finalizationPauseRequested};
    QSignalSpy settles{&model,&CybouDesktopModel::storageSettlementRequested};
    model.requestFinalizationPaused(true); model.requestStorageSettlement();
    QCOMPARE(pauses.count(),0); QCOMPARE(settles.count(),0);
    OnboardingView unlock{&model};
    auto* notice = unlock.findChild<QLabel*>(QStringLiteral("backgroundFinalizerNotice"));
    QVERIFY(notice); QVERIFY(notice->isHidden());
    model.setBackgroundFinalizerActive(true);
    QVERIFY(!notice->isHidden());
    QVERIFY(!model.isNetworkAuthority() || model.status().identity_state == CybouIdentityState::Locked);
    model.requestFinalizationPaused(true); model.requestStorageSettlement();
    QCOMPARE(pauses.count(),0); QCOMPARE(settles.count(),0);
    model.setBackgroundFinalizerActive(false);
    QVERIFY(notice->isHidden());
    model.setIdentityState(CybouIdentityState::Active,QStringLiteral("authority"),1);
    QTimer::singleShot(0,&host,[&] { authority.next_settlement_period=4; model.setNetworkAuthority(authority); });
    QVERIFY(!ReviewStorageSettlement(&model,3,86400,172800,entries,&host));
}
