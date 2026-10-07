// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cybouactivity.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include <cstdlib>

using namespace CybouUi;

QVector<CybouActivityOperation> CybouActivityOperations(const CybouDesktopModel& model)
{
    using Kind = CybouActivityOperation::Kind;
    const auto tr = [](const char* text) { return QCoreApplication::translate("CybouActivity", text); };
    const bool online = model.status().online;
    QVector<CybouActivityOperation> attention;
    QVector<CybouActivityOperation> running;
    const auto add = [&](CybouActivityOperation operation) {
        (operation.attention ? attention : running).append(std::move(operation));
    };

    for (const auto& task : model.mailTasks()) {
        if (task.state == CybouCommandState::Committed) continue;
        add({Kind::LocalMail, task.item_id, task.title,
            task.state == CybouCommandState::Failed ? task.error :
                task.state == CybouCommandState::Running ? tr("Saving changes on this computer…") : tr("Waiting to save changes…"),
            task.state == CybouCommandState::Failed});
    }
    for (const auto& file : model.fileItems()) {
        if (file.folder || file.trashed) continue;
        if (file.retrieval != CybouRetrievalState::Idle && file.retrieval != CybouRetrievalState::Ready) {
            add({Kind::Download, file.id, tr("Downloading %1").arg(file.name), CybouProduct::retrievalText(file.retrieval), false});
            continue;
        }
        const auto operation = model.displayedOperationState(file.operation_id, file.operation_state);
        if (file.state == CybouContentState::NeedsAttention) {
            add({Kind::File, file.id, tr("%1 was not uploaded").arg(file.name), tr("Needs attention"), true});
        } else if (CybouProduct::itemPending(file.state, operation)) {
            add({Kind::File, file.id, tr("Uploading %1").arg(file.name),
                CybouProduct::progressText(file.state, file.progress_percent, online, operation), false});
        }
    }
    for (const auto& mail : model.mailItems()) {
        if (mail.draft || !mail.outgoing) continue;
        const QString subject = mail.subject.isEmpty() ? tr("(no subject)") : mail.subject;
        const auto operation = model.displayedOperationState(mail.operation_id, mail.operation_state);
        if (mail.state == CybouContentState::NeedsAttention) {
            add({Kind::Mail, mail.id, tr("“%1” was not sent").arg(subject), tr("Needs attention"), true});
        } else if (CybouProduct::itemPending(mail.state, operation)) {
            add({Kind::Mail, mail.id, tr("Sending “%1” to %2").arg(subject, mail.to_name),
                CybouProduct::contentWithOperationText(mail.state, operation, online), false});
        }
    }
    for (const auto& entry : model.walletEntries()) {
        const auto operation = model.displayedOperationState(entry.operation_id, entry.operation_state);
        const bool failed = operation == CybouOperationState::Failed;
        if (!failed && !CybouProduct::operationPending(operation)) continue;
        const QString amount = cybouAmountText(static_cast<quint64>(std::llabs(entry.amount)));
        QString title;
        switch (entry.kind) {
        case CybouWalletEntryKind::Sent: title = tr("Sending %1 to %2").arg(amount, entry.counterparty_name); break;
        case CybouWalletEntryKind::MovedToSystemBalance: title = tr("Moving %1 to Network balance").arg(amount); break;
        default: continue; // fees and credits follow their own operation
        }
        add({Kind::Payment, entry.id, title, CybouProduct::operationStateText(operation), failed});
    }
    const auto& status = model.status();
    if (model.recoveryRotationPending() || (!model.fixtureMode() && model.hasPendingRecoveryRotation())) {
        add({Kind::Recovery, {}, tr("Changing your recovery phrase"),
            tr("Waiting for confirmation. Keep both the old and the new words until it is done."), false});
    }
    if (status.name_claim_pending) {
        add({Kind::Name, {}, tr("Claiming your .cybou name"),
            status.name_claim_status.isEmpty() ? tr("Waiting for confirmation") : status.name_claim_status, false});
    }
    return attention + running;
}

CybouActivityButton::CybouActivityButton(CybouDesktopModel* model, QWidget* parent)
    : QToolButton{parent}, m_model{model}
{
    setObjectName(QStringLiteral("activityButton"));
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setCursor(Qt::PointingHandCursor);
    setAutoRaise(true);
    setToolTip(tr("Operations in progress"));
    connect(this, &QToolButton::clicked, this, [this] { showPopup(); });
    for (auto signal : {&CybouDesktopModel::filesChanged, &CybouDesktopModel::mailChanged,
             &CybouDesktopModel::walletChanged, &CybouDesktopModel::statusChanged}) {
        connect(m_model, signal, this, [this] { refresh(); });
    }
    connect(m_model, &CybouDesktopModel::mailTasksChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::recoveryRotationFinished, this, [this] { refresh(); });
    refresh();
}

void CybouActivityButton::refresh()
{
    const auto operations = CybouActivityOperations(*m_model);
    int failed = 0;
    for (const auto& operation : operations) failed += operation.attention ? 1 : 0;
    const int running = operations.size() - failed;
    setVisible(!operations.isEmpty());
    const bool alert = failed > 0;
    setIcon(QIcon{glyphPixmap(alert ? Glyph::Info : Glyph::Refresh, {16, 16},
        CybouTheme::color(alert ? CybouTheme::ROSE : CybouTheme::BRAND_TEAL_DARK))});
    setText(alert ? (failed == 1 ? tr("1 needs attention") : tr("%1 need attention").arg(failed))
                  : (running == 1 ? tr("1 in progress") : tr("%1 in progress").arg(running)));
    setAccessibleName(text());
    if (m_popup && m_popup->isVisible()) rebuildRows();
    if (m_popup && operations.isEmpty()) m_popup->hide();
}

