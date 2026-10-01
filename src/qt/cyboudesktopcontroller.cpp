// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopcontroller.h>

#include <qt/cyboucoreapplicationadapter.h>
#include <qt/cyboudesktopmodel.h>

#include <cybou/authority.h>
#include <cybou/bootstrap_nodes.h>
#include <cybou/identity_service.h>
#include <cybou/network_definition.h>
#include <cybou/node_service.h>
#include <cybou/p2p/geo_database_updater.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/wallet_service.h>

#include <QFile>
#include <QMetaObject>

#include <boost/asio/ip/address.hpp>

#include <chrono>
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <QDebug>

namespace {
std::array<unsigned char, 32> GeoSha256Pin(const QString& text)
{
    const auto bytes = text.toLatin1();
    if (bytes.size() != 64) throw std::runtime_error("CYBOU_GEO_SHA256 must contain 64 hex characters");
    std::array<unsigned char, 32> result{};
    const auto nibble = [](const char c) -> unsigned {
        if (c >= '0' && c <= '9') return static_cast<unsigned>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<unsigned>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<unsigned>(c - 'A' + 10);
        throw std::runtime_error("CYBOU_GEO_SHA256 is not hexadecimal");
    };
    for (size_t i = 0; i < result.size(); ++i) {
        result[i] = static_cast<unsigned char>((nibble(bytes[2 * i]) << 4) | nibble(bytes[2 * i + 1]));
    }
    return result;
}

struct DesktopPeerAdmission {
    std::shared_ptr<const cybou::p2p::PeerAdmissionPolicy> policy;
    std::shared_ptr<cybou::p2p::GeoDatabaseUpdater> updater;
    bool geo_required{true};
};

DesktopPeerAdmission DesktopPeerAdmissionPolicy(
    const std::filesystem::path& data_directory)
{
    const auto mode = qEnvironmentVariable("CYBOU_DEV_PEER_ADMISSION");
    if (mode == QStringLiteral("lab")) {
        return {std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(cybou::p2p::PeerAdmissionPolicy::Lab()), {}, false};
    }
    if (!mode.isEmpty() && mode != QStringLiteral("france")) {
        throw std::runtime_error("CYBOU_DEV_PEER_ADMISSION must be france or lab");
    }
    const auto csv = qEnvironmentVariable("CYBOU_GEO_COUNTRY_CSV");
    const auto sha = qEnvironmentVariable("CYBOU_GEO_SHA256");
    const auto month = qEnvironmentVariable("CYBOU_GEO_ISSUED_MONTH");
    if (csv.isEmpty() && sha.isEmpty() && month.isEmpty()) {
        auto updater = cybou::p2p::GeoDatabaseUpdater::Start(data_directory / "geo");
        auto policy = std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
            cybou::p2p::PeerAdmissionPolicy::PublicWithUpdater(updater));
        return {std::move(policy), std::move(updater), true};
    }
    if (sha.isEmpty() || month.isEmpty()) {
        throw std::runtime_error("set CYBOU_GEO_SHA256 and CYBOU_GEO_ISSUED_MONTH together");
    }
    const auto issued = cybou::p2p::FrenchIpDataset::ParseIssuedMonth(month.toStdString());
    if (!issued) throw std::runtime_error("CYBOU_GEO_ISSUED_MONTH must use YYYY-MM");
    const auto csv_path = csv.isEmpty() ? data_directory / "geo" / "dbip-country-lite.csv"
        : std::filesystem::path{csv.toStdU16String()};
    const auto dataset = cybou::p2p::FrenchIpDataset::LoadDbIpCountryCsv(
        csv_path, GeoSha256Pin(sha), *issued);
    if (!dataset) throw std::runtime_error("France Geo CSV is missing, corrupt, expired, future-dated, or has the wrong SHA-256");
    return {std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
        cybou::p2p::PeerAdmissionPolicy::Public(dataset)), {}, true};
}
} // namespace

CybouDesktopController::CybouDesktopController(CybouDesktopModel* model,
    std::filesystem::path data_directory, QObject* parent)
    : QObject{parent}, m_model{model}, m_data_directory{std::move(data_directory)}
{
    if (m_model) {
        connect(m_model, &CybouDesktopModel::lockVaultRequested, this, [this] { lockIdentity(); });
        connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { updatePoaFinalizer(); });
    }
}

CybouDesktopController::~CybouDesktopController()
{
    stop();
}

