// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/serviceplaceholderpage.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

ServicePlaceholderPage::ServicePlaceholderPage(const QString& title, const QString& description,
    CybouTheme::NavIcon icon, QWidget* parent)
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
    layout->addWidget(card);
    layout->addStretch();
}
