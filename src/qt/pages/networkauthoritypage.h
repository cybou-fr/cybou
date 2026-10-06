// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_NETWORKAUTHORITYPAGE_H
#define CYBOU_QT_PAGES_NETWORKAUTHORITYPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <string>
#include <utility>
#include <vector>

class CybouDesktopModel;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

struct CybouExplorerItem {
    QString phase_or_height;
    QString identifier;
    QString classification;
    QString status;
    bool is_candidate{false};
    quint64 height{0};
};

/**
 * Central Authority operator console: visible only when the unlocked
 * Identity derives this network's genesis PoA finalizer key.
 *
 * The operator sees the local finalizer state and controls it (pause,
 * resume, finalize one block, settle storage), inspects locally executed
 * candidate operations and verified blocks via a paginated explorer,
 * reads state-root committed totals, and checks signer safety evidence.
 */
class NetworkAuthorityPage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(NetworkAuthorityPage)

public:
    explicit NetworkAuthorityPage(CybouDesktopModel* model, QWidget* parent = nullptr);
    ~NetworkAuthorityPage() override = default;

    int explorerItemCount() const { return m_filtered_items.size(); }
    int explorerPage() const { return m_explorer_page; }
    void setExplorerFilter(const QString& filter);
    void selectExplorerItem(int index);
    QTableWidget* explorerTable() const { return m_explorer_table; }
    QWidget* explorerDetailWidget() const { return m_explorer_detail_card; }

private:
    void onExplorerSelectionChanged();

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

    // Explorer widgets
    QLineEdit* m_explorer_filter_edit{nullptr};
    QTableWidget* m_explorer_table{nullptr};
    QLabel* m_explorer_page_label{nullptr};
    QPushButton* m_explorer_prev{nullptr};
    QPushButton* m_explorer_next{nullptr};
    QWidget* m_explorer_detail_card{nullptr};
    QVBoxLayout* m_explorer_detail_layout{nullptr};

    QVector<CybouExplorerItem> m_all_items;
    QVector<CybouExplorerItem> m_filtered_items;
    int m_explorer_page{1};
    int m_selected_explorer_index{-1};
    static constexpr int kPageSize = 10;

    QLabel* m_total_spendable{nullptr};
    QLabel* m_total_system{nullptr};
    QLabel* m_total_escrow{nullptr};
    QLabel* m_total_names{nullptr};

    QLabel* m_chain_tip{nullptr};
    QLabel* m_chain_state_root{nullptr};
    QLabel* m_chain_network_id{nullptr};
    QLabel* m_chain_connection{nullptr};

    // Evidence & safety labels
    QLabel* m_safety_journal{nullptr};
    QLabel* m_settlement_readiness{nullptr};
    QLabel* m_read_only_notice{nullptr};

    quint64 m_seen_height{0};
    QDateTime m_seen_at;
    bool m_height_advanced_in_view{false};

    QStringList m_cached_candidate_ids;
    std::vector<std::pair<std::string, quint64>> m_cached_recent;
    std::vector<std::string> m_cached_peers;

    void refresh();
    void updateAgeLabel();
    void rebuildExplorerItems();
    void updateExplorerPage();
    void updateExplorerDetails();
};

#endif // CYBOU_QT_PAGES_NETWORKAUTHORITYPAGE_H
