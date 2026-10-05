// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/settingspage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QXmlStreamWriter>
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

QString LoginItemPath()
{
#if defined(Q_OS_MACOS)
    return QDir::home().filePath(QStringLiteral("Library/LaunchAgents/com.cybou.desktop.plist"));
#elif defined(Q_OS_LINUX)
    return QDir{QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)}
        .filePath(QStringLiteral("autostart/cybou.desktop"));
#else
    return {};
#endif
}

bool StartsAtLogin()
{
#ifdef Q_OS_WIN
    return RunKey().contains(QStringLiteral("CYBOU"));
#else
    const auto path = LoginItemPath();
    return !path.isEmpty() && QFile::exists(path);
#endif
}

bool SetStartAtLogin(bool enabled)
{
#ifdef Q_OS_WIN
    QSettings key = RunKey();
    if (enabled) {
        key.setValue(QStringLiteral("CYBOU"),
            QStringLiteral("\"%1\"").arg(QDir::toNativeSeparators(QCoreApplication::applicationFilePath())));
    } else {
        key.remove(QStringLiteral("CYBOU"));
    }
    return key.status() == QSettings::NoError;
#elif defined(Q_OS_LINUX)
    const QString path = LoginItemPath();
    if (!enabled) return path.isEmpty() || !QFile::exists(path) || QFile::remove(path);
    QString escaped = QCoreApplication::applicationFilePath();
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    escaped.replace(QStringLiteral("`"), QStringLiteral("\\`"));
    escaped.replace(QStringLiteral("$"), QStringLiteral("\\$"));
    escaped.replace(QStringLiteral("%"), QStringLiteral("%%"));
    if (!QDir{}.mkpath(QFileInfo{path}.absolutePath())) return false;
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    const QByteArray entry = "[Desktop Entry]\nType=Application\nName=CYBOU\nExec=\"" + escaped.toUtf8() +
        "\"\nTerminal=false\nX-GNOME-Autostart-enabled=true\n";
    if (file.write(entry) != entry.size()) return false;
    return file.commit();
#elif defined(Q_OS_MACOS)
    const QString path = LoginItemPath();
    if (!enabled) return path.isEmpty() || !QFile::exists(path) || QFile::remove(path);
    if (!QDir{}.mkpath(QFileInfo{path}.absolutePath())) return false;
    QSaveFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QXmlStreamWriter xml{&file};
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("plist"));
    xml.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("dict"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("Label"));
    xml.writeTextElement(QStringLiteral("string"), QStringLiteral("com.cybou.desktop"));
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("ProgramArguments"));
    xml.writeStartElement(QStringLiteral("array"));
    xml.writeTextElement(QStringLiteral("string"), QCoreApplication::applicationFilePath());
    xml.writeEndElement();
    xml.writeTextElement(QStringLiteral("key"), QStringLiteral("RunAtLoad"));
    xml.writeEmptyElement(QStringLiteral("true"));
    xml.writeEndElement();
    xml.writeEndElement();
    xml.writeEndDocument();
    return !xml.hasError() && file.commit();
#else
    Q_UNUSED(enabled);
    return false;
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
    m_start_at_login = new QCheckBox{tr("Start CYBOU at login"), this};
    m_start_at_login->setObjectName(QStringLiteral("startAtLogin"));
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX) && !defined(Q_OS_MACOS)
    m_start_at_login->setEnabled(false);
