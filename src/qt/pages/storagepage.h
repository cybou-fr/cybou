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
 * Storage UI shaped by the object-model rules (docs 11–13):
 *
 *  - Objects are opaque and content-addressed: the list shows identifiers,
 *    never original filenames or paths (identifiers must not leak them).
 *  - Application semantics are encrypted before storage; the desktop UI
 *    renders sizes and retention, never plaintext content.
 *  - Desktop nodes may prune: retention is a first-class column and the
 *    UI says plainly that local copies can be dropped.
 *  - Protocol-level replication is not a single provider; until Object
 *    Storage is live the service stays gated and nothing pretends to upload.
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
