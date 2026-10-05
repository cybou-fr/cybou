// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/homepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/pages/onboardingview.h>

#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QLineEdit>
#include <QLocale>
#include <QPointer>
#include <QRandomGenerator>
#include <QSettings>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstdlib>
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
    m_onboarding->onFinished = [this] { refresh(); };
    m_dashboard = buildDashboard();
    m_stack->addWidget(m_dashboard);

    for (auto signal : {&CybouDesktopModel::statusChanged, &CybouDesktopModel::mailChanged,
             &CybouDesktopModel::filesChanged, &CybouDesktopModel::activityChanged,
             &CybouDesktopModel::walletChanged, &CybouDesktopModel::namesChanged}) {
        connect(m_model, signal, this, [this] { refresh(); });
    }
    // Relative activity times age while the window stays open.
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(60000);
    refresh();
}

QWidget* HomePage::buildSummaryCard(const QString& title, Glyph glyph, Tint tint, QLabel*& value,
    QLabel*& caption, const std::function<void()>& open, const QString& action_text, const std::function<void()>& action)
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
    if (!action_text.isEmpty()) {
        layout->addSpacing(6);
        auto* quick = new QPushButton{action_text, card};
        quick->setObjectName(QStringLiteral("secondaryButton"));
        quick->setCursor(Qt::PointingHandCursor);
        connect(quick, &QPushButton::clicked, card, [action] { if (action) action(); });
        layout->addWidget(quick, 0, Qt::AlignLeft);
    }
    return card;
}

QWidget* HomePage::buildDashboard()
{
    m_dashboard = new QWidget{m_stack};
    auto* outer = new QHBoxLayout{m_dashboard};
    outer->setContentsMargins(28, 24, 28, 28);
    auto* column = new QWidget{m_dashboard};
    column->setMaximumWidth(1180);
    outer->addStretch(0);
    outer->addWidget(column, 1);
    outer->addStretch(0);
    auto* root = new QVBoxLayout{column};
    root->setContentsMargins(0, 0, 0, 0);
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
        [this] { m_mail_requested(); }, tr("Compose"), [this] { if (onCompose) onCompose(); }), 1);
    summary->addWidget(buildSummaryCard(tr("Files"), Glyph::Folder, Tint::Blue, m_files_value, m_files_caption,
        [this] { m_files_requested(); }, tr("Upload"), [this] { if (onUpload) onUpload(); }), 1);
    summary->addWidget(buildSummaryCard(tr("Wallet"), Glyph::WalletCard, Tint::Indigo, m_wallet_value,
        m_wallet_caption, [this] { m_wallet_requested(); }, tr("Send"), [this] {
            // Nothing to send yet: the Wallet page explains how to get CYBOU.
            if (m_model->status().balance == 0) m_wallet_requested();
            else if (onSendPayment) onSendPayment();
        }), 1);
    root->addLayout(summary);

    // First steps for a new Identity (hidden once done or dismissed).
    m_first_steps = Card(m_dashboard);
    m_first_steps->setObjectName(QStringLiteral("card"));
    auto* steps_layout = new QVBoxLayout{m_first_steps};
    steps_layout->setContentsMargins(22, 16, 22, 16);
    steps_layout->setSpacing(6);
    auto* steps_head = new QHBoxLayout;
    steps_head->addWidget(SectionTitle(tr("Next steps"), m_first_steps), 1);
    auto* hide_steps = new QPushButton{tr("Hide"), m_first_steps};
    hide_steps->setFlat(true);
    hide_steps->setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; color: %1; min-height: 0; }")
        .arg(CybouTheme::color(CybouTheme::TEXT_MUTED).name()));
    connect(hide_steps, &QPushButton::clicked, this, [this] {
        // Hides the first steps; protective reminders stay until acted on.
        QSettings{}.setValue(QStringLiteral("home/first_steps_hidden"), true);
        rebuildFirstSteps();
    });
    steps_head->addWidget(hide_steps);
    steps_layout->addLayout(steps_head);
    auto* steps_host = new QWidget{m_first_steps};
    m_first_steps_rows = new QVBoxLayout{steps_host};
    m_first_steps_rows->setContentsMargins(0, 0, 0, 0);
    m_first_steps_rows->setSpacing(4);
    steps_layout->addWidget(steps_host);
    root->addWidget(m_first_steps);

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
    const bool show_dashboard = active && !m_onboarding->holdsActiveIdentity();
    m_stack->setCurrentWidget(show_dashboard ? m_dashboard : static_cast<QWidget*>(m_onboarding));
    if (!show_dashboard) {
        m_activity_presentation.clear();
        ClearLayout(m_activity_rows);
        return;
    }

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

    const bool mail_connected = m_model->featureAvailability().mail;
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
    m_files_value->setText(!m_model->featureAvailability().files ? tr("Not connected yet") : (files == 1 ? tr("1 file") : tr("%1 files").arg(files)));
    m_files_caption->setText(tr("%1 used").arg(CybouProduct::sizeText(status.storage_used)));

    m_wallet_value->setText(cybouAmountText(status.balance));
    m_wallet_caption->setText(tr("System Balance %1").arg(cybouAmountText(status.system_balance)));

    auto items = m_model->activity();
    std::stable_sort(items.begin(), items.end(), [](const CybouActivityItem& a, const CybouActivityItem& b) { return a.time > b.time; });
    QStringList presentation;
    for (const auto& item : items.mid(0, 8)) {
        presentation << QString::number(static_cast<int>(item.kind)) << item.title << item.subtitle
                     << item.time.date().toString(Qt::ISODate) << shortTime(item.time);
    }
    if (presentation == m_activity_presentation) { rebuildFirstSteps(); return; }
    m_activity_presentation = presentation;
    ClearLayout(m_activity_rows);
    const QDate today = QDate::currentDate();
    QString group;
    int shown = 0;
    for (const auto& item : items) {
        if (shown == 8) break;
        const QDate day = item.time.date();
        const QString heading = day == today ? tr("Today") : day == today.addDays(-1) ? tr("Yesterday") : tr("Earlier");
        if (heading != group) {
            group = heading;
            auto* label = Eyebrow(heading, m_activity_rows->parentWidget());
            label->setContentsMargins(0, shown == 0 ? 0 : 8, 0, 2);
            m_activity_rows->addWidget(label);
        }
        m_activity_rows->addWidget(ActivityRow(ActivityGlyph(item.kind), ActivityTint(item.kind), item.title, item.subtitle,
            shortTime(item.time), m_activity_rows->parentWidget()));
        ++shown;
    }
    m_activity_empty->setVisible(shown == 0);
    rebuildFirstSteps();
}

