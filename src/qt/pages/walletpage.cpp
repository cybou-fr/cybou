// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/walletpage.h>

#include <cybou/wallet_service.h>
#include <cybou/hex.h>
#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QBrush>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

QLabel* noteLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    return label;
}

/** One of the two balance cards (Available / System). */
QWidget* balanceCard(Glyph glyph, Tint tint, const QString& pill_text, Tint pill_tint,
    const QString& caption, const QString& info_tip, QLabel*& metric_out, QLabel*& caption_out, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(8);
    auto* header = new QHBoxLayout;
    header->addWidget(Chip(glyph, tint, card, 38, 19));
    auto* title = new QLabel{caption, card};
    title->setObjectName(QStringLiteral("serviceTitle"));
    header->addWidget(title, 0, Qt::AlignVCenter);
    if (!info_tip.isEmpty()) {
        auto* info = new QLabel{card};
        info->setPixmap(glyphPixmap(Glyph::Info, {14, 14}, CybouTheme::color(CybouTheme::DIM)));
        info->setToolTip(info_tip);
        header->addWidget(info, 0, Qt::AlignVCenter);
    }
    header->addStretch();
    header->addWidget(Pill(pill_text, pill_tint, card), 0, Qt::AlignVCenter);
    layout->addLayout(header);
    auto* metric = new QLabel{card};
    metric->setObjectName(QStringLiteral("metric"));
    metric_out = metric;
    layout->addWidget(metric);
    auto* body = noteLabel({}, card);
    caption_out = body;
    layout->addWidget(body);
    return card;
}

/** Clickable quick-action row (sketch: chevron rows that open the action). */
QWidget* quickActionRow(Glyph glyph, Tint tint, const QString& name, const QString& sub,
    std::function<void()> action, QWidget* parent)
{
    auto* button = new QPushButton{parent};
    button->setFlat(true);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { border: none; background: transparent; text-align: left; padding: 6px 0; }"
        "QPushButton:hover { background: transparent; }"));
    auto* row = new QHBoxLayout{button};
    row->setContentsMargins(0, 2, 0, 2);
    row->setSpacing(12);
    auto* chip = Chip(glyph, tint, button, 36, 18);
    chip->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(chip, 0, Qt::AlignVCenter);
    auto* text = new QVBoxLayout;
    text->setSpacing(0);
    auto* name_label = new QLabel{name, button};
    name_label->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
        .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    name_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* sub_label = new QLabel{sub, button};
    sub_label->setObjectName(QStringLiteral("rowSub"));
    sub_label->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    sub_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    text->addWidget(name_label);
    text->addWidget(sub_label);
    row->addLayout(text, 1);
    auto* chevron = new QLabel{button};
    chevron->setPixmap(glyphPixmap(Glyph::ChevronRight, {16, 16}, CybouTheme::color(CybouTheme::DIM)));
    chevron->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(chevron, 0, Qt::AlignVCenter);
    QObject::connect(button, &QPushButton::clicked, button, [action = std::move(action)] { action(); });
    return button;
}

} // namespace

