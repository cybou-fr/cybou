// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopmodel.h>

#include <cybou/name_service.h>

#include <QRegularExpression>

#include <utility>

CybouDesktopModel::CybouDesktopModel(QString network_name, QObject* parent)
    : QObject{parent}
{
    m_status.network_name = std::move(network_name);
}

CybouDesktopModel::~CybouDesktopModel()
{
    if (m_name_service) m_name_service->Cancel();
    if (m_name_worker.joinable()) m_name_worker.join();
}

void CybouDesktopModel::setNodeStatus(bool running, int peer_count, bool network_active,
    const QString& data_directory)
{
    if (m_status.node_running == running && m_status.peer_count == peer_count &&
        m_status.network_active == network_active &&
        (data_directory.isEmpty() || m_status.data_directory == data_directory)) return;
    m_status.node_running = running;
    m_status.peer_count = peer_count;
    m_status.network_active = network_active;
    if (!data_directory.isEmpty()) m_status.data_directory = data_directory;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setCapabilities(const CybouCapabilities& capabilities)
{
    if (m_capabilities.account_creation == capabilities.account_creation &&
        m_capabilities.payments == capabilities.payments &&
        m_capabilities.email == capabilities.email &&
        m_capabilities.storage == capabilities.storage &&
        m_capabilities.backup == capabilities.backup) {
        return;
    }
    m_capabilities = capabilities;
    Q_EMIT capabilitiesChanged();
}

void CybouDesktopModel::setNetworkInfo(const QString& network_name, const QString& network_id)
{
    if (m_status.network_name == network_name && m_status.network_id == network_id) {
        return;
    }
    m_status.network_name = network_name;
    m_status.network_id = network_id;
    Q_EMIT statusChanged();
}

#include <cybou/identity_service.h>
#include <cybou/mail_service.h>
#include <cybou/wallet_service.h>

#include <support/cleanse.h>

void CybouDesktopModel::setIdentityService(cybou::CybouIdentityService* identity_service)
{
    if (m_identity_service == identity_service) return;
    if (m_name_service) m_name_service->Cancel();
    if (m_name_worker.joinable()) m_name_worker.join();
    m_name_service.reset();
    m_identity_service = identity_service;
    if (!m_identity_service && m_capabilities.account_creation) {
        m_capabilities.account_creation = false;
        Q_EMIT capabilitiesChanged();
    }
    if (m_identity_service) {
        if (const auto path = m_identity_service->GetStoragePath()) {
            m_name_service = std::make_unique<cybou::CybouNameService>(
                m_identity_service->GetNodeRuntime(), m_identity_service->GetKeyStore(), *path);
        }
        m_capabilities.account_creation = true;
        Q_EMIT capabilitiesChanged();

        if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE &&
            m_identity_service->GetAccountId().has_value()) {
            const QString acc_hex = QString::fromStdString(m_identity_service->GetAccountId()->Value().GetHex());
            setIdentityState(CybouIdentityState::Active, acc_hex);
        }
        refreshFinalizedName();
    }
}

bool CybouDesktopModel::requestClaimName(const QString& label, const QString& vault_password)
{
    if (!m_name_service || m_status.identity_state != CybouIdentityState::Active ||
        m_status.name_claim_pending || !m_status.primary_name.isEmpty()) return false;
    if (m_name_worker.joinable()) m_name_worker.join();
    m_status.name_claim_pending = true;
    m_status.name_claim_status = tr("Saving encrypted name claim...");
    Q_EMIT statusChanged();
    m_name_worker = std::jthread([this, name = label.toStdString(), password = vault_password.toStdString()]() mutable {
        const auto result = m_name_service->ClaimSync(std::move(name), password,
            [this](cybou::NameClaimPhase, const std::string& detail) {
                QMetaObject::invokeMethod(this, [this, detail] {
                    m_status.name_claim_status = QString::fromStdString(detail);
                    Q_EMIT statusChanged();
                }, Qt::QueuedConnection);
            });
        memory_cleanse(password.data(), password.size());
        QMetaObject::invokeMethod(this, [this, result] {
            m_status.name_claim_pending = false;
            m_status.name_claim_status.clear();
            refreshFinalizedName();
            Q_EMIT statusChanged();
            if (!result.success) Q_EMIT nameClaimFailed(QString::fromStdString(result.message));
        }, Qt::QueuedConnection);
    });
    return true;
}

