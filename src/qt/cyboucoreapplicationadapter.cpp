// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboucoreapplicationadapter.h>

#include <cybou/application_service.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/identity_kem.h>
#include <cybou/identity_service.h>
#include <cybou/kv_store.h>
#include <cybou/node_runtime.h>
#include <cybou/private_application_store.h>
#include <cybou/publication_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/storage_service.h>

#include <QFile>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <set>
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

std::string RandomJobId(const char* prefix)
{
    const auto id = cybou::NewPrivateItemId();
    std::string out{prefix};
    if (!id) return {};
    for (std::size_t i{0}; i < 16; ++i) {
        out.push_back(HEX[(*id)[i] >> 4]);
        out.push_back(HEX[(*id)[i] & 0x0f]);
    }
    return out;
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

/** Operation axis of a publication job; content durability is StateOf. */
CybouOperationState OperationOf(const cybou::PublicationJobResult& job)
{
    switch (job.phase) {
    case cybou::PublicationJobPhase::QUEUED: return CybouOperationState::Preparing;
    case cybou::PublicationJobPhase::WAITING_FINALITY: return CybouOperationState::Submitted;
    case cybou::PublicationJobPhase::SECURING:
    case cybou::PublicationJobPhase::PROTECTED: return CybouOperationState::Finalized;
    case cybou::PublicationJobPhase::NEEDS_ATTENTION:
        return job.finalized_height > 0 ? CybouOperationState::Finalized : CybouOperationState::Failed;
    }
    return CybouOperationState::Failed;
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
    /** Snapshots of a replaced session (lock, rotation reopen) never reach the GUI. */
    std::uint64_t generation;
    cybou::CybouNodeRuntime& runtime;
    cybou::CybouKeyStore& keystore;
    std::filesystem::path root;
    int refresh_ms;

    std::unique_ptr<cybou::PrivateApplicationStore> db;
    std::unique_ptr<cybou::KVStore> staging;
    std::unique_ptr<cybou::RuntimeStorageTransport> transport;
    cybou::StorageTransport* transport_override{nullptr};
    std::unique_ptr<cybou::StorageService> storage;
    std::unique_ptr<cybou::PublicationService> publication;
    std::unique_ptr<cybou::ApplicationService> application;

    /** Messages sent from this device that the scanner has not indexed yet. */
    std::map<std::string, CybouMailItem> outbox;
    std::map<std::string, cybou::PublicationJobResult> jobs;
    std::uint64_t ticks{0};
    static constexpr std::uint64_t AUDIT_EVERY_TICKS{5};
    static constexpr std::size_t AUDIT_CHUNKS_PER_PASS{8};
    /** Local encrypted cache beyond pins and provider obligations; LRU-evicted. */
    static constexpr std::uint64_t GC_EVERY_TICKS{60};
    static constexpr std::uint64_t LOCAL_CACHE_BUDGET_BYTES{2ULL << 30};
    /** Files changes published from this device that the scanner has not reflected yet. */
    struct PendingFile {
        cybou::FileItem item;
        std::string job_id;
        bool deleted{false};
        QDateTime modified;
    };
    std::map<std::string, PendingFile> file_overlay;
    /** Last publication job that touched each Files item. */
    std::map<std::string, std::string> item_jobs;
    std::map<std::string, QDateTime> file_modified;
    /** Content root -> its whole encrypted tree is in the local ChunkStore; refreshed on change. */
    std::map<cybou::ChunkId, bool> locally_complete;

    /**
     * Available offline from real local chunks, never from past actions:
     * ROOT/INDEX chunks decrypt from local blobs and every DATA chunk is
     * present locally. DATA is not read.
     */
    bool AvailableOffline(const cybou::FileItem& item)
    {
        if (!item.root_chunk_id || !item.content_key) return false;
        const auto cached = locally_complete.find(*item.root_chunk_id);
        if (cached != locally_complete.end()) return cached->second;
        const auto& blobs = runtime.GetChunkBlobStore();
        bool all_present{true};
        const bool walked = cybou::EnumerateEncryptedTreeChunks(
            std::span<const unsigned char, 32>{runtime.GetNetworkId().begin(), 32}, *item.content_key,
            *item.root_chunk_id, [&](const cybou::ChunkId& id) { return blobs.Get(id); },
            [&](const cybou::ChunkId& id) {
                all_present = all_present && blobs.Has(id);
                return all_present;
            });
        const bool complete = walked && all_present;
        locally_complete[*item.root_chunk_id] = complete;
        return complete;
    }

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
        std::filesystem::path identity_root, int interval, cybou::StorageTransport* override_transport)
        : owner{adapter}, generation{adapter->m_session_generation}, runtime{rt}, keystore{ks}, root{std::move(identity_root)}, refresh_ms{interval},
          transport_override{override_transport}
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

    /** State from this session only: a late snapshot of a replaced one is stale. */
    template <typename F>
    void StateToGui(F&& f)
    {
        QMetaObject::invokeMethod(owner, [owner = owner, generation = generation, f = std::forward<F>(f)]() mutable {
            if (owner->m_session && owner->m_session_generation == generation) f();
        }, Qt::QueuedConnection);
    }

    bool Open()
    {
        try {
            std::filesystem::create_directories(root);
            try {
                db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root);
            } catch (const cybou::PrivateApplicationStoreKeyMismatch&) {
                // Encrypted under keys replaced by IdentityRotate: the projection
                // is rebuilt from finalized history (RecoveryBridge included).
                std::filesystem::remove_all(root / "app.db");
                db = std::make_unique<cybou::PrivateApplicationStore>(keystore, root);
            }
            staging = std::make_unique<cybou::KVStore>(cybou::KVStoreOptions{.path = root / "staging"});
            transport = std::make_unique<cybou::RuntimeStorageTransport>(runtime);
            storage = std::make_unique<cybou::StorageService>(runtime,
                transport_override ? *transport_override : static_cast<cybou::StorageTransport&>(*transport), *db);
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
        StateToGui([owner = owner, ready] { owner->setReady(ready); });
        if (!ready) return;
        while (!stop.stop_requested()) {
            std::deque<std::function<void(Session&)>> pending;
            {
                std::unique_lock lock{mutex};
                wake.wait_for(lock, stop, std::chrono::milliseconds{refresh_ms}, [this] { return !tasks.empty(); });
                pending.swap(tasks);
            }
            try {
                for (auto& task : pending) task(*this);
                if (!stop.stop_requested()) Refresh();
            } catch (const std::exception&) {
                // A failed refresh leaves the last snapshot in place; the next tick retries.
            }
        }
        // Commands issued just before locking (a saved draft, a send) still run.
        std::deque<std::function<void(Session&)>> remaining;
        {
            std::lock_guard lock{mutex};
            remaining.swap(tasks);
        }
        try {
            for (auto& task : remaining) task(*this);
        } catch (const std::exception&) {
        }
    }

    /** Scan, advance durability and publish one Mail snapshot. */
    void Refresh()
    {
        // Same Identity, new key material (IdentityRotate finalized elsewhere): reopen.
        if (!db->IsUnlocked() && keystore.HasKey() && keystore.GetAccountId() == std::optional{db->Account()}) {
            StateToGui([owner = owner] { owner->identityKeysChanged(); });
            return;
        }
        const auto progress = application->Scan();
        // Bounded durability audit every few ticks: Protected can fall back to Securing.
        if (++ticks % AUDIT_EVERY_TICKS == 0) storage->AuditNextPlacement(AUDIT_CHUNKS_PER_PASS);
        if (ticks % GC_EVERY_TICKS == 0) {
            (void)runtime.CollectChunkGarbage(LOCAL_CACHE_BUDGET_BYTES, static_cast<std::uint64_t>(
                QDateTime::currentMSecsSinceEpoch()));
            locally_complete.clear(); // eviction or outside changes
        }
        for (const auto& [id, status] : publication->ProcessDurability(*storage)) jobs[id] = status;
        AdvanceRotation();
        Snapshot(progress.Complete() ? CybouRestoreStepState::Done : CybouRestoreStepState::Running);
    }

    void Snapshot(CybouRestoreStepState mail_restore)
    {
        const auto loaded = runtime.GetStore().LoadState();
        const cybou::CybouState* state = loaded && loaded.state ? &*loaded.state : nullptr;
        QVector<CybouMailItem> items;
        std::map<cybou::ChunkId, std::string> saved_roots;
        for (const auto& [hex, file] : Catalog()) {
            if (file.root_chunk_id) saved_roots.emplace(*file.root_chunk_id, hex);
        }
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
                item.operation_state = job != jobs.end() ? OperationOf(job->second) : CybouOperationState::Finalized;
            } else {
                // Incoming: finalized and decrypted here, but the sender's
                // remote durability is not something this Identity has proved.
                item.state = CybouContentState::Received;
            }
            for (const auto& attachment : record.message.attachments) {
                CybouAttachmentItem a;
                a.id = QString::fromStdString(ToHex(attachment.attachment_id));
                a.name = QString::fromStdString(attachment.filename);
                a.logical_size = attachment.logical_size;
                // Sent content shares its message's durability; received content is the sender's.
                a.state = record.outgoing ? item.state : CybouContentState::Received;
                // "Saved to Files" is a Files entry referencing the same protected content.
                if (const auto saved = saved_roots.find(attachment.root_chunk_id); saved != saved_roots.end()) {
                    a.saved_file_id = QString::fromStdString(saved->second);
                }
                item.attachments.append(a);
            }
            items.append(item);
        }
        for (const auto& draft : application->ListDrafts()) {
            CybouMailItem item;
            item.id = QString::fromStdString(draft.draft_id);
            item.folder = CybouMailFolder::Drafts;
            item.draft = true;
            item.state = CybouContentState::Local;
            item.to_name = QString::fromStdString(draft.to);
            item.subject = QString::fromStdString(draft.subject);
            item.body = QString::fromStdString(draft.body);
            item.preview = item.body.simplified().left(90);
            item.time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(draft.updated_ms));
            for (std::size_t i{0}; i < draft.attachments.size(); ++i) {
                const auto& stored = draft.attachments[i];
                CybouAttachmentItem attachment;
                attachment.name = QString::fromStdString(stored.name);
                attachment.logical_size = stored.logical_size;
                attachment.source_path = QString::fromStdString(stored.source_path);
                attachment.id = stored.reference_id.empty()
                    ? QStringLiteral("att-draft-%1").arg(i) : QString::fromStdString(stored.reference_id);
                attachment.state = stored.reference_id.empty() ? CybouContentState::Local : CybouContentState::Protected;
                item.attachments.append(attachment);
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
        auto files = FilesSnapshot();
        StateToGui([owner = owner, items = std::move(items), files = std::move(files), mail_restore]() mutable {
            owner->applySnapshot(std::move(items), std::move(files), true, mail_restore);
        });
    }

    /** Current intended state of an item: pending local change, else indexed history. */
    std::optional<cybou::FileItem> CurrentFile(const std::string& hex)
    {
        if (const auto pending = file_overlay.find(hex); pending != file_overlay.end()) {
            if (pending->second.deleted) return std::nullopt;
            return pending->second.item;
        }
        const auto id = FromHex(QString::fromStdString(hex));
        if (!id) return std::nullopt;
        const auto record = application->GetFile(*id);
        return record ? std::optional{record->item} : std::nullopt;
    }

    /** Protected content behind a Compose "ref-<file>" attachment: root, key, size. */
    std::optional<std::pair<cybou::ChunkId, std::pair<cybou::ContentKey, std::uint64_t>>> ContentOfAttachmentSource(
        const QString& attachment_id)
    {
        if (!attachment_id.startsWith(QStringLiteral("ref-"))) return std::nullopt;
        const auto item = CurrentFile(attachment_id.mid(4).toStdString());
        if (!item || !item->root_chunk_id || !item->content_key) return std::nullopt;
        return std::pair{*item->root_chunk_id, std::pair{*item->content_key, item->logical_size}};
    }

    /** Streams verified content into destination via a .part file; empty on success. */
    QString DownloadContent(const cybou::ChunkId& root, const cybou::ContentKey& key, std::uint64_t size,
        const QString& destination)
    {
        const QString part = destination + QStringLiteral(".part");
        QFile out{part};
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return tr("The destination cannot be written.");
        std::set<cybou::ChunkId> seen;
        bool missing{false};
        const auto written = cybou::FetchEncryptedChunkTree(
            std::span<const unsigned char, 32>{runtime.GetNetworkId().begin(), 32}, key, root,
            [&](const cybou::ChunkId& chunk) {
                auto bytes = storage->Fetch(chunk);
                if (!bytes) missing = true;
                return bytes;
            },
            [](std::span<const unsigned char>) { return true; },
            [&](const cybou::ChunkId& chunk) { return seen.insert(chunk).second; },
            [&](std::span<const unsigned char> data) {
                return out.write(reinterpret_cast<const char*>(data.data()), static_cast<qint64>(data.size())) ==
                    static_cast<qint64>(data.size());
            },
            std::max<std::uint64_t>(size, 1));
        out.close();
        if (!written || *written != size) {
            QFile::remove(part);
            return missing ? tr("This content is temporarily unavailable. Try again later.")
                           : tr("This content could not be verified.");
        }
        QFile::remove(destination);
        if (!QFile::rename(part, destination)) {
            QFile::remove(part);
            return tr("The destination cannot be written.");
        }
        return {};
    }

    /** Current catalog: indexed history overlaid with pending local changes. */
    std::map<std::string, cybou::FileItem> Catalog()
    {
        std::map<std::string, cybou::FileItem> catalog;
        for (const auto& record : application->ListFiles()) catalog[ToHex(record.item.item_id)] = record.item;
        for (const auto& [hex, pending] : file_overlay) {
            if (pending.deleted) catalog.erase(hex);
            else catalog[hex] = pending.item;
        }
        return catalog;
    }

    /** One FILES_MUTATION_BATCH publication; the overlay shows it until indexed. */
    bool PublishFileChange(cybou::FilesMutationBatch batch, std::optional<std::pair<std::size_t, cybou::NewContent>> content)
    {
        const auto job_id = RandomJobId("files-");
        if (job_id.empty()) return false;
        std::vector<std::pair<std::size_t, cybou::NewContent>> contents;
        if (content) contents.push_back(std::move(*content));
        const auto touched = batch.mutations;
        const auto result = publication->PublishFiles(job_id, std::move(batch), std::move(contents));
        jobs[job_id] = result;
        if (result.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) return false;
        const auto now = QDateTime::currentDateTime();
        for (const auto& mutation : touched) {
            const auto hex = ToHex(mutation.item_id);
            PendingFile pending{.job_id = job_id, .deleted = mutation.kind == cybou::FileMutationKind::DELETE_ITEM,
                .modified = now};
            if (mutation.item) pending.item = *mutation.item;
            file_overlay[hex] = pending;
            item_jobs[hex] = job_id;
            file_modified[hex] = now;
        }
        return true;
    }

    QVector<CybouFileItem> FilesSnapshot()
    {
        // A pending change is dropped only once history reflects that exact publication.
        for (auto it = file_overlay.begin(); it != file_overlay.end();) {
            const auto job = jobs.find(it->second.job_id);
            const bool finalized = job != jobs.end() && !job->second.operation_id.IsNull() &&
                (job->second.phase == cybou::PublicationJobPhase::SECURING ||
                    job->second.phase == cybou::PublicationJobPhase::PROTECTED);
            bool indexed{false};
            if (finalized) {
                const auto id = FromHex(QString::fromStdString(it->first));
                const auto current = id ? application->GetFile(*id) : std::nullopt;
                indexed = it->second.deleted ? !current
                    : current && current->operation_id == job->second.operation_id;
            }
            it = indexed ? file_overlay.erase(it) : std::next(it);
        }
        std::map<std::string, uint256> operations;
        std::set<std::string> starred;
        for (const auto& record : application->ListFiles()) {
            operations[ToHex(record.item.item_id)] = record.operation_id;
            if (record.starred) starred.insert(ToHex(record.item.item_id));
        }
        const auto catalog = Catalog();
        const auto trash = ToHex(cybou::FilesTrashParent());
        const auto parent_of = [&](const cybou::FileItem& item) {
            return item.parent_id ? ToHex(*item.parent_id) : std::string{};
        };
        const auto in_trash = [&](std::string hex) {
            for (int depth{0}; depth < 64 && !hex.empty(); ++depth) {
                const auto it = catalog.find(hex);
                if (it == catalog.end()) return false;
                const auto parent = parent_of(it->second);
                if (parent == trash) return true;
                hex = parent;
            }
            return false;
        };
        QVector<CybouFileItem> files;
        for (const auto& [hex, item] : catalog) {
            CybouFileItem out;
            out.id = QString::fromStdString(hex);
            out.name = QString::fromStdString(item.name);
            const auto parent = parent_of(item);
            out.parent_id = parent == trash ? QString{} : QString::fromStdString(parent);
            out.folder = item.kind == cybou::FileItemKind::FOLDER;
            out.logical_size = item.logical_size;
            out.trashed = in_trash(hex);
            if (item.modified_ms != 0) out.modified = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.modified_ms));
            if (item.root_chunk_id) out.content_root_id = ChunkHex(*item.root_chunk_id);
            out.starred = starred.contains(hex);
            const auto job = item_jobs.find(hex);
            const auto status = job != item_jobs.end() ? jobs.find(job->second) : jobs.end();
            out.available_offline = AvailableOffline(item);
            if (status != jobs.end()) {
                out.state = StateOf(status->second);
                out.operation_state = OperationOf(status->second);
                if (!status->second.operation_id.IsNull()) out.operation_id = QString::fromStdString(status->second.operation_id.GetHex());
                out.progress_percent = status->second.phase == cybou::PublicationJobPhase::SECURING
                    ? status->second.durability_percent : -1;
                out.finalized_height = status->second.finalized_height;
            } else if (const auto op = operations.find(hex); op != operations.end()) {
                // Protected only when remote durability is known, never merely finalized.
                const auto durability = storage->GetDurability(op->second);
                out.state = durability && durability->state == cybou::DurabilityState::PROTECTED
                    ? CybouContentState::Protected : CybouContentState::Securing;
                out.finalized_height = runtime.FindFinalizedOperation(op->second).height;
                out.operation_id = QString::fromStdString(op->second.GetHex());
                out.operation_state = CybouOperationState::Finalized;
            }
            files.append(out);
        }
        return files;
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
    const auto root = cybou::IdentityDataDirectory(m_data_directory, *account);
    ++m_session_generation;
    m_session = std::make_unique<Session>(this, m_runtime, m_identity.GetKeyStore(), root, m_refresh_ms,
        m_transport_override);
}

