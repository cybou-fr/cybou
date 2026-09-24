// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopmodel.h>

#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>

#include <utility>

CybouDesktopModel::CybouDesktopModel(QString network_name, QObject* parent)
    : QObject{parent}
{
    m_status.network_name = std::move(network_name);
}

void CybouDesktopModel::setClientModel(ClientModel* client_model)
{
    if (m_client_model) disconnect(m_client_model, nullptr, this, nullptr);
    m_client_model = client_model;
    m_status.node_running = client_model != nullptr;

    if (m_client_model) {
        connect(m_client_model, &ClientModel::numConnectionsChanged, this, [this](const int count) {
            m_status.peer_count = count;
            Q_EMIT statusChanged();
        });
        connect(m_client_model, &ClientModel::numBlocksChanged, this,
            [this](const int count, const QDateTime&, double, SyncType, SynchronizationState) {
                m_status.height = count;
                Q_EMIT statusChanged();
            });
        connect(m_client_model, &ClientModel::networkActiveChanged, this, [this](const bool active) {
            m_status.network_active = active;
            Q_EMIT statusChanged();
        });
    }

    refreshFromClient();
}

void CybouDesktopModel::refreshFromClient()
{
    if (m_client_model) {
        m_status.peer_count = m_client_model->getNumConnections();
        m_status.height = m_client_model->getNumBlocks();
        m_status.data_directory = m_client_model->dataDir();
    } else {
        m_status.peer_count = 0;
        m_status.height = 0;
        m_status.data_directory.clear();
    }
    Q_EMIT statusChanged();
}

OptionsModel* CybouDesktopModel::optionsModel() const
{
    return m_client_model ? m_client_model->getOptionsModel() : nullptr;
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

void CybouDesktopModel::setIdentityService(cybou::CybouIdentityService* identity_service)
{
    m_identity_service = identity_service;
    if (m_identity_service) {
        m_capabilities.account_creation = true;
        Q_EMIT capabilitiesChanged();

        if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE &&
            m_identity_service->GetAccountId().has_value()) {
            const QString acc_hex = QString::fromStdString(m_identity_service->GetAccountId()->Value().GetHex());
            setIdentityState(CybouIdentityState::Active, acc_hex);
        }
    }
}

void CybouDesktopModel::requestCreateIdentity()
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

    m_identity_service->CreateIdentityAsync(
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
                }
            }, Qt::QueuedConnection);
        });
}

void CybouDesktopModel::setFinalityStatus(int last_finalized_height, int validator_count)
{
    if (m_status.last_finalized_height == last_finalized_height &&
        m_status.validator_count == validator_count) {
        return;
    }
    m_status.last_finalized_height = last_finalized_height;
    m_status.validator_count = validator_count;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setChainStatus(int height, int peer_count)
{
    if (m_status.height == height && m_status.peer_count == peer_count) {
        return;
    }
    m_status.height = height;
    m_status.peer_count = peer_count;
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
    m_status.creation_height = creation_height;
    if (state == CybouIdentityState::Active || state == CybouIdentityState::None) {
        m_identity_request_pending = false;
    }
    Q_EMIT statusChanged();
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
