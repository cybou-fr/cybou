// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboucoreapplicationadapter_internal.h>


using namespace cybou::qt_detail;

CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::~SessionScheduler() { Stop(); }

void CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Start(IdentitySession& session)
{
    worker = std::jthread{[&session](std::stop_token stop) { session.Run(stop); }};
}

void CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Stop()
{
    worker.request_stop();
    wake.notify_all();
    if (worker.joinable()) worker.join();
}

void CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Post(Task task)
{
    {
        std::lock_guard lock{mutex};
        background_tasks.push_back(std::move(task));
    }
    wake.notify_all();
}

void CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::PostInteractive(Task task)
{
    {
        std::lock_guard lock{mutex};
        interactive_tasks.push_back(std::move(task));
    }
    wake.notify_all();
}

std::deque<CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Task>
CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Take(
    std::stop_token stop, int interval)
{
    std::unique_lock lock{mutex};

    wake.wait_for(lock, stop, std::chrono::milliseconds{interval}, [this] {
        return !interactive_tasks.empty() || !background_tasks.empty();
    });

    std::deque<Task> pending;

    // Все уже ожидающие интерактивные команды выполняются в порядке поступления.
    if (!interactive_tasks.empty()) {
        pending.swap(interactive_tasks);
        return pending;
    }

    // Берём только одну фоновую команду. После неё worker снова проверит
    // интерактивную очередь.
    if (!background_tasks.empty()) {
        pending.push_back(std::move(background_tasks.front()));
        background_tasks.pop_front();
    }

    return pending;
}

std::deque<CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Task>
CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Drain()
{
    std::lock_guard lock{mutex};
    std::deque<Task> pending;

    while (!interactive_tasks.empty()) {
        pending.push_back(std::move(interactive_tasks.front()));
        interactive_tasks.pop_front();
    }

    while (!background_tasks.empty()) {
        pending.push_back(std::move(background_tasks.front()));
        background_tasks.pop_front();
    }

    return pending;
}