namespace {
QString PhraseCheckKey(const CybouDesktopStatus& status)
{
    const auto digest = QCryptographicHash::hash((status.data_directory + QLatin1Char('|') + status.account_id).toUtf8(),
        QCryptographicHash::Sha256).toHex().left(16);
    return QStringLiteral("identity/phrase_checked/") + QString::fromLatin1(digest);
}

/** Typical network fee: median of the latest ten paid (one large file must not skew it); 0 when none yet. */
quint64 LatestFee(const CybouDesktopModel& model)
{
    QList<quint64> fees;
    for (const auto& entry : model.walletEntries()) {
        if (entry.kind != CybouWalletEntryKind::NetworkServiceFee || entry.amount == 0) continue;
        fees << static_cast<quint64>(std::llabs(entry.amount));
        if (fees.size() == 10) break;
    }
    if (fees.isEmpty()) return 0;
    std::sort(fees.begin(), fees.end());
    return fees.at(fees.size() / 2);
}

constexpr int kPhraseCheckDays{90};
constexpr quint64 kLowSystemOperations{25};
} // namespace

QStringList HomePage::reminders() const
{
    QStringList open;
    const auto& status = m_model->status();
    if (m_model->fixtureMode() || status.account_id.isEmpty()) return open;
    const QDateTime checked = QSettings{}.value(PhraseCheckKey(status)).toDateTime();
    if (!checked.isValid() || checked.daysTo(QDateTime::currentDateTime()) > kPhraseCheckDays) open << QStringLiteral("phrase");
    const quint64 fee = LatestFee(*m_model);
    if (fee > 0 && status.system_balance < fee * kLowSystemOperations) open << QStringLiteral("system");
    return open;
}

void HomePage::checkRecoveryPhrase()
{
    QDialog dialog{this};
    dialog.setObjectName(QStringLiteral("phraseCheckDialog"));
    dialog.setWindowTitle(tr("Check your recovery phrase"));
    dialog.setMinimumWidth(440);
    auto* layout = new QVBoxLayout{&dialog};
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(8);
    layout->addWidget(SectionTitle(tr("Check your recovery phrase"), &dialog));
    layout->addWidget(MutedText(tr("Take out your written 24 words. CYBOU asks for three of them to confirm your copy "
                                   "still restores this Identity. The words are never shown here."), &dialog));
    auto* password = new QLineEdit{&dialog};
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(tr("Vault password"));
    password->setMinimumHeight(36);
    layout->addWidget(password);
    // Three distinct positions, different each time.
    QList<int> positions;
    while (positions.size() < 3) {
        const int position = static_cast<int>(QRandomGenerator::global()->bounded(24));
        if (!positions.contains(position)) positions << position;
    }
    std::sort(positions.begin(), positions.end());
    QList<QLineEdit*> inputs;
    for (const int position : positions) {
        auto* input = new QLineEdit{&dialog};
        input->setPlaceholderText(tr("Word #%1").arg(position + 1));
        input->setMinimumHeight(36);
        layout->addWidget(input);
        inputs << input;
    }
    auto* result = MutedText({}, &dialog);
    layout->addWidget(result);
    auto* buttons = new QDialogButtonBox{&dialog};
    auto* check = buttons->addButton(tr("Check"), QDialogButtonBox::AcceptRole);
    check->setObjectName(QStringLiteral("primaryButton"));
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(check, &QPushButton::clicked, &dialog, [this, &dialog, password, inputs, positions, result, check] {
        check->setEnabled(false);
        result->setText(tr("Checking…"));
        QStringList typed;
        for (auto* input : inputs) typed << input->text().trimmed().toLower();
        QPointer<QDialog> guard{&dialog};
        m_model->revealRecoveryWordsAsync(password->text(), [this, guard, typed, positions, result, check](
                                                               std::optional<QStringList> words) mutable {
            if (!guard) {
                if (words) for (auto& word : *words) word.fill(QChar{0});
                return;
            }
            check->setEnabled(true);
            if (!words) {
                result->setText(tr("The vault password is incorrect."));
                return;
            }
            bool match = words->size() == 24;
            for (int i = 0; match && i < positions.size(); ++i) match = words->at(positions.at(i)) == typed.at(i);
            for (auto& word : *words) word.fill(QChar{0});
            if (!match) {
                result->setText(tr("These words do not match. Check your written copy; if it is lost, "
                                   "show the phrase in Identity & Security and write it down again."));
                return;
            }
            QSettings{}.setValue(PhraseCheckKey(m_model->status()), QDateTime::currentDateTime());
            m_model->notify(tr("Recovery phrase checked. Keep your copy safe."));
            guard->accept();
        });
        password->clear();
    });
    dialog.exec();
    rebuildFirstSteps();
}