void CybouDesktopModel::setMailService(cybou::CybouMailService* mail_service)
{
    m_mail_service = mail_service;
}

void CybouDesktopModel::setWalletService(cybou::CybouWalletService* wallet_service)
{
    m_wallet_service = wallet_service;
    if (m_wallet_service && m_status.identity_state == CybouIdentityState::Active) {
        if (!m_capabilities.payments) {
            m_capabilities.payments = true;
            Q_EMIT capabilitiesChanged();
        }
    } else if (!m_wallet_service && m_capabilities.payments) {
        m_capabilities.payments = false;
        Q_EMIT capabilitiesChanged();
    }
}

bool CybouDesktopModel::requestUnlockIdentity(const QString& vault_password)
{
    if (!m_identity_service || !m_identity_service->LoadVault(vault_password.toStdString())) return false;
    const auto account_id = m_identity_service->GetAccountId();
    if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE && account_id) {
        const auto state = m_identity_service->GetFinalizedAccountState();
        setIdentityState(CybouIdentityState::Active,
            QString::fromStdString(account_id->Value().GetHex()),
            state ? static_cast<int>(state->creation_height) : 0);
        if (state) setBalances(state->balance, state->system_balance);
    }
    Q_EMIT statusChanged();
    return true;
}

void CybouDesktopModel::requestCreateIdentity(const QString& vault_password)
{
    // The UI boundary ends here: protocol anti-Sybil work, operation
    // construction and finality handling belong to core. The flag below is
    // request bookkeeping only — the UI shows that the request was handed
    // over and never advances protocol phases on its own.
    m_identity_request_pending = true;
    Q_EMIT createIdentityRequested();
    Q_EMIT statusChanged();

    if (!m_identity_service) {
        return;
    }

    m_identity_service->CreateIdentityAsync(vault_password.toStdString(),
        [this](cybou::IdentityCreationPhase phase, const std::string& /*detail*/) {
            QMetaObject::invokeMethod(this, [this, phase] {
                switch (phase) {
                case cybou::IdentityCreationPhase::CREATING_KEYS:
                    setIdentityState(CybouIdentityState::CreatingKeys);
                    break;
                case cybou::IdentityCreationPhase::PERFORMING_WORK:
                    setIdentityState(CybouIdentityState::PerformingWork);
                    break;
                case cybou::IdentityCreationPhase::BROADCASTING:
                    setIdentityState(CybouIdentityState::Broadcasting);
                    break;
                case cybou::IdentityCreationPhase::WAITING_FOR_FINALITY:
                    setIdentityState(CybouIdentityState::WaitingForFinality);
                    break;
                case cybou::IdentityCreationPhase::FAILED:
                    setIdentityState(CybouIdentityState::None);
                    break;
                default:
                    break;
                }
            }, Qt::QueuedConnection);
        },
        [this](const cybou::IdentityCreationResult& result) {
            QMetaObject::invokeMethod(this, [this, result] {
                if (result.success) {
                    const QString acc_hex = QString::fromStdString(result.account_id.Value().GetHex());
                    setIdentityState(CybouIdentityState::Active, acc_hex, static_cast<int>(result.creation_height));
                    setBalances(0, result.system_balance);
                } else {
                    setIdentityState(CybouIdentityState::None);
                    Q_EMIT identityCreationFailed(QString::fromStdString(result.error_message));
                }
            }, Qt::QueuedConnection);
        });
}

