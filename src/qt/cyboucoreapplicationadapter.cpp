// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboucoreapplicationadapter.h>

#include <cybou/application_service.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/identity_kem.h>
#include <cybou/identity_service.h>
#include <cybou/kv_store.h>
#include <cybou/node_runtime.h>
#include <cybou/private_application_store.h>
#include <cybou/publication_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/storage_service.h>

#include <QMetaObject>

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>

namespace {

constexpr char HEX[]{"0123456789abcdef"};

std::string ToHex(const cybou::PrivateItemId& id)
{
    std::string out;
    for (const unsigned char byte : id) {
        out.push_back(HEX[byte >> 4]);
        out.push_back(HEX[byte & 0x0f]);
    }
    return out;
}

std::optional<cybou::PrivateItemId> FromHex(const QString& text)
{
    const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
    if (text.size() != 64 || bytes.size() != 32) return std::nullopt;
    cybou::PrivateItemId id{};
    std::copy(bytes.begin(), bytes.end(), id.begin());
    return id;
}

QString ChunkHex(const cybou::ChunkId& id)
{
    return QString::fromLatin1(QByteArray{reinterpret_cast<const char*>(id.data()), static_cast<int>(id.size())}.toHex());
}

CybouContentState StateOf(const cybou::PublicationJobResult& job)
{
    switch (job.phase) {
    case cybou::PublicationJobPhase::QUEUED:
    case cybou::PublicationJobPhase::WAITING_FINALITY: return CybouContentState::WaitingForConfirmation;
    case cybou::PublicationJobPhase::SECURING: return CybouContentState::Securing;
    case cybou::PublicationJobPhase::PROTECTED: return CybouContentState::Protected;
    case cybou::PublicationJobPhase::NEEDS_ATTENTION: return CybouContentState::NeedsAttention;
    }
    return CybouContentState::NeedsAttention;
}

CybouMailFolder FolderOf(cybou::MailFolder folder)
{
    switch (folder) {
    case cybou::MailFolder::INBOX: return CybouMailFolder::Inbox;
    case cybou::MailFolder::SENT: return CybouMailFolder::Sent;
    case cybou::MailFolder::ARCHIVE: return CybouMailFolder::Archive;
    case cybou::MailFolder::TRASH: return CybouMailFolder::Trash;
    }
    return CybouMailFolder::Inbox;
}

std::optional<cybou::MailFolder> CoreFolder(CybouMailFolder folder)
{
    switch (folder) {
    case CybouMailFolder::Inbox: return cybou::MailFolder::INBOX;
    case CybouMailFolder::Sent: return cybou::MailFolder::SENT;
    case CybouMailFolder::Archive: return cybou::MailFolder::ARCHIVE;
    case CybouMailFolder::Trash: return cybou::MailFolder::TRASH;
    case CybouMailFolder::Drafts: return std::nullopt;
    }
    return std::nullopt;
}

/** ".cybou" labels are displayed with their suffix; unnamed Identities by short ID. */
QString DisplayName(const cybou::CybouState* state, const cybou::AccountId& account)
{
    if (state) {
        if (const auto* label = state->names.PrimaryName(account)) {
            return QString::fromStdString(*label) + QStringLiteral(".cybou");
        }
    }
    return CybouProduct::shortId(QString::fromStdString(account.Value().GetHex()));
}

} // namespace

struct CybouCoreApplicationAdapter::Session {
    CybouCoreApplicationAdapter* owner;
    cybou::CybouNodeRuntime& runtime;
    cybou::CybouKeyStore& keystore;
    std::filesystem::path root;
    int refresh_ms;

    std::unique_ptr<cybou::PrivateApplicationStore> db;
    std::unique_ptr<cybou::KVStore> staging;
    std::unique_ptr<cybou::RuntimeStorageTransport> transport;
    std::unique_ptr<cybou::StorageService> storage;
    std::unique_ptr<cybou::PublicationService> publication;
    std::unique_ptr<cybou::ApplicationService> application;

    /** Messages sent from this device that the scanner has not indexed yet. */
    std::map<std::string, CybouMailItem> outbox;
    std::map<std::string, cybou::PublicationJobResult> jobs;
    /** RecoveryBridge being secured before IdentityRotate. */
    struct RotationPrep {
        std::string job_id;
        cybou::RecoveryEntropy entropy{};
        ~RotationPrep() { cybou::crypto::CleanseMemory(entropy.data(), entropy.size()); }
    };
    std::unique_ptr<RotationPrep> rotation;

