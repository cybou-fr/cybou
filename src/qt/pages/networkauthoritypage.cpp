// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/networkauthoritypage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouui.h>

#include <cybou/node_runtime.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTimeZone>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

constexpr int kQueueRows{12};

QLabel* Tile(QGridLayout* grid, int row, int column, const QString& caption, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 14, 20, 14);
    layout->setSpacing(2);
    auto* label = new QLabel{caption, card};
    label->setObjectName(QStringLiteral("metricCaption"));
    auto* value = new QLabel{card};
    value->setObjectName(QStringLiteral("metric"));
    value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    value->setWordWrap(true);
    value->setMinimumWidth(0);
    value->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(label);
    layout->addWidget(value);
    grid->addWidget(card, row, column);
    return value;
}

/** Card with a title; returns the card's own layout so callers can add controls too. */
QVBoxLayout* Section(QVBoxLayout* root, const QString& title, QWidget* parent, const QString& hint = {})
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(8);
    layout->addWidget(SectionTitle(title, card));
    if (!hint.isEmpty()) layout->addWidget(MutedText(hint, card));
    root->addWidget(card);
    return layout;
}

/** Rebuildable rows inside a section. */
QVBoxLayout* Rows(QVBoxLayout* section)
{
    auto* body = new QWidget{section->parentWidget()};
    auto* rows = new QVBoxLayout{body};
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(6);
    section->addWidget(body);
    return rows;
}

void ClearLayout(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->layout()) ClearLayout(item->layout());
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
}

QLabel* Row(QVBoxLayout* layout, const QString& key, const QString& value, bool mono = false)
{
    QWidget* parent = layout->parentWidget();
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("rowSub"));
    k->setFixedWidth(200);
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    v->setTextFormat(Qt::PlainText);
    v->setMinimumWidth(0);
    v->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (mono) k->setStyleSheet(QStringLiteral("font-family: Consolas, 'Cascadia Mono', monospace;"));
    row->addWidget(k, 0, Qt::AlignTop);
    row->addWidget(v, 1);
    layout->addLayout(row);
    return v;
}

QString ShortHex(const std::string& hex)
{
    const auto text = QString::fromStdString(hex);
    return text.size() > 20 ? text.left(10) + QStringLiteral("…") + text.right(8) : text;
}

void SetTint(QLabel* pill, Tint tint)
{
    pill->setProperty("tint", tintName(tint));
    pill->style()->unpolish(pill);
    pill->style()->polish(pill);
}

} // namespace

