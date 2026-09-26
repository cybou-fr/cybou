// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPCONTROLLER_H
#define BITCOIN_QT_CYBOUDESKTOPCONTROLLER_H

#include <atomic>
#include <memory>
#include <thread>

#include <QObject>

class ClientModel;
class CybouDesktopModel;

namespace cybou {
class CybouNodeRuntime;
class CybouIdentityService;
class CybouMailService;
class CybouWalletService;
}

/** Owns the native CYBOU runtime and its services for one desktop session. */
class CybouDesktopController final : public QObject
{
public:
    explicit CybouDesktopController(CybouDesktopModel* model, QObject* parent = nullptr);
    ~CybouDesktopController() override;

    void setClientModel(ClientModel* client_model);

private:
    CybouDesktopModel* m_model;
    ClientModel* m_client_model{nullptr};
    std::unique_ptr<cybou::CybouNodeRuntime> m_node_runtime;
    std::unique_ptr<cybou::CybouIdentityService> m_identity_service;
    std::unique_ptr<cybou::CybouMailService> m_mail_service;
    std::unique_ptr<cybou::CybouWalletService> m_wallet_service;
    std::thread m_sync_thread;
    std::atomic_bool m_sync_stop{false};

    void start();
    void stop();
};

#endif // BITCOIN_QT_CYBOUDESKTOPCONTROLLER_H