    std::mutex mutex;
    std::condition_variable_any wake;
    std::deque<std::function<void(Session&)>> tasks;
    std::jthread worker;

    Session(CybouCoreApplicationAdapter* adapter, cybou::CybouNodeRuntime& rt, cybou::CybouKeyStore& ks,
        std::filesystem::path identity_root, int interval)
        : owner{adapter}, runtime{rt}, keystore{ks}, root{std::move(identity_root)}, refresh_ms{interval}
    {
        worker = std::jthread{[this](std::stop_token stop) { Run(stop); }};
    }

    ~Session()
    {
        worker.request_stop();
        wake.notify_all();
        if (worker.joinable()) worker.join();
    }

    void Post(std::function<void(Session&)> task)
    {
        {
            std::lock_guard lock{mutex};
            tasks.push_back(std::move(task));
        }
        wake.notify_all();
    }

    template <typename F>
    void ToGui(F&& f)
    {
        QMetaObject::invokeMethod(owner, std::forward<F>(f), Qt::QueuedConnection);
    }

    bool Open()
    {
        try {
            std::filesystem::create_directories(root);
            db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root / "app");
            staging = std::make_unique<cybou::KVStore>(cybou::KVStoreOptions{.path = root / "staging"});
            transport = std::make_unique<cybou::RuntimeStorageTransport>(runtime);
            storage = std::make_unique<cybou::StorageService>(runtime, *transport, *db);
            publication = std::make_unique<cybou::PublicationService>(runtime, keystore, *db,
                runtime.GetIdentityOperationCoordinator(keystore), *staging);
            application = std::make_unique<cybou::ApplicationService>(runtime, keystore, *db, *storage);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    void Run(std::stop_token stop)
    {
        const bool ready = Open();
        ToGui([owner = owner, ready] { owner->setReady(ready); });
        if (!ready) return;
        while (!stop.stop_requested()) {
            std::deque<std::function<void(Session&)>> pending;
            {
                std::unique_lock lock{mutex};
                wake.wait_for(lock, stop, std::chrono::milliseconds{refresh_ms}, [this] { return !tasks.empty(); });
                pending.swap(tasks);
            }
            if (stop.stop_requested()) break;
            try {
                for (auto& task : pending) task(*this);
                Refresh();
            } catch (const std::exception&) {
                // A failed refresh leaves the last snapshot in place; the next tick retries.
            }
        }
    }

    /** Scan, advance durability and publish one Mail snapshot. */
    void Refresh()
    {
        const auto progress = application->Scan();
        for (const auto& [id, status] : publication->ProcessDurability(*storage)) jobs[id] = status;
        AdvanceRotation();
        Snapshot(progress.Complete() ? CybouRestoreStepState::Done : CybouRestoreStepState::Running);
    }

    void Snapshot(CybouRestoreStepState mail_restore)
    {
        const auto loaded = runtime.GetStore().LoadState();
        const cybou::CybouState* state = loaded && loaded.state ? &*loaded.state : nullptr;
        QVector<CybouMailItem> items;
        for (const auto& record : application->ListMail()) {
            const auto id = ToHex(record.message.message_id);
            outbox.erase(id);
            CybouMailItem item;
            item.id = QString::fromStdString(id);
            item.folder = FolderOf(record.folder);
            item.from_name = DisplayName(state, record.sender);
            item.to_name = DisplayName(state, record.message.recipient_account_id);
            item.subject = QString::fromStdString(record.message.subject);
            item.body = QString::fromStdString(record.message.body);
            item.preview = item.body.simplified().left(90);
            item.time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(record.message.client_timestamp_ms));
            item.unread = !record.read;
            item.starred = record.starred;
            item.operation_id = QString::fromStdString(record.operation_id.GetHex());
            item.finalized_height = record.finalized_height;
            if (const auto publication_record = runtime.FindFinalizedRootPublication(record.operation_id)) {
                item.root_chunk_id = ChunkHex(publication_record->root_chunk_id);
            }
            if (record.outgoing) {
                // Sent means remotely durable, never merely finalized.
                const auto job = jobs.find(id);
                const auto durability = job == jobs.end() ? storage->GetDurability(record.operation_id) : std::nullopt;
                item.state = job != jobs.end() ? StateOf(job->second)
                    : durability && durability->state == cybou::DurabilityState::PROTECTED
                        ? CybouContentState::Protected : CybouContentState::Securing;
            } else {
                item.state = CybouContentState::Protected;
            }
            for (const auto& attachment : record.message.attachments) {
                CybouAttachmentItem a;
                a.id = QString::fromStdString(ToHex(attachment.attachment_id));
                a.name = QString::fromStdString(attachment.filename);
                a.logical_size = attachment.logical_size;
                // Attachment transfer is not connected to the desktop yet.
                a.state = CybouContentState::TemporarilyUnavailable;
                item.attachments.append(a);
            }
            items.append(item);
        }
        for (auto& [id, item] : outbox) {
            if (const auto job = jobs.find(id); job != jobs.end()) {
                item.state = StateOf(job->second);
                if (!job->second.operation_id.IsNull()) {
                    item.operation_id = QString::fromStdString(job->second.operation_id.GetHex());
                }
                item.finalized_height = job->second.finalized_height;
            }
            items.append(item);
        }
        ToGui([owner = owner, items = std::move(items), mail_restore]() mutable {
            owner->applySnapshot(std::move(items), true, mail_restore);
        });
    }

