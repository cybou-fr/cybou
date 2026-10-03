#include <cybou/hex.h>
// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopcontroller.h>

#include <qt/cyboucoreapplicationadapter.h>
#include <qt/cyboudesktopmodel.h>

#include <cybou/official_networks.h>
#include <cybou/identity_service.h>
#include <cybou/network_genesis.h>
#include <cybou/node_service.h>
#include <cybou/p2p/geo_database_updater.h>
#include <cybou/p2p/peer_admission.h>
#include <cybou/poa_auth_adjustment.h>
#include <cybou/protocol_limits.h>
#include <cybou/validation_attestation.h>
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
#include <thread>
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
};

DesktopPeerAdmission DesktopPeerAdmissionPolicy(
    const std::filesystem::path& data_directory)
{
    const auto mode = qEnvironmentVariable("CYBOU_DEV_PEER_ADMISSION");
    if (!mode.isEmpty() && mode != QStringLiteral("france")) {
        throw std::runtime_error("CYBOU_DEV_PEER_ADMISSION must be france");
    }
    const auto csv = qEnvironmentVariable("CYBOU_GEO_COUNTRY_CSV");
    const auto sha = qEnvironmentVariable("CYBOU_GEO_SHA256");
    const auto month = qEnvironmentVariable("CYBOU_GEO_ISSUED_MONTH");
    if (csv.isEmpty() && sha.isEmpty() && month.isEmpty()) {
        auto updater = cybou::p2p::GeoDatabaseUpdater::Start(data_directory / "geo");
        auto policy = std::make_shared<const cybou::p2p::PeerAdmissionPolicy>(
            cybou::p2p::PeerAdmissionPolicy::PublicWithUpdater(updater));
        return {std::move(policy), std::move(updater)};
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
        cybou::p2p::PeerAdmissionPolicy::Public(dataset)), {}};
}
} // namespace

CybouDesktopController::CybouDesktopController(CybouDesktopModel* model,
    std::filesystem::path data_directory, QObject* parent)
    : QObject{parent}, m_model{model}, m_data_directory{std::move(data_directory)}
{
    if (m_model) {
        connect(m_model, &CybouDesktopModel::lockVaultRequested, this, [this] { lockIdentity(); });
        connect(m_model, &CybouDesktopModel::statusChanged, this, [this] {
            updatePoaSigner();
            updateValidationSigner();
        });
        connect(m_model, &CybouDesktopModel::finalizationPauseRequested, this, [this](bool paused) {
            m_production_paused = paused;
            updatePoaSigner();
            publishNetworkAuthority();
        });
        connect(m_model, &CybouDesktopModel::finalizeNowRequested, this, [this] { finalizeNow(); });
        connect(m_model, &CybouDesktopModel::authAdjustmentRequested, this,
            [this](const QString& account_id, bool grant, quint64 amount) { submitAuthAdjustment(account_id, grant, amount); });
    }
}

CybouDesktopController::~CybouDesktopController()
{
    if (m_operator_worker.joinable()) m_operator_worker.join();
    stop();
}

void CybouDesktopController::finalizeNow()
{
    // One block on demand while the loop is paused; the running loop needs no help.
    if (!m_node_service || !m_production_paused) return;
    if (m_operator_worker.joinable()) m_operator_worker.join();
    m_operator_worker = std::jthread([this] {
        bool produced{false};
        try {
            produced = m_node_service->Runtime().ProduceBlock().has_value();
            if (!produced && m_node_service->Runtime().LastBlockProductionStatus() == cybou::BlockProductionStatus::SAFETY_HALT) {
                m_node_service->Runtime().DisablePoaSigner();
            }
            publishNetworkAuthority();
        } catch (const std::exception& e) {
            qWarning() << "cannot finalize a block:" << e.what();
        }
        QMetaObject::invokeMethod(m_model, [model = m_model, produced] {
            model->notify(produced ? CybouDesktopModel::tr("Block finalized.")
                                   : CybouDesktopModel::tr("No block was finalized. See the finalizer state."));
        }, Qt::QueuedConnection);
    });
}

