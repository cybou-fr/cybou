// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/walletpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>

#include <QBrush>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QLabel* noteLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    return label;
}

QFrame* metricCard(const QString& caption, CybouTheme::NavIcon icon, QWidget* parent, QLabel*& value_out)
{
    auto* card = new QFrame{parent};
    card->setObjectName(QStringLiteral("card"));
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(8);
    auto* header = new QHBoxLayout;
    auto* chip = new QLabel{card};
    chip->setObjectName(QStringLiteral("iconChip"));
    chip->setFixedSize(40, 40);
    chip->setAlignment(Qt::AlignCenter);
    chip->setPixmap(CybouTheme::iconPixmap(icon, {22, 22}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    auto* label = new QLabel{caption, card};
    label->setObjectName(QStringLiteral("cardLabel"));
    label->setAlignment(Qt::AlignVCenter);
    header->addWidget(chip);
    header->addWidget(label);
    header->addStretch();
    layout->addLayout(header);
    auto* value = new QLabel{card};
    value->setObjectName(QStringLiteral("metric"));
    value_out = value;
    layout->addWidget(value);
    return card;
}

} // namespace

WalletPage::WalletPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent},
      m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);

    auto* heading = new QLabel{tr("Wallet"), this};
    heading->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(heading);
    m_account_line = noteLabel(QString{}, this);
    root->addWidget(m_account_line);

    auto* balances = new QHBoxLayout;
    balances->setSpacing(16);
    balances->addWidget(metricCard(tr("Balance"), CybouTheme::NavIcon::Wallet, this, m_balance), 1);
    balances->addWidget(metricCard(tr("System Balance"), CybouTheme::NavIcon::Wallet, this, m_system_balance), 1);
    root->addLayout(balances);
    root->addWidget(noteLabel(tr("System Balance funds protocol services (Email fees today, Storage and Backup in the future). It does not boost Proof of Trust in Beta. The lock Balance → System Balance is one-way and cannot be reversed."), this));

    // Actions. Debits from Balance require the user's authorization; the
    // protocol never debits Balance on its own — the UI mirrors that.
    auto* actions = new QHBoxLayout;
    actions->setSpacing(12);
    m_send = new QPushButton{tr("Send…"), this};
    m_send->setObjectName(QStringLiteral("primaryButton"));
    connect(m_send, &QPushButton::clicked, this, [this] { actionNotWired(); });
    m_lock = new QPushButton{tr("Lock to System Balance…"), this};
    m_lock->setObjectName(QStringLiteral("secondaryButton"));
    m_lock->setProperty("irreversible", true);
    connect(m_lock, &QPushButton::clicked, this, [this] { actionNotWired(); });
    m_receive = new QPushButton{tr("Receive"), this};
    m_receive->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_receive, &QPushButton::clicked, this, [this] { actionNotWired(); });
    actions->addWidget(m_send);
    actions->addWidget(m_lock);
    actions->addWidget(m_receive);
    actions->addStretch();
    root->addLayout(actions);
    m_gate_hint = noteLabel(QString{}, this);
    root->addWidget(m_gate_hint);

    auto* activity_title = new QLabel{tr("Activity"), this};
    activity_title->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(activity_title);
    m_activity = new QListWidget{this};
    m_activity->setObjectName(QStringLiteral("messageList"));
    root->addWidget(m_activity, 1);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });

    refresh();
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
    m_balance->setText(cybouAmountText(status.balance));
    m_system_balance->setText(cybouAmountText(status.system_balance));
    m_account_line->setText(status.account_id.isEmpty()
        ? tr("No identity yet — balances appear once your account is created.")
        : tr("Account: %1").arg(status.account_id));

    const bool usable = status.identity_state == CybouIdentityState::Active &&
                        m_model->capabilities().payments;
    m_send->setEnabled(usable);
    m_lock->setEnabled(usable);
    m_receive->setEnabled(usable);
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
        row_layout->setContentsMargins(12, 8, 12, 8);
        row_layout->setSpacing(10);
        auto* main = new QVBoxLayout;
        main->setSpacing(2);
        auto* title = new QLabel{kindText(entry.kind), row};
        title->setStyleSheet(QStringLiteral("font-weight: 700; color: #111827; background: transparent; border: none;"));
        QString detail = QLocale{}.toString(entry.at, QLocale::ShortFormat);
        if (!entry.counterparty.isEmpty()) detail = tr("with %1 · %2").arg(entry.counterparty, detail);
        auto* sub = new QLabel{detail, row};
        sub->setObjectName(QStringLiteral("mutedText"));
        sub->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        main->addWidget(title);
        main->addWidget(sub);
        row_layout->addLayout(main, 1);
        auto* amount = new QLabel{(entry.amount >= 0 ? QStringLiteral("+") : QStringLiteral("\u2212")) +
                                      cybouAmountText(static_cast<quint64>(qAbs<qint64>(entry.amount))),
            row};
        amount->setStyleSheet(QStringLiteral("font-weight: 700; background: transparent; border: none; color: %1;")
            .arg(entry.amount >= 0 ? QStringLiteral("#047857") : QStringLiteral("#4b5563")));
        row_layout->addWidget(amount, 0, Qt::AlignVCenter);
        auto* side = new QLabel{entry.system_side ? tr("System") : tr("Balance"), row};
        side->setObjectName(QStringLiteral("neutralBadge"));
        row_layout->addWidget(side, 0, Qt::AlignVCenter);
        auto* badge = new QLabel{finalityText(entry.finality), row};
        badge->setObjectName(entry.finality == EntryFinality::Final
            ? QStringLiteral("statusBadge")
            : QStringLiteral("neutralBadge"));
        row_layout->addWidget(badge, 0, Qt::AlignVCenter);
        auto* item = new QListWidgetItem{m_activity};
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 56}));
        m_activity->setItemWidget(item, row);
    }
    if (m_activity->count() == 0) {
        auto* item = new QListWidgetItem{m_activity};
        item->setFlags(Qt::NoItemFlags);
        item->setText(tr("No activity yet.\nOnboarding bonus, Email fees and transfers will appear here once your account is active."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 120});
    }
}

void WalletPage::actionNotWired()
{
    // Reachable only when core reports payments; until the backend call
    // exists, say so instead of pretending to transfer.
    QMessageBox::information(this, tr("Not available yet"),
        tr("Transfers are not wired to the node in this build. No operation was created."));
}
