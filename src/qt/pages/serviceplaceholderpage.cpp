// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/serviceplaceholderpage.h>

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

ServicePlaceholderPage::ServicePlaceholderPage(const QString& title, const QString& description, QWidget* parent)
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
    auto* status = new QLabel{tr("Planned service"), card};
    status->setObjectName("eyebrow");
    auto* summary = new QLabel{description, card};
    summary->setObjectName("bodyText");
    summary->setWordWrap(true);
    auto* note = new QLabel{tr("This service is not enabled in the current development build."), card};
    note->setObjectName("mutedText");
    note->setWordWrap(true);
    card_layout->addWidget(status);
    card_layout->addWidget(summary);
    card_layout->addWidget(note);
    layout->addWidget(card);
    layout->addStretch();
}
