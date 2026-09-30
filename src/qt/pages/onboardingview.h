// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_ONBOARDINGVIEW_H
#define BITCOIN_QT_PAGES_ONBOARDINGVIEW_H

#include <QCoreApplication>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

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
        /** Right after a new Identity is finalized: claim name.cybou (or skip). */
        ChooseName,
    };

    explicit OnboardingView(CybouDesktopModel* model, QWidget* parent = nullptr);
    ~OnboardingView() override;

    Screen screen() const;
    void showScreen(Screen screen);
    /** Starts Identity creation exactly like the Welcome screen's button. */
    void beginCreate() { startCreate(); }
    /** Opens the recovery-phrase restore form exactly like the Welcome screen's button. */
    void beginRestore();
    /** Onboarding still has a step to show although the Identity is active. */
    bool holdsActiveIdentity() const { return screen() == Screen::ChooseName || m_offer_name; }
    /** Called when onboarding hands over to the normal app. */
    std::function<void()> onFinished;

private:
    CybouDesktopModel* const m_model;
    QStackedWidget* m_stack;

    // Create flow.
    QLineEdit* m_password{nullptr};
    QLineEdit* m_password_confirm{nullptr};
    QLabel* m_password_hint{nullptr};
    QPushButton* m_password_next{nullptr};
    QLabel* m_strength{nullptr};
    QPushButton* m_continue_restoring{nullptr};
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
    QVector<QLineEdit*> m_word_fields;
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
    QWidget* buildChooseName();
    void submitName();
    void finishNameStep();

    // Name step: offered once, right after this session created an Identity.
    bool m_offer_name{false};
    QString m_name_password;
    QLineEdit* m_name_input{nullptr};
    QLabel* m_name_hint{nullptr};
    QPushButton* m_name_claim{nullptr};

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
    QString enteredPhrase() const;
    void clearPhrase();
    void distributeWords(int start, const QStringList& words);
    void refresh();
};

#endif // BITCOIN_QT_PAGES_ONBOARDINGVIEW_H