void CybouDesktopController::submitAuthAdjustment(const QString& account_id, bool grant, quint64 amount)
{
    const auto target = cybou::ParseHash256UserHex(account_id.toStdString());
    if (!m_node_service || !target) {
        m_model->setAuthAdjustmentFinished(false, CybouDesktopModel::tr("The AUTH change could not be prepared."));
        return;
    }
    if (m_operator_worker.joinable()) m_operator_worker.join();
    m_operator_worker = std::jthread([this, target = *target, grant, amount] {
        QString message;
        bool ok{false};
        try {
            const auto result = m_node_service->Runtime().SubmitPoaAuthAdjustment(
                grant ? cybou::PoaAuthAction::GRANT : cybou::PoaAuthAction::BURN, cybou::AccountId{target}, amount);
            ok = static_cast<bool>(result);
            message = ok ? CybouDesktopModel::tr("AUTH change submitted for the next block.")
                : result.status == cybou::OperationSubmitStatus::POA_SIGNER_UNAVAILABLE
                ? CybouDesktopModel::tr("The PoA signer is not active.")
                : CybouDesktopModel::tr("The AUTH change was rejected by local execution.");
        } catch (const std::exception& e) {
            message = QString::fromLocal8Bit(e.what());
        }
        QMetaObject::invokeMethod(m_model, [model = m_model, ok, message] { model->setAuthAdjustmentFinished(ok, message); },
            Qt::QueuedConnection);
    });
}

