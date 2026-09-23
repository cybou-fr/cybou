// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_SETTINGSPAGE_H
#define BITCOIN_QT_PAGES_SETTINGSPAGE_H

#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QCheckBox;

class SettingsPage : public QWidget
{
public:
    SettingsPage(CybouDesktopModel* model, std::function<void()> preferences_requested,
        std::function<void()> diagnostics_requested, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QCheckBox* m_run_in_background;
    const std::function<void()> m_preferences_requested;
    const std::function<void()> m_diagnostics_requested;

    void refresh();
};

#endif // BITCOIN_QT_PAGES_SETTINGSPAGE_H
