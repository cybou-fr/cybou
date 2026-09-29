// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/walletpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QClipboard>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStandardItemModel>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

constexpr int kNameRole = Qt::UserRole + 1;

QString EntryTitle(const CybouWalletEntry& entry)
{
    const QString who = entry.counterparty_name.endsWith(QStringLiteral(".cybou"))
        ? entry.counterparty_name : CybouProduct::shortId(entry.counterparty_name);
    switch (entry.kind) {
    case CybouWalletEntryKind::Received: return WalletPage::tr("Received from %1").arg(who);
    case CybouWalletEntryKind::Sent: return WalletPage::tr("Sent to %1").arg(who);
    case CybouWalletEntryKind::NetworkServiceFee: return WalletPage::tr("Network service fee");
    case CybouWalletEntryKind::OnboardingCredit: return WalletPage::tr("Onboarding credit");
    case CybouWalletEntryKind::MovedToSystemBalance: return WalletPage::tr("Moved to System Balance");
    }
    return {};
}

Glyph EntryGlyph(const CybouWalletEntry& entry)
{
    switch (entry.kind) {
    case CybouWalletEntryKind::Received: return Glyph::ArrowDownLeft;
    case CybouWalletEntryKind::Sent: return Glyph::ArrowUpRight;
    case CybouWalletEntryKind::NetworkServiceFee: return Glyph::Database;
    case CybouWalletEntryKind::OnboardingCredit: return Glyph::Sparkles;
    case CybouWalletEntryKind::MovedToSystemBalance: return Glyph::Lock;
    }
    return Glyph::Info;
}