void CybouCoreApplicationAdapter::identityKeysChanged()
{
    if (!m_session || m_reopening) return;
    m_reopening = true;
    // Drafts exist only on this device: keep the latest ones across the reopen.
    auto drafts = m_known_drafts;
    for (const auto& pending : std::as_const(m_pending_drafts)) drafts.insert(pending.id, pending);
    m_session.reset();
    setReady(false);
    openIdentity();
    for (const auto& draft : std::as_const(drafts)) {
        if (!m_deleted_drafts.contains(draft.id)) saveMailDraft(draft);
    }
    m_reopening = false;
}

void CybouCoreApplicationAdapter::closeIdentity()
{
    m_session.reset();
    finishRotation(false, tr("CYBOU was locked before your data was secured. The current recovery phrase stays active."));
    m_pending_drafts.clear();
    m_known_drafts.clear();
    m_deleted_drafts.clear();
    m_pending_sends.clear();
    m_client_ids.clear();
    m_last_files.clear();
    m_pending_files.clear();
    m_pending_stars.clear();
    setReady(false);
}

void CybouCoreApplicationAdapter::setReady(bool ready)
{
    if (m_mail_ready == ready) return;
    m_mail_ready = ready;
    Q_EMIT availabilityChanged();
}

void CybouCoreApplicationAdapter::applySnapshot(QVector<CybouMailItem> items, QVector<CybouFileItem> files,
    bool ready, CybouRestoreStepState restore)
{
    if (!m_session) return;
    // Drafts: the latest local edit wins until the worker has stored it; a
    // deleted draft stays hidden until the stored copy is gone.
    QSet<QString> present;
    for (auto it = items.begin(); it != items.end();) {
        if (!it->draft) {
            ++it;
            continue;
        }
        present.insert(it->id);
        if (m_deleted_drafts.contains(it->id)) {
            it = items.erase(it);
            continue;
        }
        if (const auto pending = m_pending_drafts.find(it->id); pending != m_pending_drafts.end()) {
            if (pending->time == it->time) m_pending_drafts.erase(pending);
            else *it = *pending;
        }
        ++it;
    }
    for (auto it = m_deleted_drafts.begin(); it != m_deleted_drafts.end();) {
        it = present.contains(*it) ? std::next(it) : m_deleted_drafts.erase(it);
    }
    for (const auto& pending : std::as_const(m_pending_drafts)) {
        if (!present.contains(pending.id)) items.append(pending);
    }
    for (const auto& pending : std::as_const(m_pending_sends)) items.append(pending);
    // Drafts live only on this device. One the store no longer returns (a
    // projection rebuilt under rotated keys) is written back, never dropped;
    // only an explicit delete forgets it.
    QVector<CybouMailItem> lost;
    for (const auto& known : std::as_const(m_known_drafts)) {
        if (!present.contains(known.id) && !m_pending_drafts.contains(known.id) && !m_deleted_drafts.contains(known.id)) {
            lost.append(known);
        }
    }
    for (const auto& item : std::as_const(items)) {
        if (item.draft) m_known_drafts.insert(item.id, item);
    }
    setReady(ready);
    for (const auto& draft : std::as_const(lost)) items.append(draft);
    Q_EMIT mailSnapshot(items);
    for (const auto& draft : std::as_const(lost)) saveMailDraft(draft);
    m_last_files = std::move(files);
    emitFiles();
    Q_EMIT restoreProgressChanged(restore, restore);
}

