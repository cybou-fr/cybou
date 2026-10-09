// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_COREAPPLICATIONADAPTER_INTERNAL_H
#define CYBOU_QT_COREAPPLICATIONADAPTER_INTERNAL_H

#include <qt/cyboucoreapplicationadapter.h>

#include <cybou/application_service.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/node_runtime.h>
#include <cybou/private_application_store.h>
#include <cybou/publication_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/storage_service.h>

#include <QMetaObject>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <stop_token>
#include <thread>

#include <chrono>

namespace cybou::qt_detail {

constexpr char HEX[]{"0123456789abcdef"};

inline std::string ToHex(const cybou::PrivateItemId& id)
{
    std::string out;
    for (const unsigned char byte : id) {
        out.push_back(HEX[byte >> 4]);
        out.push_back(HEX[byte & 0x0f]);
    }
    return out;
}

inline std::optional<cybou::PrivateItemId> FromHex(const QString& text)
{
    const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
    if (text.size() != 64 || bytes.size() != 32) return std::nullopt;
    cybou::PrivateItemId id{};
    std::copy(bytes.begin(), bytes.end(), id.begin());
    return id;
}

inline std::string RandomJobId(const char* prefix)
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

inline QString ChunkHex(const cybou::ChunkId& id)
{
    return QString::fromLatin1(QByteArray{reinterpret_cast<const char*>(id.data()), static_cast<int>(id.size())}.toHex());
}

inline CybouContentState StateOf(const cybou::PublicationJobResult& job)
{
    switch (job.phase) {
    case cybou::PublicationJobPhase::QUEUED:
    case cybou::PublicationJobPhase::WAITING_FINALITY: return CybouContentState::Local; // see OperationOf
    case cybou::PublicationJobPhase::SECURING: return CybouContentState::Securing;
    case cybou::PublicationJobPhase::PROTECTED: return CybouContentState::Protected;
    case cybou::PublicationJobPhase::NEEDS_ATTENTION: return CybouContentState::NeedsAttention;
    }
    return CybouContentState::NeedsAttention;
}

/** Operation axis of a publication job; content durability is StateOf. */
inline CybouOperationState OperationOf(const cybou::PublicationJobResult& job)
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

inline CybouMailFolder FolderOf(cybou::MailFolder folder)
{
    switch (folder) {
    case cybou::MailFolder::INBOX: return CybouMailFolder::Inbox;
    case cybou::MailFolder::SENT: return CybouMailFolder::Sent;
    case cybou::MailFolder::ARCHIVE: return CybouMailFolder::Archive;
    case cybou::MailFolder::TRASH: return CybouMailFolder::Trash;
    case cybou::MailFolder::DELETED: return CybouMailFolder::Trash; // never listed
    }
    return CybouMailFolder::Inbox;
}

inline std::optional<cybou::MailFolder> CoreFolder(CybouMailFolder folder)
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
inline QString DisplayName(const cybou::CybouState* state, const cybou::AccountId& account)
{
    if (state) {
        if (const auto* label = state->names.PrimaryName(account)) {
            return QString::fromStdString(*label) + QStringLiteral(".cybou");
        }
    }
    return CybouProduct::shortId(QString::fromStdString(account.Value().GetHex()));
}

} // namespace


/** Private Qt implementation. Components below are confined to one Identity worker. */
struct CybouCoreApplicationAdapter::IdentitySession {
    /** Worker-side Mail semantics; GUI draft reconciliation stays on the adapter thread. */
    struct MailProjection {
        IdentitySession& session;
        /** Sent locally but not yet indexed from finalized history. */
        std::map<std::string, CybouMailItem> outbox;
        QVector<CybouMailItem> Snapshot();
        std::optional<cybou::AccountId> ResolveRecipient(const QString& name) const;
    };
    /** Worker-side intended catalog and local availability, independent of GUI client IDs. */
    struct FilesProjection {
        IdentitySession& session;
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