QLabel* BigAmount(QWidget* parent)
{
    auto* label = new QLabel{parent};
    label->setObjectName(QStringLiteral("heroTitleBig"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

} // namespace

WalletPage::WalletPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    setMinimumWidth(0);
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(16);

    // Balances.
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(28, 22, 28, 22);
    hero_layout->setSpacing(28);
    auto* available = new QVBoxLayout;
    available->setSpacing(2);
    available->addWidget(Eyebrow(tr("AVAILABLE"), hero));
    m_available = BigAmount(hero);
    available->addWidget(m_available);
    hero_layout->addLayout(available, 1);
    auto* system = new QVBoxLayout;
    system->setSpacing(2);
    system->addWidget(Eyebrow(tr("SYSTEM BALANCE"), hero));
    m_system = new QLabel{hero};
    m_system->setObjectName(QStringLiteral("metric"));
    system->addWidget(m_system);
    system->addWidget(MutedText(tr("For CYBOU network services"), hero));
    hero_layout->addLayout(system, 1);
    auto* actions = new QVBoxLayout;
    actions->setSpacing(8);
    m_send_button = new QPushButton{tr("Send"), hero};
    m_send_button->setObjectName(QStringLiteral("primaryButton"));
    m_send_button->setProperty("cybouId", QStringLiteral("walletSend"));
    m_send_button->setIcon(QIcon{glyphPixmap(Glyph::ArrowUpRight, {16, 16}, QColor{Qt::white})});
    m_receive_button = new QPushButton{tr("Receive"), hero};
    m_receive_button->setObjectName(QStringLiteral("secondaryButton"));
    m_receive_button->setProperty("cybouId", QStringLiteral("walletReceive"));
    m_receive_button->setIcon(QIcon{glyphPixmap(Glyph::ArrowDownLeft, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    actions->addWidget(m_send_button);
    actions->addWidget(m_receive_button);
    hero_layout->addLayout(actions);
    root->addWidget(hero);

    m_gate = MutedText({}, this);
    root->addWidget(m_gate);

    // Send panel.
    m_send_panel = Card(this);
    m_send_panel->setObjectName(QStringLiteral("card"));
    auto* send = new QVBoxLayout{m_send_panel};
    send->setContentsMargins(24, 18, 24, 18);
    send->setSpacing(8);
    send->addWidget(SectionTitle(tr("Send CYBOU"), m_send_panel));
    auto* to_label = new QLabel{tr("To"), m_send_panel};
    to_label->setObjectName(QStringLiteral("cardLabel"));
    send->addWidget(to_label);
    m_to = new QLineEdit{m_send_panel};
    m_to->setObjectName(QStringLiteral("walletTo"));
    m_to->setPlaceholderText(tr("name.cybou"));
    m_to->setAccessibleName(tr("To"));
    m_to->setMinimumHeight(38);
    send->addWidget(m_to);
    m_to_hint = MutedText({}, m_send_panel);
    send->addWidget(m_to_hint);
    m_completer = new QCompleter{this};
    m_completer->setModel(new QStandardItemModel{m_completer});
    m_completer->setCompletionRole(kNameRole);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_to->setCompleter(m_completer);
    connect(m_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this,
        [this](const QModelIndex& index) { m_to->setText(index.data(kNameRole).toString()); });
    auto* amount_label = new QLabel{tr("Amount"), m_send_panel};
    amount_label->setObjectName(QStringLiteral("cardLabel"));
    send->addWidget(amount_label);
    m_amount = new QLineEdit{m_send_panel};
    m_amount->setObjectName(QStringLiteral("walletAmount"));
    m_amount->setPlaceholderText(tr("Whole CYBOU"));
    m_amount->setAccessibleName(tr("Amount in CYBOU"));
    m_amount->setMinimumHeight(38);
    m_amount->setValidator(new QRegularExpressionValidator{QRegularExpression{QStringLiteral("[0-9]{1,11}")}, m_amount});
    send->addWidget(m_amount);
    m_fee = MutedText({}, m_send_panel);
    send->addWidget(m_fee);
    m_review = new QLabel{m_send_panel};
    m_review->setObjectName(QStringLiteral("walletReview"));
    m_review->setWordWrap(true);
    m_review->setStyleSheet(QStringLiteral("QLabel#walletReview { background: %1; border-radius: 10px; padding: 12px; color: %2; }")
        .arg(CybouTheme::color(CybouTheme::MINT_GHOST).name(), CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    m_review->hide();
    send->addWidget(m_review);
    m_send_status = MutedText({}, m_send_panel);
    send->addWidget(m_send_status);
    auto* send_buttons = new QHBoxLayout;
    m_confirm = new QPushButton{tr("Send"), m_send_panel};
    m_confirm->setObjectName(QStringLiteral("primaryButton"));
    m_confirm->setProperty("cybouId", QStringLiteral("walletConfirm"));
    auto* cancel = new QPushButton{tr("Cancel"), m_send_panel};
    cancel->setObjectName(QStringLiteral("secondaryButton"));
    send_buttons->addWidget(m_confirm);
    send_buttons->addWidget(cancel);
    send_buttons->addStretch();
    send->addLayout(send_buttons);
    m_send_panel->setVisible(false);
    root->addWidget(m_send_panel);

    // Recent activity.
    auto* activity = Card(this);
    auto* activity_layout = new QVBoxLayout{activity};
    activity_layout->setContentsMargins(22, 18, 22, 14);
    activity_layout->setSpacing(4);
    activity_layout->addWidget(SectionTitle(tr("Recent activity"), activity));
    auto* rows = new QWidget{activity};
    m_activity_rows = new QVBoxLayout{rows};
    m_activity_rows->setContentsMargins(0, 6, 0, 0);
    m_activity_rows->setSpacing(2);
    activity_layout->addWidget(rows);
    m_activity_empty = MutedText(tr("No activity yet."), activity);
    activity_layout->addWidget(m_activity_empty);
    root->addWidget(activity);
    root->addStretch();

    connect(m_send_button, &QPushButton::clicked, this, [this] { openSend(); });
    connect(m_receive_button, &QPushButton::clicked, this, [this] { showReceive(); });
    connect(cancel, &QPushButton::clicked, this, [this] {
        if (m_reviewing) {
            setReviewing(false);
            return;
        }
        m_send_panel->setVisible(false);
        m_to->clear();
        m_amount->clear();
    });
    connect(m_confirm, &QPushButton::clicked, this, [this] { submit(); });
    connect(m_to, &QLineEdit::textChanged, this, [this] { updateSendState(); });
    connect(m_amount, &QLineEdit::textChanged, this, [this] { updateSendState(); });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::walletChanged, this, [this] { rebuildActivity(); });
    connect(m_model, &CybouDesktopModel::paymentFinished, this, [this](bool ok, const QString& error) {
        if (ok) {
            m_model->notify(tr("Payment sent"));
            m_send_status->setText(tr("Sent. It appears in your activity."));
            m_to->clear();
            m_amount->clear();
        } else {
            m_send_status->setText(error.isEmpty() ? tr("The payment could not be sent.") : error);
        }
        updateSendState();
    });
    refresh();
    rebuildActivity();
}

void WalletPage::openSend(const QString& to)
{
    auto* model = static_cast<QStandardItemModel*>(m_completer->model());
    model->clear();
    for (const auto& contact : m_model->contacts()) {
        auto* item = new QStandardItem{QStringLiteral("%1  ·  %2").arg(contact.display_name, contact.name)};
        item->setData(contact.name, kNameRole);
        model->appendRow(item);
    }
    m_send_panel->setVisible(true);
    m_send_status->clear();
    m_to->setText(to);
    (to.isEmpty() ? m_to : m_amount)->setFocus();
    updateSendState();
}

void WalletPage::refresh()
{
    const auto& status = m_model->status();
    const bool active = status.identity_state == CybouIdentityState::Active;
    m_available->setText(cybouAmountText(status.balance));
    m_system->setText(cybouAmountText(status.system_balance));
    const bool payments = active && m_model->capabilities().payments;
    m_send_button->setEnabled(payments);
    m_receive_button->setEnabled(active);
    m_gate->setText(!active ? tr("Wallet needs your CYBOU Identity. Create or restore it on Home.")
        : !payments ? tr("Payments are not connected yet.") : QString{});
    m_gate->setVisible(!m_gate->text().isEmpty());
    if (!payments) m_send_panel->setVisible(false);
    updateSendState();
}

void WalletPage::updateSendState()
{
    const QString to = m_to->text().trimmed().toLower();
    const quint64 amount = m_amount->text().toULongLong();
    QString to_problem;
    if (!to.isEmpty()) {
        if (!to.endsWith(QStringLiteral(".cybou"))) to_problem = tr("Use a CYBOU name, for example alice.cybou.");
        else if (to == m_model->status().primary_name) to_problem = tr("You cannot send CYBOU to yourself.");
        else to_problem = m_model->nameLabelProblem(to.chopped(6));
    }
    QString hint = to_problem;
    if (hint.isEmpty() && !to.isEmpty()) {
        for (const auto& contact : m_model->contacts()) {
            if (contact.name == to) hint = tr("%1  ·  Verified identity").arg(contact.display_name);
        }
    }
    m_to_hint->setText(hint);
    const auto fee = m_model->paymentFee();
    m_fee->setText(fee ? tr("Network service fee: %1 (from System Balance)").arg(cybouAmountText(*fee))
                       : tr("Network service fee: calculated when sending"));
    const bool enough = amount <= m_model->status().balance;
    if (amount > 0 && !enough) m_send_status->setText(tr("Not enough CYBOU available."));
    m_confirm->setEnabled(!m_model->paymentPending() && !to.isEmpty() && to_problem.isEmpty() && amount > 0 &&
        enough && m_model->capabilities().payments);
    m_confirm->setText(m_model->paymentPending() ? tr("Sending…") : m_reviewing ? tr("Confirm and send") : tr("Review"));
}

void WalletPage::setReviewing(bool reviewing)
{
    m_reviewing = reviewing;
    m_to->setEnabled(!reviewing);
    m_amount->setEnabled(!reviewing);
    m_review->setVisible(reviewing);
    if (reviewing) {
        const QString to = m_to->text().trimmed().toLower();
        const quint64 amount = m_amount->text().toULongLong();
        const auto fee = m_model->paymentFee();
        m_review->setText(tr("<b>Send %1 to %2</b><br>Network service fee: %3 from System Balance<br>"
                             "Payments cannot be reversed.")
            .arg(cybouAmountText(amount), to.toHtmlEscaped(), fee ? cybouAmountText(*fee) : tr("calculated when sending")));
    }
    updateSendState();
}

void WalletPage::submit()
{
    // First click reviews; second click sends.
    if (!m_reviewing) {
        setReviewing(true);
        return;
    }
    const QString to = m_to->text().trimmed().toLower();
    const quint64 amount = m_amount->text().toULongLong();
    setReviewing(false);
    if (m_model->requestPayment(to, amount)) {
        m_send_status->setText(tr("Sending %1 to %2…").arg(cybouAmountText(amount), to));
    } else {
        m_send_status->setText(tr("The payment could not be started."));
    }
    updateSendState();
}

void WalletPage::showReceive()
{
    const auto& status = m_model->status();
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Receive CYBOU"));
    dialog.setMinimumWidth(460);
    auto* layout = new QVBoxLayout{&dialog};
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(10);
    layout->addWidget(SectionTitle(tr("Receive CYBOU"), &dialog));
    layout->addWidget(MutedText(tr("Share your CYBOU name. Payments to it arrive in your Available balance."), &dialog));
    auto* name = HeroTitle(status.primary_name.isEmpty() ? CybouProduct::shortId(status.account_id) : status.primary_name, &dialog);
    name->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(name);
    auto* copy = new QPushButton{tr("Copy"), &dialog};
    copy->setObjectName(QStringLiteral("secondaryButton"));
    const QString value = status.primary_name.isEmpty() ? status.account_id : status.primary_name;
    connect(copy, &QPushButton::clicked, &dialog, [value] { QGuiApplication::clipboard()->setText(value); });
    layout->addWidget(copy, 0, Qt::AlignLeft);
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Close, &dialog};
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void WalletPage::rebuildActivity()
{
    while (QLayoutItem* item = m_activity_rows->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    int shown = 0;
    for (const auto& entry : m_model->walletEntries()) {
        if (shown == 12) break;
        const QString sign = entry.amount >= 0 ? QStringLiteral("+") : QStringLiteral("−");
        const QString amount = sign + cybouAmountText(static_cast<quint64>(std::llabs(entry.amount)));
        const QString subtitle = entry.pending ? tr("Waiting for confirmation")
            : entry.system_side ? tr("System Balance") : tr("Available");
        auto* row = ActivityRow(EntryGlyph(entry),
            entry.amount >= 0 ? Tint::Mint : Tint::Indigo, EntryTitle(entry), subtitle,
            amount + QStringLiteral("  ·  ") + relTime(entry.time), m_activity_rows->parentWidget());
        if (auto* meta = row->findChild<QLabel*>(QStringLiteral("rowMeta")); meta && entry.amount > 0) {
            meta->setStyleSheet(QStringLiteral("background: transparent; border: none; color: %1; font-weight: 700;")
                .arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
        }
        m_activity_rows->addWidget(row);
        ++shown;
    }
    m_activity_empty->setVisible(shown == 0);
}
