// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_QT_CYBOUNOTIFIER_H
#define CYBOU_QT_CYBOUNOTIFIER_H

#include <qt/cyboutheme.h>

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include <functional>
#include <utility>

namespace CybouUi {

/**
 * Transient confirmation at the bottom of the window ("Moved to Trash Â·
 * Undo"). One notice at a time; a new one replaces the current one. The
 * notice follows its host's size and never takes keyboard focus away.
 */
class Notifier final : public QFrame
{
public:
    explicit Notifier(QWidget* host) : QFrame{host}, m_host{host}
    {
        setObjectName(QStringLiteral("notifier"));
        setAttribute(Qt::WA_StyledBackground);
        setStyleSheet(QStringLiteral(
            "QFrame#notifier { background: %1; border-radius: 10px; }"
            "QFrame#notifier QLabel { color: white; font-size: 14px; background: transparent; }"
            "QFrame#notifier QPushButton { background: transparent; border: none; color: %2; font-weight: 700;"
            " padding: 4px 8px; min-height: 0px; }"
            "QFrame#notifier QPushButton:hover { color: white; }")
            .arg(CybouTheme::isDark() ? QStringLiteral("#3a3f49") : CybouTheme::color(CybouTheme::TEXT_PRIMARY).name(),
                CybouTheme::color(CybouTheme::LOGO_MINT).name()));
        auto* layout = new QHBoxLayout{this};
        layout->setContentsMargins(18, 10, 10, 10);
        layout->setSpacing(14);
        m_text = new QLabel{this};
        m_text->setObjectName(QStringLiteral("notifierText"));
        m_text->setAccessibleName(QStringLiteral("Notification"));
        layout->addWidget(m_text, 1);
        m_action = new QPushButton{this};
        m_action->setObjectName(QStringLiteral("notifierAction"));
        m_action->setFocusPolicy(Qt::NoFocus);
        m_action->setCursor(Qt::PointingHandCursor);
        layout->addWidget(m_action);
        QObject::connect(m_action, &QPushButton::clicked, this, [this] {
            auto action = std::exchange(m_callback, {});
            hide();
            if (action) action();
        });
        m_timer.setSingleShot(true);
        QObject::connect(&m_timer, &QTimer::timeout, this, [this] {
            m_callback = {};
            hide();
        });
        host->installEventFilter(this);
        hide();
    }

    void show(const QString& text, const QString& action_label = {}, std::function<void()> action = {})
    {
        m_text->setText(text);
        m_callback = std::move(action);
        m_action->setText(action_label);
        m_action->setVisible(!action_label.isEmpty() && m_callback);
        adjustSize();
        place();
        QFrame::show();
        raise();
        m_timer.start(m_callback ? 7000 : 4000);
    }

    QString text() const { return m_text->text(); }
    bool hasAction() const { return m_action->isVisible(); }
    void trigger() { if (m_action->isVisible()) m_action->click(); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_host && event->type() == QEvent::Resize && isVisible()) place();
        return QFrame::eventFilter(watched, event);
    }

private:
    QWidget* const m_host;
    QLabel* m_text{nullptr};
    QPushButton* m_action{nullptr};
    QTimer m_timer;
    std::function<void()> m_callback;

    void place()
    {
        const int width = qMin(qMax(sizeHint().width(), 320), m_host->width() - 48);
        resize(width, sizeHint().height());
        move((m_host->width() - width) / 2, m_host->height() - height() - 28);
    }
};

} // namespace CybouUi

#endif // CYBOU_QT_CYBOUNOTIFIER_H
