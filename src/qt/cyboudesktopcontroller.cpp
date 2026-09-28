// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopcontroller.h>

#include <qt/cyboudesktopmodel.h>

#include <cybou/bootstrap_nodes.h>
#include <cybou/identity_service.h>
#include <cybou/mail_service.h>
#include <cybou/network_definition.h>
#include <cybou/node_service.h>
#include <cybou/storage_service.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/wallet_service.h>
#include <support/cleanse.h>

#include <QDateTime>
#include <QMetaObject>

#include <boost/asio/ip/address.hpp>

#include <chrono>
#include <algorithm>
#include <array>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

#include <QDebug>

namespace {

QString StorageObjectHex(const cybou::StorageObjectId& object_id)
{
    static constexpr char HEX[] = "0123456789abcdef";
    QByteArray result;
    result.resize(static_cast<qsizetype>(object_id.size() * 2));
    for (size_t i = 0; i < object_id.size(); ++i) {
        result[static_cast<qsizetype>(i * 2)] = HEX[object_id[i] >> 4];
        result[static_cast<qsizetype>(i * 2 + 1)] = HEX[object_id[i] & 0x0f];
    }
    return QString::fromLatin1(result);
}

std::optional<cybou::StorageObjectId> ParseStorageObjectHex(const QString& value)
{
    if (value.size() != 64) return std::nullopt;
    const QByteArray bytes = value.toLatin1();
    cybou::StorageObjectId result{};
    for (size_t i = 0; i < result.size(); ++i) {
        const auto hex_value = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int h = hex_value(bytes[static_cast<qsizetype>(i * 2)]);
        const int l = hex_value(bytes[static_cast<qsizetype>(i * 2 + 1)]);
        if (h < 0 || l < 0) return std::nullopt;
        result[i] = static_cast<unsigned char>((h << 4) | l);
    }
    return result;
}

void ConnectStoragePeers(cybou::p2p::PeerManager& peers, cybou::CybouNodeRuntime& runtime)
{
    auto endpoints = runtime.GetPeerEndpointsForGossip();
    const auto& bootstrap = cybou::CYBOU_DEV_BOOTSTRAP_AUTHORITIES.front();
    endpoints.emplace_back(std::string{bootstrap.host}, bootstrap.p2p_port);
    for (const auto& [address, port] : endpoints) {
        if (peers.StoragePeers().size() >= cybou::StoragePlacement::MAX_REPLICAS) break;
        (void)peers.Connect(address, port);
    }
}

} // namespace

CybouDesktopController::CybouDesktopController(CybouDesktopModel* model,
    std::filesystem::path data_directory, QObject* parent)
    : QObject{parent}, m_model{model}, m_data_directory{std::move(data_directory)}
{
    connect(m_model, &CybouDesktopModel::storageListRequested, this,
        [this](const QString& password) { listStorageFiles(password); });
    connect(m_model, &CybouDesktopModel::storageUploadRequested, this,
        [this](const QString& source, const QString& password) { uploadStorageFile(source, password); });
    connect(m_model, &CybouDesktopModel::storageDownloadRequested, this,
        [this](const QString& object_id, const QString& destination, const QString& password) {
            downloadStorageFile(object_id, destination, password);
        });
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

        const auto mailbox_path = m_data_directory / "mailbox.dat";
        m_mail_service = std::make_unique<cybou::CybouMailService>(
            runtime, m_identity_service->GetKeyStore(), mailbox_path);
        m_model->setMailService(m_mail_service.get());

        m_wallet_service = std::make_unique<cybou::CybouWalletService>(
            runtime, m_identity_service->GetKeyStore());
        m_model->setWalletService(m_wallet_service.get());
        m_model->setFilesTransferAvailable(true);

        const auto status = runtime.GetStatus();
        m_model->setFinalityStatus(static_cast<int>(status.finalized_height),
            static_cast<int>(status.validator_count));
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
                    if (m_mail_service) m_mail_service->SyncMailbox();
                    if (m_wallet_service) {
                        m_wallet_service->SyncLedger();
                        const auto [balance, system_balance] = m_wallet_service->GetBalances();
                        QMetaObject::invokeMethod(m_model, [model = m_model, balance, system_balance] {
                            model->setBalances(balance, system_balance);
                        }, Qt::QueuedConnection);
                    }
                } catch (const std::exception& e) {
                    qWarning() << "cybou desktop service refresh error:" << e.what();
                }
                QMetaObject::invokeMethod(m_model, [model = m_model, runtime_status, bootstrap_reachable, connected_peer_count, sync_error] {
                    model->setSyncError(sync_error);
                    model->setFinalityStatus(static_cast<int>(runtime_status.finalized_height),
                        static_cast<int>(runtime_status.validator_count));
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
        m_model->setMailService(nullptr);
        m_model->setWalletService(nullptr);
        m_model->setFilesTransferAvailable(false);
        m_wallet_service.reset();
        m_mail_service.reset();
        m_identity_service.reset();
        m_node_service.reset();
        m_model->setNodeStatus(false, 0, false);
        m_model->setSyncError(reason);
        Q_EMIT startupFailed(reason);
    }
}