QStringList HomePage::openFirstSteps() const
{
    QStringList open;
    if (m_model->names().isEmpty()) open << QStringLiteral("name");
    const bool sent = std::any_of(m_model->mailItems().begin(), m_model->mailItems().end(),
        [](const CybouMailItem& item) { return item.folder == CybouMailFolder::Sent || (item.outgoing && !item.draft); });
    if (!sent) open << QStringLiteral("mail");
    const bool file = std::any_of(m_model->fileItems().begin(), m_model->fileItems().end(),
        [](const CybouFileItem& item) { return !item.folder; });
    if (!file) open << QStringLiteral("files");
    return open;
}

void HomePage::rebuildFirstSteps()
{
    ClearLayout(m_first_steps_rows);
    const bool hidden = QSettings{}.value(QStringLiteral("home/first_steps_hidden"), false).toBool();
    const auto open = hidden ? QStringList{} : openFirstSteps();
    const auto reminders = this->reminders();
    m_first_steps->setVisible(!open.isEmpty() || !reminders.isEmpty());
    if (!m_first_steps->isVisible()) return;
    const auto step = [this](const QString& title, const QString& subtitle, const QString& action, std::function<void()> fn) {
        auto* row = new QWidget{m_first_steps_rows->parentWidget()};
        auto* layout = new QHBoxLayout{row};
        layout->setContentsMargins(0, 4, 0, 4);
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        auto* t = new QLabel{title, row};
        t->setObjectName(QStringLiteral("rowTitle"));
        auto* s = new QLabel{subtitle, row};
        s->setObjectName(QStringLiteral("rowSub"));
        text->addWidget(t);
        text->addWidget(s);
        layout->addLayout(text, 1);
        auto* button = new QPushButton{action, row};
        button->setObjectName(QStringLiteral("secondaryButton"));
        connect(button, &QPushButton::clicked, row, [fn = std::move(fn)] { if (fn) fn(); });
        layout->addWidget(button);
        m_first_steps_rows->addWidget(row);
    };
    // Reminders first: they protect what the user already has.
    if (reminders.contains(QStringLiteral("phrase"))) {
        step(tr("Check your recovery phrase"),
            tr("Your 24 words are the only way back if this computer is lost. Confirm your copy."), tr("Check"),
            [this] { checkRecoveryPhrase(); });
    }
    if (reminders.contains(QStringLiteral("system"))) {
        const quint64 fee = LatestFee(*m_model);
        step(tr("System Balance is running low"),
            tr("It covers about %1 more network operations for Mail, Files and payments.")
                .arg(QLocale{}.toString(fee > 0 ? m_model->status().system_balance / fee : 0)),
            tr("Wallet"), [this] { m_wallet_requested(); });
    }
    if (open.contains(QStringLiteral("name"))) {
        const auto& status = m_model->status();
        if (status.name_claim_pending) {
            step(tr("Claiming your .cybou name"), status.name_claim_status.isEmpty()
                    ? tr("Waiting for the network to confirm it.") : status.name_claim_status, tr("Details"),
                [this] { m_identity_requested(); });
        } else {
            step(tr("Claim your .cybou name"), tr("People reach you as name.cybou instead of a long ID."), tr("Claim"),
                [this] { m_identity_requested(); });
        }
    }
    if (open.contains(QStringLiteral("mail")))
        step(tr("Send your first message"), tr("Mail is end-to-end encrypted and post-quantum protected."), tr("Compose"),
            [this] { if (onCompose) onCompose(); });
    if (open.contains(QStringLiteral("files")))
        step(tr("Upload your first file"), tr("Files are encrypted on this computer before they leave it."), tr("Upload"),
            [this] { if (onUpload) onUpload(); });
}