void CybouCoreApplicationAdapter::showPendingFile(const CybouFileItem& item)
{
    m_pending_files.insert(item.id, item);
    emitFiles();
}

void CybouCoreApplicationAdapter::emitFiles()
{
    QVector<CybouFileItem> files = m_last_files;
    for (const auto& file : std::as_const(m_last_files)) m_pending_files.remove(file.id);
    for (const auto& pending : std::as_const(m_pending_files)) files.append(pending);
    for (auto& file : files) {
        const auto pending = m_pending_stars.find(file.id);
        if (pending == m_pending_stars.end()) continue;
        const bool indexed = std::any_of(m_last_files.begin(), m_last_files.end(),
            [&](const CybouFileItem& f) { return f.id == file.id; });
        if (indexed && file.starred == *pending) {
            m_pending_stars.erase(pending);
            continue;
        }
        // Starred before it was indexed: store it now that the item exists.
        if (indexed) postFileStar(file.id, *pending);
        file.starred = *pending;
    }
    Q_EMIT filesSnapshot(files);
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
    m_pending_drafts.insert(draft.id, draft);
    m_deleted_drafts.remove(draft.id);
    Q_EMIT mailItemChanged(draft);
    cybou::MailDraft stored{.draft_id = draft.id.toStdString(), .to = draft.to_name.toStdString(),
        .subject = draft.subject.toStdString(), .body = draft.body.toStdString(),
        .updated_ms = static_cast<std::uint64_t>(std::max<qint64>(0, draft.time.toMSecsSinceEpoch()))};
    for (const auto& attachment : draft.attachments) {
        stored.attachments.push_back({.name = attachment.name.toStdString(), .logical_size = attachment.logical_size,
            .source_path = attachment.source_path.toStdString(),
            .reference_id = attachment.source_path.isEmpty() ? attachment.id.toStdString() : std::string{}});
    }
    m_session->Post([stored = std::move(stored)](Session& s) {
        if (!s.application->SaveDraft(stored)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The draft could not be saved.")); });
        }
    });
}