#endif
    connect(m_start_at_login, &QCheckBox::toggled, this, [this](bool on) {
        if (SetStartAtLogin(on)) return;
        const QSignalBlocker blocker{m_start_at_login};
        m_start_at_login->setChecked(StartsAtLogin());
        QMessageBox::warning(this, tr("Startup setting"), tr("CYBOU could not update the start-at-login setting."));
    });
    general->addWidget(m_start_at_login);
    m_run_in_background = new QCheckBox{tr("Keep running in background when the window is closed"), this};
    m_run_in_background->setObjectName(QStringLiteral("runInBackground"));
    connect(m_run_in_background, &QCheckBox::toggled, this,
        [](bool on) { QSettings{}.setValue(runInBackgroundKey(), on); });
    general->addWidget(m_run_in_background);

    auto* language_row = new QHBoxLayout;
    auto* language_caption = new QLabel{tr("Language"), this};
    language_caption->setObjectName(QStringLiteral("rowSub"));
    language_row->addWidget(language_caption);
    m_language = new QComboBox{this};
    m_language->setObjectName(QStringLiteral("language"));
    m_language->setAccessibleName(tr("Language"));
    m_language->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_language->setMinimumWidth(200);
    language_row->addWidget(m_language);
    language_row->addStretch();
    general->addLayout(language_row);
    connect(m_language, &QComboBox::activated, this, [this](int index) {
        const QString language = m_language->itemData(index).toString();
        QSettings{}.setValue(languageKey(), language);
        QTimer::singleShot(0, this, [fn = onLanguageChanged, language] { if (fn) fn(language); });
    });

    auto* appearance = Section(root, tr("Appearance"), {}, this);
    auto* appearance_row = new QHBoxLayout;
    auto* appearance_caption = new QLabel{tr("Theme"), this};
    appearance_caption->setObjectName(QStringLiteral("rowSub"));
    appearance_row->addWidget(appearance_caption);
    m_appearance = new QComboBox{this};
    m_appearance->setObjectName(QStringLiteral("appearanceMode"));
    m_appearance->setAccessibleName(tr("Theme"));
    m_appearance->addItem(tr("Same as system"), static_cast<int>(CybouTheme::Appearance::System));
    m_appearance->addItem(tr("Light"), static_cast<int>(CybouTheme::Appearance::Light));
    m_appearance->addItem(tr("Dark"), static_cast<int>(CybouTheme::Appearance::Dark));
    m_appearance->setMinimumWidth(200);
    appearance_row->addWidget(m_appearance);
    appearance_row->addStretch();
    appearance->addLayout(appearance_row);
    connect(m_appearance, &QComboBox::activated, this, [this](int index) {
        CybouTheme::saveAppearance(static_cast<CybouTheme::Appearance>(m_appearance->itemData(index).toInt()));
        // Rebuild after this slot returns; the combo box itself is replaced.
        QTimer::singleShot(0, this, [fn = onAppearanceChanged] { if (fn) fn(); });
    });

    auto* privacy = Section(root, tr("Privacy and security"), {}, this);
    m_notifications = new QCheckBox{tr("Notify me about new mail, received payments and problems"), this};
    m_notifications->setObjectName(QStringLiteral("notifications"));
    connect(m_notifications, &QCheckBox::toggled, this, [this](bool on) {
        QSettings{}.setValue(notificationsKey(), on);
        m_mail_previews->setEnabled(on);
    });
    privacy->addWidget(m_notifications);
    m_mail_previews = new QCheckBox{tr("Show Mail previews in notifications"), this};
    m_mail_previews->setObjectName(QStringLiteral("mailPreviews"));
    m_mail_previews->setToolTip(tr("When off, notifications only say “New CYBOU Mail”."));
    connect(m_mail_previews, &QCheckBox::toggled, this, [](bool on) { QSettings{}.setValue(mailPreviewsKey(), on); });
    privacy->addWidget(m_mail_previews);
    auto* lock_row = new QHBoxLayout;
    auto* lock_caption = new QLabel{tr("Lock CYBOU after inactivity"), this};
    lock_caption->setObjectName(QStringLiteral("rowSub"));
    lock_row->addWidget(lock_caption);
    m_auto_lock = new QComboBox{this};
    m_auto_lock->setObjectName(QStringLiteral("autoLock"));
    m_auto_lock->setAccessibleName(tr("Lock CYBOU after inactivity"));
    for (const int minutes : {5, 15, 30, 60}) m_auto_lock->addItem(tr("%1 minutes").arg(minutes), minutes);
    m_auto_lock->addItem(tr("Never"), 0);
    m_auto_lock->setMinimumWidth(200);
    lock_row->addWidget(m_auto_lock);
    lock_row->addStretch();
    privacy->addLayout(lock_row);
    connect(m_auto_lock, &QComboBox::activated, this, [this](int index) {
        QSettings{}.setValue(autoLockMinutesKey(), m_auto_lock->itemData(index).toInt());
    });

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

    Section(root, tr("Network storage"),
        tr("Every Full Node stores encrypted network content. Disk space is allocated automatically while keeping free space for your computer."), this);

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
    connect(m_model, &CybouDesktopModel::featureAvailabilityChanged, this, [this] { refresh(); });
    refresh();
}

void SettingsPage::refresh()
{
    const QSignalBlocker b1{m_start_at_login};
    const QSignalBlocker b2{m_run_in_background};
    const QSignalBlocker b3{m_mail_previews};
    const QSignalBlocker b4{m_appearance};
    const QSignalBlocker b_language{m_language};
    m_appearance->setCurrentIndex(m_appearance->findData(static_cast<int>(CybouTheme::savedAppearance())));
    m_language->setCurrentIndex(std::max(0, m_language->findData(
        QSettings{}.value(languageKey(), QStringLiteral("fr")).toString())));
    m_start_at_login->setChecked(StartsAtLogin());
    m_run_in_background->setChecked(QSettings{}.value(runInBackgroundKey(), false).toBool());
    m_mail_previews->setChecked(QSettings{}.value(mailPreviewsKey(), false).toBool());
    const QSignalBlocker b6{m_notifications};
    const QSignalBlocker b7{m_auto_lock};
    const bool notify = QSettings{}.value(notificationsKey(), true).toBool();
    m_notifications->setChecked(notify);
    m_mail_previews->setEnabled(notify);
    const int lock = QSettings{}.value(autoLockMinutesKey(), DEFAULT_AUTO_LOCK_MINUTES).toInt();
    m_auto_lock->setCurrentIndex(std::max(0, m_auto_lock->findData(lock)));
    m_download_folder->setText(QDir::toNativeSeparators(DownloadFolder()));
    const QString data_dir = m_model->status().data_directory;
    m_data_directory->setText(data_dir.isEmpty() ? tr("Available after node startup") : QDir::toNativeSeparators(data_dir));
}