    /** Answers the rotation request once the bridge is durable and verified. */
    void AdvanceRotation()
    {
        if (!rotation) return;
        const auto job = jobs.find(rotation->job_id);
        if (job == jobs.end()) return;
        if (job->second.phase == cybou::PublicationJobPhase::PROTECTED) {
            const bool ok = publication->VerifyRecoveryBridge(rotation->job_id,
                std::span<const unsigned char, 32>{rotation->entropy.data(), 32}, *storage);
            rotation.reset();
            ToGui([owner = owner, ok] {
                owner->finishRotation(ok, ok ? QString{} : tr("Your recovery data could not be verified."));
            });
        } else if (job->second.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
            rotation.reset();
            ToGui([owner = owner, error = QString::fromStdString(job->second.error)] {
                owner->finishRotation(false, error.isEmpty() ? tr("Your recovery data could not be secured.") : error);
            });
        }
    }

    std::optional<cybou::AccountId> ResolveRecipient(const QString& name) const
    {
        QString label = name.trimmed().toLower();
        if (label.endsWith(QStringLiteral(".cybou"))) label.chop(6);
        const auto loaded = runtime.GetStore().LoadState();
        if (!loaded || !loaded.state) return std::nullopt;
        if (const auto* account = loaded.state->names.Resolve(label.toStdString())) return *account;
        // An Identity without a name can be addressed by its full AccountID.
        const QByteArray bytes = QByteArray::fromHex(label.toLatin1());
        if (label.size() != 64 || bytes.size() != 32) return std::nullopt;
        uint256 value;
        std::copy(bytes.rbegin(), bytes.rend(), value.begin());
        const cybou::AccountId account{value};
        if (loaded.state->identities.Find(account)) return account;
        if (const auto direct = cybou::AccountId::FromBytes(std::span<const unsigned char>{
                reinterpret_cast<const unsigned char*>(bytes.constData()), 32});
            direct && loaded.state->identities.Find(*direct)) return direct;
        return std::nullopt;
    }
};

CybouCoreApplicationAdapter::CybouCoreApplicationAdapter(cybou::CybouNodeRuntime& runtime,
    cybou::CybouIdentityService& identity, std::filesystem::path data_directory, QObject* parent)
    : CybouApplicationBackend{parent}, m_runtime{runtime}, m_identity{identity},
      m_data_directory{std::move(data_directory)}
{
}

CybouCoreApplicationAdapter::~CybouCoreApplicationAdapter()
{
    m_session.reset();
}

void CybouCoreApplicationAdapter::setRefreshInterval(int ms)
{
    m_refresh_ms = std::max(10, ms);
}

void CybouCoreApplicationAdapter::openIdentity()
{
    if (m_session) return;
    const auto account = m_identity.GetAccountId();
    if (!account) return;
    // One encrypted, rebuildable Application DB per Identity.
    const auto root = m_data_directory / "identities" / account->Value().GetHex();
    m_session = std::make_unique<Session>(this, m_runtime, m_identity.GetKeyStore(), root, m_refresh_ms);
}

void CybouCoreApplicationAdapter::closeIdentity()
{
    m_session.reset();
    finishRotation(false, tr("CYBOU was locked before your data was secured. The current recovery phrase stays active."));
    m_drafts.clear();
    m_pending_sends.clear();
    setReady(false);
}

void CybouCoreApplicationAdapter::setReady(bool ready)
{
    if (m_mail_ready == ready) return;
    m_mail_ready = ready;
    Q_EMIT availabilityChanged();
}

