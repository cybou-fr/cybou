// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_LOCAL_APPLICATION_SERVICE_H
#define CYBOU_LOCAL_APPLICATION_SERVICE_H
#include <cybou/application_service.h>
#include <cybou/local_content_stager.h>
#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>
#include <atomic>
namespace cybou {
struct PendingPublication {
    std::string job_id;
    std::uint64_t sequence{0};
    LocalPreparedContent content;
    PrivateApplicationDocument document;
    std::optional<AccountId> recipient;
    PublicationJobResult status{.phase = PublicationJobPhase::QUEUED};
};
/// Disk-only semantic state. No runtime, transport, publication or consensus dependency.
class LocalApplicationService final {
public:
    explicit LocalApplicationService(PrivateApplicationStore& db);
    ~LocalApplicationService();
    void Post(std::function<void()> task);
    void Stop();
    std::uint64_t Revision() const { return m_revision.load(); }
    std::vector<MailRecord> ListMail();
    std::optional<MailRecord> GetMail(const PrivateItemId& id);
    bool SetMailRead(const PrivateItemId& id, bool read);
    bool SetMailStarred(const PrivateItemId& id, bool starred);
    bool SetFileStarred(const PrivateItemId& id, bool starred);
    bool MoveMail(const PrivateItemId& id, MailFolder folder);
    std::vector<FileRecord> ListFiles();
    std::optional<FileRecord> GetFile(const PrivateItemId& id);
    bool SaveDraft(const MailDraft& draft);
    std::vector<MailDraft> ListDrafts();
    bool DeleteDraft(std::string_view id, bool keep_send_binding = false);
    std::optional<PrivateItemId> BindDraftToMessage(std::string_view id, const PrivateItemId& proposed);
    bool CheckDraftSendPayload(const MailDraft& draft, bool replace);
    bool ImportMail(const MailRecord& record);
    bool ImportFile(const FileRecord& record);
    bool MigrateFrom(PrivateApplicationStore& source);
    bool QueuePublication(std::string_view job, const LocalPreparedContent& content,
        const PrivateApplicationDocument& document, std::optional<AccountId> recipient = std::nullopt,
        std::string_view draft_id = {});
    std::vector<PendingPublication> Outbox();
    bool SetPublicationStatus(std::string_view job, const PublicationJobResult& status);
private:
    std::optional<MailRecord> LoadMail(const PrivateItemId& id) const;
    bool SaveMail(const MailRecord& record);
    std::optional<FileRecord> LoadFile(const PrivateItemId& id, bool desired = true) const;
    bool SaveFile(const FileRecord& record, bool desired = false);
    PrivateApplicationStore& m_application_db;
    std::recursive_mutex m_mutex;
    std::mutex m_queue_mutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_tasks;
    std::jthread m_worker;
    std::atomic<std::uint64_t> m_revision{0};
};
}
#endif
