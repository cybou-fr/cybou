// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/settingspage.h>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

SettingsPage::SettingsPage(std::function<void()> preferences_requested,
    std::function<void()> diagnostics_requested, QWidget* parent)
    : QWidget{parent}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);
    auto* heading = new QLabel{tr("Settings"), this};
    heading->setObjectName("pageTitle");
    root->addWidget(heading);

    auto* card = new QFrame{this};
    card->setObjectName("card");
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(28, 26, 28, 26);
    layout->setSpacing(12);
    auto* title = new QLabel{tr("Desktop and network preferences"), card};
    title->setObjectName("cardTitle");
    auto* description = new QLabel{tr("Language, startup, background operation, proxy and connection settings remain backed by the existing node options during this transition."), card};
    description->setObjectName("bodyText");
    description->setWordWrap(true);
    auto* preferences = new QPushButton{tr("Open preferences"), card};
    preferences->setObjectName("primaryButton");
    auto* diagnostics = new QPushButton{tr("Open diagnostics"), card};
    diagnostics->setObjectName("secondaryButton");
    connect(preferences, &QPushButton::clicked, this, [callback = std::move(preferences_requested)] { callback(); });
    connect(diagnostics, &QPushButton::clicked, this, [callback = std::move(diagnostics_requested)] { callback(); });
    layout->addWidget(title);
    layout->addWidget(description);
    layout->addSpacing(8);
    layout->addWidget(preferences, 0, Qt::AlignLeft);
    layout->addWidget(diagnostics, 0, Qt::AlignLeft);
    root->addWidget(card);
    root->addStretch();
}
