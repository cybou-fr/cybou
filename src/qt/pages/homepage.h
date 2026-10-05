// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_HOMEPAGE_H
#define CYBOU_QT_PAGES_HOMEPAGE_H

#include <qt/cybouui.h>

#include <QCoreApplication>
#include <QWidget>
#include <QStringList>
#include <QHash>

#include <functional>

class CybouDesktopModel;
class OnboardingView;
class QLabel;
class QPushButton;
class QStackedWidget;
class QFrame;
class QVBoxLayout;

/**
 * Home: onboarding until an Identity is active, then an Identity-centred
 * overview (Mail, Files, Wallet, recent activity). Home is not a node
 * dashboard; technical status lives in Diagnostics.
 */
class HomePage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(HomePage)

public:
    HomePage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
        std::function<void()> identity_requested, std::function<void()> wallet_requested,
        std::function<void()> mail_requested, std::function<void()> files_requested,
        QWidget* parent = nullptr);

    OnboardingView* onboarding() const { return m_onboarding; }

    /** Quick actions, set by the shell. */
    std::function<void()> onCompose;
    std::function<void()> onUpload;
    std::function<void()> onSendPayment;
    /** Items in the first-steps checklist that are still open. */
    QStringList openFirstSteps() const;
    /**
     * Reminders that stay until acted on, even when first steps are hidden:
     * "phrase" (never checked, or not in 90 days) and "system" (System
     * Balance covers few network operations).
     */
    QStringList reminders() const;
    /** Asks for three of the 24 words (never shows them) and records the check. */
    void checkRecoveryPhrase();

private:
    CybouDesktopModel* const m_model;
    QStackedWidget* m_stack{nullptr};
    OnboardingView* m_onboarding{nullptr};
    QWidget* m_dashboard{nullptr};
    QLabel* m_identity_name{nullptr};
    QLabel* m_identity_state{nullptr};
    QLabel* m_restore_banner{nullptr};
    QLabel* m_mail_value{nullptr};
    QLabel* m_mail_caption{nullptr};
    QLabel* m_files_value{nullptr};
    QLabel* m_files_caption{nullptr};
    QLabel* m_wallet_value{nullptr};
    QLabel* m_wallet_caption{nullptr};
    QVBoxLayout* m_activity_rows{nullptr};
    QLabel* m_activity_empty{nullptr};
    QStringList m_activity_presentation;
    QHash<QString, QWidget*> m_activity_widgets;
    QPushButton* m_activity_refresh{nullptr};
    QLabel* m_activity_refresh_hint{nullptr};
    QFrame* m_first_steps{nullptr};
    QVBoxLayout* m_first_steps_rows{nullptr};
    const std::function<void()> m_identity_requested;
    const std::function<void()> m_wallet_requested;
    const std::function<void()> m_mail_requested;
    const std::function<void()> m_files_requested;

    QWidget* buildDashboard();
    QWidget* buildSummaryCard(const QString& title, CybouUi::Glyph glyph, CybouUi::Tint tint,
        QLabel*& value, QLabel*& caption, const std::function<void()>& open,
        const QString& action_text = {}, const std::function<void()>& action = {});
    void rebuildFirstSteps();
    void refresh();
};

#endif // CYBOU_QT_PAGES_HOMEPAGE_H