void CybouDesktopController::listStorageFiles(const QString& vault_password)
{
    if (!m_identity_service || !m_node_service || !m_identity_service->GetAccountId()) return;
    if (m_storage_worker.joinable()) m_storage_worker.join();
    m_storage_worker = std::jthread([this, password = vault_password.toStdString()]() mutable {
        try {
        auto& keystore = m_identity_service->GetKeyStore();
        const auto keyring = m_data_directory / "storage-keyring.cybv2";
        if (!keystore.GetCurrentStorageKeyEpoch() && !std::filesystem::exists(keyring)) {
            const bool local_files_exist = std::filesystem::exists(m_data_directory / "files");
            memory_cleanse(password.data(), password.size());
            QMetaObject::invokeMethod(m_model, [model = m_model, local_files_exist] {
                model->setStorageFiles({}, local_files_exist
                    ? QObject::tr("Files keys are unavailable for the local index.") : QString{});
            }, Qt::QueuedConnection);
            return;
        }
        if (!keystore.GetCurrentStorageKeyEpoch() && !keystore.LoadStorageKeyRing(keyring, password)) {
            QMetaObject::invokeMethod(m_model, [model = m_model] {
                model->setStorageFiles({}, QObject::tr("Files keys are unavailable. Unlock the identity vault first."));
            }, Qt::QueuedConnection);
            memory_cleanse(password.data(), password.size());
            return;
        }
        cybou::p2p::PeerManager peers{m_identity_service->GetNodeRuntime()};
        cybou::StoragePlacement placement{peers};
        const auto account_id = *m_identity_service->GetAccountId();
        const auto network_id = m_identity_service->GetNodeRuntime().GetNetworkId();
        std::array<unsigned char, 32> network_bytes{};
        std::copy(network_id.begin(), network_id.end(), network_bytes.begin());
        cybou::StorageService storage{network_bytes, account_id, keystore, placement,
            m_data_directory / "files"};
        auto listed = storage.ListFiles(password);
        QVector<CybouDesktopFile> files;
        QString error;
        if (!listed) {
            error = QObject::tr("Could not read the encrypted Files index. Check the vault password.");
        } else {
            files.reserve(static_cast<qsizetype>(listed->size()));
            for (const auto& file : *listed) {
                files.append({StorageObjectHex(file.object_id), QString::fromStdString(file.filename),
                    file.size, file.key_epoch, file.pending_verification});
            }
        }
        memory_cleanse(password.data(), password.size());
        QMetaObject::invokeMethod(m_model, [model = m_model, files = std::move(files), error] () mutable {
            model->setStorageFiles(std::move(files), error);
        }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            memory_cleanse(password.data(), password.size());
            const auto message = QString::fromLocal8Bit(e.what());
            QMetaObject::invokeMethod(m_model, [model = m_model, message] {
                model->setStorageFiles({}, message);
            }, Qt::QueuedConnection);
        }
    });
}

void CybouDesktopController::uploadStorageFile(const QString& source, const QString& vault_password)
{
    if (!m_identity_service || !m_node_service || !m_identity_service->GetAccountId()) return;
    if (m_storage_worker.joinable()) m_storage_worker.join();
    m_storage_worker = std::jthread([this, source, password = vault_password.toStdString()]() mutable {
        try {
        const auto finish = [this](const QString& message) {
            QMetaObject::invokeMethod(m_model, [model = m_model, message] {
                model->setStorageOperationStatus(message, false);
            }, Qt::QueuedConnection);
        };
        auto& keystore = m_identity_service->GetKeyStore();
        const auto keyring = m_data_directory / "storage-keyring.cybv2";
        if (!keystore.GetCurrentStorageKeyEpoch()) {
            const bool ready = std::filesystem::exists(keyring)
                ? keystore.LoadStorageKeyRing(keyring, password)
                : keystore.CreateStorageKeyRing(keyring, password);
            if (!ready) {
                finish(QObject::tr("Files keys could not be opened. Check the vault password."));
                memory_cleanse(password.data(), password.size());
                return;
            }
        }
        auto& runtime = m_identity_service->GetNodeRuntime();
        cybou::p2p::PeerManager peers{runtime};
        ConnectStoragePeers(peers, runtime);
        cybou::StoragePlacement placement{peers};
        const auto account_id = *m_identity_service->GetAccountId();
        std::array<unsigned char, 32> network_bytes{};
        std::copy(runtime.GetNetworkId().begin(), runtime.GetNetworkId().end(), network_bytes.begin());
        cybou::StorageService storage{network_bytes, account_id, keystore, placement,
            m_data_directory / "files"};
        QMetaObject::invokeMethod(m_model, [model = m_model] {
            model->setStorageOperationStatus(QObject::tr("Uploading encrypted file…"), true);
        }, Qt::QueuedConnection);
        const auto result = storage.UploadFile(source.toStdString(), password);
        const QString password_for_list = QString::fromStdString(password);
        memory_cleanse(password.data(), password.size());
        if (!result) {
            finish(result.status == cybou::StorageTransferStatus::PROVIDER_ERROR
                ? QObject::tr("No connected Storage peer accepted the upload. Try again when a Storage provider is online.")
                : QObject::tr("Upload did not complete. The file is not marked protected."));
            return;
        }
        finish(QObject::tr("Encrypted object stored; %1 of %2 peer acknowledgements. Files catalog finality is not available yet.")
            .arg(result.committed_replicas).arg(result.target_replicas));
        QMetaObject::invokeMethod(m_model, [model = m_model, password_for_list] {
            model->requestStorageList(password_for_list);
        }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            memory_cleanse(password.data(), password.size());
            const auto message = QString::fromLocal8Bit(e.what());
            QMetaObject::invokeMethod(m_model, [model = m_model, message] {
                model->setStorageOperationStatus(QObject::tr("Upload failed: %1").arg(message), false);
            }, Qt::QueuedConnection);
        }
    });
}

void CybouDesktopController::downloadStorageFile(const QString& object_id, const QString& destination,
    const QString& vault_password)
{
    if (!m_identity_service || !m_node_service || !m_identity_service->GetAccountId()) return;
    if (m_storage_worker.joinable()) m_storage_worker.join();
    m_storage_worker = std::jthread([this, object_id, destination, password = vault_password.toStdString()]() mutable {
        try {
        const auto finish = [this](const QString& message) {
            QMetaObject::invokeMethod(m_model, [model = m_model, message] {
                model->setStorageOperationStatus(message, false);
            }, Qt::QueuedConnection);
        };
        const auto parsed_id = ParseStorageObjectHex(object_id);
        if (!parsed_id) {
            finish(QObject::tr("This file reference is invalid."));
            memory_cleanse(password.data(), password.size());
            return;
        }
        auto& keystore = m_identity_service->GetKeyStore();
        const auto keyring = m_data_directory / "storage-keyring.cybv2";
        if (!keystore.GetCurrentStorageKeyEpoch() && !keystore.LoadStorageKeyRing(keyring, password)) {
            finish(QObject::tr("Files keys are unavailable. Check the vault password."));
            memory_cleanse(password.data(), password.size());
            return;
        }
        auto& runtime = m_identity_service->GetNodeRuntime();
        cybou::p2p::PeerManager peers{runtime};
        ConnectStoragePeers(peers, runtime);
        cybou::StoragePlacement placement{peers};
        std::array<unsigned char, 32> network_bytes{};
        std::copy(runtime.GetNetworkId().begin(), runtime.GetNetworkId().end(), network_bytes.begin());
        cybou::StorageService storage{network_bytes, *m_identity_service->GetAccountId(), keystore,
            placement, m_data_directory / "files"};
        QMetaObject::invokeMethod(m_model, [model = m_model] {
            model->setStorageOperationStatus(QObject::tr("Downloading and verifying…"), true);
        }, Qt::QueuedConnection);
        const auto result = storage.DownloadFile(*parsed_id, destination.toStdString(), password);
        memory_cleanse(password.data(), password.size());
        finish(result ? QObject::tr("Downloaded and integrity-checked.")
                      : QObject::tr("Download was not verified; no file was released."));
        } catch (const std::exception& e) {
            memory_cleanse(password.data(), password.size());
            const auto message = QString::fromLocal8Bit(e.what());
            QMetaObject::invokeMethod(m_model, [model = m_model, message] {
                model->setStorageOperationStatus(QObject::tr("Download failed: %1").arg(message), false);
            }, Qt::QueuedConnection);
        }
    });
}

void CybouDesktopController::stop()
{
    if (m_storage_worker.joinable()) m_storage_worker.join();
    if (m_node_service) m_node_service->StopNetwork();
    if (m_model) {
        m_model->setFilesTransferAvailable(false);
        m_model->setIdentityService(nullptr);
        m_model->setMailService(nullptr);
        m_model->setWalletService(nullptr);
    }
}