        /** Decrypt ROOT/INDEX locally and check every DATA blob exists, without reading DATA. */
        bool AvailableOffline(const cybou::FileItem& item);
        std::optional<cybou::FileItem> CurrentFile(const std::string& hex);
        std::optional<std::pair<cybou::ChunkId, std::pair<cybou::ContentKey, std::uint64_t>>> ContentOfAttachmentSource(
            const QString& attachment_id);
        std::map<std::string, cybou::FileItem> Catalog();
        bool PublishFileChange(cybou::FilesMutationBatch batch, std::optional<std::pair<std::size_t, cybou::NewContent>> content);
        QVector<CybouFileItem> FilesSnapshot();
        void ReportChange(CommandProgress progress, bool saved, const QString& error);
    };
    /** Shared publication durability, maintenance and settlement preparation. */
    struct StorageProjection {
        IdentitySession& session;
        std::map<std::string, cybou::PublicationJobResult> jobs;
        std::uint64_t ticks{0};
        static constexpr std::uint64_t AUDIT_EVERY_TICKS{5};
        static constexpr std::size_t AUDIT_CHUNKS_PER_PASS{8};
        /** Local encrypted cache beyond pins and provider obligations; LRU-evicted. */
        static constexpr std::uint64_t GC_EVERY_TICKS{60};
        static constexpr std::uint64_t LOCAL_CACHE_BUDGET_BYTES{2ULL << 30};
        /** Deleted Mail/Files content is revoked from the network a few ticks apart. */
        static constexpr std::uint64_t REVOKE_EVERY_TICKS{10};
        void Refresh(bool index_complete);
        QString DownloadContent(const cybou::ChunkId& root, const cybou::ContentKey& key, std::uint64_t size,
            const QString& destination);
        /** Keep catalog tombstones for replay and all live Mail/Files content references. */
        void RevokeUnreferenced();
        std::vector<cybou::StorageSettlementEntry> Settlement(std::uint64_t period, std::int64_t verified_since_ms);
    };
    /** Owns the sole worker; stopping joins before any session service is destroyed. */
    struct SessionScheduler {
        using Task = std::function<void(IdentitySession&)>;

        std::mutex mutex;
        std::condition_variable_any wake;
        std::deque<Task> interactive_tasks;
        std::deque<Task> background_tasks;
        std::jthread worker;

        ~SessionScheduler();
        void Start(IdentitySession& session);
        void Stop();

        // Обычные фоновые или потенциально долгие команды.
        void Post(Task task);

        // Короткие локальные действия пользователя.
        void PostInteractive(Task task);

        std::deque<Task> Take(std::stop_token stop, int interval);
        std::deque<Task> Drain();
    };
    CybouCoreApplicationAdapter* owner;
    /** Snapshots of a replaced session (lock, rotation reopen) never reach the GUI. */
    std::uint64_t generation;
    cybou::CybouNodeRuntime& runtime;
    cybou::CybouKeyStore& keystore;
    std::filesystem::path root;
    int refresh_ms;
    /** The private index is still behind finalized history. */
    bool catching_up{false};

    std::unique_ptr<cybou::PrivateApplicationStore> db;
    std::unique_ptr<cybou::RuntimeStorageTransport> transport;
    cybou::StorageTransport* transport_override{nullptr};
    std::unique_ptr<cybou::StorageService> storage;
    std::unique_ptr<cybou::PublicationService> publication;
    std::unique_ptr<cybou::ApplicationService> application;

    /** RecoveryBridge being secured before IdentityRotate. */
    struct RotationPrep {
        std::string job_id;
        cybou::RecoveryEntropy entropy{};
        ~RotationPrep() { cybou::crypto::CleanseMemory(entropy.data(), entropy.size()); }
    };
    std::unique_ptr<RotationPrep> rotation;

    MailProjection mail{*this};
    FilesProjection files{*this};
    StorageProjection storage_projection{*this};
    SessionScheduler scheduler;
    IdentitySession(CybouCoreApplicationAdapter*, cybou::CybouNodeRuntime&, cybou::CybouKeyStore&,
        std::filesystem::path, int, cybou::StorageTransport*);
    ~IdentitySession();
    void Post(std::function<void(IdentitySession&)> task) { scheduler.Post(std::move(task)); }
    void PostInteractive(std::function<void(IdentitySession&)> task)
    {
        scheduler.PostInteractive(std::move(task));
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

    bool Open();
    void Run(std::stop_token stop);
    void Refresh();
    void AdvanceRotation();
    void Snapshot(CybouRestoreStepState restore, bool initial_complete = false);
};

#endif
