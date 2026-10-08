// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_IO_SCHEDULER_H
#define CYBOU_STORAGE_IO_SCHEDULER_H

#include <array>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace cybou {
// Node-local policy. No wire capability, role or consensus state.
// Four persistent workers, eight queued jobs, one job per proven provider,
// and at most two read/verification jobs (including possible full GET fallback).
class StorageIoScheduler {
public:
    enum class Kind { READ, WRITE };
    StorageIoScheduler();
    ~StorageIoScheduler();
    // Conservative nonblocking observation admission hint; not a reservation.
    bool IsIdle();
    template <typename F>
    auto Submit(const std::array<unsigned char, 32>& provider, Kind kind, F&& function)
    {
        using Result = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<F>(function));
        auto result = task->get_future();
        Enqueue({provider, kind, [task] { (*task)(); }});
        return result;
    }
private:
    struct Job {
        std::array<unsigned char, 32> provider;
        Kind kind;
        std::function<void()> run;
    };
    void Enqueue(Job job);
    void Work();
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Job> m_queue;
    std::set<std::array<unsigned char, 32>> m_active;
    size_t m_reads{0};
    bool m_stopping{false};
    std::vector<std::thread> m_workers;
};
} // namespace cybou
#endif
