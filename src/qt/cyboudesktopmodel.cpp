// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopmodel.h>

#include <qt/clientmodel.h>

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
