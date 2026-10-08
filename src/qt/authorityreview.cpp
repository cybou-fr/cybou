// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <qt/authorityreview.h>
#include <qt/cyboudesktopmodel.h>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTextEdit>
#include <QTimeZone>
#include <limits>

bool ReviewStorageSettlement(CybouDesktopModel* model, uint64_t period, uint64_t start,
    uint64_t end, const std::vector<cybou::StorageSettlementEntry>& entries, QWidget* parent)
{
    const auto tr = [](const char* text) { return QCoreApplication::translate("AuthorityReview", text); };
    const auto account = model->status().account_id;
    const auto observed_escrow = model->networkAuthority().storage_escrow;
    const auto eligible = [=] {
        const auto& a = model->networkAuthority();
        return model->status().identity_state == CybouIdentityState::Active && model->status().account_id == account &&
            a.proven && a.signer_enabled && a.finalizer != CybouFinalizerState::SafetyHalt &&
            a.settlement_due && a.next_settlement_period == period && a.next_settlement_start_utc == start && a.next_settlement_due_utc == end && a.storage_escrow == observed_escrow;
    };
    if (!eligible()) return false;
    uint64_t total = 0;
    QSet<QString> providers;
    QStringList details;
    for (const auto& entry : entries) {
        if (entry.amount > std::numeric_limits<uint64_t>::max() - total) return false;
        total += entry.amount;
        const auto provider = QString::fromStdString(entry.payout_account.Value().GetHex());
        providers.insert(provider);
        if (details.size() < 100) details << QStringLiteral("%1 | %2 | %3").arg(
            QString::fromStdString(entry.publication_id.GetHex()), provider, cybouAmountText(entry.amount));
    }
    const auto escrow = model->networkAuthority().storage_escrow;
    if (total > escrow) return false;
    QMessageBox review{QMessageBox::Question, tr("Review storage settlement"),
        tr("Period %1\nUTC interval: %2 — %3\nPayout accounts: %4\nPrepared entries: %5\nAmount: %6\nCurrent storage escrow: %7\n\nEvidence: locally prepared off-chain storage observations. These do not prove global reliability or independent failure domains.\n\nConfirmation submits these exact entries for local execution. Only a finalized PoA block records settlement."),
        QMessageBox::NoButton, parent};
    review.setTextFormat(Qt::PlainText);
    review.setObjectName(QStringLiteral("authoritySettlementReview"));
    review.setText(review.text().arg(period)
        .arg(QDateTime::fromSecsSinceEpoch(start, QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm")),
             QDateTime::fromSecsSinceEpoch(end, QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm")))
        .arg(providers.size()).arg(entries.size()).arg(cybouAmountText(total), cybouAmountText(escrow)));
    review.setDetailedText(tr("PublicationID | payout AccountID | CYBOU\nFirst %1 of %2 entries:").arg(details.size()).arg(entries.size()) + QLatin1Char{'\n'} + details.join(QLatin1Char{'\n'}));
    if (entries.empty()) review.setInformativeText(tr("No eligible payouts were prepared. This may reflect missing local evidence. A finalized settlement closes this period; later evidence cannot amend it."));
    auto* confirm = review.addButton(tr("Confirm settlement"), QMessageBox::AcceptRole);
    confirm->setObjectName(QStringLiteral("authorityConfirmSettlement"));
    auto* cancel = review.addButton(QMessageBox::Cancel);
    cancel->setText(tr("Cancel"));
    review.setDefaultButton(cancel);
    for (auto* button : review.buttons()) {
        if (review.buttonRole(button) != QMessageBox::ActionRole) continue;
        button->setText(tr("Show payout entries"));
        // The native details button caches its original sizeHint before translation.
        button->setFixedWidth(button->fontMetrics().horizontalAdvance(button->text()) + 48);
        QObject::connect(button, &QAbstractButton::clicked, &review, [button, &review, tr] {
            const auto* text = review.findChild<QTextEdit*>();
            button->setText(text && text->isVisible() ? tr("Hide payout entries") : tr("Show payout entries"));
            button->setFixedWidth(button->fontMetrics().horizontalAdvance(button->text()) + 48);
        });
    }
    const auto invalidate = [&] { if (!eligible()) review.reject(); };
    QObject::connect(model, &CybouDesktopModel::statusChanged, &review, invalidate);
    QObject::connect(model, &CybouDesktopModel::networkAuthorityChanged, &review, invalidate);
    review.exec();
    return review.clickedButton() == confirm && eligible() && total <= model->networkAuthority().storage_escrow;
}
