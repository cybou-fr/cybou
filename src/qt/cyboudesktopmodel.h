// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUDESKTOPMODEL_H
#define BITCOIN_QT_CYBOUDESKTOPMODEL_H

#include <QObject>
#include <QString>
#include <QLocale>

class ClientModel;
class OptionsModel;

namespace cybou {
class CybouIdentityService;
class CybouMailService;
class CybouWalletService;
}

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
    int peer_count{0};
    bool node_running{false};
    bool network_active{false};
    CybouIdentityState identity_state{CybouIdentityState::None};
    QString account_id;
    QString primary_name;
    int creation_height{0};
    QString data_directory;
    quint64 balance{0};
    quint64 system_balance{0};
    /** Last height committed by a BFT finality certificate; -1 until core
        exposes the finality feed. The GUI never derives finality locally. */
    int last_finalized_height{-1};
    /** Validators in the current epoch, equal weight 1 each; 0 until core
        exposes the validator set. */
    int validator_count{0};
};

/**
 * Canonical CYBOU amount rendering.
 *
 * CYBOU is indivisible (decimals = 0, 1 CYBOU = minimum unit), so the
 * rendering is an integer with locale grouping — never a decimal fraction.
 */
inline QString cybouAmountText(quint64 amount)
{
    return QLocale{}.toString(amount) + QStringLiteral(" CYBOU");
}

class CybouDesktopModel : public QObject
{
    Q_OBJECT

public:
    explicit CybouDesktopModel(QString network_name, QObject* parent = nullptr);

    const CybouDesktopStatus& status() const { return m_status; }
    const CybouCapabilities& capabilities() const { return m_capabilities; }
    /** True once the user requested identity creation and the node has not
        picked the request up yet (identity state still None). This is a
        UI-side request tracker only — protocol phases are driven by core. */
    bool identityCreationRequestPending() const { return m_identity_request_pending; }
    void setClientModel(ClientModel* client_model);
    OptionsModel* optionsModel() const;

    /** Drives capability flags; called by the core-facing adapter when a
        backend capability becomes available. */
    void setCapabilities(const CybouCapabilities& capabilities);

    /** Core-facing adapter entry: canonical network name and NetworkID once exposed. */
    void setNetworkInfo(const QString& network_name, const QString& network_id);

    /** Core-facing adapter entry (doc 73): the BFT finality feed reports the
        last certificate-committed height and the current validator set.
        -1 / 0 mean "not exposed" and render as such. No-op when unchanged. */
    void setFinalityStatus(int last_finalized_height, int validator_count);

    /** Core-facing adapter entry: connectivity of the CYBOU runtime.
        peer_count reflects the configured bootstrap authorities currently
        reachable (DEV: 0 or 1). No-op when unchanged. */
    void setPeerCount(int peer_count);

    /** Core-facing adapter entry (doc 73): core drives identity lifecycle
        transitions only. The GUI never sets these states on its own. */
    void setIdentityState(CybouIdentityState state, const QString& account_id = {},
        int creation_height = 0);

    /** Core-facing adapter entry (doc 73): balances from AccountState after
        every finalized transition that moves them. */
    void setBalances(quint64 balance, quint64 system_balance);

    /** Sets identity service provider and enables account_creation capability. */
    void setIdentityService(cybou::CybouIdentityService* identity_service);
    cybou::CybouIdentityService* identityService() const { return m_identity_service; }

    /** Sets mail service provider and updates email capability. */
    void setMailService(cybou::CybouMailService* mail_service);
    cybou::CybouMailService* mailService() const { return m_mail_service; }

    /** Sets wallet service provider and updates payments capability. */
    void setWalletService(cybou::CybouWalletService* wallet_service);
    cybou::CybouWalletService* walletService() const { return m_wallet_service; }

    /** Requests identity creation from the backend.
        The UI only emits the request; protocol behavior belongs to core. */
    void requestCreateIdentity(const QString& vault_password);
    bool requestRestoreIdentity(const QString& recovery_phrase, const QString& vault_password);
    bool requestUnlockIdentity(const QString& vault_password);

Q_SIGNALS:
    void statusChanged();
    void capabilitiesChanged();
    void createIdentityRequested();
    void identityCreationFailed(const QString& reason);

private:
    ClientModel* m_client_model{nullptr};
    cybou::CybouIdentityService* m_identity_service{nullptr};
    cybou::CybouMailService* m_mail_service{nullptr};
    cybou::CybouWalletService* m_wallet_service{nullptr};
    CybouDesktopStatus m_status;
    CybouCapabilities m_capabilities;
    bool m_identity_request_pending{false};

    void refreshFromClient();
    void refreshFinalizedName();
};

#endif // BITCOIN_QT_CYBOUDESKTOPMODEL_H
