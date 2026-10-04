// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_QT_PAGES_NETWORKAUTHORITYPAGE_H
#define CYBOU_QT_PAGES_NETWORKAUTHORITYPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QWidget>

class CybouDesktopModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

/**
 * Central Authority operator console: visible only when the unlocked
 * Identity derives this network's genesis PoA finalizer key.
 *
 * The operator sees the local finalizer state and controls it (pause,
 * resume, finalize one block), sees the candidates waiting for the next
 * block, and reads network totals from
 * this node's own independently validated finalized state.
 */
class NetworkAuthorityPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(NetworkAuthorityPage)

public:
    explicit NetworkAuthorityPage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;

    QLabel* m_finalizer_state{nullptr};
    QLabel* m_finalizer_detail{nullptr};
    QPushButton* m_pause{nullptr};
    QPushButton* m_finalize_now{nullptr};
    QPushButton* m_settle_storage{nullptr};

    QLabel* m_height{nullptr};
    QLabel* m_last_block{nullptr};
    QLabel* m_candidates{nullptr};
    QLabel* m_peers{nullptr};
    QLabel* m_identities{nullptr};
    QLabel* m_escrow{nullptr};

    QVBoxLayout* m_queue{nullptr};
    QVBoxLayout* m_recent{nullptr};
    QVBoxLayout* m_totals{nullptr};
    QVBoxLayout* m_peer_rows{nullptr};
    QVBoxLayout* m_chain{nullptr};

    quint64 m_seen_height{0};
    QDateTime m_seen_at;
    bool m_height_advanced_in_view{false};

    void refresh();
};

#endif // CYBOU_QT_PAGES_NETWORKAUTHORITYPAGE_H
