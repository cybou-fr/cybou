// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_STORAGEPAGE_H
#define BITCOIN_QT_PAGES_STORAGEPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>
#include <QWidget>

#include <qt/cyboudesktopmodel.h>

class CybouDesktopModel;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * Single-installation Files view over the encrypted local Storage index.
 * This is not a synchronized/finalized Files catalog (docs 83 and 91).
 */
class StoragePage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(StoragePage)

public:
    StoragePage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    CybouDesktopModel* m_model;
    QVector<CybouDesktopFile> m_objects;

    QLabel* m_usage_value{nullptr};
    QLabel* m_usage_caption{nullptr};
    QLabel* m_gate_hint{nullptr};
    QLineEdit* m_search{nullptr};
    QListWidget* m_list{nullptr};
    QPushButton* m_upload{nullptr};
    QPushButton* m_refresh{nullptr};
    QWidget* m_details{nullptr};

    void refresh();
    void rebuildList();
    void showDetails(int index);
    void promptForIndex();
};

#endif // BITCOIN_QT_PAGES_STORAGEPAGE_H
