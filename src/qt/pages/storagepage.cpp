// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/storagepage.h>

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

StoragePage::StoragePage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent},
      m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);

    auto* heading = new QLabel{tr("Storage"), this};
    heading->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(heading);
    m_account_line = noteLabel(QString{}, this);
    root->addWidget(m_account_line);

    auto* metrics = new QHBoxLayout;
    metrics->setSpacing(16);
    metrics->addWidget(metricCard(tr("Objects"), this, m_object_count), 1);
    metrics->addWidget(metricCard(tr("Local size"), this, m_local_size), 1);
    metrics->addWidget(metricCard(tr("Prunable"), this, m_prunable_size), 1);
    root->addLayout(metrics);

    root->addWidget(noteLabel(tr("Objects are opaque and content-addressed; names and paths are encrypted before anything leaves this device. Desktop nodes may prune local copies — pin what must stay. Protocol-level replication replaces any single provider once Object Storage is live; mass-scale Email is gated on it."), this));

    auto* actions = new QHBoxLayout;
    actions->setSpacing(12);
    m_upload = new QPushButton{tr("Add object…"), this};
    m_upload->setObjectName(QStringLiteral("primaryButton"));
    connect(m_upload, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Object Storage is not wired to the node in this build. No object was created."));
    });
    m_pin = new QPushButton{tr("Pin / unpin"), this};
    m_pin->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_pin, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, tr("Not available yet"),
            tr("Object Storage is not wired to the node in this build. Retention was not changed."));
    });
    actions->addWidget(m_upload);
    actions->addWidget(m_pin);
    actions->addStretch();
    root->addLayout(actions);
    m_gate_hint = noteLabel(QString{}, this);
    root->addWidget(m_gate_hint);

    auto* list_title = new QLabel{tr("Objects"), this};
    list_title->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(list_title);
    m_list = new QListWidget{this};
    m_list->setObjectName(QStringLiteral("messageList"));
    root->addWidget(m_list, 1);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });

    refresh();
}

QString StoragePage::replicationText(Replication replication)
{
    switch (replication) {
    case Replication::Planned: return tr("Replication planned");
    case Replication::Placed: return tr("Placed on peers");
    }
    return {};
}

void StoragePage::refresh()
{
    const auto& status = m_model->status();
    m_account_line->setText(status.account_id.isEmpty()
        ? tr("No identity yet — storage accounting binds to your account.")
        : tr("Account: %1").arg(status.account_id));

    qint64 total = 0;
    qint64 prunable = 0;
    for (const StoredObject& object : m_objects) {
        total += object.size;
        if (!object.pinned) prunable += object.size;
    }
    m_object_count->setText(QString::number(m_objects.size()));
    m_local_size->setText(tr("%1 bytes").arg(QLocale{}.toString(total)));
    m_prunable_size->setText(tr("%1 bytes").arg(QLocale{}.toString(prunable)));

    const bool usable = status.identity_state == CybouIdentityState::Active &&
                        m_model->capabilities().storage;
    m_upload->setEnabled(usable);
    m_pin->setEnabled(usable);
    m_gate_hint->setText(status.identity_state != CybouIdentityState::Active
        ? tr("Create an identity to use storage accounting.")
        : usable ? QString{}
                 : tr("Object Storage is planned — the node will report the storage capability once it is live."));
    rebuildList();
}

void StoragePage::rebuildList()
{
    m_list->clear();
    for (const StoredObject& object : m_objects) {
        auto* row = new QFrame{m_list};
        auto* row_layout = new QHBoxLayout{row};
        row_layout->setContentsMargins(12, 8, 12, 8);
        row_layout->setSpacing(10);
        auto* main = new QVBoxLayout;
        main->setSpacing(2);
        // Identifiers are deliberately truncated: the full CID is available
        // in core; the UI never invents a resolvable address.
        auto* cid = new QLabel{object.cid.left(20) + QStringLiteral("…"), row};
        cid->setStyleSheet(QStringLiteral("font-weight: 700; color: #111827; background: transparent; border: none;"));
        auto* sub = new QLabel{tr("%1 bytes · %2")
            .arg(QLocale{}.toString(object.size),
                 QLocale{}.toString(object.at, QLocale::ShortFormat)), row};
        sub->setObjectName(QStringLiteral("mutedText"));
        sub->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        main->addWidget(cid);
        main->addWidget(sub);
        row_layout->addLayout(main, 1);
        auto* retention = new QLabel{object.pinned ? tr("Pinned") : tr("Prunable"), row};
        retention->setObjectName(object.pinned ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));
        row_layout->addWidget(retention, 0, Qt::AlignVCenter);
        auto* replication = new QLabel{replicationText(object.replication), row};
        replication->setObjectName(object.replication == Replication::Placed
            ? QStringLiteral("statusBadge")
            : QStringLiteral("neutralBadge"));
        row_layout->addWidget(replication, 0, Qt::AlignVCenter);
        auto* item = new QListWidgetItem{m_list};
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 56}));
        m_list->setItemWidget(item, row);
    }
    if (m_list->count() == 0) {
        auto* item = new QListWidgetItem{m_list};
        item->setFlags(Qt::NoItemFlags);
        item->setText(tr("No objects stored yet.\nEncrypted, content-addressed objects will appear here once Object Storage is live."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 120});
    }
}
