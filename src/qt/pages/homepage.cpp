// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/homepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/pages/onboardingview.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <utility>

using namespace CybouUi;

namespace {

/** Card that behaves like a button: whole surface opens the product. */
class ClickableCard final : public QFrame
{
public:
    ClickableCard(std::function<void()> open, QWidget* parent)
        : QFrame{parent}, m_open{std::move(open)}
    {
        setObjectName(QStringLiteral("card"));
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()) && m_open) m_open();
        QFrame::mouseReleaseEvent(event);
    }
    void keyPressEvent(QKeyEvent* event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) && m_open) {
            m_open();
            return;
        }
        QFrame::keyPressEvent(event);
    }

private:
    std::function<void()> m_open;
};

Glyph ActivityGlyph(CybouActivityKind kind)
{
    switch (kind) {
    case CybouActivityKind::MailReceived: return Glyph::Envelope;
    case CybouActivityKind::MailSent: return Glyph::Send;
    case CybouActivityKind::FileUploaded: return Glyph::File;
    case CybouActivityKind::PaymentSent: return Glyph::ArrowUpRight;
    case CybouActivityKind::PaymentReceived: return Glyph::ArrowDownLeft;
    case CybouActivityKind::ServiceFee: return Glyph::Database;
    case CybouActivityKind::OnboardingCredit: return Glyph::Sparkles;
    case CybouActivityKind::IdentitySynced: return Glyph::Refresh;
    }
    return Glyph::Info;
}

Tint ActivityTint(CybouActivityKind kind)
{
    switch (kind) {
    case CybouActivityKind::MailReceived:
    case CybouActivityKind::MailSent: return Tint::Mint;
    case CybouActivityKind::FileUploaded: return Tint::Blue;
    case CybouActivityKind::PaymentSent:
    case CybouActivityKind::PaymentReceived:
    case CybouActivityKind::ServiceFee: return Tint::Indigo;
    case CybouActivityKind::OnboardingCredit: return Tint::Amber;
    case CybouActivityKind::IdentitySynced: return Tint::Neutral;
    }
    return Tint::Neutral;
}

void ClearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

} // namespace

