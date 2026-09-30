// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPCONTROLLER_H
#define BITCOIN_QT_CYBOUDESKTOPCONTROLLER_H

#include <filesystem>
#include <memory>

#include <QObject>
#include <QString>

class CybouCoreApplicationAdapter;
class CybouDesktopModel;

namespace cybou {
class CybouNodeService;
class CybouIdentityService;
class CybouWalletService;
class AuthorityIndex;
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
    /** Derived Identity Authority preview; used only on the network refresh thread. */
    std::unique_ptr<cybou::AuthorityIndex> m_authority_index;
    /** Where data of an older DEV network was moved at startup, if it was. */
    QString m_archived_network;
    void stop();
    void publishAuthority();
};

#endif // BITCOIN_QT_CYBOUDESKTOPCONTROLLER_H
