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

void CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Post(std::function<void(IdentitySession&)> task)
{
    {
        std::lock_guard lock{mutex};
        tasks.push_back(std::move(task));
    }
    wake.notify_all();
}

std::deque<std::function<void(CybouCoreApplicationAdapter::IdentitySession&)>>
CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Take(std::stop_token stop, int interval)
{
    std::unique_lock lock{mutex};
    wake.wait_for(lock, stop, std::chrono::milliseconds{interval}, [this] { return !tasks.empty(); });
    std::deque<std::function<void(IdentitySession&)>> pending;
    pending.swap(tasks);
    return pending;
}

std::deque<std::function<void(CybouCoreApplicationAdapter::IdentitySession&)>>
CybouCoreApplicationAdapter::IdentitySession::SessionScheduler::Drain()
{
    std::lock_guard lock{mutex};
    std::deque<std::function<void(IdentitySession&)>> pending;
    pending.swap(tasks);
    return pending;
}
