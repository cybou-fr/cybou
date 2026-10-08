// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_CYBOUDESKTOPCONTROLLER_H
#define CYBOU_QT_CYBOUDESKTOPCONTROLLER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <filesystem>
#include <memory>
#include <thread>
#include <mutex>

#include <QHash>
#include <QObject>
#include <QString>

class CybouCoreApplicationAdapter;
class CybouDesktopModel;

namespace cybou {
class CybouNodeService;
class CybouIdentityService;
class CybouWalletService;
namespace p2p { class GeoDatabaseUpdater; }
}

/** Owns the native CYBOU runtime and its services for one desktop session. */
class CybouDesktopController final : public QObject
{
    Q_OBJECT
public:
    explicit CybouDesktopController(CybouDesktopModel* model,
        std::filesystem::path data_directory, QObject* parent = nullptr);
    ~CybouDesktopController() override;

    void start();

Q_SIGNALS:
    void startupFailed(const QString& reason);

private:
    CybouDesktopModel* m_model;
    bool m_settlement_pending{false};
    quint64 m_settlement_generation{0};
    std::jthread m_settlement_worker;
    std::filesystem::path m_data_directory;
    std::unique_ptr<cybou::CybouNodeService> m_node_service;
    std::unique_ptr<cybou::CybouIdentityService> m_identity_service;
    std::unique_ptr<cybou::CybouWalletService> m_wallet_service;
    std::unique_ptr<CybouCoreApplicationAdapter> m_application;
    std::shared_ptr<cybou::p2p::GeoDatabaseUpdater> m_geo_database_updater;
    std::mutex m_identity_access_mutex;
    bool m_identity_signer_enabled{false};
    /** Identity state (and pause) last applied to the signers: status changes several times a
        second, and re-applying them takes the chain lock on the GUI thread (GUI thread only). */
    std::optional<int> m_identity_signer_state;
    std::optional<std::pair<int, bool>> m_poa_signer_state;
    /** Network-thread throttle of the wallet and authority refresh. */
    std::uint64_t m_refreshed_height{std::numeric_limits<std::uint64_t>::max()};
    std::chrono::steady_clock::time_point m_last_refresh{};
    /** Operator pause of the local block production loop (GUI thread). */
    std::atomic<bool> m_production_paused{false};
    QHash<QString, qint64> m_candidate_seen_times;
    /** Runs one operator command (finalize now) off the GUI thread. */
    std::jthread m_operator_worker;
    void finalizeNow();
    /** Signs and submits the StorageSettlement of the next due period (PoA only, DEC-282). */
    void settleStoragePeriod();
    /** Where data of another official network was moved at startup, if it was. */
    std::optional<std::filesystem::path> m_retired_network_data;
    void stop();
    void updatePoaSigner();
    void updateIdentitySigner();
    void publishAuthority();
    void lockIdentity();
    /** Canonical network totals, published only when the unlocked Identity is the genesis authority. */
    void publishNetworkAuthority();
};

#endif // CYBOU_QT_CYBOUDESKTOPCONTROLLER_H