void CybouCoreApplicationAdapter::sendMail(const CybouMailItem& message)
{
    if (!m_session) return;
    const QString client_id = message.id;
    CybouMailItem pending = message;
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
        // New local files become encrypted child trees of this publication; Files
        // references reuse their protected content without re-upload.
        std::vector<std::pair<std::size_t, cybou::NewContent>> new_content;
        QString attachment_error;
        for (const auto& attachment : message.attachments) {
            const auto attachment_id = cybou::NewPrivateItemId();
            if (!attachment_id) {
                attachment_error = tr("Could not prepare the message.");
                break;
            }
            cybou::MailAttachment descriptor{.attachment_id = *attachment_id, .filename = attachment.name.toStdString(),
                .logical_size = attachment.logical_size};
            if (!attachment.source_path.isEmpty()) {
                auto file = std::make_shared<QFile>(attachment.source_path);
                if (!file->open(QIODevice::ReadOnly)) {
                    attachment_error = tr("%1 could not be read.").arg(attachment.name);
                    break;
                }
                new_content.emplace_back(mail.attachments.size(), cybou::NewContent{
                    [file](std::span<unsigned char> out) -> std::optional<std::size_t> {
                        const auto n = file->read(reinterpret_cast<char*>(out.data()), static_cast<qint64>(out.size()));
                        if (n < 0) return std::nullopt;
                        return static_cast<std::size_t>(n);
                    }});
            } else if (const auto reused = s.ContentOfAttachmentSource(attachment.id)) {
                descriptor.root_chunk_id = reused->first;
                descriptor.content_key = reused->second.first;
                descriptor.logical_size = reused->second.second;
            } else {
                attachment_error = tr("%1 is not protected yet.").arg(attachment.name);
                break;
            }
            mail.attachments.push_back(descriptor);
        }
        if (!attachment_error.isEmpty()) {
            s.ToGui([owner = s.owner, client_id, attachment_error] {
                if (auto it = owner->m_pending_sends.find(client_id); it != owner->m_pending_sends.end()) {
                    it->state = CybouContentState::NeedsAttention;
                }
                Q_EMIT owner->mailStateChanged(client_id, CybouContentState::NeedsAttention);
                Q_EMIT owner->commandFailed(attachment_error);
            });
            return;
        }
        // The job ID is the message ID, so the outbox and Sent entries line up.
        const auto job_id = ToHex(*message_id);
        CybouMailItem outgoing = message;
        outgoing.id = QString::fromStdString(job_id);
        outgoing.state = CybouContentState::Preparing;
        for (int i = 0; i < outgoing.attachments.size(); ++i) {
            auto& shown = outgoing.attachments[i];
            shown.id = QString::fromStdString(ToHex(mail.attachments[static_cast<std::size_t>(i)].attachment_id));
            shown.source_path.clear();
            if (shown.state != CybouContentState::Protected) shown.state = CybouContentState::Preparing;
        }
        s.outbox[job_id] = outgoing;
        s.ToGui([owner = s.owner, client_id, outgoing] {
            owner->m_pending_sends.remove(client_id);
            Q_EMIT owner->mailItemRemoved(client_id);
            Q_EMIT owner->mailItemChanged(outgoing);
        });
        s.jobs[job_id] = s.publication->PublishMail(job_id, std::move(mail), std::move(new_content));
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
    if (m_pending_sends.remove(id) > 0) {
        Q_EMIT mailItemRemoved(id);
        return;
    }
    if (id.startsWith(QStringLiteral("draft-"))) {
        m_pending_drafts.remove(id);
        m_known_drafts.remove(id);
        m_deleted_drafts.insert(id);
        Q_EMIT mailItemRemoved(id);
        m_session->Post([draft_id = id.toStdString()](Session& s) { s.application->DeleteDraft(draft_id); });
        return;
    }
    // Delivered mail is part of finalized history and rebuilds from it; it stays in Trash.
    moveMail(id, CybouMailFolder::Trash);
}

