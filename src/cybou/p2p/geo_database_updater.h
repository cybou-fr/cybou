// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_GEO_DATABASE_UPDATER_H
#define CYBOU_P2P_GEO_DATABASE_UPDATER_H

#include <cybou/p2p/peer_admission.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
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
    void Refresh();
    void Run(std::stop_token stop);

    std::filesystem::path m_data_directory;
    std::atomic<std::shared_ptr<const Snapshot>> m_current;
    std::mutex m_wait_mutex;
    std::condition_variable_any m_wakeup;
    std::jthread m_worker;
};

} // namespace cybou::p2p

#endif // CYBOU_P2P_GEO_DATABASE_UPDATER_H