WalletPage::WalletPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent},
      m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    // ---- Hero ---------------------------------------------------------------
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QHBoxLayout{hero};
    hero_layout->setContentsMargins(30, 26, 30, 26);
    hero_layout->setSpacing(24);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(8);
    hero_text->addWidget(Eyebrow(tr("WALLET"), hero));
    auto* hero_title = HeroTitle(tr("Your CYBOU wallet"), hero, true);
    hero_text->addWidget(hero_title);
    auto* hero_tag = new QLabel{tr("Two balances. One identity."), hero};
    hero_tag->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: 750; color: %1; background: transparent; border: none;")
        .arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
    hero_text->addWidget(hero_tag);
    hero_text->addWidget(HeroSubtitle(tr("Use CYBOU for payments and transfers, and to fund your communication services. Both balances work together to keep your digital life running."), hero));
    hero_text->addStretch();
    hero_layout->addLayout(hero_text, 1);
    auto* hero_art = new QLabel{hero};
    hero_art->setFixedSize(96, 96);
    hero_art->setAlignment(Qt::AlignCenter);
    hero_art->setPixmap(glyphPixmap(Glyph::WalletCard, {72, 72}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    hero_layout->addWidget(hero_art, 0, Qt::AlignVCenter);
    root->addWidget(hero);

    // ---- Balance cards -------------------------------------------------------
    auto* balances = new QHBoxLayout;
    balances->setSpacing(14);

    auto* available = balanceCard(Glyph::WalletCard, Tint::Mint, tr("For payments and transfers"), Tint::Mint,
        tr("Available balance"),
        tr("Your transferable Balance. Send payments to other accounts or lock CYBOU into System Balance to fund services."),
        m_available_metric, m_available_caption, this);
    auto* available_actions = new QHBoxLayout;
    m_send = new QPushButton{tr("Send"), available};
    m_send->setObjectName(QStringLiteral("primaryButton"));
    m_send->setIcon(QIcon{glyphPixmap(Glyph::ArrowUpRight, {16, 16}, QColor{0xffffff})});
    connect(m_send, &QPushButton::clicked, this, [this] { onSendClicked(); });
    m_receive = new QPushButton{tr("Receive"), available};
    m_receive->setObjectName(QStringLiteral("softButton"));
    m_receive->setIcon(QIcon{glyphPixmap(Glyph::ArrowDownLeft, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    connect(m_receive, &QPushButton::clicked, this, [this] { onReceiveClicked(); });
    m_lock = new QPushButton{tr("Transfer"), available};
    m_lock->setObjectName(QStringLiteral("secondaryButton"));
    m_lock->setIcon(QIcon{glyphPixmap(Glyph::Transfer, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    m_lock->setProperty("irreversible", true);
    connect(m_lock, &QPushButton::clicked, this, [this] { onLockClicked(); });
    available_actions->addWidget(m_send);
    available_actions->addWidget(m_receive);
    available_actions->addWidget(m_lock);
    available_actions->addStretch();
    qobject_cast<QVBoxLayout*>(available->layout())->addLayout(available_actions);
    balances->addWidget(available, 1);

    auto* system = balanceCard(Glyph::Database, Tint::Indigo, tr("Funds services and protocol"), Tint::Indigo,
        tr("System balance"),
        tr("Service budget for Email, Storage and Backup fees plus protocol fees. Locked CYBOU cannot be transferred back."),
        m_system_metric, m_system_caption, this);
    auto* system_actions = new QHBoxLayout;
    auto* fund = new QPushButton{tr("Fund services"), system};
    fund->setObjectName(QStringLiteral("primaryButton"));
    fund->setIcon(QIcon{glyphPixmap(Glyph::Database, {16, 16}, QColor{0xffffff})});
    connect(fund, &QPushButton::clicked, this, [this] { onLockClicked(); });
    auto* system_transfer = new QPushButton{tr("Transfer"), system};
    system_transfer->setObjectName(QStringLiteral("secondaryButton"));
    connect(system_transfer, &QPushButton::clicked, this, [this] { onLockClicked(); });
    m_view_usage = new QPushButton{tr("View usage"), system};
    m_view_usage->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_view_usage, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Service usage"),
            tr("A per-service usage breakdown (Email fees today, Storage and Backup in the future) arrives with the protocol fee feed."));
    });
    system_actions->addWidget(fund);
    system_actions->addWidget(system_transfer);
    system_actions->addWidget(m_view_usage);
    system_actions->addStretch();
    qobject_cast<QVBoxLayout*>(system->layout())->addLayout(system_actions);
    balances->addWidget(system, 1);
    root->addLayout(balances);

    // ---- Transactions + quick actions ----------------------------------------
    auto* bottom_row = new QHBoxLayout;
    bottom_row->setSpacing(14);

    auto* transactions = Card(this);
    auto* transactions_layout = new QVBoxLayout{transactions};
    transactions_layout->setContentsMargins(22, 18, 22, 18);
    transactions_layout->setSpacing(8);
    QLabel* link = nullptr;
    transactions_layout->addLayout(SectionHeader(tr("Recent transactions"), {}, link, transactions));
    m_activity = new QListWidget{transactions};
    m_activity->setObjectName(QStringLiteral("messageList"));
    m_activity->setFocusPolicy(Qt::NoFocus);
    transactions_layout->addWidget(m_activity, 1);
    bottom_row->addWidget(transactions, 3);

    auto* quick = Card(this);
    auto* quick_layout = new QVBoxLayout{quick};
    quick_layout->setContentsMargins(22, 18, 22, 18);
    quick_layout->setSpacing(4);
    quick_layout->addWidget(SectionTitle(tr("Quick actions"), quick));
    quick_layout->addWidget(quickActionRow(Glyph::ArrowUpRight, Tint::Mint,
        tr("Send CYBOU"), tr("Pay another user"), [this] { onSendClicked(); }, quick));
    quick_layout->addWidget(quickActionRow(Glyph::ArrowDownLeft, Tint::Blue,
        tr("Receive CYBOU"), tr("Get paid by others"), [this] { onReceiveClicked(); }, quick));
    quick_layout->addWidget(quickActionRow(Glyph::Database, Tint::Indigo,
        tr("Fund services"), tr("Add to System Balance"), [this] { onLockClicked(); }, quick));
    quick_layout->addWidget(quickActionRow(Glyph::Transfer, Tint::Violet,
        tr("Transfer between balances"), tr("One-way lock, cannot be reversed"), [this] { onLockClicked(); }, quick));
    bottom_row->addWidget(quick, 2);
    root->addLayout(bottom_row, 1);

    m_gate_hint = noteLabel({}, this);
    root->addWidget(m_gate_hint);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); syncLedger(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); syncLedger(); });

    auto* sync_timer = new QTimer{this};
    connect(sync_timer, &QTimer::timeout, this, [this] { syncLedger(); });
    sync_timer->start(3000);

    refresh();
    syncLedger();
}

QString WalletPage::kindText(EntryKind kind)
{
    switch (kind) {
    case EntryKind::OnboardingBonus: return tr("Onboarding bonus");
    case EntryKind::MailFee: return tr("Email fee");
    case EntryKind::Payment: return tr("Payment");
    case EntryKind::LockToSystem: return tr("Lock to System Balance");
    }
    return {};
}

QString WalletPage::finalityText(EntryFinality finality)
{
    switch (finality) {
    case EntryFinality::Pending: return tr("Pending BFT finality");
    case EntryFinality::Final: return tr("Final");
    }
    return {};
}

void WalletPage::refresh()
{
    const auto& status = m_model->status();
    m_available_metric->setText(cybouAmountText(status.balance));
    m_system_metric->setText(cybouAmountText(status.system_balance));
    m_available_caption->setText(tr("Use your available balance to send payments, receive funds and transfer to other people."));
    m_system_caption->setText(tr("Used to pay for your CYBOU services (Email, Storage, Backup) and protocol fees. Locked CYBOU cannot be transferred back."));

    const bool usable = status.identity_state == CybouIdentityState::Active &&
                        m_model->capabilities().payments;
    for (auto* btn : findChildren<QPushButton*>()) {
        btn->setEnabled(usable);
    }
    m_gate_hint->setText(status.identity_state != CybouIdentityState::Active
        ? tr("Create an identity to receive the onboarding bonus and use your wallet.")
        : usable ? QString{}
                 : tr("Transfers become available once the node reports the payments capability."));
    rebuildActivity();
}

void WalletPage::rebuildActivity()
{
    m_activity->clear();
    for (const Entry& entry : m_entries) {
        auto* row = new QFrame{m_activity};
        auto* row_layout = new QHBoxLayout{row};
        row_layout->setContentsMargins(10, 9, 10, 9);
        row_layout->setSpacing(12);

        Glyph glyph = Glyph::Transfer;
        Tint tint = Tint::Neutral;
        QString title = kindText(entry.kind);
        QString subtitle;
        switch (entry.kind) {
        case EntryKind::Payment:
            glyph = entry.amount >= 0 ? Glyph::ArrowDownLeft : Glyph::ArrowUpRight;
            tint = entry.amount >= 0 ? Tint::Mint : Tint::Indigo;
            title = entry.amount >= 0
                ? tr("Received from %1").arg(entry.counterparty)
                : tr("Sent to %1").arg(entry.counterparty);
            break;
        case EntryKind::OnboardingBonus:
            glyph = Glyph::Sparkles;
            tint = Tint::Amber;
            subtitle = tr("Welcome to CYBOU");
            break;
        case EntryKind::MailFee:
            glyph = Glyph::Envelope;
            tint = Tint::Blue;
            subtitle = tr("CYBOU Mail");
            break;
        case EntryKind::LockToSystem:
            glyph = Glyph::Lock;
            tint = Tint::Violet;
            subtitle = tr("Funding services");
            break;
        }
        if (subtitle.isEmpty() && !entry.counterparty.isEmpty()) subtitle = entry.counterparty;
        subtitle = tr("%1 \u00b7 %2").arg(subtitle, QLocale{}.toString(entry.at, QLocale::ShortFormat));

        row_layout->addWidget(Chip(glyph, tint, row, 36, 18), 0, Qt::AlignTop);
        auto* main = new QVBoxLayout;
        main->setSpacing(2);
        auto* title_label = new QLabel{title, row};
        title_label->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
            .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
        auto* sub = new QLabel{subtitle, row};
        sub->setObjectName(QStringLiteral("rowSub"));
        sub->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        main->addWidget(title_label);
        main->addWidget(sub);
        row_layout->addLayout(main, 1);

        auto* side = new QVBoxLayout;
        side->setSpacing(4);
        auto* amount = new QLabel{(entry.amount >= 0 ? QStringLiteral("+") : QStringLiteral("\u2212")) +
                                      cybouAmountText(static_cast<quint64>(qAbs<qint64>(entry.amount))),
            row};
        amount->setStyleSheet(QStringLiteral("font-weight: 800; background: transparent; border: none; color: %1;")
            .arg(entry.amount >= 0 ? CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()
                                   : CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
        side->addWidget(amount, 0, Qt::AlignRight);
        auto* badge = new QLabel{finalityText(entry.finality), row};
        badge->setObjectName(entry.finality == EntryFinality::Final
            ? QStringLiteral("pill")
            : QStringLiteral("pill"));
        badge->setProperty("tint", entry.finality == EntryFinality::Final ? "mint" : "neutral");
        side->addWidget(badge, 0, Qt::AlignRight);
        row_layout->addLayout(side);

        auto* item = new QListWidgetItem{m_activity};
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 56}));
        m_activity->setItemWidget(item, row);
    }
    if (m_activity->count() == 0) {
        auto* item = new QListWidgetItem{m_activity};
        item->setFlags(Qt::NoItemFlags);
        item->setText(tr("No transactions yet.\nOnboarding bonus, Email fees and transfers will appear here once your account is active."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 120});
    }
}

