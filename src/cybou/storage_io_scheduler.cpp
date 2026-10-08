// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_io_scheduler.h>
#include <algorithm>
#include <stdexcept>

namespace cybou {
StorageIoScheduler::StorageIoScheduler()
{
    try {
        for (size_t i = 0; i < 4; ++i) m_workers.emplace_back([this] { Work(); });
    } catch (...) {
        { std::lock_guard lock{m_mutex}; m_stopping = true; }
        m_cv.notify_all();
        for (auto& worker : m_workers) worker.join();
        throw;
    }
}
StorageIoScheduler::~StorageIoScheduler()
{
    { std::lock_guard lock{m_mutex}; m_stopping = true; }
    m_cv.notify_all();
    for (auto& worker : m_workers) worker.join();
}
bool StorageIoScheduler::IsIdle()
{
    std::unique_lock lock{m_mutex, std::try_to_lock};
    return lock.owns_lock() && !m_stopping && m_queue.empty() && m_active.empty();
}
void StorageIoScheduler::Enqueue(Job job)
{
    std::unique_lock lock{m_mutex};
    m_cv.wait(lock, [this] { return m_stopping || m_queue.size() < 8; });
    if (m_stopping) throw std::runtime_error("Storage I/O scheduler is stopping");
    m_queue.push_back(std::move(job));
    m_cv.notify_all();
}
void StorageIoScheduler::Work()
{
    std::unique_lock lock{m_mutex};
    for (;;) {
        const auto eligible = [this](const Job& job) {
            return !m_active.contains(job.provider) && (job.kind != Kind::READ || m_reads < 2);
        };
        m_cv.wait(lock, [&] {
            return (m_stopping && m_queue.empty()) ||
                std::any_of(m_queue.begin(), m_queue.end(), eligible);
        });
        if (m_stopping && m_queue.empty()) return;
        const auto it = std::find_if(m_queue.begin(), m_queue.end(), eligible);
        Job job = std::move(*it);
        m_queue.erase(it);
        m_active.insert(job.provider);
        if (job.kind == Kind::READ) ++m_reads;
        m_cv.notify_all();
        lock.unlock();
        // packaged_task captures exceptions in its future; budgets always release.
        job.run();
        lock.lock();
        m_active.erase(job.provider);
        if (job.kind == Kind::READ) --m_reads;
        m_cv.notify_all();
    }
}
} // namespace cybou
