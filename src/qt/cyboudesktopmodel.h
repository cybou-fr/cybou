// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPMODEL_H
#define BITCOIN_QT_CYBOUDESKTOPMODEL_H

#include <QObject>
#include <QString>

class ClientModel;

struct CybouCapabilities {
    bool account_creation{false};
    bool payments{false};
    bool email{false};
    bool storage{false};
    bool backup{false};
};

struct CybouDesktopStatus {
    QString network_name{"CYBOU-DEV"};
    int height{0};
    int peer_count{0};
    bool node_running{false};
    bool network_active{false};
    bool has_identity{false};
    QString account_id;
    QString data_directory;
    quint64 balance{0};
    quint64 system_balance{0};
};

class CybouDesktopModel : public QObject
{
    Q_OBJECT

public:
    explicit CybouDesktopModel(QString network_name, QObject* parent = nullptr);

    const CybouDesktopStatus& status() const { return m_status; }
    const CybouCapabilities& capabilities() const { return m_capabilities; }
    void setClientModel(ClientModel* client_model);

Q_SIGNALS:
    void statusChanged();
    void capabilitiesChanged();

private:
    ClientModel* m_client_model{nullptr};
    CybouDesktopStatus m_status;
    CybouCapabilities m_capabilities;

    void refreshFromClient();
};

#endif // BITCOIN_QT_CYBOUDESKTOPMODEL_H