void WalletPage::onSendClicked()
{
    auto* service = m_model->walletService();
    if (!service) {
        actionNotWired();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Send CYBOU"));
    dialog.setMinimumWidth(440);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);

    auto* desc = noteLabel(tr("Enter the recipient Account ID (hex) and the amount of whole CYBOU to transfer. Debits user balance only; deterministic fee of 1 CYBOU is paid from System Balance."), &dialog);
    layout->addWidget(desc);

    auto* form = new QFormLayout;
    form->setSpacing(12);

    auto* recipient_edit = new QLineEdit(&dialog);
    recipient_edit->setPlaceholderText(tr("Recipient Account ID (64-character hex)"));
    form->addRow(tr("Recipient:"), recipient_edit);

    auto* amount_spin = new QSpinBox(&dialog);
    amount_spin->setRange(1, 1'000'000'000);
    amount_spin->setValue(10);
    amount_spin->setSuffix(QStringLiteral(" CYBOU"));
    form->addRow(tr("Amount:"), amount_spin);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString to_str = recipient_edit->text().trimmed();
    const auto rec_u256 = cybou::ParseUint256UserHex(to_str.toStdString());
    if (!rec_u256 || rec_u256->IsNull()) {
        QMessageBox::warning(this, tr("Invalid Recipient"), tr("Please enter a valid 64-character hex Account ID."));
        return;
    }

    const cybou::AccountId recipient{*rec_u256};
    const uint64_t amount = static_cast<uint64_t>(amount_spin->value());

    const auto res = service->SendPayment(recipient, amount);
    if (!res) {
        QMessageBox::warning(this, tr("Payment Failed"), tr("Payment failed: %1").arg(QString::fromStdString(res.error_message)));
        return;
    }

    QMessageBox::information(this, tr("Payment Submitted"), tr("Payment of %1 submitted to the network. BFT finality will confirm it shortly.").arg(cybouAmountText(amount)));
    syncLedger();
    refresh();
}

void WalletPage::onLockClicked()
{
    auto* service = m_model->walletService();
    if (!service) {
        actionNotWired();
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Transfer to System Balance"));
    dialog.setMinimumWidth(440);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);

    auto* warning = new QLabel(tr("<b>One-way transfer.</b><br>Moving Balance into System Balance permanently assigns it to protocol services (such as Email fees). System Balance cannot be transferred, traded, or converted back to Balance."), &dialog);
    warning->setWordWrap(true);
    warning->setStyleSheet(QStringLiteral("color: #b45309;"));
    layout->addWidget(warning);

    auto* form = new QFormLayout;
    form->setSpacing(12);

    auto* amount_spin = new QSpinBox(&dialog);
    amount_spin->setRange(1, 1'000'000'000);
    amount_spin->setValue(50);
    amount_spin->setSuffix(QStringLiteral(" CYBOU"));
    form->addRow(tr("Amount to transfer:"), amount_spin);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Confirm one-way transfer"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const uint64_t amount = static_cast<uint64_t>(amount_spin->value());
    const auto res = service->LockToSystemBalance(amount);
    if (!res) {
        QMessageBox::warning(this, tr("Transfer Failed"), tr("Transfer failed: %1").arg(QString::fromStdString(res.error_message)));
        return;
    }

    QMessageBox::information(this, tr("Transfer Submitted"), tr("Transfer of %1 submitted to the network. BFT finality will confirm it shortly.").arg(cybouAmountText(amount)));
    syncLedger();
    refresh();
}

void WalletPage::onReceiveClicked()
{
    const auto& status = m_model->status();
    if (status.identity_state != CybouIdentityState::Active || status.account_id.isEmpty()) {
        QMessageBox::information(this, tr("No Active Identity"), tr("Create an active CYBOU identity before receiving payments."));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Receive CYBOU"));
    dialog.setMinimumWidth(480);

    auto* layout = new QVBoxLayout(&dialog);
    layout->setSpacing(16);

    auto* desc = noteLabel(tr("Give your Account ID to the sender. Anyone on the CYBOU network can transfer whole CYBOU to this account."), &dialog);
    layout->addWidget(desc);

    auto* acc_edit = new QLineEdit(status.account_id, &dialog);
    acc_edit->setReadOnly(true);
    acc_edit->selectAll();
    layout->addWidget(acc_edit);

    auto* btn_layout = new QHBoxLayout;
    auto* copy_btn = new QPushButton(tr("Copy to Clipboard"), &dialog);
    copy_btn->setObjectName(QStringLiteral("primaryButton"));
    connect(copy_btn, &QPushButton::clicked, [&] {
        QGuiApplication::clipboard()->setText(status.account_id);
        copy_btn->setText(tr("Copied!"));
    });
    btn_layout->addWidget(copy_btn);

    auto* close_btn = new QPushButton(tr("Close"), &dialog);
    close_btn->setObjectName(QStringLiteral("secondaryButton"));
    connect(close_btn, &QPushButton::clicked, &dialog, &QDialog::accept);
    btn_layout->addWidget(close_btn);

    layout->addLayout(btn_layout);
    dialog.exec();
}

void WalletPage::syncLedger()
{
    auto* service = m_model->walletService();
    if (!service) {
        return;
    }

    service->SyncLedger();

    const auto [bal, sys] = service->GetBalances();
    m_model->setBalances(bal, sys);

    const auto entries = service->GetLedgerEntries();
    QVector<Entry> loaded;
    for (const auto& e : entries) {
        Entry entry;
        entry.id = QString::fromStdString(e.entry_id.GetHex());
        entry.amount = e.amount;
        entry.system_side = e.system_side;
        entry.counterparty = e.counterparty.IsNull() ? QString{} : QString::fromStdString(e.counterparty.Value().GetHex());
        entry.at = e.timestamp > 0 ? QDateTime::fromSecsSinceEpoch(static_cast<qint64>(e.timestamp)) : QDateTime::currentDateTime();
        entry.finality = (e.finality == cybou::WalletEntryFinality::FINAL) ? EntryFinality::Final : EntryFinality::Pending;

        switch (e.kind) {
        case cybou::WalletEntryKind::ONBOARDING_BONUS:
            entry.kind = EntryKind::OnboardingBonus;
            break;
        case cybou::WalletEntryKind::MAIL_FEE:
            entry.kind = EntryKind::MailFee;
            break;
        case cybou::WalletEntryKind::PAYMENT:
            entry.kind = EntryKind::Payment;
            break;
        case cybou::WalletEntryKind::LOCK_TO_SYSTEM:
            entry.kind = EntryKind::LockToSystem;
            break;
        }
        loaded.append(entry);
    }

    bool changed = (m_entries.size() != loaded.size());
    if (!changed) {
        for (int i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].id != loaded[i].id ||
                m_entries[i].finality != loaded[i].finality ||
                m_entries[i].amount != loaded[i].amount) {
                changed = true;
                break;
            }
        }
    }

    if (changed) {
        m_entries = std::move(loaded);
        rebuildActivity();
    }
}

void WalletPage::actionNotWired()
{
    // Reachable only when core reports payments; until the backend call
    // exists, say so instead of pretending to transfer.
    QMessageBox::information(this, tr("Not available yet"),
        tr("Transfers are not wired to the node in this build. No operation was created."));
}
