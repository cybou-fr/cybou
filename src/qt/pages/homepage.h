// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_HOMEPAGE_H
#define BITCOIN_QT_PAGES_HOMEPAGE_H

#include <qt/cybouui.h>

#include <QCoreApplication>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class OnboardingView;
class QLabel;
class QStackedWidget;
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
    const std::function<void()> m_identity_requested;
    const std::function<void()> m_wallet_requested;
    const std::function<void()> m_mail_requested;
    const std::function<void()> m_files_requested;

    QWidget* buildDashboard();
    QWidget* buildSummaryCard(const QString& title, CybouUi::Glyph glyph, CybouUi::Tint tint,
        QLabel*& value, QLabel*& caption, const std::function<void()>& open);
    void refresh();
};

#endif // BITCOIN_QT_PAGES_HOMEPAGE_H