NetworkAuthorityPage::NetworkAuthorityPage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    setObjectName(QStringLiteral("CYBOUNetworkAuthority"));
    setMinimumWidth(0);
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(28, 24, 28, 28);
    root->setSpacing(16);

    // Finalizer: the one thing the operator must see first, with its controls.
    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QVBoxLayout{hero};
    hero_layout->setContentsMargins(28, 20, 28, 20);
    hero_layout->setSpacing(18);
    auto* hero_text = new QVBoxLayout;
    hero_text->setSpacing(4);
    auto* title_row = new QHBoxLayout;
    title_row->setSpacing(12);
    title_row->addWidget(HeroTitle(tr("PoA finalizer"), hero));
    m_finalizer_state = Pill({}, Tint::Neutral, hero);
    m_finalizer_state->setProperty("cybouId", QStringLiteral("authorityFinalizerState"));
    title_row->addWidget(m_finalizer_state, 0, Qt::AlignVCenter);
    title_row->addStretch();
    hero_text->addLayout(title_row);
    m_finalizer_detail = HeroSubtitle({}, hero);
    m_finalizer_detail->setWordWrap(true);
    hero_text->addWidget(m_finalizer_detail);
    hero_layout->addLayout(hero_text);
    hero_layout->addWidget(MutedText(tr("Locking the user Vault does not stop an already active PoA finalizer. Pause finalization explicitly before locking if needed. Unlock the Identity to use operator controls."), hero));
    auto* actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_pause = new QPushButton{hero};
    m_pause->setObjectName(QStringLiteral("secondaryButton"));
    m_pause->setProperty("cybouId", QStringLiteral("authorityPause"));
    m_pause->setCursor(Qt::PointingHandCursor);
    actions->addWidget(m_pause);
    m_finalize_now = new QPushButton{tr("Finalize one block"), hero};
    m_finalize_now->setObjectName(QStringLiteral("primaryButton"));
    m_finalize_now->setProperty("cybouId", QStringLiteral("authorityFinalizeNow"));
    m_finalize_now->setCursor(Qt::PointingHandCursor);
    m_finalize_now->setToolTip(tr("Available while finalization is paused"));
    actions->addWidget(m_finalize_now);
    m_settle_storage = new QPushButton{tr("Review storage settlement"), hero};
    m_settle_storage->setObjectName(QStringLiteral("secondaryButton"));
    m_settle_storage->setProperty("cybouId", QStringLiteral("authoritySettleStorage"));
    m_settle_storage->setCursor(Qt::PointingHandCursor);
    m_settle_storage->setToolTip(tr("Pays verified storage service of the next complete period and returns escrow of ended leases"));
    actions->addWidget(m_settle_storage);
    actions->addStretch();
    hero_layout->addLayout(actions);
    root->addWidget(hero);
    connect(m_pause, &QPushButton::clicked, this, [this] {
        if (m_model->networkAuthority().finalizer == CybouFinalizerState::Paused) { m_model->requestFinalizationPaused(false); return; }
        const auto account = m_model->status().account_id;
        QMessageBox review{QMessageBox::Question, tr("Pause finalization"),
            tr("New operations will remain pending until finalization resumes. The Full Node continues networking and storage. Locking the Vault alone does not pause finalization."),
            QMessageBox::NoButton, this};
        review.setObjectName(QStringLiteral("authorityPauseReview"));
        auto* confirm = review.addButton(tr("Pause finalization"), QMessageBox::AcceptRole);
        confirm->setObjectName(QStringLiteral("authorityConfirmPause"));
        auto* cancel = review.addButton(QMessageBox::Cancel);
        cancel->setText(tr("Cancel"));
        review.setDefaultButton(cancel);
        const auto invalidate = [&] {
            const auto& a = m_model->networkAuthority();
            if (m_model->status().identity_state != CybouIdentityState::Active || m_model->status().account_id != account || !a.proven || !a.signer_enabled || a.finalizer != CybouFinalizerState::Finalizing) review.reject();
        };
        connect(m_model, &CybouDesktopModel::statusChanged, &review, invalidate);
        connect(m_model, &CybouDesktopModel::networkAuthorityChanged, &review, invalidate);
        review.exec();
        if (review.clickedButton() == confirm && m_model->status().account_id == account) m_model->requestFinalizationPaused(true);
    });
    connect(m_finalize_now, &QPushButton::clicked, this, [this] { m_model->requestFinalizeNow(); });
    connect(m_settle_storage, &QPushButton::clicked, this, [this] { m_model->requestStorageSettlement(); });

    // Live network health.
    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    m_height = Tile(grid, 0, 0, tr("Finalized height"), this);
    m_last_block = Tile(grid, 0, 1, tr("Last new block"), this);
    m_candidates = Tile(grid, 0, 2, tr("Waiting for next block"), this);
    m_peers = Tile(grid, 1, 0, tr("Connected peers"), this);
    m_identities = Tile(grid, 1, 1, tr("Identities"), this);
    m_escrow = Tile(grid, 1, 2, tr("Storage escrow"), this);
    for (int column = 0; column < 3; ++column) grid->setColumnStretch(column, 1);
    root->addLayout(grid);

    // Verified block & operation explorer
    auto* explorer_sec = Section(root, tr("Verified explorer"), this,
        tr("Paginated in-memory index of candidate operations and verified blocks. 10 items per page."));
    auto* filter_bar = new QHBoxLayout;
    m_explorer_filter_edit = new QLineEdit{explorer_sec->parentWidget()};
    m_explorer_filter_edit->setObjectName(QStringLiteral("authorityExplorerFilter"));
    m_explorer_filter_edit->setPlaceholderText(tr("Filter by operation ID, block height, or status…"));
    m_explorer_filter_edit->setClearButtonEnabled(true);
    m_explorer_filter_edit->setMaxLength(128);
    filter_bar->addWidget(m_explorer_filter_edit);
    explorer_sec->addLayout(filter_bar);

    m_explorer_table = new QTableWidget{explorer_sec->parentWidget()};
    m_explorer_table->setObjectName(QStringLiteral("authorityExplorerTable"));
    m_explorer_table->setColumnCount(4);
    m_explorer_table->setHorizontalHeaderLabels({
        tr("Phase / Height"),
        tr("Identifier"),
        tr("Classification"),
        tr("Status")
    });
    m_explorer_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_explorer_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_explorer_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_explorer_table->setShowGrid(false);
    m_explorer_table->verticalHeader()->setVisible(false);
    m_explorer_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_explorer_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_explorer_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_explorer_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_explorer_table->setMinimumHeight(240);
    explorer_sec->addWidget(m_explorer_table);

    auto* pager_bar = new QHBoxLayout;
    m_explorer_prev = new QPushButton{tr("Previous"), explorer_sec->parentWidget()};
    m_explorer_prev->setObjectName(QStringLiteral("authorityExplorerPrev"));
    m_explorer_prev->setCursor(Qt::PointingHandCursor);
    m_explorer_page_label = new QLabel{explorer_sec->parentWidget()};
    m_explorer_page_label->setObjectName(QStringLiteral("authorityExplorerPageLabel"));
    m_explorer_next = new QPushButton{tr("Next"), explorer_sec->parentWidget()};
    m_explorer_next->setObjectName(QStringLiteral("authorityExplorerNext"));
    m_explorer_next->setCursor(Qt::PointingHandCursor);
    pager_bar->addWidget(m_explorer_prev);
    pager_bar->addStretch();
    pager_bar->addWidget(m_explorer_page_label);
    pager_bar->addStretch();
    pager_bar->addWidget(m_explorer_next);
    explorer_sec->addLayout(pager_bar);

    m_explorer_detail_card = Card(explorer_sec->parentWidget());
    m_explorer_detail_card->setObjectName(QStringLiteral("authorityExplorerDetail"));
    m_explorer_detail_layout = new QVBoxLayout{m_explorer_detail_card};
    m_explorer_detail_layout->setContentsMargins(16, 12, 16, 12);
    m_explorer_detail_layout->setSpacing(6);
    m_explorer_detail_layout->addWidget(SectionTitle(tr("Selected item details"), m_explorer_detail_card));
    explorer_sec->addWidget(m_explorer_detail_card);

    // Off-chain evidence & signer safety
    auto* evidence_sec = Section(root, tr("Off-chain evidence & signer safety"), this,
        tr("Safety journals, settlement readiness, and operational signer status."));
    auto* evidence_rows = Rows(evidence_sec);
    m_safety_journal = Row(evidence_rows, tr("Signing safety"), QStringLiteral("—"));
    m_settlement_readiness = Row(evidence_rows, tr("Settlement readiness"), QStringLiteral("—"));
    m_read_only_notice = Row(evidence_rows, tr("Signer authority"), QStringLiteral("—"));

    // Candidate operations this node executed and will finalize.
    auto* queue = Section(root, tr("Candidate operations"), this,
        tr("Operations this node executed against its finalized state. Each is executed again before signing."));
    m_queue = Rows(queue);
    m_recent = Rows(Section(root, tr("Recently finalized"), this,
        tr("Operations this node saw finalized recently, newest first.")));

    m_totals = Rows(Section(root, tr("Canonical state root commitments"), this,
        tr("Account and storage values cryptographically committed by the latest state root.")));
    m_total_spendable = Row(m_totals, tr("Spendable Balance"), QStringLiteral("—"));
    m_total_system = Row(m_totals, tr("System Balance"), QStringLiteral("—"));
    m_total_escrow = Row(m_totals, tr("Storage escrow"), QStringLiteral("—"));
    m_total_names = Row(m_totals, tr(".cybou names"), QStringLiteral("—"));

    m_peer_rows = Rows(Section(root, tr("Peers"), this,
        tr("Peer heights are their own announcements, not verified state.")));

    m_chain = Rows(Section(root, tr("Chain"), this));
    m_chain_tip = Row(m_chain, tr("Tip"), QStringLiteral("—"), true);
    m_chain_state_root = Row(m_chain, tr("State root"), QStringLiteral("—"), true);
    m_chain_network_id = Row(m_chain, tr("Network binding"), QStringLiteral("—"), true);
    m_chain_connection = Row(m_chain, tr("Connection"), QStringLiteral("—"));

    root->addStretch();

    connect(m_explorer_filter_edit, &QLineEdit::textChanged, this, [this](const QString& text) {
        setExplorerFilter(text);
    });
    connect(m_explorer_prev, &QPushButton::clicked, this, [this] {
        if (m_explorer_page > 1) {
            --m_explorer_page;
            updateExplorerPage();
        }
    });
    connect(m_explorer_next, &QPushButton::clicked, this, [this] {
        const int total_pages = qMax(1, (m_filtered_items.size() + kPageSize - 1) / kPageSize);
        if (m_explorer_page < total_pages) {
            ++m_explorer_page;
            updateExplorerPage();
        }
    });
    connect(m_explorer_table, &QTableWidget::itemSelectionChanged, this, [this] {
        onExplorerSelectionChanged();
    });

    connect(m_model, &CybouDesktopModel::networkAuthorityChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { updateAgeLabel(); });
    ticker->start(2000);
    refresh();
}

