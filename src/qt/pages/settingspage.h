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

    /** QSettings keys shared with the shell. */
    static QString runInBackgroundKey() { return QStringLiteral("desktop/run_in_background"); }
    static QString mailPreviewsKey() { return QStringLiteral("privacy/mail_previews"); }
    static QString downloadFolderKey() { return QStringLiteral("files/download_folder"); }
    /** Show "Validated" on operations (informational; only finality is canonical). */
    static QString showValidationKey() { return QStringLiteral("network/show_validation"); }

private:
    CybouDesktopModel* const m_model;
    QComboBox* m_appearance{nullptr};
    QCheckBox* m_start_with_windows{nullptr};
    QCheckBox* m_run_in_background{nullptr};
    QCheckBox* m_mail_previews{nullptr};
    QWidget* m_validation_section{nullptr};
    QCheckBox* m_show_validation{nullptr};
    QLabel* m_download_folder{nullptr};
    QLabel* m_data_directory{nullptr};

    void refresh();
};

#endif // BITCOIN_QT_PAGES_SETTINGSPAGE_H
