// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPMODEL_H
#define BITCOIN_QT_CYBOUDESKTOPMODEL_H

#include <QObject>
#include <QString>

class ClientModel;
class OptionsModel;

struct CybouCapabilities {
    bool account_creation{false};
    bool payments{false};
    bool email{false};
    bool storage{false};
    bool backup{false};
};

/**
 * Identity lifecycle as surfaced to the UI.
 *
 * The desktop never invents transitions: core wiring drives state changes
 * through this model. Until account creation is connected, the state
 * remains None.
 */
enum class CybouIdentityState {
    None,
    CreatingKeys,
    PerformingWork,
    Broadcasting,
    WaitingForFinality,
    Active,
};

struct CybouDesktopStatus {
    QString network_name{"CYBOU-DEV"};
    /** Canonical network identifier once core exposes it; empty until then. */
    QString network_id{};
    int height{0};
    int peer_count{0};
    bool node_running{false};
    bool network_active{false};
    CybouIdentityState identity_state{CybouIdentityState::None};
    QString account_id;
    int creation_height{0};
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
    OptionsModel* optionsModel() const;

    /** Drives capability flags; called by the core-facing adapter when a
        backend capability becomes available. */
    void setCapabilities(const CybouCapabilities& capabilities);

    /** Requests identity creation from the backend.
        The UI only emits the request; protocol behavior belongs to core. */
    void requestCreateIdentity();

Q_SIGNALS:
    void statusChanged();
    void capabilitiesChanged();
    void createIdentityRequested();

private:
    ClientModel* m_client_model{nullptr};
    CybouDesktopStatus m_status;
    CybouCapabilities m_capabilities;

    void refreshFromClient();
};

#endif // BITCOIN_QT_CYBOUDESKTOPMODEL_H
