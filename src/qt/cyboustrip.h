// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUSTRIP_H
#define BITCOIN_QT_CYBOUSTRIP_H

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouui.h>

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

/**
 * Top status strip shown under the menu bar on every page (sketch: the
 * Connected / Synced / unread / balances indicator row).
 *
 * Indicators are honest: an item only appears when a backend reports its
 * value. The caller supplies the unread-mail counter so this header does not
 * pull in the mail service.
 */
namespace CybouUi {

class StatusStrip
{
public:
    StatusStrip(CybouDesktopModel* model, std::function<int()> unread_counter, QWidget* parent)
        : m_model{model}, m_unread_counter{std::move(unread_counter)}
    {
        m_frame = new QFrame{parent};
        m_frame->setObjectName(QStringLiteral("statusStrip"));
        m_frame->setFixedHeight(56);
        auto* layout = new QHBoxLayout{m_frame};
        layout->setContentsMargins(20, 6, 20, 6);
        layout->setSpacing(26);

        // Connection state: dot + word.
        m_connection_dot = Dot(Tint::Mint, m_frame, 9);
        m_connection_text = new QLabel{m_frame};
        m_connection_text->setObjectName(QStringLiteral("stripValue"));
        layout->addWidget(m_connection_dot, 0, Qt::AlignVCenter);
        layout->addWidget(m_connection_text, 0, Qt::AlignVCenter);

        // Last sync round.
        auto* sync_icon = new QLabel{m_frame};
        sync_icon->setPixmap(glyphPixmap(Glyph::Refresh, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
        m_sync_text = new QLabel{m_frame};
        m_sync_text->setObjectName(QStringLiteral("stripValue"));
        layout->addWidget(sync_icon, 0, Qt::AlignVCenter);
        layout->addWidget(m_sync_text, 0, Qt::AlignVCenter);

        // Unread mail.
        m_unread_icon = new QLabel{m_frame};
        m_unread_text = new QLabel{m_frame};
        m_unread_text->setObjectName(QStringLiteral("stripValue"));
        layout->addWidget(m_unread_icon, 0, Qt::AlignVCenter);
        layout->addWidget(m_unread_text, 0, Qt::AlignVCenter);

        // Balances: icon + caption/value column.
        layout->addWidget(balanceItem(Glyph::WalletCard, Tint::Mint, m_available_caption, m_available_value));
        layout->addWidget(balanceItem(Glyph::Database, Tint::Indigo, m_system_caption, m_system_value));

        layout->addStretch();

        QObject::connect(model, &CybouDesktopModel::statusChanged, m_frame, [this] { refresh(); });
        auto* ticker = new QTimer{m_frame};
        QObject::connect(ticker, &QTimer::timeout, m_frame, [this] { refresh(); });
        ticker->start(30000);
        refresh();
    }

    QFrame* frame() const { return m_frame; }

    void refresh()
    {
        const auto& status = m_model->status();
        const bool connected = status.node_running && status.peer_count > 0;
        m_connection_text->setText(connected ? QStringLiteral("Connected") : QStringLiteral("Connecting"));
        m_connection_dot->setProperty("tint", connected ? "mint" : "amber");
        restyle(m_connection_dot);

        m_sync_text->setText(m_model->lastSync().isValid()
            ? relTime(m_model->lastSync())
            : QStringLiteral("Sync pending"));

        const int unread = m_unread_counter ? m_unread_counter() : 0;
        m_unread_icon->setPixmap(glyphWithBadge(Glyph::Envelope, {16, 16}, CybouTheme::color(CybouTheme::TEXT_SECONDARY), unread));
        m_unread_text->setText(unread > 0
            ? QStringLiteral("%1 unread").arg(unread)
            : QStringLiteral("No unread mail"));
        m_unread_text->setEnabled(unread > 0);

        m_available_value->setText(cybouAmountText(status.balance));
        m_system_value->setText(cybouAmountText(status.system_balance));
        m_available_caption->setText(QCoreApplication::translate("CybouStrip", "Available balance"));
        m_system_caption->setText(QCoreApplication::translate("CybouStrip", "System balance"));
    }

private:
    static QWidget* balanceItem(Glyph glyph, Tint tint, QLabel*& caption_out, QLabel*& value_out)
    {
        auto* host = new QFrame;
        auto* layout = new QHBoxLayout{host};
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto* icon = new QLabel{host};
        icon->setPixmap(glyphPixmap(glyph, {18, 18}, CybouTheme::color(tintInk(tint))));
        layout->addWidget(icon, 0, Qt::AlignVCenter);
        auto* column = new QVBoxLayout;
        column->setSpacing(0);
        auto* caption = new QLabel{host};
        caption->setObjectName(QStringLiteral("stripCaption"));
        auto* value = new QLabel{host};
        value->setObjectName(QStringLiteral("stripValue"));
        column->addWidget(value);
        column->addWidget(caption);
        layout->addLayout(column);
        caption_out = caption;
        value_out = value;
        return host;
    }

    static void restyle(QWidget* widget)
    {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }

    CybouDesktopModel* const m_model;
    std::function<int()> m_unread_counter;
    QFrame* m_frame{nullptr};
    QLabel* m_connection_dot{nullptr};
    QLabel* m_connection_text{nullptr};
    QLabel* m_sync_text{nullptr};
    QLabel* m_unread_icon{nullptr};
    QLabel* m_unread_text{nullptr};
    QLabel* m_available_caption{nullptr};
    QLabel* m_available_value{nullptr};
    QLabel* m_system_caption{nullptr};
    QLabel* m_system_value{nullptr};
};

} // namespace CybouUi

#endif // BITCOIN_QT_CYBOUSTRIP_H
