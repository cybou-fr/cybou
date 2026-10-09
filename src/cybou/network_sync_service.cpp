// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/network_sync_service.h>
#include <cybou/local_application_service.h>
#include <chrono>
namespace cybou {
void NetworkSyncService::ProcessOutbox(LocalApplicationService& local, PublicationService& publication,
    PrivateApplicationStore& network_db)
{
    // Local mutex/store are released before any signing, relay or storage call.
    const auto outbox = local.Outbox();
    for (const auto& pending : outbox) {
        if (pending.status.phase == PublicationJobPhase::PROTECTED) continue;
        auto status = publication.GetJob(pending.job_id);
        if (!status) {
            // A saved OperationID must never be silently replaced after loss of
            // its network journal. Preserve local work and ask for recovery.
            if (!pending.status.operation_id.IsNull()) {
                auto failed = pending.status;
                failed.phase = PublicationJobPhase::NEEDS_ATTENTION;
                failed.error = "Saved publication job is missing; exact-operation recovery required";
                if (!local.SetPublicationStatus(pending.job_id, failed)) throw std::runtime_error{"cannot save Outbox recovery state"};
                break;
            }
            std::vector<unsigned char> leaves;
            for (const auto& leaf : pending.content.leaves) leaves.insert(leaves.end(), leaf.begin(), leaf.end());
            if (!network_db.Put("publication/leaves/" + pending.job_id, leaves)) throw std::runtime_error{"cannot save publication leaves"};
            status = publication.SubmitPrepared(pending.job_id, pending.content.bundle, pending.recipient);
        } else status = publication.Resume(pending.job_id);
        if (!local.SetPublicationStatus(pending.job_id, *status)) throw std::runtime_error{"cannot save Outbox status"};
        // Preserve submission order. Finalized jobs may still secure copies,
        // while the next intent can be sent on a later pass.
        if (status->phase != PublicationJobPhase::SECURING && status->phase != PublicationJobPhase::PROTECTED) break;
    }
}
NetworkSyncService::NetworkSyncService(std::function<void()> refresh, int interval)
{
    m_worker = std::jthread{[this, refresh = std::move(refresh), interval](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::function<void()> task;
            {
                std::unique_lock lock{m_mutex};
                m_wake.wait_for(lock, stop, std::chrono::milliseconds{interval}, [this] { return !m_tasks.empty(); });
                if (!m_tasks.empty()) { task = std::move(m_tasks.front()); m_tasks.pop_front(); }
            }
            if (task) task();
            else if (!stop.stop_requested()) refresh();
        }
        // These are network requests, not acknowledged local commits. Never
        // initiate new network I/O during shutdown; durable intents resume later.
    }};
}
NetworkSyncService::~NetworkSyncService() { Stop(); }
void NetworkSyncService::Stop()
{
    m_worker.request_stop();
    m_wake.notify_all();
    if (m_worker.joinable()) m_worker.join();
}
void NetworkSyncService::Post(std::function<void()> task)
{
    std::lock_guard lock{m_mutex};
    m_tasks.push_back(std::move(task));
    m_wake.notify_all();
}
}
