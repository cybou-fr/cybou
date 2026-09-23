// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_WALLETPAGE_H
#define BITCOIN_QT_PAGES_WALLETPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>
#include <QWidget>
#include <functional>

class CybouDesktopModel;
class QLabel;
class QListWidget;
class QPushButton;

/**
 * Wallet UI shaped by the Balance / System Balance rules (docs 52):
 *
 *  - One native asset, indivisible: amounts render as whole CYBOU.
 *  - Balance is user-controlled: a debit requires the user's own
 *    authorization; the UI offers no path that contradicts this.
 *  - System Balance is frozen CYBOU assigned to protocol use: it funds
 *    deterministic protocol fees (Email, future services). It does not boost
 *    Proof of Trust in Beta. It cannot be transferred, withdrawn or traded.
 *  - Balance -> System Balance is a one-way LOCK_TO_SYSTEM; the UI renders
 *    it as irreversible and never offers a reverse direction.
 *
 * The ledger is an in-memory view model for now: entries appear once core
 * streams protocol operations to the desktop model. Action buttons stay
 * gated on the payments capability and an active identity.
 */
class WalletPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(WalletPage)

public:
    WalletPage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    enum class EntryKind {
        OnboardingBonus, /**< OnboardingPool -> System Balance at AccountCreate. */
        MailFee,         /**< Deterministic size-aware fee, debited from System Balance. */
        Payment,         /**< User-authorized Balance transfer. */
        LockToSystem,    /**< Irreversible Balance -> System Balance lock. */
    };

    enum class EntryFinality {
        Pending,
        Final,
    };

    struct Entry {
        QString id;
        EntryKind kind{EntryKind::MailFee};
        qint64 amount{0};      /**< Signed: positive credits, negative debits. */
        bool system_side{true}; /**< true: moved System Balance; false: Balance. */
        QString counterparty;  /**< Peer account, service name, or empty. */
        QDateTime at;
        EntryFinality finality{EntryFinality::Pending};
    };

    CybouDesktopModel* m_model;
    QVector<Entry> m_entries;

    QLabel* m_account_line{nullptr};
    QLabel* m_balance{nullptr};
    QLabel* m_system_balance{nullptr};
    QLabel* m_gate_hint{nullptr};
    QListWidget* m_activity{nullptr};
    QPushButton* m_send{nullptr};
    QPushButton* m_lock{nullptr};
    QPushButton* m_receive{nullptr};

    void refresh();
    void rebuildActivity();
    void actionNotWired();
    static QString kindText(EntryKind kind);
    static QString finalityText(EntryFinality finality);
};

#endif // BITCOIN_QT_PAGES_WALLETPAGE_H
