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

/** Files view over the product model (docs/cybou/83_STORAGE_UI_UX.md). */
class StoragePage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(StoragePage)

public:
    StoragePage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    CybouDesktopModel* m_model;
    QVector<CybouFileItem> m_objects;

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
};

#endif // BITCOIN_QT_PAGES_STORAGEPAGE_H
