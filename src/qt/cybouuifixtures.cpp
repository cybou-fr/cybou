// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cybouuifixtures.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboufixturebackend.h>

#include <QTimeZone>
#include <QTimer>

namespace CybouUiFixtures {

namespace {

constexpr quint64 MB = 1024ULL * 1024ULL;

QDateTime At(int days_ago, int hour, int minute)
{
    return referenceTime().addDays(-days_ago).addSecs((hour - 10) * 3600 + (minute - 42) * 60);
}

CybouAttachmentItem Attachment(const QString& id, const QString& name, quint64 size,
    CybouContentState state = CybouContentState::Protected)
{
    CybouAttachmentItem item;
    item.id = id;
    item.name = name;
    item.logical_size = size;
    item.state = state;
    return item;
}

CybouMailItem Mail(const QString& id, CybouMailFolder folder, const QString& from, const QString& to,
    const QString& subject, const QString& body, const QDateTime& time, bool unread = false)
{
    CybouMailItem item;
    item.id = id;
    item.folder = folder;
    item.from_name = from;
    item.to_name = to;
    item.subject = subject;
    item.body = body;
    item.preview = body.simplified().left(90);
    item.time = time;
    item.unread = unread;
    item.draft = folder == CybouMailFolder::Drafts;
    // Own sent mail is Protected (remote durability); incoming mail is Received.
    item.state = folder == CybouMailFolder::Drafts ? CybouContentState::Local
        : folder == CybouMailFolder::Sent ? CybouContentState::Protected : CybouContentState::Received;
    if (!item.draft) {
        item.operation_id = QStringLiteral("9c41e7a0b3d25f86e1c07a4d92b3f5e8c6a1d0e7f2b4a9c3d8e5f1a0b7c26d4e");
        item.finalized_height = 1180 + static_cast<quint64>(qHash(id) % 60);
        item.root_chunk_id = QStringLiteral("5e0a91c4d7b2f83e6a1c9d04b7e5f2a8c3d6b190e4f7a2c5d8b1e04f9a3c67b2");
    }
    return item;
}

CybouFileItem File(const QString& id, const QString& name, const QString& parent, quint64 size,
    const QDateTime& modified, CybouContentState state = CybouContentState::Protected)
{
    CybouFileItem item;
    item.id = id;
    item.name = name;
    item.parent_id = parent;
    item.logical_size = size;
    item.modified = modified;
    item.state = state;
    if (state == CybouContentState::Protected) {
        item.content_root_id = QStringLiteral("b81f0c4e92d7a35e6f1c0d8b4a29e7f3c5d61a08e9b2f4c7d0a3e5b6f1c8d924");
        item.finalized_height = 1100 + static_cast<quint64>(qHash(id) % 120);
    }
    return item;
}

CybouFileItem Folder(const QString& id, const QString& name, const QDateTime& modified)
{
    CybouFileItem item = File(id, name, {}, 0, modified);
    item.folder = true;
    return item;
}

void ApplyIdentity(CybouDesktopModel& model, CybouIdentityState state)
{
    model.setNetworkInfo(QStringLiteral("CYBOU DEV"),
        QStringLiteral("7c1e0d52a4f9b3e8c6d10f2a9b8e7d6c5b4a39281706f5e4d3c2b1a0918273645"));
    model.setNodeStatus(true, 3, true, QStringLiteral("C:/Users/stan/AppData/Local/CYBOU"));
    model.setFinalizedHeight(1242);
    model.setLastSync(referenceTime());
    model.setIdentityState(state,
        QStringLiteral("2af3c8e41b9d07f6a25e3c19d84b72a0f6e5d4c3b2a1908f7e6d5c4b3a2991bc"), 118);
    model.setPrimaryName(QStringLiteral("stan.cybou"));
    model.setKeyEpoch(1);
    model.setNames({{QStringLiteral("stan.cybou"), true}});
    model.setBalances(5820, 4621);
    model.setStorageUsage(13314398618ULL, 100ULL * 1024 * MB);

    CybouCapabilities caps;
    caps.account_creation = true;
    caps.payments = true;
    caps.mail = true;
    caps.files = true;
    model.setCapabilities(caps);

    model.setAuthority(1'200'000);

    model.setPaymentFee(1);
    model.setContacts({
        {QStringLiteral("Alice"), QStringLiteral("alice.cybou"), true},
        {QStringLiteral("Alina Petrova"), QStringLiteral("alina.cybou"), true},
        {QStringLiteral("Bob"), QStringLiteral("bobby.cybou"), true},
        {QStringLiteral("Carol Martin"), QStringLiteral("carol.cybou"), true},
        {QStringLiteral("CYBOU"), QStringLiteral("cybou-team.cybou"), true},
    });
}

QVector<CybouMailItem> FixtureMail()
{
    QVector<CybouMailItem> mail;
    auto project = Mail(QStringLiteral("m-project"), CybouMailFolder::Inbox, QStringLiteral("alice.cybou"),
        QStringLiteral("stan.cybou"), QStringLiteral("Project files"),
        QStringLiteral("Hi Stan,\n\nI attached the final report and the photo from Tuesday. "
                       "Let me know if the numbers on page 4 look right to you.\n\nAlice"),
        At(0, 10, 42), true);
    project.attachments = {
        Attachment(QStringLiteral("c-report"), QStringLiteral("report.pdf"), 4404019),
        Attachment(QStringLiteral("c-photo"), QStringLiteral("photo.jpg"), 8493466),
    };
    mail.append(project);
    mail.append(Mail(QStringLiteral("m-dinner"), CybouMailFolder::Inbox, QStringLiteral("bobby.cybou"),
        QStringLiteral("stan.cybou"), QStringLiteral("Dinner on Friday?"),
        QStringLiteral("Are you free on Friday evening? The new place near the station finally opened."),
        At(0, 9, 15), true));
    mail.append(Mail(QStringLiteral("m-contract"), CybouMailFolder::Inbox, QStringLiteral("carol.cybou"),
        QStringLiteral("stan.cybou"), QStringLiteral("Signed contract"),
        QStringLiteral("Here is the signed version. Please keep a copy in your Files."),
        At(1, 16, 5), true));
    mail.back().attachments = {Attachment(QStringLiteral("c-contract"), QStringLiteral("contract-signed.pdf"), 1258291)};
    mail.append(Mail(QStringLiteral("m-alina"), CybouMailFolder::Inbox, QStringLiteral("alina.cybou"),
        QStringLiteral("stan.cybou"), QStringLiteral("Trip photos"),
        QStringLiteral("Uploading the rest tonight. The mountains came out beautifully."),
        At(2, 20, 31), true));
    auto welcome = Mail(QStringLiteral("m-welcome"), CybouMailFolder::Inbox, QStringLiteral("cybou-team.cybou"),
        QStringLiteral("stan.cybou"), QStringLiteral("Welcome to CYBOU"),
        QStringLiteral("Your Identity is ready. Mail and Files are protected end to end, and your "
                       "recovery phrase restores everything on a new computer."),
        At(6, 11, 0));
    welcome.starred = true;
    mail.append(welcome);
    mail.append(Mail(QStringLiteral("m-sent-1"), CybouMailFolder::Sent, QStringLiteral("stan.cybou"),
        QStringLiteral("alice.cybou"), QStringLiteral("Re: Budget draft"),
        QStringLiteral("Looks good to me. I left two comments in the second section."),
        At(1, 12, 20)));
    auto securing = Mail(QStringLiteral("m-sent-securing"), CybouMailFolder::Sent, QStringLiteral("stan.cybou"),
        QStringLiteral("carol.cybou"), QStringLiteral("Quarterly numbers"),
        QStringLiteral("Carol, the Q3 report is attached. The summary is on the first page."), At(0, 10, 35));
    securing.state = CybouContentState::Securing;
    securing.attachments = {Attachment(QStringLiteral("c-q3"), QStringLiteral("q3-report.pdf"), 7340032,
        CybouContentState::Securing)};
    securing.attachments.first().progress_percent = 42;
    mail.append(securing);
    // Outgoing message validated by the network but not yet PoA-finalized.
    auto validated = Mail(QStringLiteral("m-sent-validated"), CybouMailFolder::Sent, QStringLiteral("stan.cybou"),
        QStringLiteral("carol.cybou"), QStringLiteral("Contract questions"),
        QStringLiteral("Two small questions about section 3 before I sign."), At(0, 10, 38));
    validated.state = CybouContentState::Local;
    validated.operation_state = CybouOperationState::Validated;
    validated.operation_id = QStringLiteral("op-m-sent-validated");
    validated.finalized_height = 0;
    mail.append(validated);
    mail.append(Mail(QStringLiteral("m-draft-1"), CybouMailFolder::Drafts, QStringLiteral("stan.cybou"),
        QStringLiteral("bobby.cybou"), QStringLiteral("Weekend plans"),
        QStringLiteral("Hey Bob, about Saturday —"), At(0, 8, 2)));
    mail.append(Mail(QStringLiteral("m-archive-1"), CybouMailFolder::Archive, QStringLiteral("carol.cybou"),
        QStringLiteral("stan.cybou"), QStringLiteral("Meeting notes"),
        QStringLiteral("Notes from the planning session are below."), At(12, 15, 0)));
    for (auto& item : mail) {
        if (item.state != CybouContentState::Received) continue;
        for (auto& attachment : item.attachments) {
            if (attachment.state == CybouContentState::Protected) attachment.state = CybouContentState::Received;
        }
    }
    return mail;
}

QVector<CybouFileItem> FixtureFiles()
{
    QVector<CybouFileItem> files;
    files.append(Folder(QStringLiteral("f-docs"), QStringLiteral("Documents"), At(0, 9, 0)));
    files.append(Folder(QStringLiteral("f-photos"), QStringLiteral("Photos"), At(3, 18, 0)));
    auto report = File(QStringLiteral("f-report"), QStringLiteral("report.pdf"), {}, 4404019, At(0, 10, 50));
    report.starred = true;
    report.available_offline = true;
    files.append(report);
    files.append(File(QStringLiteral("f-photo"), QStringLiteral("photo.jpg"), {}, 8493466, At(1, 19, 12)));
    files.back().available_offline = true;
    auto archive = File(QStringLiteral("f-archive"), QStringLiteral("archive.zip"), {}, 1288490189ULL,
        At(4, 14, 3), CybouContentState::Securing);
    archive.progress_percent = 42;
    archive.available_offline = true; // still uploading from this device
    files.append(archive);
    files.append(File(QStringLiteral("f-budget"), QStringLiteral("budget-2026.xlsx"), QStringLiteral("f-docs"), 245760, At(2, 11, 30)));
    files.back().available_offline = true;
    files.append(File(QStringLiteral("f-notes"), QStringLiteral("meeting-notes.txt"), QStringLiteral("f-docs"), 12288, At(5, 9, 45)));
    files.append(File(QStringLiteral("f-mountain"), QStringLiteral("mountains.jpg"), QStringLiteral("f-photos"), 6291456, At(3, 18, 0)));
    auto old = File(QStringLiteral("f-old"), QStringLiteral("old-draft.docx"), {}, 88064, At(20, 10, 0));
    old.trashed = true;
    files.append(old);
    return files;
}

/** The fixture Mail/Files backend owned by the model, created on first use. */
CybouFixtureApplicationBackend* Backend(CybouDesktopModel& model)
{
    auto* backend = qobject_cast<CybouFixtureApplicationBackend*>(model.applicationBackend());
    if (!backend) {
        backend = new CybouFixtureApplicationBackend{&model};
        backend->setOnlineProvider([&model] { return model.status().online; });
        model.setApplicationBackend(backend);
    }
    return backend;
}

void ApplyWallet(CybouDesktopModel& model)
{
    QVector<CybouWalletEntry> entries;
    const auto entry = [](QString id, CybouWalletEntryKind kind, qint64 amount, bool system_side, QString counterparty,
                           QDateTime time, CybouOperationState state = CybouOperationState::Finalized) {
        CybouWalletEntry item;
        item.id = id;
        item.kind = kind;
        item.amount = amount;
        item.system_side = system_side;
        item.counterparty_name = std::move(counterparty);
        item.time = std::move(time);
        item.operation_state = state;
        item.operation_id = QStringLiteral("op-") + id;
        item.finalized_height = state == CybouOperationState::Finalized ? 1180 + static_cast<quint64>(qHash(id) % 60) : 0;
        return item;
    };
    // One payment of each operation state: Submitted, Validated, Finalized.
    entries.append(entry(QStringLiteral("w-submitted"), CybouWalletEntryKind::Sent, -40, false, QStringLiteral("carol.cybou"),
        At(0, 10, 40), CybouOperationState::Submitted));
    entries.append(entry(QStringLiteral("w-validated"), CybouWalletEntryKind::Sent, -75, false, QStringLiteral("alice.cybou"),
        At(0, 10, 35), CybouOperationState::Validated));
    entries.back().validation_signatures = 2;
    entries.append(entry(QStringLiteral("w1"), CybouWalletEntryKind::Received, 250, false, QStringLiteral("alice.cybou"), At(0, 9, 30)));
    entries.append(entry(QStringLiteral("w2"), CybouWalletEntryKind::Sent, -100, false, QStringLiteral("bobby.cybou"), At(1, 17, 44)));
    entries.append(entry(QStringLiteral("w3"), CybouWalletEntryKind::NetworkServiceFee, -4, true, {}, At(1, 12, 20)));
    entries.append(entry(QStringLiteral("w4"), CybouWalletEntryKind::OnboardingCredit, 5000, true, {}, At(6, 10, 58)));
    model.setWalletEntries(entries);
}

void ApplyActivity(CybouDesktopModel& model)
{
    model.setActivity({
        {CybouActivityKind::MailReceived, QStringLiteral("Message from alice.cybou"), QStringLiteral("Project files"), At(0, 10, 42)},
        {CybouActivityKind::FileUploaded, QStringLiteral("report.pdf uploaded"), QStringLiteral("Protected"), At(0, 10, 50)},
        {CybouActivityKind::PaymentSent, QStringLiteral("100 CYBOU sent to bobby.cybou"), QString{}, At(1, 17, 44)},
        {CybouActivityKind::IdentitySynced, QStringLiteral("Identity synchronized"), QString{}, At(0, 8, 0)},
    });
}

} // namespace

QStringList names()
{
    return {QStringLiteral("empty"), QStringLiteral("active"), QStringLiteral("mail"),
        QStringLiteral("files"), QStringLiteral("offline"), QStringLiteral("restoring")};
}

QDateTime referenceTime()
{
    return QDateTime{QDate{2026, 9, 29}, QTime{10, 42}, QTimeZone::LocalTime};
}

QString requestedFixture()
{
    return qEnvironmentVariable("CYBOU_UI_FIXTURE").trimmed().toLower();
}

bool apply(CybouDesktopModel& model, const QString& name)
{
    if (!names().contains(name)) return false;
    model.setFixtureMode(true);
    auto* backend = Backend(model);
    backend->seed({}, {});

    if (name == QLatin1String{"empty"}) {
        model.setNodeStatus(true, 3, true, QStringLiteral("C:/Users/stan/AppData/Local/CYBOU"));
        model.setFinalizedHeight(1242);
        CybouCapabilities caps;
        caps.account_creation = true;
        model.setCapabilities(caps);
        return true;
    }

    if (name == QLatin1String{"restoring"}) {
        ApplyIdentity(model, CybouIdentityState::Restoring);
        CybouRestoreProgress progress;
        progress.identity = CybouRestoreStepState::Done;
        progress.wallet = CybouRestoreStepState::Done;
        progress.names = CybouRestoreStepState::Done;
        progress.mail = CybouRestoreStepState::Running;
        progress.files = CybouRestoreStepState::Running;
        model.setRestoreProgress(progress);
        model.setSyncing(true);
        return true;
    }

    ApplyIdentity(model, CybouIdentityState::Active);
    ApplyWallet(model);
    ApplyActivity(model);
    auto mail = FixtureMail();

    if (name == QLatin1String{"offline"}) {
        model.setNodeStatus(true, 0, false);
        auto outgoing = Mail(QStringLiteral("m-outgoing"), CybouMailFolder::Sent, QStringLiteral("stan.cybou"),
            QStringLiteral("carol.cybou"), QStringLiteral("Invoice for September"),
            QStringLiteral("Please find the invoice attached."), At(0, 10, 30));
        outgoing.state = CybouContentState::Local;
        outgoing.operation_state = CybouOperationState::Submitted;
        outgoing.attachments = {Attachment(QStringLiteral("c-invoice"), QStringLiteral("invoice-09.pdf"), 310272,
            CybouContentState::Local)};
        mail.prepend(outgoing);
    }
    backend->seed(mail, FixtureFiles());
    return true;
}

QString initialPage(const QString& name)
{
    if (name == QLatin1String{"mail"} || name == QLatin1String{"offline"}) return QStringLiteral("mail");
    if (name == QLatin1String{"files"}) return QStringLiteral("files");
    return QStringLiteral("home");
}

Driver::Driver(CybouDesktopModel* model, QObject* parent)
    : QObject{parent}, m_model{model}
{
    connect(m_model, &CybouDesktopModel::createIdentityRequested, this, [this] { runCreate(); });
    connect(m_model, &CybouDesktopModel::restoreIdentityRequested, this, [this] { runRestore(); });
    Backend(*m_model)->setAutoAdvance(true, m_step_ms);
    connect(m_model, &CybouDesktopModel::paymentRequested, this, [this](const QString& to, quint64 amount) {
        later(2, [this, to, amount] {
            const quint64 fee = m_model->paymentFee().value_or(0);
            const auto& status = m_model->status();
            if (amount > status.balance) {
                m_model->setPaymentFinished(false, tr("Not enough CYBOU available."));
                return;
            }
            m_model->setBalances(status.balance - amount, status.system_balance - qMin(fee, status.system_balance));
            QVector<CybouWalletEntry> entries = m_model->walletEntries();
            entries.prepend({QStringLiteral("w-fee-%1").arg(entries.size()), CybouWalletEntryKind::NetworkServiceFee,
                -static_cast<qint64>(fee), true, {}, QDateTime::currentDateTime()});
            entries.prepend({QStringLiteral("w-sent-%1").arg(entries.size()), CybouWalletEntryKind::Sent,
                -static_cast<qint64>(amount), false, to, QDateTime::currentDateTime()});
            m_model->setWalletEntries(entries);
            m_model->addActivity({CybouActivityKind::PaymentSent, tr("%1 sent to %2").arg(cybouAmountText(amount), to),
                QString{}, QDateTime::currentDateTime()});
            m_model->setPaymentFinished(true);
        });
    });
    connect(m_model, &CybouDesktopModel::nameClaimRequested, this, [this](const QString& label) {
        later(3, [this, label] {
            const QString name = label + QStringLiteral(".cybou");
            m_model->setPrimaryName(name);
            m_model->setNames({{name, true}});
            m_model->setNameClaimFinished();
        });
    });
}

void Driver::setStepDelay(int ms)
{
    m_step_ms = ms;
    Backend(*m_model)->setAutoAdvance(true, ms);
}

void Driver::later(int steps, std::function<void()> action)
{
    QTimer::singleShot(steps * m_step_ms, this, std::move(action));
}

void Driver::runCreate()
{
    m_model->setIdentityStep(CybouIdentityStep::PreparingKeys);
    m_model->setIdentityState(CybouIdentityState::Creating);
    later(1, [this] { m_model->setIdentityStep(CybouIdentityStep::CreatingIdentity); });
    later(2, [this] { m_model->setIdentityStep(CybouIdentityStep::WaitingForConfirmation); });
    later(4, [this] {
        ApplyIdentity(*m_model, CybouIdentityState::Active);
        m_model->setPrimaryName({});
        m_model->setNames({});
        m_model->setBalances(0, 5000);
        m_model->setWalletEntries({{QStringLiteral("w-onboard"), CybouWalletEntryKind::OnboardingCredit,
            5000, true, {}, referenceTime()}});
        m_model->setActivity({{CybouActivityKind::OnboardingCredit, QStringLiteral("Identity created"),
            QStringLiteral("5,000 CYBOU onboarding credit"), referenceTime()}});
    });
}

void Driver::runRestore()
{
    const auto step = [this](auto mutate) {
        CybouRestoreProgress progress = m_model->restoreProgress();
        mutate(progress);
        m_model->setRestoreProgress(progress);
    };
    later(1, [step] { step([](CybouRestoreProgress& p) { p.identity = CybouRestoreStepState::Done; p.wallet = CybouRestoreStepState::Running; }); });
    later(2, [step] { step([](CybouRestoreProgress& p) { p.wallet = CybouRestoreStepState::Done; p.names = CybouRestoreStepState::Running; }); });
    later(3, [this, step] {
        step([](CybouRestoreProgress& p) { p.names = CybouRestoreStepState::Done; });
        // Mail and Files rebuild progressively in the backend; the desktop
        // is usable ("Open CYBOU") while they fill in.
        Backend(*m_model)->seedProgressively(FixtureMail(), FixtureFiles());
    });
    later(6, [this] {
        const auto state = m_model->status().identity_state;
        if (state != CybouIdentityState::Restoring && state != CybouIdentityState::Syncing) return;
        ApplyIdentity(*m_model, CybouIdentityState::Active);
        ApplyWallet(*m_model);
        ApplyActivity(*m_model);
    });
}

} // namespace CybouUiFixtures
