// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_WALLETPAGE_H
#define BITCOIN_QT_PAGES_WALLETPAGE_H

#include <QCoreApplication>
#include <QSet>
#include <QWidget>

class CybouDesktopModel;
class QCompleter;
class QFrame;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

/**
 * Wallet around .cybou names. CYBOU is indivisible (whole CYBOU only).
 * Balance is user-controlled; System Balance funds CYBOU network services
 * and cannot be sent. Payments go to a .cybou name; the deterministic
 * network service fee is shown before sending. Authority (AUTH) is shown
 * beside both balances with the limits it currently grants.
 */
class WalletPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(WalletPage)

public:
    explicit WalletPage(CybouDesktopModel* model, QWidget* parent = nullptr);

    void openSend(const QString& to = {});

private:
    CybouDesktopModel* const m_model;
    QLabel* m_available{nullptr};
    QLabel* m_system{nullptr};
    QLabel* m_gate{nullptr};
    QPushButton* m_send_button{nullptr};
    QPushButton* m_receive_button{nullptr};
    QPushButton* m_lock_button{nullptr};
    QLabel* m_system_hint{nullptr};
    QLabel* m_authority{nullptr};
    QLabel* m_authority_hint{nullptr};
    /** Current limits granted by finalized AUTH. */
    QLabel* m_limit_storage{nullptr};
    QLabel* m_limit_operations{nullptr};
    QLabel* m_limit_validation{nullptr};
    QLabel* m_limit_next{nullptr};
    QProgressBar* m_next_tier_bar{nullptr};
    /** Fee groups the user expanded (keyed by the group's first entry id). */
    QSet<QString> m_expanded_fees;
    QFrame* m_send_panel{nullptr};
    QLineEdit* m_to{nullptr};
    QLabel* m_to_hint{nullptr};
    /** One-click recipients: the most recent contacts. */
    QHBoxLayout* m_recent{nullptr};
    QCompleter* m_completer{nullptr};
    QLineEdit* m_amount{nullptr};
    QLabel* m_fee{nullptr};
    QLabel* m_send_status{nullptr};
    QPushButton* m_confirm{nullptr};
    QVBoxLayout* m_activity_rows{nullptr};
    bool m_reviewing{false};
    QLabel* m_review{nullptr};
    QLabel* m_activity_empty{nullptr};

    void refresh();
    void rebuildActivity();
    /** Modal with every fact about one ledger entry (status, block, OperationID). */
    void showEntryDetails(const QString& entry_id);
    bool eventFilter(QObject* watched, QEvent* event) override;
    void updateSendState();
    void submit();
    void setReviewing(bool reviewing);
    void showReceive();
    /** Balance -> System Balance, with an explicit irreversible review. */
    void showLockDialog();
};

#endif // BITCOIN_QT_PAGES_WALLETPAGE_H
