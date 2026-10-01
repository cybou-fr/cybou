// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_PAGES_NETWORKAUTHORITYPAGE_H
#define BITCOIN_QT_PAGES_NETWORKAUTHORITYPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QWidget>

class CybouDesktopModel;
class QLabel;
class QVBoxLayout;

/**
 * Central Authority: visible only when the unlocked Identity derives this
 * network's genesis PoA finalizer key. Reports this node's independently
 * validated finalized state and the local finalizer's operating model.
 */
class NetworkAuthorityPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(NetworkAuthorityPage)

public:
    explicit NetworkAuthorityPage(CybouDesktopModel* model, QWidget* parent = nullptr);

private:
    CybouDesktopModel* const m_model;
    QLabel* m_height{nullptr};
    QLabel* m_last_block{nullptr};
    QLabel* m_safety{nullptr};
    QLabel* m_identities{nullptr};
    QLabel* m_names{nullptr};
    QLabel* m_peers{nullptr};
    QVBoxLayout* m_finality{nullptr};
    QVBoxLayout* m_economy{nullptr};
    QVBoxLayout* m_providers{nullptr};
    quint64 m_seen_height{0};
    QDateTime m_seen_at;
    bool m_seen_authority_height{false};
    bool m_height_advanced_in_view{false};

    void refresh();
};

#endif // BITCOIN_QT_PAGES_NETWORKAUTHORITYPAGE_H