void CybouCoreApplicationAdapter::downloadAttachment(const QString& message_id, const QString& attachment_id,
    const QString& destination)
{
    const auto message = FromHex(message_id);
    if (!m_session || !message) return;
    m_session->Post([message = *message, message_id, attachment_id, destination](Session& s) {
        QString error = tr("This attachment is not available.");
        if (const auto record = s.application->GetMail(message)) {
            for (const auto& attachment : record->message.attachments) {
                if (QString::fromStdString(ToHex(attachment.attachment_id)) != attachment_id) continue;
                error = s.DownloadContent(attachment.root_chunk_id, attachment.content_key, attachment.logical_size,
                    destination);
            }
        }
        s.ToGui([owner = s.owner, message_id, attachment_id, error] {
            if (!error.isEmpty()) {
                Q_EMIT owner->attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Idle);
                Q_EMIT owner->commandFailed(error);
                return;
            }
            Q_EMIT owner->attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Ready);
            QMetaObject::invokeMethod(owner, [owner, message_id, attachment_id] {
                Q_EMIT owner->attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Idle);
            }, Qt::QueuedConnection);
        });
    });
}

void CybouCoreApplicationAdapter::saveAttachmentToFiles(const QString& message_id, const QString& attachment_id,
    const QString& file_id)
{
    const auto message = FromHex(message_id);
    const auto item_id = cybou::NewPrivateItemId();
    if (!m_session || !message || !item_id) return;
    const QString hex = QString::fromStdString(ToHex(*item_id));
    m_client_ids.insert(file_id, hex);
    m_session->Post([message = *message, attachment_id, item_id = *item_id](Session& s) {
        const auto record = s.application->GetMail(message);
        if (!record) return;
        for (const auto& attachment : record->message.attachments) {
            if (QString::fromStdString(ToHex(attachment.attachment_id)) != attachment_id) continue;
            // A new Files entry referencing the same protected content: no download, no upload.
            cybou::FilesMutationBatch batch;
            batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
                cybou::FileItem{.item_id = item_id, .kind = cybou::FileItemKind::FILE, .name = attachment.filename,
                    .logical_size = attachment.logical_size, .root_chunk_id = attachment.root_chunk_id,
                    .content_key = attachment.content_key}});
            if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
                s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The attachment could not be saved to Files.")); });
            }
            return;
        }
    });
}