bool CybouDesktopModel::requestRestoreIdentity(const QString& recovery_phrase, const QString& vault_password)
{
    if (!m_identity_service) return false;
    const auto parts = recovery_phrase.trimmed().split(QRegularExpression{QStringLiteral("\\s+")}, Qt::SkipEmptyParts);
    if (parts.size() != 24) return false;
    cybou::RecoveryWords words;
    for (int i{0}; i < parts.size(); ++i) words[i] = parts[i].toStdString();
    if (!cybou::DecodeRecoveryWords(words)) return false;
    m_identity_request_pending = true;
    Q_EMIT statusChanged();
    m_identity_service->RestoreIdentityAsync(std::move(words), vault_password.toStdString(),
        [this](cybou::IdentityCreationPhase phase, const std::string&) {
            QMetaObject::invokeMethod(this, [this, phase] {
                switch (phase) {
                case cybou::IdentityCreationPhase::CREATING_KEYS: setIdentityState(CybouIdentityState::CreatingKeys); break;
                case cybou::IdentityCreationPhase::BROADCASTING: setIdentityState(CybouIdentityState::Broadcasting); break;
                case cybou::IdentityCreationPhase::WAITING_FOR_FINALITY: setIdentityState(CybouIdentityState::WaitingForFinality); break;
                case cybou::IdentityCreationPhase::FAILED: setIdentityState(CybouIdentityState::None); break;
                default: break;
                }
            }, Qt::QueuedConnection);
        },
        [this](const cybou::IdentityCreationResult& result) {
            QMetaObject::invokeMethod(this, [this, result] {
                if (result.success) {
                    setIdentityState(CybouIdentityState::Active,
                        QString::fromStdString(result.account_id.Value().GetHex()),
                        static_cast<int>(result.creation_height));
                    setBalances(0, result.system_balance);
                } else {
                    setIdentityState(CybouIdentityState::None);
                    Q_EMIT identityCreationFailed(QString::fromStdString(result.error_message));
                }
            }, Qt::QueuedConnection);
        });
    return true;
}

void CybouDesktopModel::setFinalityStatus(int last_finalized_height, int validator_count)
{
    refreshFinalizedName();
    if (m_status.last_finalized_height == last_finalized_height &&
        m_status.validator_count == validator_count) {
        return;
    }
    m_status.last_finalized_height = last_finalized_height;
    m_status.validator_count = validator_count;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::refreshFinalizedName()
{
    const auto name = m_identity_service && m_status.identity_state == CybouIdentityState::Active
        ? m_identity_service->GetFinalizedPrimaryName() : std::nullopt;
    const QString finalized = name ? QString::fromStdString(*name) + QStringLiteral(".cybou") : QString{};
    if (m_status.primary_name == finalized) return;
    m_status.primary_name = finalized;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setPeerCount(int peer_count)
{
    if (m_status.peer_count == peer_count) {
        return;
    }
    m_status.peer_count = peer_count;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setLastSync(const QDateTime& when)
{
    m_last_sync = when;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setIdentityState(CybouIdentityState state, const QString& account_id,
    int creation_height)
{
    if (m_status.identity_state == state && m_status.account_id == account_id &&
        m_status.creation_height == creation_height) {
        return;
    }
    m_status.identity_state = state;
    m_status.account_id = account_id;
    if (state != CybouIdentityState::Active) m_status.primary_name.clear();
    m_status.creation_height = creation_height;
    if (state == CybouIdentityState::Active || state == CybouIdentityState::None) {
        m_identity_request_pending = false;
    }
    if (state == CybouIdentityState::Active) {
        bool caps_changed = false;
        if (m_wallet_service && !m_capabilities.payments) {
            m_capabilities.payments = true;
            caps_changed = true;
        }
        if (caps_changed) {
            Q_EMIT capabilitiesChanged();
        }
    }
    Q_EMIT statusChanged();
    if (state == CybouIdentityState::Active) refreshFinalizedName();
}

void CybouDesktopModel::setBalances(quint64 balance, quint64 system_balance)
{
    if (m_status.balance == balance && m_status.system_balance == system_balance) {
        return;
    }
    m_status.balance = balance;
    m_status.system_balance = system_balance;
    Q_EMIT statusChanged();
}
