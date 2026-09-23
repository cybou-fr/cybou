// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/backuppage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>

#include <QBrush>
#include <QCoreApplication>
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

QFrame* metricCard(const QString& caption, QWidget* parent, QLabel*& value_out)
{
    auto* card = new QFrame{parent};
    card->setObjectName(QStringLiteral("card"));
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(8);
    auto* label = new QLabel{caption, card};
    label->setObjectName(QStringLiteral("cardLabel"));
    layout->addWidget(label);
    auto* value = new QLabel{card};
    value->setObjectName(QStringLiteral("metric"));
    value_out = value;
    layout->addWidget(value);
    return card;
}

} // namespace

BackupPage::BackupPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent},
      m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);

    auto* heading = new QLabel{tr("Backup"), this};
    heading->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(heading);
    m_account_line = noteLabel(QString{}, this);
    root->addWidget(m_account_line);

    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(16);
    metrics->addWidget(metricCard(tr("Last backup"), this, m_last_backup), 1);
    metrics->addWidget(metricCard(tr("Backup size"), this, m_backup_size), 1);
    metrics->addWidget(metricCard(tr("Restore key binding"), this, m_key_binding), 1);
    root->addLayout(metrics);

    root->addWidget(noteLabel(tr("Backups encrypt your local CYBOU data before anything leaves this device; resilience comes from Storage replication, not from a single provider. Restore is bound to the same local identity keys — without them no backup can be opened. Desktop nodes may prune pre-Store MailTx history; a backup is what protects local data against that."), this));

    auto* actions = new QHBoxLayout;
    actions->setSpacing(12);
    m_backup_now = new QPushButton{tr("Back up now…"), this};
    m_backup_now->setObjectName(QStringLiteral("primaryButton"));
    connect(m_backup_now, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Backup is not wired to the node in this build. No backup was created."));
    });
    m_restore = new QPushButton{tr("Restore…"), this};
    m_restore->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_restore, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Backup is not wired to the node in this build. Nothing was restored."));
    });
    actions->addWidget(m_backup_now);
    actions->addWidget(m_restore);
    actions->addStretch();
    root->addLayout(actions);
    m_gate_hint = noteLabel(QString{}, this);
    root->addWidget(m_gate_hint);

    auto* list_title = new QLabel{tr("Backup sets"), this};
    list_title->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(list_title);
    m_list = new QListWidget{this};
    m_list->setObjectName(QStringLiteral("messageList"));
    root->addWidget(m_list, 1);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });

    refresh();
}

void BackupPage::refresh()
{
    const auto& status = m_model->status();
    const bool identity_active = status.identity_state == CybouIdentityState::Active;
    m_account_line->setText(identity_active
        ? tr("Account: %1").arg(status.account_id.isEmpty() ? tr("active") : status.account_id)
        : tr("No identity yet — backups bind to your local identity keys."));

    if (m_sets.isEmpty()) {
        m_last_backup->setText(tr("Never"));
        m_backup_size->setText(tr("0 bytes"));
    } else {
        const BackupSet& latest = m_sets.last();
        m_last_backup->setText(QLocale{}.toString(latest.at, QLocale::ShortFormat));
        qint64 total = 0;
        for (const BackupSet& set : m_sets) total += set.size;
        m_backup_size->setText(tr("%1 bytes").arg(QLocale{}.toString(total)));
    }
    m_key_binding->setText(identity_active ? tr("Bound to identity") : tr("No identity"));

    const bool usable = identity_active && m_model->capabilities().backup;
    m_backup_now->setEnabled(usable);
    m_restore->setEnabled(usable);
    m_gate_hint->setText(!identity_active
        ? tr("Create an identity first — restore is bound to those keys.")
        : usable ? QString{}
                 : tr("Backup starts after native Email and Object Storage work — the node will report the backup capability once it is live."));
    rebuildList();
}

void BackupPage::rebuildList()
{
    m_list->clear();
    for (const BackupSet& set : m_sets) {
        auto* row = new QFrame{m_list};
        auto* row_layout = new QHBoxLayout{row};
        row_layout->setContentsMargins(12, 8, 12, 8);
        row_layout->setSpacing(10);
        auto* main = new QVBoxLayout;
        main->setSpacing(2);
        auto* label = new QLabel{set.label.isEmpty() ? set.id : set.label, row};
        label->setStyleSheet(QStringLiteral("font-weight: 700; color: #111827; background: transparent; border: none;"));
        auto* sub = new QLabel{tr("%1 bytes · %2")
            .arg(QLocale{}.toString(set.size),
                 QLocale{}.toString(set.at, QLocale::ShortFormat)), row};
        sub->setObjectName(QStringLiteral("mutedText"));
        sub->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        main->addWidget(label);
        main->addWidget(sub);
        row_layout->addLayout(main, 1);
        auto* verified = new QLabel{set.verified ? tr("Verified") : tr("Pending verification"), row};
        verified->setObjectName(set.verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));
        row_layout->addWidget(verified, 0, Qt::AlignVCenter);
        auto* item = new QListWidgetItem{m_list};
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 56}));
        m_list->setItemWidget(item, row);
    }
    if (m_list->count() == 0) {
        auto* item = new QListWidgetItem{m_list};
        item->setFlags(Qt::NoItemFlags);
        item->setText(tr("No backups yet.\nEncrypted backup sets will appear here once the Backup service is live."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 120});
    }
}