/* ---- Files ---- */

namespace {
std::optional<cybou::PrivateItemId> ParentId(const QString& hex)
{
    if (hex.isEmpty()) return std::nullopt;
    return FromHex(hex);
}
} // namespace

void CybouCoreApplicationAdapter::uploadFile(const QString& file_id, const QString& source_path, const QString& parent_id)
{
    const auto item_id = cybou::NewPrivateItemId();
    if (!m_session || !item_id) return;
    const QString hex = QString::fromStdString(ToHex(*item_id));
    m_client_ids.insert(file_id, hex);
    const QFileInfo info{source_path};
    CybouFileItem shown;
    shown.id = hex;
    shown.name = info.fileName();
    shown.parent_id = resolveFileId(parent_id);
    shown.logical_size = static_cast<quint64>(std::max<qint64>(0, info.size()));
    shown.modified = QDateTime::currentDateTime();
    shown.state = CybouContentState::Preparing;
    shown.operation_state = CybouOperationState::Preparing;
    shown.available_offline = true;
    showPendingFile(shown);
    m_session->Post([item_id = *item_id, name = shown.name.toStdString(), parent = ParentId(shown.parent_id),
                        path = source_path, hex = hex.toStdString()](Session& s) {
        auto file = std::make_shared<QFile>(path);
        if (!file->open(QIODevice::ReadOnly)) {
            s.ToGui([owner = s.owner, id = QString::fromStdString(hex)] {
                if (auto it = owner->m_pending_files.find(id); it != owner->m_pending_files.end()) {
                    it->state = CybouContentState::NeedsAttention;
                }
                Q_EMIT owner->fileStateChanged(id, CybouContentState::NeedsAttention, -1);
                Q_EMIT owner->commandFailed(tr("The file could not be read."));
            });
            return;
        }
        cybou::FileItem item{.item_id = item_id, .parent_id = parent, .kind = cybou::FileItemKind::FILE, .name = name};
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id, item});
        // Content is streamed from disk into encrypted chunks; no plaintext copy is kept.
        cybou::NewContent content{[file](std::span<unsigned char> out) -> std::optional<std::size_t> {
            const auto n = file->read(reinterpret_cast<char*>(out.data()), static_cast<qint64>(out.size()));
            if (n < 0) return std::nullopt;
            return static_cast<std::size_t>(n);
        }};
        const bool ok = s.PublishFileChange(std::move(batch), std::pair{std::size_t{0}, std::move(content)});
        if (ok) {
            // The staged size is authoritative once indexed; show the source size meanwhile.
            s.file_overlay[hex].item.logical_size = static_cast<std::uint64_t>(file->size());
        } else {
            s.ToGui([owner = s.owner, id = QString::fromStdString(hex)] {
                if (auto it = owner->m_pending_files.find(id); it != owner->m_pending_files.end()) {
                    it->state = CybouContentState::NeedsAttention;
                }
                Q_EMIT owner->fileStateChanged(id, CybouContentState::NeedsAttention, -1);
                Q_EMIT owner->commandFailed(tr("The file could not be uploaded."));
            });
        }
    });
}

