// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/settingspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <QCheckBox>
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QUrl>
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

        layout->addWidget(Note(tr("Interface language and further low-level options remain in the legacy preferences dialog until the native settings cover them."), general));
        auto* preferences = new QPushButton{tr("Legacy preferences"), general};
        preferences->setObjectName("secondaryButton");
        connect(preferences, &QPushButton::clicked, this, [this] { m_preferences_requested(); });
        layout->addWidget(preferences, 0, Qt::AlignLeft);
    }
    root->addWidget(general);

    // Network
    auto* network = SectionCard(tr("Network"), this);
    {
        auto* layout = qobject_cast<QVBoxLayout*>(network->layout());
        m_proxy_enabled = new QCheckBox{tr("Connect through a SOCKS5 proxy"), network};
        connect(m_proxy_enabled, &QCheckBox::toggled, this, [this](bool checked) {
            if (auto* options = m_model->optionsModel()) {
                options->setOption(OptionsModel::ProxyUse, checked);
            }
            m_proxy_host->setEnabled(checked);
            m_proxy_port->setEnabled(checked);
        });
        layout->addWidget(m_proxy_enabled);

        auto* proxy_row = new QHBoxLayout;
        proxy_row->setSpacing(12);
        auto* host_label = new QLabel{tr("Host"), network};
        host_label->setObjectName("mutedText");
        m_proxy_host = new QLineEdit{network};
        m_proxy_host->setPlaceholderText(tr("Proxy host"));
        m_proxy_host->setMinimumWidth(220);
        connect(m_proxy_host, &QLineEdit::editingFinished, this, [this] {
            if (auto* options = m_model->optionsModel()) {
                options->setOption(OptionsModel::ProxyIP, m_proxy_host->text().trimmed());
            }
        });
        auto* port_label = new QLabel{tr("Port"), network};
        port_label->setObjectName("mutedText");
        m_proxy_port = new QSpinBox{network};
        m_proxy_port->setRange(1, 65535);
        m_proxy_port->setFixedWidth(110);
        m_proxy_port->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        connect(m_proxy_port, &QSpinBox::editingFinished, this, [this] {
            if (auto* options = m_model->optionsModel()) {
                options->setOption(OptionsModel::ProxyPort, m_proxy_port->value());
            }
        });
        proxy_row->addWidget(host_label);
        proxy_row->addWidget(m_proxy_host, 1);
        proxy_row->addWidget(port_label);
        proxy_row->addWidget(m_proxy_port);
        proxy_row->addStretch();
        auto* proxy_form = new QVBoxLayout;
        proxy_form->setSpacing(8);
        proxy_form->addLayout(proxy_row);
        layout->addLayout(proxy_form);

        m_listen = new QCheckBox{tr("Allow incoming connections"), network};
        m_listen->setToolTip(tr("Other nodes connect to you, strengthening the network. Disable only if you are behind a restrictive firewall."));
        connect(m_listen, &QCheckBox::toggled, this, [this](bool checked) {
            if (auto* options = m_model->optionsModel()) {
                options->setOption(OptionsModel::Listen, checked);
            }
        });
        layout->addWidget(m_listen);
        layout->addWidget(Note(tr("Proxy and connection changes take effect after CYBOU is restarted."), network));
    }
    root->addWidget(network);

    // Storage
    auto* storage = SectionCard(tr("Storage"), this);
    {
        auto* layout = qobject_cast<QVBoxLayout*>(storage->layout());
        auto* dir_row = new QHBoxLayout;
        m_data_directory = new QLabel{storage};
        m_data_directory->setObjectName("bodyText");
        m_data_directory->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_data_directory->setWordWrap(true);
        dir_row->addWidget(m_data_directory, 1);
        auto* open_dir = new QPushButton{tr("Open folder"), storage};
        open_dir->setObjectName("secondaryButton");
        connect(open_dir, &QPushButton::clicked, this, [this] {
            const QString dir = m_model->status().data_directory;
            if (!dir.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });
        dir_row->addWidget(open_dir, 0, Qt::AlignVCenter);
        layout->addLayout(dir_row);
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

    const QSignalBlocker background_blocker{m_run_in_background};
    m_run_in_background->setEnabled(options != nullptr);
    m_run_in_background->setChecked(options && options->getMinimizeOnClose());

    const QSignalBlocker proxy_blocker{m_proxy_enabled};
    const QSignalBlocker host_blocker{m_proxy_host};
    const QSignalBlocker port_blocker{m_proxy_port};
    const QSignalBlocker listen_blocker{m_listen};
    const bool proxy_enabled = options && options->getOption(OptionsModel::ProxyUse).toBool();
    m_proxy_enabled->setEnabled(options != nullptr);
    m_proxy_enabled->setChecked(proxy_enabled);
    m_proxy_host->setEnabled(proxy_enabled);
    m_proxy_port->setEnabled(proxy_enabled);
    m_proxy_host->setText(options ? options->getOption(OptionsModel::ProxyIP).toString() : QString{});
    m_proxy_port->setValue(options ? options->getOption(OptionsModel::ProxyPort).toInt() : 9050);
    m_listen->setEnabled(options != nullptr);
    m_listen->setChecked(options && options->getOption(OptionsModel::Listen).toBool());

    m_data_directory->setText(m_model->status().data_directory.isEmpty()
        ? tr("Available after node startup")
        : m_model->status().data_directory);
}
