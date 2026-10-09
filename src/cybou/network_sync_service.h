// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_NETWORK_SYNC_SERVICE_H
#define CYBOU_NETWORK_SYNC_SERVICE_H
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
namespace cybou {
class LocalApplicationService;
class PublicationService;
class PrivateApplicationStore;
/// Independent network execution. No local command waits for this executor.
class NetworkSyncService final {
public:
    NetworkSyncService(std::function<void()> refresh, int interval);
    ~NetworkSyncService();
    void Post(std::function<void()> task);
    void Stop();
    static void ProcessOutbox(LocalApplicationService& local, PublicationService& publication, PrivateApplicationStore& network_db);
private:
    std::mutex m_mutex;
    std::condition_variable_any m_wake;
    std::deque<std::function<void()>> m_tasks;
    std::jthread m_worker;
};
}
#endif