void CybouCoreApplicationAdapter::downloadFile(const QString& file_id, const QString& destination)
{
    const QString hex = resolveFileId(file_id);
    if (!m_session) return;
    m_session->Post([hex = hex.toStdString(), destination](Session& s) {
        const auto item = s.CurrentFile(hex);
        const auto id = QString::fromStdString(hex);
        const QString error = item && item->root_chunk_id && item->content_key
            ? s.DownloadContent(*item->root_chunk_id, *item->content_key, item->logical_size, destination)
            : tr("This file has no content yet.");
        if (error.isEmpty()) s.locally_complete.clear();
        s.ToGui([owner = s.owner, id, error] {
            if (!error.isEmpty()) {
                Q_EMIT owner->fileRetrievalChanged(id, CybouRetrievalState::Idle);
                Q_EMIT owner->commandFailed(error);
                return;
            }
            Q_EMIT owner->fileRetrievalChanged(id, CybouRetrievalState::Ready);
            QMetaObject::invokeMethod(owner, [owner, id] {
                Q_EMIT owner->fileRetrievalChanged(id, CybouRetrievalState::Idle);
            }, Qt::QueuedConnection);
        });
    });
}

void CybouCoreApplicationAdapter::createFolder(const QString& folder_id, const QString& name, const QString& parent_id)
{
    const auto item_id = cybou::NewPrivateItemId();
    if (!m_session || !item_id) return;
    const QString hex = QString::fromStdString(ToHex(*item_id));
    m_client_ids.insert(folder_id, hex);
    CybouFileItem shown;
    shown.id = hex;
    shown.name = name;
    shown.parent_id = resolveFileId(parent_id);
    shown.folder = true;
    shown.modified = QDateTime::currentDateTime();
    shown.state = CybouContentState::Preparing;
    shown.operation_state = CybouOperationState::Preparing;
    showPendingFile(shown);
    m_session->Post([item_id = *item_id, name = name.toStdString(), parent = ParentId(shown.parent_id)](Session& s) {
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
            cybou::FileItem{.item_id = item_id, .parent_id = parent, .kind = cybou::FileItemKind::FOLDER, .name = name}});
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The folder could not be created.")); });
        }
    });
}