HomePage::HomePage(CybouDesktopModel* model, std::function<void()> /*diagnostics_requested*/,
    std::function<void()> identity_requested, std::function<void()> wallet_requested,
    std::function<void()> mail_requested, std::function<void()> files_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_identity_requested{std::move(identity_requested)},
      m_wallet_requested{std::move(wallet_requested)},
      m_mail_requested{std::move(mail_requested)},
      m_files_requested{std::move(files_requested)}
{
    setMinimumWidth(0);
    auto* outer = new QVBoxLayout{this};
    outer->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget{this};
    outer->addWidget(m_stack);
    m_onboarding = new OnboardingView{m_model, m_stack};
    m_stack->addWidget(m_onboarding);
    m_dashboard = buildDashboard();
    m_stack->addWidget(m_dashboard);

    for (auto signal : {&CybouDesktopModel::statusChanged, &CybouDesktopModel::mailChanged,
             &CybouDesktopModel::filesChanged, &CybouDesktopModel::activityChanged,
             &CybouDesktopModel::walletChanged}) {
        connect(m_model, signal, this, [this] { refresh(); });
    }
    // Relative activity times age while the window stays open.
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(60000);
    refresh();
}

QWidget* HomePage::buildSummaryCard(const QString& title, Glyph glyph, Tint tint, QLabel*& value,
    QLabel*& caption, const std::function<void()>& open)
{
    auto* card = new ClickableCard{open, m_dashboard};
    card->setAccessibleName(title);
    card->setMinimumWidth(0);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(6);
    auto* head = new QHBoxLayout;
    head->setSpacing(10);
    head->addWidget(Chip(glyph, tint, card, 34, 18));
    auto* name = new QLabel{title, card};
    name->setObjectName(QStringLiteral("serviceTitle"));
    head->addWidget(name, 1);
    auto* chevron = new QLabel{card};
    chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
    head->addWidget(chevron);
    layout->addLayout(head);
    layout->addSpacing(6);
    value = new QLabel{card};
    value->setObjectName(QStringLiteral("metric"));
    caption = new QLabel{card};
    caption->setObjectName(QStringLiteral("metricCaption"));
    layout->addWidget(value);
    layout->addWidget(caption);
    return card;
}

QWidget* HomePage::buildDashboard()
{
    m_dashboard = new QWidget{m_stack};
    auto* root = new QVBoxLayout{m_dashboard};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(18);

    // Identity hero.
    auto* hero = new QFrame{m_dashboard};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(28, 22, 28, 22);
    hero_layout->setSpacing(18);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(4);
    m_identity_name = HeroTitle({}, hero, true);
    m_identity_name->setObjectName(QStringLiteral("heroTitleBig"));
    m_identity_name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hero_text->addWidget(m_identity_name);
    hero_text->addWidget(HeroSubtitle(tr("Your CYBOU Identity"), hero));
    hero_layout->addLayout(hero_text, 1);
    m_identity_state = Pill(tr("Protected"), Tint::Mint, hero);
    hero_layout->addWidget(m_identity_state, 0, Qt::AlignVCenter);
    auto* manage = new QPushButton{tr("Identity && Security"), hero};
    manage->setObjectName(QStringLiteral("secondaryButton"));
    connect(manage, &QPushButton::clicked, this, [this] { m_identity_requested(); });
    hero_layout->addWidget(manage, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    m_restore_banner = MutedText({}, m_dashboard);
    m_restore_banner->setObjectName(QStringLiteral("warningBadge"));
    m_restore_banner->setVisible(false);
    root->addWidget(m_restore_banner);

    // Product summary: Mail, Files, Wallet.
    auto* summary = new QHBoxLayout;
    summary->setSpacing(16);
    summary->addWidget(buildSummaryCard(tr("Mail"), Glyph::Envelope, Tint::Mint, m_mail_value, m_mail_caption,
        [this] { m_mail_requested(); }), 1);
    summary->addWidget(buildSummaryCard(tr("Files"), Glyph::Folder, Tint::Blue, m_files_value, m_files_caption,
        [this] { m_files_requested(); }), 1);
    summary->addWidget(buildSummaryCard(tr("Wallet"), Glyph::WalletCard, Tint::Indigo, m_wallet_value,
        m_wallet_caption, [this] { m_wallet_requested(); }), 1);
    root->addLayout(summary);

    // Recent activity.
    auto* activity = Card(m_dashboard);
    auto* activity_layout = new QVBoxLayout{activity};
    activity_layout->setContentsMargins(22, 18, 22, 14);
    activity_layout->setSpacing(4);
    activity_layout->addWidget(SectionTitle(tr("Recent activity"), activity));
    auto* rows_host = new QWidget{activity};
    m_activity_rows = new QVBoxLayout{rows_host};
    m_activity_rows->setContentsMargins(0, 6, 0, 0);
    m_activity_rows->setSpacing(2);
    activity_layout->addWidget(rows_host);
    m_activity_empty = MutedText(tr("Nothing yet. Your mail, files and payments will appear here."), activity);
    activity_layout->addWidget(m_activity_empty);
    root->addWidget(activity);
    root->addStretch();
    return m_dashboard;
}

void HomePage::refresh()
{
    const auto& status = m_model->status();
    const bool active = status.identity_state == CybouIdentityState::Active ||
        status.identity_state == CybouIdentityState::Syncing;
    m_stack->setCurrentWidget(active ? m_dashboard : static_cast<QWidget*>(m_onboarding));
    if (!active) return;

    m_identity_name->setText(status.primary_name.isEmpty()
        ? CybouProduct::shortId(status.account_id) : status.primary_name);
    const bool attention = !status.sync_error.isEmpty();
    m_identity_state->setText(attention ? tr("Needs attention") : tr("Protected"));
    m_identity_state->setProperty("tint", attention ? "amber" : "mint");
    m_identity_state->style()->unpolish(m_identity_state);
    m_identity_state->style()->polish(m_identity_state);

    const auto& restore = m_model->restoreProgress();
    const bool restoring = restore.mail == CybouRestoreStepState::Running ||
        restore.files == CybouRestoreStepState::Running;
    m_restore_banner->setText(tr("Mail and Files are still being restored. They appear as they are verified."));
    m_restore_banner->setVisible(restoring);

    const bool mail_connected = m_model->capabilities().mail;
    const int unread = m_model->unreadMailCount();
    m_mail_value->setText(!mail_connected ? tr("Not connected yet")
        : unread > 0 ? tr("%1 unread").arg(unread) : tr("No unread mail"));
    int drafts = 0;
    for (const auto& item : m_model->mailItems()) {
        if (item.folder == CybouMailFolder::Drafts) ++drafts;
    }
    m_mail_caption->setText(drafts == 1 ? tr("1 draft") : drafts > 1 ? tr("%1 drafts").arg(drafts) : tr("Inbox"));

    int files = 0;
    for (const auto& file : m_model->fileItems()) {
        if (!file.folder && !file.trashed) ++files;
    }
    m_files_value->setText(!m_model->capabilities().files ? tr("Not connected yet") : (files == 1 ? tr("1 file") : tr("%1 files").arg(files)));
    m_files_caption->setText(tr("%1 used").arg(CybouProduct::sizeText(status.storage_used)));

    m_wallet_value->setText(cybouAmountText(status.balance));
    m_wallet_caption->setText(tr("Available"));

    ClearLayout(m_activity_rows);
    int shown = 0;
    for (const auto& item : m_model->activity()) {
        if (shown == 6) break;
        auto* row = ActivityRow(ActivityGlyph(item.kind), ActivityTint(item.kind), item.title, item.subtitle,
            relTime(item.time), m_activity_rows->parentWidget());
        m_activity_rows->addWidget(row);
        ++shown;
    }
    m_activity_empty->setVisible(shown == 0);
}
