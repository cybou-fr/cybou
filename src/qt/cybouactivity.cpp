// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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
        case CybouWalletEntryKind::MovedToSystemBalance: title = tr("Moving %1 to System Balance").arg(amount); break;
        default: continue; // fees and credits follow their own operation
        }
        add({Kind::Payment, entry.id, title, CybouProduct::operationStateText(operation), failed});
    }
    const auto& status = model.status();
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

void CybouActivityButton::rebuildRows()
{
    while (QLayoutItem* item = m_rows->takeAt(0)) {
        if (item->layout()) {
            while (QLayoutItem* inner = item->layout()->takeAt(0)) {
                if (inner->widget()) inner->widget()->deleteLater();
                delete inner;
            }
        }
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    QWidget* parent = m_rows->parentWidget();
    const auto operations = CybouActivityOperations(*m_model);
    for (qsizetype i = 0; i < operations.size() && i < 12; ++i) {
        const auto& operation = operations.at(i);
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        Glyph glyph{Glyph::CloudUp};
        switch (operation.kind) {
        case CybouActivityOperation::Kind::File: glyph = Glyph::CloudUp; break;
        case CybouActivityOperation::Kind::Download: glyph = Glyph::Download; break;
        case CybouActivityOperation::Kind::Mail: glyph = Glyph::Envelope; break;
        case CybouActivityOperation::Kind::Payment: glyph = Glyph::WalletCard; break;
        case CybouActivityOperation::Kind::Name: glyph = Glyph::User; break;
        }
        row->addWidget(Chip(glyph, operation.attention ? Tint::Rose : Tint::Mint, parent, 32, 16), 0, Qt::AlignTop);
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        auto* title = new QLabel{operation.title, parent};
        title->setObjectName(QStringLiteral("rowTitle"));
        title->setWordWrap(true);
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
    case CybouActivityOperation::Kind::Mail:
        if (onOpenMail) onOpenMail(operation.id);
        break;
    case CybouActivityOperation::Kind::Payment:
        if (onOpenWallet) onOpenWallet();
        break;
    case CybouActivityOperation::Kind::Name:
        if (onOpenIdentity) onOpenIdentity();
        break;
    }
}