void CybouCoreApplicationAdapter::renameFile(const QString& id, const QString& name)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString(), name = name.toStdString()](Session& s) {
        auto item = s.CurrentFile(hex);
        if (!item) return;
        item->name = name;
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be renamed.")); });
        }
    });
}

void CybouCoreApplicationAdapter::moveFile(const QString& id, const QString& parent_id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString(), parent = ParentId(resolveFileId(parent_id))](Session& s) {
        auto item = s.CurrentFile(hex);
        if (!item) return;
        // Never move a folder into itself or its own subtree.
        for (auto cursor = parent; cursor;) {
            if (*cursor == item->item_id) return;
            const auto above = s.CurrentFile(ToHex(*cursor));
            cursor = above ? above->parent_id : std::nullopt;
        }
        item->parent_id = parent;
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be moved.")); });
        }
    });
}

void CybouCoreApplicationAdapter::copyFile(const QString& id, const QString& copy_id, const QString& parent_id)
{
    const auto new_id = cybou::NewPrivateItemId();
    if (!m_session || !new_id) return;
    m_client_ids.insert(copy_id, QString::fromStdString(ToHex(*new_id)));
    m_session->Post([hex = resolveFileId(id).toStdString(), new_id = *new_id,
                        parent = ParentId(resolveFileId(parent_id))](Session& s) {
        auto item = s.CurrentFile(hex);
        if (!item || item->kind != cybou::FileItemKind::FILE) return;
        // A copy is a new catalog entry referencing the same protected content.
        item->item_id = new_id;
        item->parent_id = parent;
        item->modified_ms = static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch());
        item->name = tr("Copy of %1").arg(QString::fromStdString(item->name)).toStdString();
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, new_id, *item});
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The copy could not be created.")); });
        }
    });
}

void CybouCoreApplicationAdapter::setFileStarred(const QString& id, bool starred)
{
    const QString hex = resolveFileId(id);
    if (!m_session) return;
    // Shown at once; persisted as encrypted Identity state once the item is indexed.
    m_pending_stars.insert(hex, starred);
    emitFiles();
    postFileStar(hex, starred);
}

void CybouCoreApplicationAdapter::postFileStar(const QString& hex, bool starred)
{
    const auto item_id = FromHex(hex);
    if (!m_session || !item_id) return;
    m_session->Post([item_id = *item_id, starred](Session& s) { (void)s.application->SetFileStarred(item_id, starred); });
}

void CybouCoreApplicationAdapter::trashFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](Session& s) {
        auto item = s.CurrentFile(hex);
        if (!item || item->parent_id == cybou::FilesTrashParent()) return;
        // Contents follow their folder into Trash without separate changes.
        item->parent_id = cybou::FilesTrashParent();
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be moved to Trash.")); });
        }
    });
}

void CybouCoreApplicationAdapter::restoreFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](Session& s) {
        auto item = s.CurrentFile(hex);
        if (!item) return;
        // Trash does not remember the old location; restored items return to My files.
        item->parent_id = std::nullopt;
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be restored.")); });
        }
    });
}

void CybouCoreApplicationAdapter::deleteFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](Session& s) {
        const auto root = FromHex(QString::fromStdString(hex));
        if (!root || !s.CurrentFile(hex)) return;
        const auto catalog = s.Catalog();
        std::vector<cybou::PrivateItemId> doomed{*root};
        for (std::size_t i{0}; i < doomed.size(); ++i) {
            for (const auto& [child_hex, item] : catalog) {
                if (item.parent_id == doomed[i]) doomed.push_back(item.item_id);
            }
        }
        cybou::FilesMutationBatch batch;
        for (const auto& item_id : doomed) {
            batch.mutations.push_back({cybou::FileMutationKind::DELETE_ITEM, item_id, std::nullopt});
        }
        if (!s.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be deleted.")); });
        }
    });
}