void CybouCoreApplicationAdapter::applySnapshot(QVector<CybouMailItem> items, bool ready,
    CybouRestoreStepState mail_restore)
{
    if (!m_session) return;
    for (const auto& draft : std::as_const(m_drafts)) items.append(draft);
    for (const auto& pending : std::as_const(m_pending_sends)) items.append(pending);
    setReady(ready);
    Q_EMIT mailSnapshot(items);
    Q_EMIT restoreProgressChanged(mail_restore, CybouRestoreStepState::Pending);
}

void CybouCoreApplicationAdapter::finishRotation(bool ok, const QString& error)
{
    if (!m_rotation_done) return;
    auto done = std::move(m_rotation_done);
    m_rotation_done = nullptr;
    done(ok, error);
}

void CybouCoreApplicationAdapter::prepareIdentityRotation(const QStringList& new_words,
    std::function<void(bool, const QString&)> done)
{
    if (!m_session || !m_mail_ready || m_rotation_done) {
        done(false, tr("Your data cannot be secured for a new recovery phrase right now."));
        return;
    }
    cybou::RecoveryWords words{};
    if (new_words.size() != static_cast<int>(words.size())) {
        done(false, tr("The new recovery phrase is invalid."));
        return;
    }
    for (std::size_t i{0}; i < words.size(); ++i) words[i] = new_words.at(static_cast<int>(i)).toStdString();
    m_rotation_done = std::move(done);
    m_session->Post([words = std::move(words)](Session& s) mutable {
        auto entropy = cybou::DecodeRecoveryWords(words);
        for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
        auto seed = entropy ? cybou::DeriveIdentityXWingSeed(*entropy) : std::nullopt;
        const auto future = seed ? cybou::DeriveXWingPublicKey(*seed) : std::nullopt;
        if (seed) cybou::crypto::CleanseMemory(seed->data(), seed->size());
        if (!entropy || !future) {
            if (entropy) cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
            s.ToGui([owner = s.owner] { owner->finishRotation(false, tr("The new recovery phrase is invalid.")); });
            return;
        }
        // Stable per new phrase (public key fingerprint), so a retry resumes the same bridge.
        std::string job_id{"bridge-"};
        for (std::size_t i{0}; i < 16; ++i) {
            job_id.push_back(HEX[(*future)[i] >> 4]);
            job_id.push_back(HEX[(*future)[i] & 0x0f]);
        }
        auto prep = std::make_unique<Session::RotationPrep>();
        prep->job_id = job_id;
        prep->entropy = *entropy;
        cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
        const auto result = s.publication->PublishRecoveryBridge(job_id,
            std::span<const unsigned char, 32>{prep->entropy.data(), 32});
        s.jobs[job_id] = result;
        if (result.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
            s.ToGui([owner = s.owner, error = QString::fromStdString(result.error)] {
                owner->finishRotation(false, error.isEmpty() ? tr("Your recovery data could not be secured.") : error);
            });
            return;
        }
        s.rotation = std::move(prep);
    });
}

void CybouCoreApplicationAdapter::notAvailable()
{
    Q_EMIT commandFailed(tr("This action is not available yet."));
}

/* ---- Mail ---- */

void CybouCoreApplicationAdapter::saveMailDraft(const CybouMailItem& draft)
{
    if (!m_session) return;
    m_drafts.insert(draft.id, draft);
    Q_EMIT mailItemChanged(draft);
}

