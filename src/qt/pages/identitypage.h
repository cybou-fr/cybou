// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_IDENTITYPAGE_H
#define CYBOU_QT_PAGES_IDENTITYPAGE_H

#include <QCoreApplication>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QFrame;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

/**
 * Identity & Security: the user's CYBOU Identity, .cybou names, recovery
 * and local vault. The recovery phrase is never shown persistently; it is
 * revealed only after re-entering the vault password and an explicit
 * confirmation. Protocol details live under "Advanced security details".
 */
class IdentityPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(IdentityPage)

public:
    explicit IdentityPage(CybouDesktopModel* model, std::function<void()> home_requested = {},
        QWidget* parent = nullptr);
    /** Opens Identity creation (false) or restore (true) on Home. */
    std::function<void(bool restore)> onSetupRequested;
    /** Expands the Identity Authority breakdown (screenshots and tests). */

private:
    CybouDesktopModel* const m_model;
    const std::function<void()> m_home_requested;
    QWidget* m_setup{nullptr};
    QWidget* m_content{nullptr};
    QLabel* m_name{nullptr};
    QLabel* m_name_caption{nullptr};
    QLabel* m_account_id{nullptr};
    QLabel* m_vault_state{nullptr};
    QLabel* m_recovery_state{nullptr};
    QLabel* m_pq_state{nullptr};
    QVBoxLayout* m_names_rows{nullptr};
    QPushButton* m_claim{nullptr};
    QPushButton* m_setup_create{nullptr};
    QPushButton* m_setup_restore{nullptr};
    QLabel* m_claim_status{nullptr};
    QPushButton* m_lock{nullptr};
    QLabel* m_balance{nullptr};
    QLabel* m_system_balance{nullptr};
    QToolButton* m_advanced_toggle{nullptr};
    QFrame* m_advanced{nullptr};
    QVBoxLayout* m_advanced_rows{nullptr};

    QWidget* buildSetupPrompt();
    QWidget* buildContent();
    void refresh();
    void copyAccountId();
    void revealRecoveryPhrase();
    void replaceRecoveryPhrase();
    void claimName();
};

#endif // CYBOU_QT_PAGES_IDENTITYPAGE_H
