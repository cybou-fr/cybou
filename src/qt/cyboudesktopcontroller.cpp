// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopcontroller.h>

#include <qt/cyboudesktopmodel.h>

#include <cybou/bootstrap_nodes.h>
#include <cybou/identity_service.h>
#include <cybou/mail_service.h>
#include <cybou/network_definition.h>
#include <cybou/node_runtime.h>
#include <cybou/wallet_service.h>

#include <QDateTime>
#include <QMetaObject>

#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

#include <QDebug>

CybouDesktopController::CybouDesktopController(CybouDesktopModel* model,
    std::filesystem::path data_directory, QObject* parent)
    : QObject{parent}, m_model{model}, m_data_directory{std::move(data_directory)}
{
}

CybouDesktopController::~CybouDesktopController()
{
    stop();
}

void CybouDesktopController::start()
{
    if (m_node_runtime) return;
    m_sync_stop.store(false);
    try {
        const auto network_path = m_data_directory / "network.bin";
        const auto network_file = cybou::LoadCybouNetworkFile(network_path);
        if (!network_file) throw std::runtime_error("missing or invalid CYBOU network.bin");
        const auto& genesis = network_file->genesis;
        const auto& definition = network_file->definition;
        m_model->setNetworkInfo(
            QStringLiteral("CYBOU-DEV"),
            QString::fromStdString(cybou::NetworkId(definition).GetHex()));
        const std::filesystem::path data_dir = m_data_directory / "cybou_state";
        if (qEnvironmentVariableIsSet("CYBOU_DEV_VALIDATOR")) {
            throw std::runtime_error(
                "desktop validator mode is disabled; run cybou-node serve for DEV validation");
        }

        const auto& endpoint = cybou::CYBOU_DEV_BOOTSTRAP_AUTHORITIES.front();
        bool p2p_port_ok{false};
        const int p2p_port = qEnvironmentVariableIntValue("CYBOU_DEV_P2P_PORT", &p2p_port_ok);
        if (qEnvironmentVariableIsSet("CYBOU_DEV_P2P_PORT") &&
            (!p2p_port_ok || p2p_port <= 0 || p2p_port > 65535)) {
            throw std::runtime_error("invalid CYBOU_DEV_P2P_PORT");
        }
        const QString p2p_host = qEnvironmentVariable("CYBOU_DEV_P2P_HOST");
        const auto configured_p2p = p2p_port_ok ?
            std::optional<std::pair<std::string, uint16_t>>{std::make_pair(
                p2p_host.isEmpty() ? std::string{endpoint.host} : p2p_host.toStdString(),
                static_cast<uint16_t>(p2p_port))} : std::nullopt;
        cybou::NodeRuntimeConfig config{
            .network_definition = definition,
            .data_dir = data_dir,
            .validator_private_key = std::nullopt,
            .submit_endpoint = configured_p2p ? std::nullopt :
                std::optional<std::pair<std::string, uint16_t>>{std::make_pair(std::string{endpoint.host}, endpoint.port)},
            .p2p_endpoint = configured_p2p,
            .db_cache_bytes = 8 << 20,
        };
        m_node_runtime = std::make_unique<cybou::CybouNodeRuntime>(std::move(config));
        const auto init_status = m_node_runtime->GetStatus();
        if (init_status.runtime_state == cybou::NodeRuntimeState::NETWORK_MISMATCH) {
            throw std::runtime_error("CYBOU state belongs to another network; DEV reset requires an explicit cutover");
        }
        if (init_status.runtime_state == cybou::NodeRuntimeState::UNINITIALIZED) {
            if (!m_node_runtime->InitializeGenesis(genesis)) {
                throw std::runtime_error("cannot initialize CYBOU genesis");
            }
        } else if (init_status.runtime_state != cybou::NodeRuntimeState::READY) {
            throw std::runtime_error("CYBOU state is unavailable or corrupt");
        }
        m_model->setNodeStatus(true, 0, true, QString::fromStdString(m_data_directory.string()));

        const auto identity_path = m_data_directory / "identity.cybou";
        m_identity_service = std::make_unique<cybou::CybouIdentityService>(*m_node_runtime, identity_path);
        m_model->setIdentityService(m_identity_service.get());

        const auto mailbox_path = m_data_directory / "mailbox.dat";
        m_mail_service = std::make_unique<cybou::CybouMailService>(
            *m_node_runtime, m_identity_service->GetKeyStore(), mailbox_path);
        if (std::filesystem::exists(mailbox_path)) m_mail_service->LoadMailbox();
        m_model->setMailService(m_mail_service.get());

        m_wallet_service = std::make_unique<cybou::CybouWalletService>(
            *m_node_runtime, m_identity_service->GetKeyStore());
        m_model->setWalletService(m_wallet_service.get());

        const auto status = m_node_runtime->GetStatus();
        m_model->setFinalityStatus(static_cast<int>(status.finalized_height),
            static_cast<int>(status.validator_count));
        m_model->setPeerCount(0);

        m_sync_thread = std::thread{[this] {
            const auto& bootstrap = cybou::CYBOU_DEV_BOOTSTRAP_AUTHORITIES.front();
            while (!m_sync_stop.load()) {
                bool bootstrap_reachable = false;
                try {
                    const auto sync_result = m_node_runtime->HasP2pEndpoint() ?
                        m_node_runtime->SyncFromConfiguredPeer(1) :
                        m_node_runtime->SyncFromPeer(std::string{bootstrap.host}, bootstrap.port, 100);
                    bootstrap_reachable = sync_result.IsConnected();
                    if (m_node_runtime->HasP2pEndpoint() &&
                        (sync_result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR ||
                         sync_result.status == cybou::SyncPeerStatus::NETWORK_MISMATCH)) {
                        qWarning() << "cybou P2P peer rejected by protocol verification";
                        m_sync_stop.store(true);
                    }
                    if (m_mail_service) m_mail_service->SyncMailbox();
                    if (m_wallet_service) {
                        m_wallet_service->SyncLedger();
                        const auto [balance, system_balance] = m_wallet_service->GetBalances();
                        QMetaObject::invokeMethod(m_model, [model = m_model, balance, system_balance] {
                            model->setBalances(balance, system_balance);
                        }, Qt::QueuedConnection);
                    }
                } catch (const std::exception& e) {
                    bootstrap_reachable = false;
                    qWarning() << "cybou bootstrap sync error:" << e.what();
                }
                const auto runtime_status = m_node_runtime->GetStatus();
                QMetaObject::invokeMethod(m_model, [model = m_model, runtime_status, bootstrap_reachable] {
                    model->setFinalityStatus(static_cast<int>(runtime_status.finalized_height),
                        static_cast<int>(runtime_status.validator_count));
                    model->setNodeStatus(true, bootstrap_reachable ? 1 : 0, true);
                    if (bootstrap_reachable) model->setLastSync(QDateTime::currentDateTime());
                }, Qt::QueuedConnection);
                for (int i = 0; i < 15 && !m_sync_stop.load(); ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            }
        }};
    } catch (const std::exception& e) {
        const QString reason = QString::fromLocal8Bit(e.what());
        qWarning() << "CYBOU desktop startup error:" << reason;
        m_sync_stop.store(true);
        if (m_sync_thread.joinable()) m_sync_thread.join();
        m_model->setIdentityService(nullptr);
        m_model->setMailService(nullptr);
        m_model->setWalletService(nullptr);
        m_wallet_service.reset();
        m_mail_service.reset();
        m_identity_service.reset();
        m_node_runtime.reset();
        m_model->setNodeStatus(false, 0, false);
        Q_EMIT startupFailed(reason);
    }
}

void CybouDesktopController::stop()
{
    m_sync_stop.store(true);
    if (m_sync_thread.joinable()) m_sync_thread.join();
    if (m_model) {
        m_model->setIdentityService(nullptr);
        m_model->setMailService(nullptr);
        m_model->setWalletService(nullptr);
    }
}
