// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_HOMEPAGE_H
#define BITCOIN_QT_PAGES_HOMEPAGE_H

#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QLabel;
class QProgressBar;
class QPushButton;

class HomePage : public QWidget
{
public:
    HomePage(CybouDesktopModel* model, std::function<void()> diagnostics_requested,
        std::function<void()> identity_requested, std::function<void()> wallet_requested,
        QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_identity_name;
    QLabel* m_identity_subtitle;
    QLabel* m_chip_protected;
    QLabel* m_chip_ready;
    QPushButton* m_share_button;
    QPushButton* m_manage_button;
    QLabel* m_mail_metric;
    QWidget* m_mail_avatars;
    QLabel* m_files_metric;
    QLabel* m_files_caption;
    QProgressBar* m_files_meter;
    QLabel* m_devices_metric;
    QWidget* m_device_rows;
    QWidget* m_activity_rows;
    QLabel* m_activity_empty;
    const std::function<void()> m_diagnostics_requested;
    const std::function<void()> m_identity_requested;
    const std::function<void()> m_wallet_requested;

    void refresh();
    QWidget* buildIdentityHero();
    QWidget* buildMailCard();
    QWidget* buildFilesCard();
    QWidget* buildDevicesCard();
    QWidget* buildActivityCard();
};

#endif // BITCOIN_QT_PAGES_HOMEPAGE_H