void CybouCoreApplicationAdapter::sendMail(const CybouMailItem& message)
{
    if (!m_session) return;
    const QString client_id = message.id;
    CybouMailItem pending = message;
    if (!message.attachments.isEmpty()) {
        pending.state = CybouContentState::NeedsAttention;
        m_pending_sends.insert(client_id, pending);
        Q_EMIT mailStateChanged(client_id, CybouContentState::NeedsAttention);
        Q_EMIT commandFailed(tr("Attachments cannot be sent yet. Remove them and send again."));
        return;
    }
    m_pending_sends.insert(client_id, pending);
    m_session->Post([client_id, message](Session& s) {
        const auto recipient = s.ResolveRecipient(message.to_name);
        auto message_id = cybou::NewPrivateItemId();
        if (!recipient || !message_id) {
            s.ToGui([owner = s.owner, client_id, found = recipient.has_value()] {
                if (auto it = owner->m_pending_sends.find(client_id); it != owner->m_pending_sends.end()) {
                    it->state = CybouContentState::NeedsAttention;
                }
                Q_EMIT owner->mailStateChanged(client_id, CybouContentState::NeedsAttention);
                Q_EMIT owner->commandFailed(found ? tr("Could not prepare the message.")
                                                  : tr("No CYBOU Identity has this name."));
            });
            return;
        }
        cybou::MailMessage mail;
        mail.message_id = *message_id;
        mail.recipient_account_id = *recipient;
        mail.client_timestamp_ms = static_cast<std::uint64_t>(message.time.toMSecsSinceEpoch());
        mail.subject = message.subject.toStdString();
        mail.body = message.body.toStdString();
        // The job ID is the message ID, so the outbox and Sent entries line up.
        const auto job_id = ToHex(*message_id);
        CybouMailItem outgoing = message;
        outgoing.id = QString::fromStdString(job_id);
        outgoing.state = CybouContentState::Preparing;
        s.outbox[job_id] = outgoing;
        s.ToGui([owner = s.owner, client_id, outgoing] {
            owner->m_pending_sends.remove(client_id);
            Q_EMIT owner->mailItemRemoved(client_id);
            Q_EMIT owner->mailItemChanged(outgoing);
        });
        s.jobs[job_id] = s.publication->PublishMail(job_id, std::move(mail));
        if (s.jobs[job_id].phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The message could not be sent.")); });
        }
    });
}

void CybouCoreApplicationAdapter::retryMail(const QString& id)
{
    if (!m_session) return;
    // Refused before publication: send the same message again.
    if (const auto pending = m_pending_sends.find(id); pending != m_pending_sends.end()) {
        CybouMailItem message = *pending;
        sendMail(message);
        return;
    }
    m_session->Post([job_id = id.toStdString()](Session& s) { s.jobs[job_id] = s.publication->Resume(job_id); });
}

void CybouCoreApplicationAdapter::setMailRead(const QString& id, bool read)
{
    const auto message_id = FromHex(id);
    if (!m_session || !message_id) return;
    m_session->Post([message_id = *message_id, read](Session& s) { s.application->SetMailRead(message_id, read); });
}

void CybouCoreApplicationAdapter::setMailStarred(const QString& id, bool starred)
{
    const auto message_id = FromHex(id);
    if (!m_session || !message_id) return;
    m_session->Post([message_id = *message_id, starred](Session& s) { s.application->SetMailStarred(message_id, starred); });
}

void CybouCoreApplicationAdapter::moveMail(const QString& id, CybouMailFolder folder)
{
    const auto message_id = FromHex(id);
    const auto target = CoreFolder(folder);
    if (!m_session || !message_id || !target) return;
    m_session->Post([owner = this, message_id = *message_id, target = *target](Session& s) {
        if (!s.application->MoveMail(message_id, target)) {
            s.ToGui([owner] { Q_EMIT owner->commandFailed(tr("This message cannot be moved there.")); });
        }
    });
}

void CybouCoreApplicationAdapter::deleteMail(const QString& id)
{
    if (!m_session) return;
    if (m_drafts.remove(id) > 0 || m_pending_sends.remove(id) > 0) {
        Q_EMIT mailItemRemoved(id);
        return;
    }
    // Delivered mail is part of finalized history and rebuilds from it; it stays in Trash.
    moveMail(id, CybouMailFolder::Trash);
}

void CybouCoreApplicationAdapter::downloadAttachment(const QString& message_id, const QString& attachment_id,
    const QString&)
{
    Q_EMIT attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Idle);
    notAvailable();
}

void CybouCoreApplicationAdapter::saveAttachmentToFiles(const QString&, const QString&, const QString&)
{
    notAvailable();
}

/* ---- Files: not connected yet (filesAvailable() is false) ---- */

void CybouCoreApplicationAdapter::uploadFile(const QString&, const QString&, const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::downloadFile(const QString& id, const QString&)
{
    Q_EMIT fileRetrievalChanged(id, CybouRetrievalState::Idle);
    notAvailable();
}
void CybouCoreApplicationAdapter::createFolder(const QString&, const QString&, const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::renameFile(const QString&, const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::moveFile(const QString&, const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::copyFile(const QString&, const QString&, const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::setFileStarred(const QString&, bool) { notAvailable(); }
void CybouCoreApplicationAdapter::trashFile(const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::restoreFile(const QString&) { notAvailable(); }
void CybouCoreApplicationAdapter::deleteFile(const QString&) { notAvailable(); }
