// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/settingspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <utility>

namespace {

QFrame* SectionCard(const QString& title, QWidget* parent)
{
    auto* card = new QFrame{parent};
    card->setObjectName("card");
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);
    auto* heading = new QLabel{title, card};
    heading->setObjectName("sectionTitle");
    layout->addWidget(heading);
    return card;
}

QLabel* Note(const QString& text, QWidget* parent)
{
    auto* note = new QLabel{text, parent};
    note->setObjectName("mutedText");
    note->setWordWrap(true);
    return note;
}

} // namespace

SettingsPage::SettingsPage(CybouDesktopModel* model, std::function<void()> preferences_requested,
    std::function<void()> diagnostics_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_preferences_requested{std::move(preferences_requested)},
      m_diagnostics_requested{std::move(diagnostics_requested)}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);
    auto* heading = new QLabel{tr("Settings"), this};
    heading->setObjectName("pageTitle");
    root->addWidget(heading);

    // General
    auto* general = SectionCard(tr("General"), this);
    {
        auto* layout = qobject_cast<QVBoxLayout*>(general->layout());
        m_run_in_background = new QCheckBox{tr("Keep CYBOU running in the background when the window is closed"), general};
        m_run_in_background->setToolTip(tr("When enabled, closing the window hides CYBOU and the node keeps running. Use File -> Quit CYBOU to shut down."));
        connect(m_run_in_background, &QCheckBox::toggled, this, [this](bool checked) {
            if (auto* options = m_model->optionsModel()) {
                options->setOption(OptionsModel::MinimizeOnClose, checked);
            }
        });
        layout->addWidget(m_run_in_background);

        auto* startup_row = new QHBoxLayout;
        auto* startup = new QCheckBox{tr("Start CYBOU with the operating system"), general};
        startup->setEnabled(false);
        startup_row->addWidget(startup);
        auto* startup_badge = new QLabel{tr("Planned"), general};
        startup_badge->setObjectName("neutralBadge");
        startup_row->addWidget(startup_badge, 0, Qt::AlignVCenter);
        startup_row->addStretch();
        layout->addLayout(startup_row);

        layout->addWidget(Note(tr("Interface language and further general options remain in the preferences dialog during this transition."), general));
        auto* preferences = new QPushButton{tr("Open preferences"), general};
        preferences->setObjectName("secondaryButton");
        connect(preferences, &QPushButton::clicked, this, [this] { m_preferences_requested(); });
        layout->addWidget(preferences, 0, Qt::AlignLeft);
    }
    root->addWidget(general);

    // Network
    auto* network = SectionCard(tr("Network"), this);
    {
        auto* layout = qobject_cast<QVBoxLayout*>(network->layout());
        layout->addWidget(Note(tr("Proxy, incoming connections and bandwidth limits remain in the preferences dialog during this transition."), network));
        auto* preferences = new QPushButton{tr("Open network preferences"), network};
        preferences->setObjectName("secondaryButton");
        connect(preferences, &QPushButton::clicked, this, [this] { m_preferences_requested(); });
        layout->addWidget(preferences, 0, Qt::AlignLeft);
    }
    root->addWidget(network);

    // Storage
    auto* storage = SectionCard(tr("Storage"), this);
    {
        auto* layout = qobject_cast<QVBoxLayout*>(storage->layout());
        layout->addWidget(Note(tr("Future storage allocation for Email, Storage and Backup will be configured here."), storage));
    }
    root->addWidget(storage);

    // Advanced
    auto* advanced = SectionCard(tr("Advanced"), this);
    {
        auto* layout = qobject_cast<QVBoxLayout*>(advanced->layout());
        layout->addWidget(Note(tr("Diagnostics and logs are intended for developers and support."), advanced));
        auto* row = new QHBoxLayout;
        auto* diagnostics = new QPushButton{tr("Open diagnostics"), advanced};
        diagnostics->setObjectName("secondaryButton");
        connect(diagnostics, &QPushButton::clicked, this, [this] { m_diagnostics_requested(); });
        auto* debug_log = new QPushButton{tr("Open debug log"), advanced};
        debug_log->setObjectName("secondaryButton");
        connect(debug_log, &QPushButton::clicked, this, [] { GUIUtil::openDebugLogfile(); });
        row->addWidget(diagnostics);
        row->addWidget(debug_log);
        row->addStretch();
        layout->addLayout(row);
    }
    root->addWidget(advanced);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    refresh();
}

void SettingsPage::refresh()
{
    auto* options = m_model->optionsModel();
    const QSignalBlocker blocker{m_run_in_background};
    m_run_in_background->setEnabled(options != nullptr);
    m_run_in_background->setChecked(options && options->getMinimizeOnClose());
}
