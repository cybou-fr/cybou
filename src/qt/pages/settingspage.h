// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_SETTINGSPAGE_H
#define BITCOIN_QT_PAGES_SETTINGSPAGE_H

#include <QWidget>

#include <functional>

class SettingsPage : public QWidget
{
public:
    SettingsPage(std::function<void()> preferences_requested,
        std::function<void()> diagnostics_requested, QWidget* parent = nullptr);
};

#endif // BITCOIN_QT_PAGES_SETTINGSPAGE_H