void CybouActivityButton::showPopup()
{
    if (!m_popup) {
        m_popup = new QFrame{this, Qt::Popup};
        m_popup->setObjectName(QStringLiteral("activityPopup"));
        m_popup->setStyleSheet(QStringLiteral("QFrame#activityPopup { background: %1; border: 1px solid %2; border-radius: 12px; }")
            .arg(CybouTheme::color(CybouTheme::CARD).name(), CybouTheme::color(CybouTheme::BORDER).name()));
        auto* layout = new QVBoxLayout{m_popup};
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(6);
        layout->addWidget(SectionTitle(tr("Activity"), m_popup));
        layout->addWidget(MutedText(tr("Everything on its way to the network, and anything that needs you."), m_popup));
        auto* host = new QWidget{m_popup};
        m_rows = new QVBoxLayout{host};
        m_rows->setContentsMargins(0, 4, 0, 0);
        m_rows->setSpacing(4);
        layout->addWidget(host);
        m_popup->setFixedWidth(420);
    }
    rebuildRows();
    m_popup->adjustSize();
    QPoint at = mapToGlobal(QPoint{width() - m_popup->width(), height() + 6});
    if (const QScreen* screen = this->screen()) {
        const QRect area = screen->availableGeometry();
        at.setX(std::clamp(at.x(), area.left() + 8, area.right() - m_popup->width() - 8));
    }
    m_popup->move(at);
    m_popup->show();
}

namespace {
/// Empties a layout at every depth. Row labels live in a nested layout, so a one-level
/// clear left stale titles painted under the new rows; widgets are hidden at once because
/// deleteLater() only removes them on the next event loop pass.
void ClearRows(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QLayout* inner = item->layout()) ClearRows(inner);
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}
} // namespace

void CybouActivityButton::rebuildRows()
{
    ClearRows(m_rows);
    QWidget* parent = m_rows->parentWidget();
    const auto operations = CybouActivityOperations(*m_model);
    for (qsizetype i = 0; i < operations.size() && i < 12; ++i) {
        const auto& operation = operations.at(i);
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        Glyph glyph{Glyph::CloudUp};
        switch (operation.kind) {
        case CybouActivityOperation::Kind::File: glyph = Glyph::Upload; break;
        case CybouActivityOperation::Kind::Download: glyph = Glyph::Download; break;
        case CybouActivityOperation::Kind::LocalMail:
        case CybouActivityOperation::Kind::Mail: glyph = Glyph::Envelope; break;
        case CybouActivityOperation::Kind::Payment: glyph = Glyph::WalletCard; break;
        case CybouActivityOperation::Kind::Name: glyph = Glyph::User; break;
        case CybouActivityOperation::Kind::Recovery: glyph = Glyph::Key; break;
        }
        row->addWidget(Chip(glyph, operation.attention ? Tint::Rose : Tint::Mint, parent, 32, 16), 0, Qt::AlignTop);
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        // One line: a long file name without spaces would wrap mid-word and overlap the status.
        auto* title = new QLabel{parent};
        title->setObjectName(QStringLiteral("rowTitle"));
        title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        title->setText(title->fontMetrics().elidedText(operation.title, Qt::ElideMiddle, 300));
        title->setToolTip(operation.title);
        auto* status = new QLabel{operation.status, parent};
        status->setObjectName(QStringLiteral("rowSub"));
        if (operation.attention) status->setStyleSheet(QStringLiteral("color: %1;").arg(CybouTheme::color(CybouTheme::ROSE).name()));
        text->addWidget(title);
        text->addWidget(status);
        row->addLayout(text, 1);
        if (operation.attention && (operation.kind == CybouActivityOperation::Kind::File ||
                                    operation.kind == CybouActivityOperation::Kind::Mail)) {
            auto* retry = new QPushButton{tr("Try again"), parent};
            retry->setObjectName(QStringLiteral("secondaryButton"));
            connect(retry, &QPushButton::clicked, this, [this, operation] {
                if (operation.kind == CybouActivityOperation::Kind::File) m_model->requestRetryFile(operation.id);
                else m_model->requestRetryMail(operation.id);
            });
            row->addWidget(retry, 0, Qt::AlignVCenter);
        }
        auto* show = new QPushButton{tr("Open"), parent};
        show->setObjectName(QStringLiteral("softButton"));
        connect(show, &QPushButton::clicked, this, [this, operation] {
            m_popup->hide();
            open(operation);
        });
        row->addWidget(show, 0, Qt::AlignVCenter);
        m_rows->addLayout(row);
    }
    if (operations.isEmpty()) m_rows->addWidget(MutedText(tr("Nothing in progress."), parent));
    m_popup->adjustSize();
}

void CybouActivityButton::open(const CybouActivityOperation& operation)
{
    switch (operation.kind) {
    case CybouActivityOperation::Kind::File:
    case CybouActivityOperation::Kind::Download:
        if (onOpenFile) onOpenFile(operation.id);
        break;
    case CybouActivityOperation::Kind::LocalMail:
    case CybouActivityOperation::Kind::Mail:
        if (onOpenMail) onOpenMail(operation.id);
        break;
    case CybouActivityOperation::Kind::Payment:
        if (onOpenWallet) onOpenWallet();
        break;
    case CybouActivityOperation::Kind::Name:
    case CybouActivityOperation::Kind::Recovery:
        if (onOpenIdentity) onOpenIdentity();
        break;
    }
}
