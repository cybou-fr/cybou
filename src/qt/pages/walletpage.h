// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_WALLETPAGE_H
#define BITCOIN_QT_PAGES_WALLETPAGE_H

#include <QCoreApplication>
#include <QWidget>

class CybouDesktopModel;
class QCompleter;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

/**
 * Wallet around .cybou names. CYBOU is indivisible (whole CYBOU only).
 * Balance is user-controlled; System Balance funds CYBOU network services
 * and cannot be sent. Payments go to a .cybou name; the deterministic
 * network service fee is shown before sending.
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
    QFrame* m_send_panel{nullptr};
    QLineEdit* m_to{nullptr};
    QLabel* m_to_hint{nullptr};
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
    void updateSendState();
    void submit();
    void setReviewing(bool reviewing);
    void showReceive();
};

#endif // BITCOIN_QT_PAGES_WALLETPAGE_H
