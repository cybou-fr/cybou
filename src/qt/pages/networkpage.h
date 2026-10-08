// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_NETWORKPAGE_H
#define CYBOU_QT_PAGES_NETWORKPAGE_H

#include <QCoreApplication>
#include <QDateTime>
#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class CybouDesktopModel;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QTableWidget;
class QTimer;
class QScrollArea;
class QPushButton;
class QVBoxLayout;
class QTabWidget;

struct CybouPeerItem {
    QString endpoint;
    quint64 advertised_height{0};
    QString storage_id;
    bool is_lan{false};
    bool connected{true};
    QString classification;
    QDateTime last_seen; // when this session last had it connected
    QPointF map_coord; // Normalized [0, 1] coordinate on the France map
};

class SchematicFranceMap final : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(SchematicFranceMap)

public:
    explicit SchematicFranceMap(QWidget* parent = nullptr);
    ~SchematicFranceMap() override = default;

    void setPeers(const QVector<CybouPeerItem>& peers);
    void setSelectedPeer(int index);
    void setOverview(const QString& network, const QString& summary);
    int selectedPeer() const { return m_selected_index; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    std::function<void(int)> on_peer_clicked;

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    QVector<CybouPeerItem> m_peers;
    int m_selected_index{-1};
    QVector<QRectF> m_peer_hit_rects;
    QString m_network;
    QString m_summary;
};

class CybouObservationChart;
class NetworkPage final : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(NetworkPage)

public:
    explicit NetworkPage(CybouDesktopModel* model, QWidget* parent = nullptr);
    ~NetworkPage() override = default;

    int peerCount() const { return m_peers.size(); }
    int selectedPeerIndex() const { return m_selected_peer_index; }
    void selectPeer(int index);
    void setDiagnosticsWidget(QWidget* widget);
    void showAdvanced();
    void showBenchmarkDetails();
    void showTechnicalDetails();

    SchematicFranceMap* mapWidget() const { return m_map; }
    QTableWidget* tableWidget() const { return m_table; }
    QWidget* detailsWidget() const { return m_details_card; }

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void onTableSelectionChanged();
    void onMapPeerClicked(int index);
    /** A refresh was skipped while the page was hidden. */
    bool m_stale{true};
    /** Local tip the peer table was built for (its lag column depends on it). */
    quint64 m_table_height{0};

    CybouDesktopModel* const m_model;
    QVector<CybouPeerItem> m_peers;
    int m_selected_peer_index{-1};
    QDateTime m_last_update;
    QString m_network_binding;
    QWidget* m_advanced{nullptr};
    QScrollArea* m_advanced_scroll{nullptr};
    QPushButton* m_advanced_button{nullptr};
    QVBoxLayout* m_advanced_layout{nullptr};
    QTabWidget* m_advanced_tabs{nullptr};
    QWidget* m_peer_section{nullptr};
    QVBoxLayout* m_peer_layout{nullptr};
    QVBoxLayout* m_technical_layout{nullptr};

    // Header & summary
    QLabel* m_scope_note{nullptr};
    QLabel* m_benchmark_reference{nullptr};
    QWidget* m_benchmark_card{nullptr};
    QLabel* m_benchmark_summary{nullptr};
    QLabel* m_benchmark_scope{nullptr};
    QLabel* m_observed_capacity_title{nullptr};
    QLabel* m_observed_capacity{nullptr};
    QLabel* m_observed_storage_detail{nullptr};
    QLabel* m_observed_traffic{nullptr};
    QLabel* m_observed_traffic_detail{nullptr};
    QLabel* m_observed_cpu{nullptr};
    QLabel* m_observed_cpu_detail{nullptr};
    QLabel* m_observed_coverage{nullptr};
    QLabel* m_metric_height{nullptr};
    QLabel* m_metric_height_sub{nullptr};
    QLabel* m_metric_peers{nullptr};
    QLabel* m_metric_peers_sub{nullptr};
    QLabel* m_metric_uptime{nullptr};
    QLabel* m_metric_uptime_sub{nullptr};
    QLabel* m_metric_queue{nullptr};
    QLabel* m_metric_queue_sub{nullptr};
    QLabel* m_metric_memory{nullptr};
    QLabel* m_metric_memory_sub{nullptr};
    QLabel* m_metric_cpu{nullptr};
    QLabel* m_metric_cpu_sub{nullptr};
    QLabel* m_metric_traffic{nullptr};
    QLabel* m_metric_traffic_sub{nullptr};
    QLabel* m_metric_finalization{nullptr};
    QLabel* m_metric_finalization_sub{nullptr};
    CybouObservationChart* m_traffic_chart{nullptr};
    CybouObservationChart* m_finalization_chart{nullptr};
    QLabel* m_metric_storage{nullptr};
    QLabel* m_metric_storage_sub{nullptr};
    QLabel* m_metric_disk{nullptr};
    QLabel* m_metric_disk_sub{nullptr};
    QLabel* m_metric_protection{nullptr};
    QLabel* m_metric_protection_sub{nullptr};

    // Main views
    SchematicFranceMap* m_map{nullptr};
    QTableWidget* m_table{nullptr};

    // Details panel
    QWidget* m_details_card{nullptr};
    QLabel* m_details_title{nullptr};
    QVBoxLayout* m_details_layout{nullptr};

    QTimer* m_refresh_timer{nullptr};
    void scheduleRefresh();
    void refresh();
    void updateDetails();
    void positionOverlays();
};

#endif // CYBOU_QT_PAGES_NETWORKPAGE_H
