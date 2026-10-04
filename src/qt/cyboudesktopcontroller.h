// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_QT_CYBOUDESKTOPCONTROLLER_H
#define CYBOU_QT_CYBOUDESKTOPCONTROLLER_H

#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>
#include <mutex>

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
    std::filesystem::path m_data_directory;
    std::unique_ptr<cybou::CybouNodeService> m_node_service;
    std::unique_ptr<cybou::CybouIdentityService> m_identity_service;
    std::unique_ptr<cybou::CybouWalletService> m_wallet_service;
    std::unique_ptr<CybouCoreApplicationAdapter> m_application;
    std::shared_ptr<cybou::p2p::GeoDatabaseUpdater> m_geo_database_updater;
    std::mutex m_identity_access_mutex;
    bool m_identity_signer_enabled{false};
    /** Operator pause of the local block production loop (GUI thread). */
    std::atomic<bool> m_production_paused{false};
    /** Runs one operator command (finalize now) off the GUI thread. */
    std::jthread m_operator_worker;
    void finalizeNow();
    /** Where data of an older DEV network was moved at startup, if it was. */
    void stop();
    void updatePoaSigner();
    void updateIdentitySigner();
    void publishAuthority();
    void lockIdentity();
    /** Canonical network totals, published only when the unlocked Identity is the genesis authority. */
    void publishNetworkAuthority();
};

#endif // CYBOU_QT_CYBOUDESKTOPCONTROLLER_H
