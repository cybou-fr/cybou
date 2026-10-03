// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_GEO_DATABASE_UPDATER_H
#define CYBOU_P2P_GEO_DATABASE_UPDATER_H

#include <cybou/p2p/peer_admission.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#if defined(CYBOU_ENABLE_TEST_HOOKS)
#include <functional>
#include <map>
#endif
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <mutex>
#include <thread>

namespace cybou::p2p {

struct GeoDatabaseRelease {
    std::chrono::year_month month;
    std::string download_path;
    std::string sha1;
};

/** Keeps the local DB-IP Lite country dataset current for this CYBOU process. */
class GeoDatabaseUpdater final {
public:
    static std::shared_ptr<GeoDatabaseUpdater> Start(const std::filesystem::path& data_directory);
    static std::optional<GeoDatabaseRelease> ParseOfficialReleasePage(std::string_view page);
    ~GeoDatabaseUpdater();
#if defined(CYBOU_ENABLE_TEST_HOOKS)
    using FetchForTest = std::function<std::string(std::string_view, std::string_view, size_t,
        const std::map<std::string, std::string>&)>;
    using WaitForTest = std::function<bool(std::stop_token, std::chrono::milliseconds)>;
    /** Synchronous test instance: never starts a worker or accesses the network. */
    static std::shared_ptr<GeoDatabaseUpdater> CreateForTest(const std::filesystem::path& directory,
        FetchForTest fetch, WaitForTest wait = {});
    bool RefreshForTest(std::stop_token stop = {}) { return RefreshWithRetries(stop); }
    std::chrono::milliseconds NextDelayForTest(bool success) const { return NextRefreshDelay(success); }
#endif

    GeoDatabaseUpdater(const GeoDatabaseUpdater&) = delete;
    GeoDatabaseUpdater& operator=(const GeoDatabaseUpdater&) = delete;

    std::shared_ptr<const FrenchIpDataset> CurrentDataset() const;
    bool Ready() const { return static_cast<bool>(CurrentDataset()); }

private:
    struct Snapshot {
        std::shared_ptr<const FrenchIpDataset> dataset;
        std::chrono::year_month issued_month;
    };

    explicit GeoDatabaseUpdater(std::filesystem::path data_directory);
    void LoadCached();
    enum class RefreshResult { CURRENT, UPDATED };
    RefreshResult RefreshOnce();
    bool RefreshWithRetries(std::stop_token stop);
    bool Wait(std::stop_token stop, std::chrono::milliseconds delay);
    std::chrono::milliseconds NextRefreshDelay(bool success) const;
    std::string Fetch(std::string_view host, std::string_view path, size_t limit);
    void Run(std::stop_token stop);

    std::filesystem::path m_data_directory;
    std::atomic<std::shared_ptr<const Snapshot>> m_current;
    std::mutex m_wait_mutex;
    std::condition_variable_any m_wakeup;
    std::jthread m_worker;
#if defined(CYBOU_ENABLE_TEST_HOOKS)
    FetchForTest m_fetch_for_test;
    WaitForTest m_wait_for_test;
#endif
};

} // namespace cybou::p2p

#endif // CYBOU_P2P_GEO_DATABASE_UPDATER_H
