// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/settingspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <utility>

using namespace CybouUi;

namespace {

QVBoxLayout* Section(QVBoxLayout* root, const QString& title, const QString& subtitle, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(10);
    layout->addWidget(SectionTitle(title, card));
    if (!subtitle.isEmpty()) layout->addWidget(MutedText(subtitle, card));
    root->addWidget(card);
    return layout;
}

#ifdef Q_OS_WIN
QSettings RunKey()
{
    return QSettings{QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
        QSettings::NativeFormat};
}
#endif

bool StartsWithWindows()
{
#ifdef Q_OS_WIN
    return RunKey().contains(QStringLiteral("CYBOU"));
#else
    return false;
#endif
}

void SetStartWithWindows(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings key = RunKey();
    if (enabled) {
        key.setValue(QStringLiteral("CYBOU"),
            QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        key.remove(QStringLiteral("CYBOU"));
    }
#else
    Q_UNUSED(enabled);
#endif
}

QString DownloadFolder()
{
    return QSettings{}.value(SettingsPage::downloadFolderKey(),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).toString();
}

} // namespace

SettingsPage::SettingsPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    setMinimumWidth(0);
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(16);

    auto* general = Section(root, tr("General"), {}, this);
    m_start_with_windows = new QCheckBox{tr("Start CYBOU with Windows"), this};
    m_start_with_windows->setObjectName(QStringLiteral("startWithWindows"));
#ifndef Q_OS_WIN
    m_start_with_windows->setEnabled(false);
#endif
    connect(m_start_with_windows, &QCheckBox::toggled, this, [](bool on) { SetStartWithWindows(on); });
    general->addWidget(m_start_with_windows);
    m_run_in_background = new QCheckBox{tr("Keep running in background when the window is closed"), this};
    m_run_in_background->setObjectName(QStringLiteral("runInBackground"));
    connect(m_run_in_background, &QCheckBox::toggled, this,
        [](bool on) { QSettings{}.setValue(runInBackgroundKey(), on); });
    general->addWidget(m_run_in_background);

    auto* privacy = Section(root, tr("Privacy"), {}, this);
    m_mail_previews = new QCheckBox{tr("Show Mail previews in notifications"), this};
    m_mail_previews->setObjectName(QStringLiteral("mailPreviews"));
    m_mail_previews->setToolTip(tr("When off, notifications only say “New CYBOU Mail”."));
    connect(m_mail_previews, &QCheckBox::toggled, this, [](bool on) { QSettings{}.setValue(mailPreviewsKey(), on); });
    privacy->addWidget(m_mail_previews);

    auto* files = Section(root, tr("Files"), {}, this);
    auto* folder_row = new QHBoxLayout;
    auto* folder_caption = new QLabel{tr("Download folder"), this};
    folder_caption->setObjectName(QStringLiteral("rowSub"));
    folder_row->addWidget(folder_caption);
    m_download_folder = new QLabel{this};
    m_download_folder->setObjectName(QStringLiteral("rowTitle"));
    m_download_folder->setWordWrap(true);
    m_download_folder->setMinimumWidth(0);
    folder_row->addWidget(m_download_folder, 1);
    auto* choose = new QPushButton{tr("Change…"), this};
    choose->setObjectName(QStringLiteral("secondaryButton"));
    connect(choose, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Download folder"), DownloadFolder());
        if (folder.isEmpty()) return;
        QSettings{}.setValue(downloadFolderKey(), folder);
        refresh();
    });
    folder_row->addWidget(choose);
    files->addLayout(folder_row);

    auto* contribution = Section(root, tr("Storage contribution"),
        tr("Contribute disk space to the CYBOU network. This advanced option arrives in a later release."), this);
    auto* later = new QCheckBox{tr("Contribute storage"), this};
    later->setEnabled(false);
    contribution->addWidget(later);

    auto* advanced = Section(root, tr("Advanced"), {}, this);
    auto* data_row = new QHBoxLayout;
    auto* data_caption = new QLabel{tr("Data directory"), this};
    data_caption->setObjectName(QStringLiteral("rowSub"));
    data_row->addWidget(data_caption);
    m_data_directory = new QLabel{this};
    m_data_directory->setObjectName(QStringLiteral("rowTitle"));
    m_data_directory->setWordWrap(true);
    m_data_directory->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_data_directory->setMinimumWidth(0);
    data_row->addWidget(m_data_directory, 1);
    advanced->addLayout(data_row);
    auto* diagnostics = new QPushButton{tr("Open Diagnostics"), this};
    diagnostics->setObjectName(QStringLiteral("secondaryButton"));
    connect(diagnostics, &QPushButton::clicked, this, [fn = std::move(diagnostics_requested)] { if (fn) fn(); });
    advanced->addWidget(diagnostics, 0, Qt::AlignLeft);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    refresh();
}

void SettingsPage::refresh()
{
    const QSignalBlocker b1{m_start_with_windows};
    const QSignalBlocker b2{m_run_in_background};
    const QSignalBlocker b3{m_mail_previews};
    m_start_with_windows->setChecked(StartsWithWindows());
    m_run_in_background->setChecked(QSettings{}.value(runInBackgroundKey(), false).toBool());
    m_mail_previews->setChecked(QSettings{}.value(mailPreviewsKey(), false).toBool());
    m_download_folder->setText(QDir::toNativeSeparators(DownloadFolder()));
    const QString data_dir = m_model->status().data_directory;
    m_data_directory->setText(data_dir.isEmpty() ? tr("Available after node startup") : QDir::toNativeSeparators(data_dir));
}