void NetworkAuthorityPage::updateAgeLabel()
{
    const auto& a = m_model->networkAuthority();
    const auto now = QDateTime::currentDateTimeUtc();
    const qint64 age = m_seen_at.isValid() ? m_seen_at.secsTo(now) : 0;
    m_last_block->setText(!a.proven ? QStringLiteral("—")
        : m_height_advanced_in_view ? (age < 60 ? tr("%1 s ago").arg(age) : relTime(m_seen_at.toLocalTime()))
        : tr("None in this view"));
}

void NetworkAuthorityPage::refresh()
{
    const auto& a = m_model->networkAuthority();
    const auto& d = m_model->networkDiagnostics();
    const QLocale locale;
    const auto now = QDateTime::currentDateTimeUtc();
    if (!a.proven) {
        m_seen_at = {};
        m_height_advanced_in_view = false;
    } else if (!m_seen_at.isValid()) {
        m_seen_height = a.finalized_height;
        m_seen_at = now;
    } else if (a.finalized_height != m_seen_height) {
        m_seen_height = a.finalized_height;
        m_seen_at = now;
        m_height_advanced_in_view = true;
    }
    updateAgeLabel();

    // Finalizer state and controls.
    switch (a.finalizer) {
    case CybouFinalizerState::Finalizing:
        m_finalizer_state->setText(tr("Finalizing"));
        SetTint(m_finalizer_state, Tint::Mint);
        m_finalizer_detail->setText(tr("Valid candidates are executed and signed into a block as soon as one is waiting."));
        break;
    case CybouFinalizerState::Paused:
        m_finalizer_state->setText(tr("Paused"));
        SetTint(m_finalizer_state, Tint::Amber);
        m_finalizer_detail->setText(tr("No blocks are produced. Candidates wait in the pool; finalize one block "
                                       "on demand or resume."));
        break;
    case CybouFinalizerState::SafetyHalt:
        m_finalizer_state->setText(tr("Safety halt"));
        SetTint(m_finalizer_state, Tint::Rose);
        m_finalizer_detail->setText(tr("Signing safety stopped finalization fail-closed. Inspect the signing "
                                       "journal and evidence before any further signing."));
        break;
    case CybouFinalizerState::SignerUnavailable:
        m_finalizer_state->setText(tr("Signer unavailable"));
        SetTint(m_finalizer_state, Tint::Neutral);
        m_finalizer_detail->setText(tr("The PoA signer is not active. Unlock the vault of this Identity to finalize."));
        break;
    }

    const bool is_authorized = a.proven && a.signer_enabled && m_model->status().identity_state == CybouIdentityState::Active;
    const bool paused = a.finalizer == CybouFinalizerState::Paused;
    m_pause->setText(paused ? tr("Resume finalization") : tr("Pause finalization"));
    m_pause->setEnabled(is_authorized && (a.finalizer == CybouFinalizerState::Finalizing || paused));
    m_finalize_now->setVisible(paused);
    m_finalize_now->setEnabled(is_authorized && paused);
    m_settle_storage->setEnabled(is_authorized && a.settlement_due && a.finalizer != CybouFinalizerState::SafetyHalt);

    m_height->setText(a.proven ? locale.toString(a.finalized_height) : QStringLiteral("—"));
    m_candidates->setText(locale.toString(a.candidates));
    m_peers->setText(locale.toString(static_cast<qulonglong>(d.peers.size())));
    m_identities->setText(locale.toString(a.identities));
    m_escrow->setText(cybouAmountText(a.storage_escrow));

    // Off-chain evidence & signer safety status
    m_safety_journal->setText(a.safety_journal_status.isEmpty() ? tr("Unknown: no local signing journal observation is available.") : a.safety_journal_status);
    m_settlement_readiness->setText(a.next_settlement_due_utc == 0 ? tr("Unknown: no settlement period is available.") :
        tr("Period %1 · %2 UTC. Review prepares actual payout entries from local off-chain storage observations. Provider independence and network-wide audit quality are not established.")
            .arg(a.next_settlement_period).arg(QDateTime::fromSecsSinceEpoch(a.next_settlement_due_utc, QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    if (is_authorized) {
        m_read_only_notice->setText(tr("Authorized signer: Genesis-authorized PoA signing key is active. Finalization and settlement controls are enabled."));
    } else {
        m_read_only_notice->setText(tr("Read-only console: Active signing key is not unlocked or authorized for this network. Finalization and settlement actions are restricted."));
    }

    if (a.candidate_ids != m_cached_candidate_ids) {
        m_cached_candidate_ids = a.candidate_ids;
        ClearLayout(m_queue);
        if (a.candidate_ids.isEmpty()) {
            m_queue->addWidget(MutedText(tr("Idle chain: Pool is empty (0 candidates). Blocks are produced on demand as operations arrive, not on an idle empty-block timer."), m_queue->parentWidget()));
        }
        for (int i = 0; i < a.candidate_ids.size(); ++i) {
            if (i == kQueueRows) {
                m_queue->addWidget(MutedText(tr("and %1 more").arg(a.candidate_ids.size() - kQueueRows), m_queue->parentWidget()));
                break;
            }
            Row(m_queue, ShortHex(a.candidate_ids.at(i).toStdString()), tr("waiting for the next block"), true);
        }
    }

    std::vector<std::pair<std::string, quint64>> recent_ops;
    int recent = 0;
    for (auto it = d.operations.rbegin(); it != d.operations.rend() && recent < kQueueRows; ++it) {
        if (it->state != static_cast<std::uint32_t>(cybou::OperationStatusKind::FINALIZED)) continue;
        recent_ops.emplace_back(it->operation_id, static_cast<quint64>(it->finalized_height));
        ++recent;
    }
    if (recent_ops != m_cached_recent) {
        m_cached_recent = recent_ops;
        ClearLayout(m_recent);
        for (const auto& [op_id, blk_height] : recent_ops) {
            Row(m_recent, ShortHex(op_id),
                tr("block %1").arg(locale.toString(static_cast<qulonglong>(blk_height))), true);
        }
        if (recent_ops.empty()) {
            m_recent->addWidget(MutedText(tr("Nothing finalized in this session yet."), m_recent->parentWidget()));
        }
    }

    m_total_spendable->setText(cybouAmountText(a.total_balance));
    m_total_system->setText(cybouAmountText(a.total_system_balance));
    m_total_escrow->setText(cybouAmountText(a.storage_escrow));
    m_total_names->setText(tr("%1  ·  %2 commits pending")
        .arg(locale.toString(a.names), locale.toString(a.pending_name_commits)));

    std::vector<std::string> peer_sig{std::to_string(d.height), d.initialized ? "initialized" : "unknown"};
    for (const auto& peer : d.peers) {
        peer_sig.push_back(peer.endpoint + ":" + std::to_string(peer.advertised_height) + ":" + peer.storage_id);
    }
    if (peer_sig != m_cached_peers) {
        m_cached_peers = peer_sig;
        ClearLayout(m_peer_rows);
        if (d.peers.empty()) m_peer_rows->addWidget(MutedText(tr("No peers connected."), m_peer_rows->parentWidget()));
        for (const auto& peer : d.peers) {
            const quint64 lag = d.height > peer.advertised_height ? d.height - peer.advertised_height : 0;
            QString text = tr("Advertised height %1 (unverified)").arg(locale.toString(static_cast<qulonglong>(peer.advertised_height)));
            if (d.initialized) text += peer.advertised_height > d.height
                ? tr(" · Ahead %1").arg(locale.toString(peer.advertised_height-d.height))
                : tr(" · Behind %1").arg(locale.toString(lag));
            if (!peer.storage_id.empty()) text += tr("  ·  StorageId %1").arg(ShortHex(peer.storage_id));
            Row(m_peer_rows, QString::fromStdString(peer.endpoint), text);
        }
    }

    m_chain_tip->setText(QString::fromStdString(d.tip));
    m_chain_state_root->setText(QString::fromStdString(d.state_root));
    m_chain_network_id->setText(QString::fromStdString(d.network_binding));
    m_chain_connection->setText(cybouConnectionText(m_model->status()));

    rebuildExplorerItems();
}

void NetworkAuthorityPage::rebuildExplorerItems()
{
    const auto selected_id = m_selected_explorer_index >= 0 && m_selected_explorer_index < m_filtered_items.size()
        ? m_filtered_items[m_selected_explorer_index].identifier : QString{};
    const auto& a = m_model->networkAuthority();
    const auto& d = m_model->networkDiagnostics();
    const QLocale locale;

    m_all_items.clear();

    // 1. Candidate pool items
    for (const auto& cid : a.candidate_ids) {
        CybouExplorerItem item;
        item.phase_or_height = tr("Candidate (volatile)");
        item.identifier = cid;
        item.classification = tr("Candidate operation");
        item.status = tr("Waiting for next block");
        item.is_candidate = true;
        item.height = 0;
        m_all_items.append(item);
    }

    // 2. Finalized operations
    for (auto it = d.operations.rbegin(); it != d.operations.rend(); ++it) {
        if (it->state != static_cast<std::uint32_t>(cybou::OperationStatusKind::FINALIZED)) continue;
        CybouExplorerItem item;
        item.height = static_cast<quint64>(it->finalized_height);
        item.phase_or_height = (it->finalized_height > 0)
            ? tr("Block %1").arg(locale.toString(static_cast<qulonglong>(item.height)))
            : tr("Pending");
        item.identifier = QString::fromStdString(it->operation_id);
        item.classification = tr("Verified operation");
        item.status = (it->state == static_cast<std::uint32_t>(cybou::OperationStatusKind::FINALIZED))
            ? tr("Finalized")
            : tr("Submitted");
        item.is_candidate = false;
        m_all_items.append(item);
    }

    // 3. Finalized block header tip if observed
    if (d.initialized && d.height > 0 && !d.tip.empty()) {
        CybouExplorerItem block_item;
        block_item.height = static_cast<quint64>(d.height);
        block_item.phase_or_height = tr("Block %1 (Tip)").arg(locale.toString(static_cast<qulonglong>(d.height)));
        block_item.identifier = QString::fromStdString(d.tip);
        block_item.classification = tr("Finalized block");
        block_item.status = tr("Finalized by PoA key");
        block_item.is_candidate = false;
        m_all_items.append(block_item);
    }

    // Apply filter
    const QString filter = m_explorer_filter_edit ? m_explorer_filter_edit->text().trimmed() : QString{};
    m_filtered_items.clear();
    for (const auto& item : m_all_items) {
        if (filter.isEmpty() ||
            item.identifier.contains(filter, Qt::CaseInsensitive) ||
            item.phase_or_height.contains(filter, Qt::CaseInsensitive) ||
            item.classification.contains(filter, Qt::CaseInsensitive) ||
            item.status.contains(filter, Qt::CaseInsensitive)) {
            m_filtered_items.append(item);
        }
    }

    m_selected_explorer_index = -1;
    if (!selected_id.isEmpty()) for (int i=0;i<m_filtered_items.size();++i) {
        if (m_filtered_items[i].identifier == selected_id) { m_selected_explorer_index = i; m_explorer_page = i/kPageSize+1; break; }
    }
    updateExplorerPage();
}

void NetworkAuthorityPage::updateExplorerPage()
{
    const int total_items = m_filtered_items.size();
    const int total_pages = qMax(1, (total_items + kPageSize - 1) / kPageSize);
    m_explorer_page = qBound(1, m_explorer_page, total_pages);

    m_explorer_prev->setEnabled(m_explorer_page > 1);
    m_explorer_next->setEnabled(m_explorer_page < total_pages);
    m_explorer_page_label->setText(tr("Page %1 of %2 (%3 items)")
        .arg(m_explorer_page).arg(total_pages).arg(total_items));

    const int start_index = (m_explorer_page - 1) * kPageSize;
    const int end_index = qMin(total_items, start_index + kPageSize);
    const int page_count = qMax(0, end_index - start_index);

    m_explorer_table->blockSignals(true);
    m_explorer_table->setRowCount(page_count);
    for (int i = 0; i < page_count; ++i) {
        const auto& item = m_filtered_items.at(start_index + i);
        auto* cell_phase = new QTableWidgetItem{item.phase_or_height};
        auto* cell_id = new QTableWidgetItem{ShortHex(item.identifier.toStdString())};
        cell_id->setFont(QFont(QStringLiteral("Consolas"), 9));
        auto* cell_class = new QTableWidgetItem{item.classification};
        auto* cell_status = new QTableWidgetItem{item.status};

        m_explorer_table->setItem(i, 0, cell_phase);
        m_explorer_table->setItem(i, 1, cell_id);
        m_explorer_table->setItem(i, 2, cell_class);
        m_explorer_table->setItem(i, 3, cell_status);
    }

    if (m_selected_explorer_index >= start_index && m_selected_explorer_index < end_index) {
        m_explorer_table->selectRow(m_selected_explorer_index - start_index);
    } else {
        m_explorer_table->clearSelection();
    }
    m_explorer_table->blockSignals(false);

    updateExplorerDetails();
}

void NetworkAuthorityPage::updateExplorerDetails()
{
    ClearLayout(m_explorer_detail_layout);
    m_explorer_detail_layout->addWidget(SectionTitle(tr("Selected item details"), m_explorer_detail_card));

    if (m_selected_explorer_index >= 0 && m_selected_explorer_index < m_filtered_items.size()) {
        const auto& item = m_filtered_items.at(m_selected_explorer_index);

        auto* id_row = Row(m_explorer_detail_layout, tr("Identifier"), item.identifier, true);
        id_row->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);

        Row(m_explorer_detail_layout, tr("Phase / Height"), item.phase_or_height);
        Row(m_explorer_detail_layout, tr("Classification"), item.classification);
        Row(m_explorer_detail_layout, tr("Status"), item.status);

        const QString provenance = item.is_candidate
            ? tr("Volatile candidate in local pool. Executed against finalized state; re-executed before block signing.")
            : tr("Committed in finalized PoA block. Cryptographically verified against latest state root.");
        Row(m_explorer_detail_layout, tr("Verification notes"), provenance);
    } else {
        auto* hint = MutedText(tr("Select an operation or block in the explorer table to inspect its full un-truncated identifier and verification status."), m_explorer_detail_card);
        m_explorer_detail_layout->addWidget(hint);
    }
}

void NetworkAuthorityPage::onExplorerSelectionChanged()
{
    const int row = m_explorer_table->currentRow();
    if (row >= 0) {
        const int global_index = (m_explorer_page - 1) * kPageSize + row;
        if (global_index >= 0 && global_index < m_filtered_items.size()) {
            m_selected_explorer_index = global_index;
        } else {
            m_selected_explorer_index = -1;
        }
    } else {
        m_selected_explorer_index = -1;
    }
    updateExplorerDetails();
}

void NetworkAuthorityPage::selectExplorerItem(int index)
{
    if (index >= 0 && index < m_filtered_items.size()) {
        m_selected_explorer_index = index;
        m_explorer_page = (index / kPageSize) + 1;
        updateExplorerPage();
        const int row = index % kPageSize;
        m_explorer_table->selectRow(row);
    } else {
        m_selected_explorer_index = -1;
        if (m_explorer_table) m_explorer_table->clearSelection();
        updateExplorerDetails();
    }
}

void NetworkAuthorityPage::setExplorerFilter(const QString& filter)
{
    if (m_explorer_filter_edit && m_explorer_filter_edit->text() != filter) {
        m_explorer_filter_edit->setText(filter);
    }
    m_filtered_items.clear();
    const QString trimmed = filter.trimmed();
    for (const auto& item : m_all_items) {
        if (trimmed.isEmpty() ||
            item.identifier.contains(trimmed, Qt::CaseInsensitive) ||
            item.phase_or_height.contains(trimmed, Qt::CaseInsensitive) ||
            item.classification.contains(trimmed, Qt::CaseInsensitive) ||
            item.status.contains(trimmed, Qt::CaseInsensitive)) {
            m_filtered_items.append(item);
        }
    }
    m_explorer_page = 1;
    m_selected_explorer_index = -1;
    updateExplorerPage();
}
