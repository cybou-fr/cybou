// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopcontroller.h>

#include <qt/cyboudesktopmodel.h>

#include <cybou/bootstrap_nodes.h>
#include <cybou/identity_service.h>
#include <cybou/network_definition.h>
#include <cybou/node_service.h>
#include <cybou/wallet_service.h>

#include <QDateTime>
#include <QMetaObject>

#include <boost/asio/ip/address.hpp>

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
    if (m_node_service) return;
    try {
        const auto network_path = m_data_directory / "network.bin";
        const auto network_file = cybou::LoadCybouNetworkFile(network_path);
        if (!network_file) throw std::runtime_error("missing or invalid CYBOU network.bin");
        const auto& genesis = network_file->genesis;
        const auto& definition = network_file->definition;
        m_model->setNetworkInfo(
            QStringLiteral("CYBOU DEV"),
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
        const auto selected_p2p_port = p2p_port_ok ? static_cast<uint16_t>(p2p_port) : endpoint.p2p_port;
        const auto selected_p2p_host = p2p_host.isEmpty() ? std::string{endpoint.host} : p2p_host.toStdString();
        const bool use_legacy_block_feed = qEnvironmentVariable("CYBOU_DEV_LEGACY_BLOCK_FEED") == "1";
        const auto configured_p2p = use_legacy_block_feed ? std::nullopt :
            std::optional<std::pair<std::string, uint16_t>>{
                std::make_pair(selected_p2p_host, selected_p2p_port)};
        cybou::CybouNetworkServiceConfig network_config;
        bool listen_port_ok{false};
        const int listen_port = qEnvironmentVariableIntValue("CYBOU_DEV_P2P_LISTEN_PORT", &listen_port_ok);
        if (qEnvironmentVariableIsSet("CYBOU_DEV_P2P_LISTEN_PORT") &&
            (!listen_port_ok || listen_port <= 0 || listen_port > 65535)) {
            throw std::runtime_error("invalid CYBOU_DEV_P2P_LISTEN_PORT");
        }
        if (listen_port_ok) {
            const auto listen_host = qEnvironmentVariable("CYBOU_DEV_P2P_LISTEN_HOST", "127.0.0.1");
            const auto bind_address = boost::asio::ip::make_address(listen_host.toStdString());
            const auto listener = std::make_pair(bind_address.to_string(), static_cast<uint16_t>(listen_port));
            network_config.listen_endpoint = listener;
        }
        cybou::NodeRuntimeConfig config{
            .network_definition = definition,
            .data_dir = data_dir,
            .validator_private_key = std::nullopt,
            .submit_endpoint = !use_legacy_block_feed ? std::nullopt :
                std::optional<std::pair<std::string, uint16_t>>{std::make_pair(std::string{endpoint.host}, endpoint.port)},
            .p2p_endpoint = configured_p2p,
            .local_p2p_endpoint = network_config.listen_endpoint,
            .db_cache_bytes = 8 << 20,
        };
        m_node_service = std::make_unique<cybou::CybouNodeService>(cybou::CybouNodeServiceConfig{
            .runtime = std::move(config),
            .genesis = genesis,
        });
        m_node_service->Start();
        auto& runtime = m_node_service->Runtime();
        // Starting the local node is not proof of network connectivity.
        m_model->setNodeStatus(true, 0, false, QString::fromStdString(m_data_directory.string()));

        const auto identity_path = m_data_directory / "identity.cybou";
        m_identity_service = std::make_unique<cybou::CybouIdentityService>(runtime, identity_path);
        m_model->setIdentityService(m_identity_service.get());

        // Mail and Files wait for the RootPublication client; the old
        // legacy mail and object-storage services are intentionally not wired.
        m_wallet_service = std::make_unique<cybou::CybouWalletService>(
            runtime, m_identity_service->GetKeyStore());
        m_model->setWalletService(m_wallet_service.get());

        const auto status = runtime.GetStatus();
        m_model->setFinalizedHeight(status.finalized_height);
        m_model->setPeerCount(0);

        m_node_service->StartNetwork(
            {std::string{endpoint.host}, endpoint.port},
            network_config,
            [this](const cybou::SyncPeerResult& sync_result, const cybou::NodeRuntimeStatus& runtime_status,
                const size_t connected_peer_count) {
                const bool bootstrap_reachable = sync_result.IsConnected();
                const bool local_state_unavailable =
                    runtime_status.runtime_state == cybou::NodeRuntimeState::NETWORK_MISMATCH ||
                    runtime_status.runtime_state == cybou::NodeRuntimeState::CORRUPT;
                if (local_state_unavailable) {
                    const auto message = runtime_status.runtime_state == cybou::NodeRuntimeState::NETWORK_MISMATCH
                        ? QStringLiteral("Local CYBOU state belongs to another network.")
                        : QStringLiteral("Local CYBOU state is corrupt or unavailable.");
                    qWarning() << message;
                    QMetaObject::invokeMethod(m_model, [model = m_model, message] {
                        model->setNodeStatus(true, 0, false);
                        model->setSyncError(message);
                    }, Qt::QueuedConnection);
                    return false;
                }
                QString sync_error;
                if (sync_result.status == cybou::SyncPeerStatus::PROTOCOL_ERROR ||
                    sync_result.status == cybou::SyncPeerStatus::NETWORK_MISMATCH) {
                    sync_error = sync_result.status == cybou::SyncPeerStatus::NETWORK_MISMATCH
                        ? QStringLiteral("A peer belongs to another CYBOU network; retrying other peers.")
                        : QStringLiteral("A peer failed CYBOU protocol verification; retrying other peers.");
                    qWarning() << sync_error;
                }
                try {
                    if (m_wallet_service) {
                        m_wallet_service->SyncLedger();
                        const auto [balance, system_balance] = m_wallet_service->GetBalances();
                        QVector<CybouWalletEntry> entries;
                        for (const auto& entry : m_wallet_service->GetLedgerEntries()) {
                            CybouWalletEntry item;
                            item.id = QString::fromStdString(entry.entry_id.GetHex());
                            item.amount = entry.amount;
                            item.system_side = entry.system_side;
                            item.time = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(entry.timestamp));
                            item.pending = entry.finality == cybou::WalletEntryFinality::PENDING;
                            if (!entry.counterparty.IsNull()) {
                                item.counterparty_name = QString::fromStdString(entry.counterparty.Value().GetHex());
                            }
                            switch (entry.kind) {
                            case cybou::WalletEntryKind::PAYMENT:
                                item.kind = entry.amount >= 0 ? CybouWalletEntryKind::Received : CybouWalletEntryKind::Sent;
                                break;
                            case cybou::WalletEntryKind::ONBOARDING_BONUS:
                                item.kind = CybouWalletEntryKind::OnboardingCredit;
                                break;
                            case cybou::WalletEntryKind::LOCK_TO_SYSTEM:
                                item.kind = CybouWalletEntryKind::MovedToSystemBalance;
                                break;
                            case cybou::WalletEntryKind::MAIL_FEE:
                            case cybou::WalletEntryKind::ROOT_PUBLICATION_FEE:
                                item.kind = CybouWalletEntryKind::NetworkServiceFee;
                                break;
                            }
                            entries.append(item);
                        }
                        QMetaObject::invokeMethod(m_model, [model = m_model, balance, system_balance, entries = std::move(entries)]() mutable {
                            model->setBalances(balance, system_balance);
                            model->setWalletEntries(std::move(entries));
                        }, Qt::QueuedConnection);
                    }
                } catch (const std::exception& e) {
                    qWarning() << "cybou desktop service refresh error:" << e.what();
                }
                QMetaObject::invokeMethod(m_model, [model = m_model, runtime_status, bootstrap_reachable, connected_peer_count, sync_error] {
                    model->setSyncError(sync_error);
                    model->setFinalizedHeight(runtime_status.finalized_height);
                    model->setNodeStatus(true, static_cast<int>(connected_peer_count), bootstrap_reachable);
                    if (bootstrap_reachable) model->setLastSync(QDateTime::currentDateTime());
                }, Qt::QueuedConnection);
                return true;
            });
    } catch (const std::exception& e) {
        const QString reason = QString::fromLocal8Bit(e.what());
        qWarning() << "CYBOU desktop startup error:" << reason;
        if (m_node_service) m_node_service->StopNetwork();
        m_model->setIdentityService(nullptr);
        m_model->setWalletService(nullptr);
        m_wallet_service.reset();
        m_identity_service.reset();
        m_node_service.reset();
        m_model->setNodeStatus(false, 0, false);
        m_model->setSyncError(reason);
        Q_EMIT startupFailed(reason);
    }
}

void CybouDesktopController::stop()
{
    if (m_node_service) m_node_service->StopNetwork();
    if (m_model) {
        m_model->setIdentityService(nullptr);
        m_model->setWalletService(nullptr);
    }
}
