// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/serviceplaceholderpage.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

ServicePlaceholderPage::ServicePlaceholderPage(const QString& title, const QString& description,
    CybouTheme::NavIcon icon, const QStringList& what_to_expect, const QString& dependencies, QWidget* parent)
    : QWidget{parent}
{
    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(34, 32, 34, 32);
    layout->setSpacing(18);

    auto* heading = new QLabel{title, this};
    heading->setObjectName("pageTitle");
    layout->addWidget(heading);

    auto* card = new QFrame{this};
    card->setObjectName("card");
    auto* card_layout = new QVBoxLayout{card};
    card_layout->setContentsMargins(28, 26, 28, 26);
    card_layout->setSpacing(12);
    auto* header = new QHBoxLayout;
    auto* chip = new QLabel{card};
    chip->setObjectName("iconChip");
    chip->setFixedSize(44, 44);
    chip->setAlignment(Qt::AlignCenter);
    chip->setPixmap(CybouTheme::iconPixmap(icon, {24, 24}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    auto* summary = new QLabel{description, card};
    summary->setObjectName("bodyText");
    summary->setWordWrap(true);
    summary->setAlignment(Qt::AlignVCenter);
    header->addWidget(chip);
    header->addWidget(summary, 1);
    auto* badge = new QLabel{tr("Planned"), card};
    badge->setObjectName("neutralBadge");
    header->addWidget(badge, 0, Qt::AlignTop);
    auto* note = new QLabel{tr("Not available in this development build."), card};
    note->setObjectName("mutedText");
    note->setWordWrap(true);
    card_layout->addLayout(header);
    card_layout->addWidget(note);

    if (!what_to_expect.isEmpty()) {
        auto* expect_title = new QLabel{tr("What to expect"), card};
        expect_title->setObjectName("sectionTitle");
        card_layout->addSpacing(6);
        card_layout->addWidget(expect_title);
        QStringList bullets;
        for (const QString& item : what_to_expect) bullets.append(QStringLiteral("\u2022 %1").arg(item));
        auto* expect_body = new QLabel{bullets.join(QStringLiteral("\n")), card};
        expect_body->setObjectName("bodyText");
        expect_body->setWordWrap(true);
        card_layout->addWidget(expect_body);
    }

    if (!dependencies.isEmpty()) {
        auto* requires_label = new QLabel{tr("Requires: %1").arg(dependencies), card};
        requires_label->setObjectName("mutedText");
        card_layout->addWidget(requires_label);
    }
    layout->addWidget(card);
    layout->addStretch();
}
