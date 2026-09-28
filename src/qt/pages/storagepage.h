// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_STORAGEPAGE_H
#define BITCOIN_QT_PAGES_STORAGEPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QVector>
#include <QWidget>

class CybouDesktopModel;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

/**
 * Transitional Files shell. The encrypted catalog and desktop transfer
 * controller are not connected yet (see docs 83 and 91):
 *
 *  - The user's client must display decrypted filenames and folders.
 *  - Storage providers receive ciphertext and opaque identifiers only.
 *  - Provider capability does not mean the client Files flow is available.
 */
class StoragePage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(StoragePage)

public:
    StoragePage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    enum class Replication {
        Planned, /**< Service not live yet; nothing is placed on peers. */
        Placed,
    };

    struct StoredObject {
        QString cid;      /**< Content identifier; opaque, no filenames. */
        qint64 size{0};
        bool pinned{false}; /**< Pinned objects are exempt from pruning. */
        QDateTime at;
        Replication replication{Replication::Planned};
    };

    CybouDesktopModel* m_model;
    QVector<StoredObject> m_objects;

    QLabel* m_all_files_count{nullptr};
    QLabel* m_usage_value{nullptr};
    QLabel* m_usage_caption{nullptr};
    QLabel* m_gate_hint{nullptr};
    QLineEdit* m_search{nullptr};
    QListWidget* m_list{nullptr};
    QPushButton* m_upload{nullptr};
    QPushButton* m_pin{nullptr};
    QWidget* m_details{nullptr};
    int m_selected{-1};

    void refresh();
    void rebuildList();
    void showDetails(int index);
    static QString replicationText(Replication replication);
};

#endif // BITCOIN_QT_PAGES_STORAGEPAGE_H
