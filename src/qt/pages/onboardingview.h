// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_ONBOARDINGVIEW_H
#define BITCOIN_QT_PAGES_ONBOARDINGVIEW_H

#include <QCoreApplication>
#include <QStringList>
#include <QVector>
#include <QWidget>

class CybouDesktopModel;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

/**
 * First-run Identity onboarding shown on Home while no Identity is active:
 * welcome, create (password -> recovery words -> confirmation -> progress),
 * restore from mnemonic, and unlock of an existing local vault.
 *
 * The view only collects user input and renders model state; key
 * generation, account creation and restore run behind the desktop model.
 */
class OnboardingView : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(OnboardingView)

public:
    enum class Screen {
        Welcome,
        Password,
        RecoveryWords,
        ConfirmWords,
        Creating,
        Restore,
        Restoring,
        Unlock,
    };

    explicit OnboardingView(CybouDesktopModel* model, QWidget* parent = nullptr);
    ~OnboardingView() override;

    Screen screen() const;
    void showScreen(Screen screen);

private:
    CybouDesktopModel* const m_model;
    QStackedWidget* m_stack;

    // Create flow.
    QLineEdit* m_password{nullptr};
    QLineEdit* m_password_confirm{nullptr};
    QLabel* m_password_hint{nullptr};
    QPushButton* m_password_next{nullptr};
    QWidget* m_words_grid{nullptr};
    QVector<int> m_confirm_positions;
    QVector<QLineEdit*> m_confirm_inputs;
    QVector<QLabel*> m_confirm_labels;
    QLabel* m_confirm_hint{nullptr};
    QPushButton* m_confirm_next{nullptr};
    QStringList m_words;
    QString m_pending_password;
    QVector<QLabel*> m_create_steps;
    QLabel* m_create_error{nullptr};
    QPushButton* m_create_retry{nullptr};

    // Restore flow.
    QPlainTextEdit* m_phrase{nullptr};
    QLabel* m_phrase_count{nullptr};
    QLineEdit* m_restore_password{nullptr};
    QLineEdit* m_restore_confirm{nullptr};
    QLabel* m_restore_hint{nullptr};
    QPushButton* m_restore_button{nullptr};
    QVector<QLabel*> m_restore_steps;

    // Unlock.
    QLabel* m_unlock_title{nullptr};
    QLineEdit* m_unlock_password{nullptr};
    QLabel* m_unlock_hint{nullptr};

    QWidget* buildWelcome();
    QWidget* buildPassword();
    QWidget* buildRecoveryWords();
    QWidget* buildConfirmWords();
    QWidget* buildCreating();
    QWidget* buildRestore();
    QWidget* buildRestoring();
    QWidget* buildUnlock();

    void startCreate();
    void acceptPassword();
    void populateWords();
    void prepareConfirmation();
    void acceptConfirmation();
    void updatePasswordState();
    void updateRestoreState();
    void submitRestore();
    void submitUnlock();
    void cancelCreate();
    void clearSecrets();
    void refresh();
};

#endif // BITCOIN_QT_PAGES_ONBOARDINGVIEW_H
