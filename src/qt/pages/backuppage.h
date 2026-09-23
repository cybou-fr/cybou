// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_BACKUPPAGE_H
#define BITCOIN_QT_PAGES_BACKUPPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>
#include <QWidget>

class CybouDesktopModel;
class QLabel;
class QListWidget;
class QPushButton;

/**
 * Backup UI shaped by the Backup rules (docs 14, 64):
 *
 *  - Backup is an application of Object Storage: it starts only after
 *    native Email and Object Storage work, so the service stays gated.
 *  - Everything is encrypted locally before placement; paths and names are
 *    never plaintext to storage peers.
 *  - Restore is bound to the same local identity keys — the UI shows the
 *    binding state instead of offering an anonymous restore.
 *  - Desktop nodes may prune pre-Store MailTx history; a backup is what
 *    protects local data against that, not the protocol.
 */
class BackupPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(BackupPage)

public:
    BackupPage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    struct BackupSet {
        QString id;
        QString label;
        qint64 size{0};
        QDateTime at;
        bool verified{false};
    };

    CybouDesktopModel* m_model;
    QVector<BackupSet> m_sets;

    QLabel* m_account_line{nullptr};
    QLabel* m_last_backup{nullptr};
    QLabel* m_backup_size{nullptr};
    QLabel* m_key_binding{nullptr};
    QLabel* m_gate_hint{nullptr};
    QListWidget* m_list{nullptr};
    QPushButton* m_backup_now{nullptr};
    QPushButton* m_restore{nullptr};

    void refresh();
    void rebuildList();
};

#endif // BITCOIN_QT_PAGES_BACKUPPAGE_H