void CybouDesktopController::start()
{
    if (m_node_service) return;
    try {
        // The desktop starts only from the compiled, verified DEVNET constants.
        const auto& network = cybou::RequireOfficialNetwork("devnet");
        const auto& genesis = network.genesis_state;
        m_model->setNetworkInfo(QStringLiteral("CYBOU DEVNET"),
            QString::fromStdString(cybou::HexEncode(network.genesis.GetNetworkId())));
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
        const auto geo_policy = peer_admission.policy;
        m_model->setGeoAdmissionStatus(geo_policy->Ready() ? CybouGeoAdmissionStatus::Ready : CybouGeoAdmissionStatus::Waiting);
        auto config = cybou::MakeNodeRuntimeConfig(network, data_dir);
        if (configured_p2p) config.configured_peers.push_back({*configured_p2p, std::nullopt});
        config.advertised_endpoint = network_config.listen_endpoint;
        config.peer_admission_policy = std::move(peer_admission.policy);
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
        m_model->requestApplicationFeatureAvailability(/*mail=*/true, /*files=*/true);

        m_wallet_service = std::make_unique<cybou::CybouWalletService>(
            runtime, m_identity_service->GetKeyStore());
        m_model->setWalletService(m_wallet_service.get());

        const auto status = runtime.GetStatus();
        m_model->setFinalizedHeight(status.finalized_height);
        m_model->setPeerCount(0);

        m_node_service->StartNetwork(
            network_config,
            [this, geo_policy](const cybou::SyncPeerResult& sync_result, const cybou::NodeRuntimeStatus& runtime_status,
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
                            // The ledger knows only pending/final; Validation comes from
                            // this node's own candidate pool and attestations.
                            item.operation_state = entry.finality == cybou::WalletEntryFinality::PENDING
                                ? CybouOperationState::Submitted : CybouOperationState::Finalized;
                            if (entry.finality == cybou::WalletEntryFinality::PENDING) {
                                const auto status = m_node_service->Runtime().GetOperationStatus(entry.entry_id);
                                if (status.IsValidated()) {
                                    item.operation_state = CybouOperationState::Validated;
                                    item.validation_signatures = status.validation_signatures;
                                }
                            }
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
                const bool syncing = !sync_result.caught_up_with_known_peers;
                const auto geo_status = geo_policy->Ready() ? CybouGeoAdmissionStatus::Ready : CybouGeoAdmissionStatus::Waiting;
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
        if (m_node_service) m_node_service->Runtime().SetValidationSigner(nullptr);
        m_validation_signer_enabled = false;
        m_model->setApplicationBackend(nullptr);
        m_application.reset();
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

void CybouDesktopController::publishNetworkAuthority()
{
    CybouNetworkAuthorityStatus status;
    std::lock_guard identity_access{m_identity_access_mutex};
    if (m_identity_service && m_node_service && m_identity_service->IsNetworkAuthority()) {
        auto& runtime = m_node_service->Runtime();
        status.signer_enabled = runtime.IsPoaSignerActive();
        status.candidates = runtime.CandidateOperationCount();
        status.finalizer = runtime.LastBlockProductionStatus() == cybou::BlockProductionStatus::SAFETY_HALT
            ? CybouFinalizerState::SafetyHalt
            : !status.signer_enabled ? CybouFinalizerState::SignerUnavailable
            : m_production_paused ? CybouFinalizerState::Paused
            : CybouFinalizerState::Finalizing;
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
                status.total_authority += account.authority;
                if (account.authority > cybou::VALIDATION_AUTHORITY_THRESHOLD) ++status.validators;
            }
            status.onboarding_pool = state.onboarding_pool;
        }
    }
    QMetaObject::invokeMethod(m_model, [model = m_model, status] { model->setNetworkAuthority(status); },
        Qt::QueuedConnection);
}

void CybouDesktopController::lockIdentity()
{
    if (!m_model || !m_identity_service) return;
    if (m_node_service) m_node_service->Runtime().DisablePoaSigner();
    if (m_node_service) m_node_service->StopBlockProduction();
    if (!m_model->beginVaultLock()) return;
    {
        std::lock_guard identity_access{m_identity_access_mutex};
        m_identity_service->Lock();
    }
    m_model->completeVaultLock();
    publishNetworkAuthority();
}

void CybouDesktopController::updateValidationSigner()
{
    if (!m_model || !m_node_service || !m_identity_service) return;
    std::lock_guard identity_access{m_identity_access_mutex};
    const bool active = m_model->status().identity_state == CybouIdentityState::Active &&
        m_identity_service->IsUnlocked();
    if (active == m_validation_signer_enabled) return;
    // The runtime signs only for candidates it executed itself and only while
    // this Identity's finalized AUTH exceeds 10,000,000.
    m_node_service->Runtime().SetValidationSigner(active
        ? std::make_shared<cybou::CybouKeyStoreValidationSigner>(m_identity_service->GetKeyStore())
        : nullptr);
    m_validation_signer_enabled = active;
}

void CybouDesktopController::updatePoaSigner()
{
    if (!m_model || !m_node_service || !m_identity_service) return;
    std::lock_guard identity_access{m_identity_access_mutex};
    try {
        // The genesis Identity finalizes its own AccountCreate: the signer runs
        // while it is created or restored too, not only once it is Active.
        const auto state = m_model->status().identity_state;
        const bool holds_keys = state == CybouIdentityState::Active || state == CybouIdentityState::Syncing ||
            state == CybouIdentityState::Creating || state == CybouIdentityState::Restoring ||
            state == CybouIdentityState::NeedsAttention;
        if (!holds_keys || !m_identity_service->IsUnlocked() || !m_identity_service->IsNetworkAuthority()) {
            m_node_service->Runtime().DisablePoaSigner();
            m_node_service->StopBlockProduction();
            return;
        }
        auto signer = std::make_shared<cybou::CybouKeyStorePoaSigner>(m_identity_service->GetKeyStore());
        if (!m_node_service->Runtime().EnablePoaSigner(std::move(signer))) {
            m_node_service->StopBlockProduction();
            m_node_service->Runtime().DisablePoaSigner();
            qWarning() << "unlocked Identity does not match the genesis PoA key";
            return;
        }
        if (m_production_paused) m_node_service->StopBlockProduction();
        else m_node_service->StartBlockProduction();
    } catch (const std::exception& e) {
        m_node_service->StopBlockProduction();
        m_node_service->Runtime().DisablePoaSigner();
        qWarning() << "cannot enable the Central Authority finalizer:" << e.what();
    }
}

void CybouDesktopController::publishAuthority()
{
    if (!m_identity_service || !m_node_service) return;
    std::lock_guard identity_access{m_identity_access_mutex};
    quint64 auth_val{0};
    quint64 quota_used{0};
    quint32 epoch_operations{0};
    quint64 blocks_left{0};
    if (const auto account = m_identity_service->GetAccountId()) {
        auto& runtime = m_node_service->Runtime();
        if (const auto account_state = runtime.GetAccountState(*account)) {
            auth_val = account_state->authority;
        }
        // The next block's window: counters of an older epoch no longer count.
        const auto& params = runtime.GetNetworkGenesis().GetProtocolParameters();
        const uint64_t next_height = runtime.GetFinalizedHeight().value_or(0) + 1;
        const uint64_t epoch = cybou::EpochForHeight(next_height, params);
        if (params.epoch_blocks > 0) blocks_left = (epoch + 1) * params.epoch_blocks - next_height;
        const auto loaded = runtime.GetStore().LoadState();
        if (loaded && loaded.state) {
            if (const auto usage = loaded.state->usage.find(*account); usage != loaded.state->usage.end()) {
                quota_used = usage->second.stored_chunks * cybou::QUOTA_CHUNK_BYTES;
                if (usage->second.epoch == epoch) epoch_operations = usage->second.epoch_operations;
            }
        }
    }
    QMetaObject::invokeMethod(m_model, [model = m_model, auth_val, quota_used, epoch_operations, blocks_left] {
        model->setAuthority(auth_val);
        model->setResourceUsage(quota_used, epoch_operations, blocks_left);
    }, Qt::QueuedConnection);
}

void CybouDesktopController::stop()
{
    if (m_node_service) m_node_service->StopNetwork();
    // The signer references the Identity key store; never let it outlive it.
    if (m_node_service) m_node_service->Runtime().SetValidationSigner(nullptr);
    m_validation_signer_enabled = false;
    // Joins the application worker before the runtime goes away.
    if (m_model) m_model->setApplicationBackend(nullptr);
    m_application.reset();
    if (m_model) {
        m_model->setIdentityService(nullptr);
        m_model->setWalletService(nullptr);
    }
}