void CybouDesktopController::start()
{
    if (m_node_service) return;
    try {
        const auto explicit_network = qEnvironmentVariable("CYBOU_NETWORK_FILE");
        const auto network_path = explicit_network.isEmpty() ? m_data_directory / "network.bin"
            : std::filesystem::path{explicit_network.toStdU16String()};
        // A release pins its network to the bundled public manifest. A mismatch
        // must not move or rewrite user state; use an explicit LAB override.
        QFile bundled{QStringLiteral(":/network/cybou-dev-network.bin")};
        if (!bundled.open(QIODevice::ReadOnly)) throw std::runtime_error("the bundled CYBOU network is missing");
        const QByteArray current = bundled.readAll();
        const auto path_text = [](const std::filesystem::path& path) {
            return QString::fromStdU16String(path.u16string());
        };
        const bool lab_override = !explicit_network.isEmpty() || qEnvironmentVariableIsSet("CYBOU_DEV_KEEP_NETWORK");
        if (!lab_override) {
            const auto pinned = cybou::DeserializeCybouNetworkFile(std::span<const unsigned char>{
                reinterpret_cast<const unsigned char*>(current.constData()), static_cast<size_t>(current.size())});
            if (!pinned) throw std::runtime_error("bundled DEV network manifest is invalid");
            if (std::filesystem::exists(network_path)) {
                const auto selected = cybou::LoadCybouNetworkFile(network_path);
                if (!selected || cybou::NetworkId(selected->definition) != cybou::NetworkId(pinned->definition))
                    throw std::runtime_error("network differs from the pinned release; existing Identity and state are preserved");
            }
        }
        if (explicit_network.isEmpty() && !std::filesystem::exists(network_path)) {
            QFile installed{path_text(network_path)};
            if (!installed.open(QIODevice::WriteOnly) || installed.write(current) != current.size()) {
                installed.remove();
                throw std::runtime_error("cannot install the CYBOU network file");
            }
        }
        const auto network_file = cybou::LoadCybouNetworkFile(network_path);
        if (!network_file) throw std::runtime_error("missing or invalid CYBOU network.bin");
        const auto& genesis = network_file->genesis;
        const auto& definition = network_file->definition;
        m_model->setNetworkInfo(
            lab_override ? QStringLiteral("CYBOU LAB") : QStringLiteral("CYBOU DEV"),
            QString::fromStdString(cybou::NetworkId(definition).GetHex()));
        if (!m_archived_network.isEmpty()) {
            qWarning() << "CYBOU: data of an older DEV network moved to" << m_archived_network;
            m_model->notify(tr("This computer had data from an older CYBOU DEV network. It was moved to %1. "
                               "Create or restore your Identity on the current network.").arg(m_archived_network));
        }
        const std::filesystem::path data_dir = m_data_directory / "cybou_state";
        bool p2p_port_ok{false};
        const int p2p_port = qEnvironmentVariableIntValue("CYBOU_DEV_P2P_PORT", &p2p_port_ok);
        if (qEnvironmentVariableIsSet("CYBOU_DEV_P2P_PORT") &&
            (!p2p_port_ok || p2p_port <= 0 || p2p_port > 65535)) {
            throw std::runtime_error("invalid CYBOU_DEV_P2P_PORT");
        }
        const QString p2p_host = qEnvironmentVariable("CYBOU_DEV_P2P_HOST");
        std::optional<std::pair<std::string, uint16_t>> configured_p2p;
        if (!p2p_host.isEmpty() || p2p_port_ok) {
            if (p2p_host.isEmpty() || !p2p_port_ok) {
                throw std::runtime_error("CYBOU_DEV_P2P_HOST and CYBOU_DEV_P2P_PORT must be set together");
            }
            configured_p2p = std::make_pair(p2p_host.toStdString(), static_cast<uint16_t>(p2p_port));
        }
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
        auto peer_admission = DesktopPeerAdmissionPolicy(m_data_directory);
        m_geo_database_updater = peer_admission.updater;
        m_geo_admission_required = peer_admission.geo_required;
        m_model->setGeoAdmissionStatus(!m_geo_admission_required ? CybouGeoAdmissionStatus::NotRequired
            : (peer_admission.policy->Ready() ? CybouGeoAdmissionStatus::Ready : CybouGeoAdmissionStatus::Waiting));
        cybou::NodeRuntimeConfig config{
            .network_definition = definition,
            .data_dir = data_dir,
            .poa_finalizer_recovery_entropy = std::nullopt,
            .p2p_endpoint = configured_p2p,
            .local_p2p_endpoint = network_config.listen_endpoint,
            .db_cache_bytes = 8 << 20,
            .peer_admission_policy = std::move(peer_admission.policy),
        };
        m_node_service = std::make_unique<cybou::CybouNodeService>(cybou::CybouNodeServiceConfig{
            .runtime = std::move(config),
            .genesis = genesis,
        });
        m_node_service->Start();
        auto& runtime = m_node_service->Runtime();
        // Starting the local node is not proof of network connectivity.
        m_model->setNodeStatus(true, 0, false, QString::fromStdString(m_data_directory.string()));
        m_model->setSyncing(true);

        const auto identity_path = m_data_directory / "identity.cybou";
        m_identity_service = std::make_unique<cybou::CybouIdentityService>(runtime, identity_path);
        m_model->setIdentityService(m_identity_service.get());

        // Live Mail and Files run through the core application services.
        m_application = std::make_unique<CybouCoreApplicationAdapter>(runtime, *m_identity_service, m_data_directory);
        m_model->setApplicationBackend(m_application.get());
        m_model->requestApplicationCapabilities(/*mail=*/true, /*files=*/true);

        m_wallet_service = std::make_unique<cybou::CybouWalletService>(
            runtime, m_identity_service->GetKeyStore());
        m_model->setWalletService(m_wallet_service.get());
        // Read-only, rebuildable Authority preview over finalized history.
        m_authority_index = std::make_unique<cybou::AuthorityIndex>(runtime, m_data_directory / "authority-index.bin");

        const auto status = runtime.GetStatus();
        m_model->setFinalizedHeight(status.finalized_height);
        m_model->setPeerCount(0);

        m_node_service->StartNetwork(
            network_config,
            [this, geo_updater = m_geo_database_updater, geo_required = m_geo_admission_required](const cybou::SyncPeerResult& sync_result, const cybou::NodeRuntimeStatus& runtime_status,
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
                        model->setSyncing(true);
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
                    std::lock_guard identity_access{m_identity_access_mutex};
                    if (m_wallet_service && m_identity_service && m_identity_service->IsUnlocked()) {
                        m_wallet_service->SyncLedger();
                        const auto [balance, system_balance] = m_wallet_service->GetBalances();
                        QVector<CybouWalletEntry> entries;
                        for (const auto& entry : m_wallet_service->GetLedgerEntries()) {
                            CybouWalletEntry item;
                            item.id = QString::fromStdString(entry.entry_id.GetHex());
                            item.amount = entry.amount;
                            item.system_side = entry.system_side;
                            item.time = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(entry.timestamp));
                            // The ledger knows only pending/final; validation (when it
                            // exists) is joined in by the model via operation_id.
                            item.operation_state = entry.finality == cybou::WalletEntryFinality::PENDING
                                ? CybouOperationState::Submitted : CybouOperationState::Finalized;
                            item.operation_id = QString::fromStdString(entry.entry_id.GetHex());
                            item.finalized_height = entry.height;
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
                try {
                    publishAuthority();
                } catch (const std::exception& e) {
                    qWarning() << "cybou authority refresh error:" << e.what();
                }
                try {
                    publishNetworkAuthority();
                } catch (const std::exception& e) {
                    qWarning() << "cybou network authority refresh error:" << e.what();
                }
                const auto diagnostics = m_node_service->Runtime().GetDiagnostics();
                const bool syncing = !sync_result.reached_peer_tip;
                const auto geo_status = !geo_required ? CybouGeoAdmissionStatus::NotRequired
                    : ((geo_updater && geo_updater->Ready()) ? CybouGeoAdmissionStatus::Ready : CybouGeoAdmissionStatus::Waiting);
                QMetaObject::invokeMethod(m_model, [model = m_model, diagnostics, runtime_status, bootstrap_reachable, connected_peer_count, sync_error, syncing, geo_status] {
                    model->setNetworkDiagnostics(diagnostics);
                    model->setGeoAdmissionStatus(geo_status);
                    model->setSyncing(syncing);
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
        m_model->setApplicationBackend(nullptr);
        m_application.reset();
        m_model->setIdentityService(nullptr);
        m_model->setWalletService(nullptr);
        m_authority_index.reset();
        m_wallet_service.reset();
        m_identity_service.reset();
        m_node_service.reset();
        m_model->setNodeStatus(false, 0, false);
        m_model->setSyncError(reason);
        Q_EMIT startupFailed(reason);
    }
}

void CybouDesktopController::publishNetworkAuthority()
{
    CybouNetworkAuthorityStatus status;
    std::lock_guard identity_access{m_identity_access_mutex};
    if (m_identity_service && m_node_service && m_identity_service->IsNetworkAuthority()) {
        status.signer_enabled = m_node_service->Runtime().IsPoaFinalizerEnabled();
        const auto loaded = m_node_service->Runtime().GetStore().LoadState();
        if (loaded && loaded.state) {
            const auto& state = *loaded.state;
            status.proven = true;
            status.finalized_height = m_node_service->Runtime().GetFinalizedHeight().value_or(0);
            status.identities = state.accounts.size();
            status.names = state.names.names.size();
            status.pending_name_commits = state.names.pending_commits.size();
            for (const auto& [id, account] : state.accounts) {
                status.total_balance += account.balance;
                status.total_system_balance += account.system_balance;
            }
            status.onboarding_pool = state.onboarding_pool;
            status.security_reward_pool = state.security_reward_pool;
            status.pending_fee_pool = state.pending_fee_pool;
        }
    }
    QMetaObject::invokeMethod(m_model, [model = m_model, status] { model->setNetworkAuthority(status); },
        Qt::QueuedConnection);
}

void CybouDesktopController::lockIdentity()
{
    if (!m_model || !m_identity_service) return;
    if (m_node_service) m_node_service->Runtime().DisablePoaFinalizer();
    if (m_node_service) m_node_service->StopDesktopFinalizer();
    if (!m_model->beginVaultLock()) return;
    {
        std::lock_guard identity_access{m_identity_access_mutex};
        m_identity_service->Lock();
    }
    m_model->completeVaultLock();
    publishNetworkAuthority();
}

void CybouDesktopController::updatePoaFinalizer()
{
    if (!m_model || !m_node_service || !m_identity_service) return;
    std::lock_guard identity_access{m_identity_access_mutex};
    try {
        if (m_model->status().identity_state != CybouIdentityState::Active ||
            !m_identity_service->IsUnlocked() || !m_identity_service->IsNetworkAuthority()) {
            m_node_service->Runtime().DisablePoaFinalizer();
            m_node_service->StopDesktopFinalizer();
            return;
        }
        auto signer = std::make_shared<cybou::CybouKeyStorePoaSigner>(m_identity_service->GetKeyStore());
        if (!m_node_service->Runtime().EnablePoaFinalizer(std::move(signer))) {
            m_node_service->StopDesktopFinalizer();
            m_node_service->Runtime().DisablePoaFinalizer();
            qWarning() << "unlocked Identity does not match the genesis PoA key";
            return;
        }
        m_node_service->StartDesktopFinalizer();
    } catch (const std::exception& e) {
        m_node_service->StopDesktopFinalizer();
        m_node_service->Runtime().DisablePoaFinalizer();
        qWarning() << "cannot enable the Central Authority finalizer:" << e.what();
    }
}

void CybouDesktopController::publishAuthority()
{
    if (!m_authority_index || !m_identity_service) return;
    std::lock_guard identity_access{m_identity_access_mutex};
    m_authority_index->Sync();
    CybouAuthoritySummary summary;
    summary.scanned_height = m_authority_index->ScannedHeight();
    if (const auto account = m_identity_service->GetAccountId()) {
        if (const auto record = m_authority_index->Get(*account)) {
            summary.available = true;
            summary.age = record->age;
            summary.activity = record->activity;
            summary.system_contribution = record->system_contribution;
            summary.value = record->value;
            summary.validator_qualified = record->value >= cybou::AUTHORITY_VALIDATOR_QUALIFICATION;
        }
    }
    QMetaObject::invokeMethod(m_model, [model = m_model, summary] { model->setAuthority(summary); },
        Qt::QueuedConnection);
}

void CybouDesktopController::stop()
{
    if (m_node_service) m_node_service->StopNetwork();
    m_authority_index.reset();
    // Joins the application worker before the runtime goes away.
    if (m_model) m_model->setApplicationBackend(nullptr);
    m_application.reset();
    if (m_model) {
        m_model->setIdentityService(nullptr);
        m_model->setWalletService(nullptr);
    }
}
