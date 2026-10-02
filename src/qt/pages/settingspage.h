// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_SETTINGSPAGE_H
#define BITCOIN_QT_PAGES_SETTINGSPAGE_H

#include <QCoreApplication>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QCheckBox;
class QComboBox;
class QLabel;

/** Settings: General, Privacy, Files, Storage contribution, Advanced. */
class SettingsPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(SettingsPage)

public:
    SettingsPage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
        QWidget* parent = nullptr);

    /** Set by the shell: re-renders the app after an appearance change. */
    std::function<void()> onAppearanceChanged;
    std::function<void(const QString&)> onLanguageChanged;

    /** QSettings keys shared with the shell. */
    static QString runInBackgroundKey() { return QStringLiteral("desktop/run_in_background"); }
    static QString mailPreviewsKey() { return QStringLiteral("privacy/mail_previews"); }
    /** System notifications for new mail, received payments and failures. */
    static QString notificationsKey() { return QStringLiteral("privacy/notifications"); }
    /** Minutes without input before the vault locks; 0 = never. */
    static QString autoLockMinutesKey() { return QStringLiteral("security/auto_lock_minutes"); }
    static constexpr int DEFAULT_AUTO_LOCK_MINUTES{15};
    static QString downloadFolderKey() { return QStringLiteral("files/download_folder"); }
    static QString languageKey() { return QStringLiteral("desktop/language"); }

private:
    CybouDesktopModel* const m_model;
    QComboBox* m_appearance{nullptr};
    QComboBox* m_language{nullptr};
    QCheckBox* m_start_at_login{nullptr};
    QCheckBox* m_run_in_background{nullptr};
    QCheckBox* m_mail_previews{nullptr};
    QCheckBox* m_notifications{nullptr};
    QComboBox* m_auto_lock{nullptr};
    QLabel* m_download_folder{nullptr};
    QLabel* m_data_directory{nullptr};

    void refresh();
};

#endif // BITCOIN_QT_PAGES_SETTINGSPAGE_H
