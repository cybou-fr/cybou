// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/networkauthoritypage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouui.h>

#include <cybou/node_runtime.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QTimer>
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

void Row(QVBoxLayout* layout, const QString& key, const QString& value, bool mono = false)
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
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (mono) k->setStyleSheet(QStringLiteral("font-family: Consolas, 'Cascadia Mono', monospace;"));
    row->addWidget(k, 0, Qt::AlignTop);
    row->addWidget(v, 1);
    layout->addLayout(row);
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
    auto* hero_layout = new QHBoxLayout{hero};
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
    hero_layout->addLayout(hero_text, 1);
    m_pause = new QPushButton{hero};
    m_pause->setObjectName(QStringLiteral("secondaryButton"));
    m_pause->setProperty("cybouId", QStringLiteral("authorityPause"));
    m_pause->setCursor(Qt::PointingHandCursor);
    hero_layout->addWidget(m_pause, 0, Qt::AlignVCenter);
    m_finalize_now = new QPushButton{tr("Finalize one block"), hero};
    m_finalize_now->setObjectName(QStringLiteral("primaryButton"));
    m_finalize_now->setProperty("cybouId", QStringLiteral("authorityFinalizeNow"));
    m_finalize_now->setCursor(Qt::PointingHandCursor);
    m_finalize_now->setToolTip(tr("Available while finalization is paused"));
    hero_layout->addWidget(m_finalize_now, 0, Qt::AlignVCenter);
    root->addWidget(hero);
    connect(m_pause, &QPushButton::clicked, this, [this] {
        m_model->requestFinalizationPaused(m_model->networkAuthority().finalizer != CybouFinalizerState::Paused);
    });
    connect(m_finalize_now, &QPushButton::clicked, this, [this] { m_model->requestFinalizeNow(); });

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

    // Candidate operations this node executed and will finalize.
    auto* queue = Section(root, tr("Candidate operations"), this,
        tr("Operations this node executed against its finalized state. Each is executed again before signing."));
    m_queue = Rows(queue);
    m_recent = Rows(Section(root, tr("Recently finalized"), this,
        tr("Operations this node saw finalized recently, newest first.")));

    m_totals = Rows(Section(root, tr("Network totals"), this));
    m_peer_rows = Rows(Section(root, tr("Peers"), this,
        tr("Peer heights are their own announcements, not verified state.")));
    m_chain = Rows(Section(root, tr("Chain"), this));
    root->addStretch();

    connect(m_model, &CybouDesktopModel::networkAuthorityChanged, this, [this] { refresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(2000);
    refresh();
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
    const qint64 age = m_seen_at.isValid() ? m_seen_at.secsTo(now) : 0;

    // Finalizer state and controls.
    switch (a.finalizer) {
    case CybouFinalizerState::Finalizing:
        m_finalizer_state->setText(tr("Finalizing"));
        SetTint(m_finalizer_state, Tint::Mint);
        m_finalizer_detail->setText(tr("Valid candidates are executed and signed into a block about every second."));
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
    const bool paused = a.finalizer == CybouFinalizerState::Paused;
    m_pause->setText(paused ? tr("Resume") : tr("Pause"));
    m_pause->setEnabled(a.finalizer == CybouFinalizerState::Finalizing || paused);
    m_finalize_now->setVisible(paused);

    m_height->setText(a.proven ? locale.toString(a.finalized_height) : QStringLiteral("—"));
    m_last_block->setText(!a.proven ? QStringLiteral("—")
        : m_height_advanced_in_view ? (age < 60 ? tr("%1 s ago").arg(age) : relTime(m_seen_at.toLocalTime()))
        : tr("None in this view"));
    m_candidates->setText(locale.toString(a.candidates));
    m_peers->setText(locale.toString(static_cast<qulonglong>(d.peers.size())));
    m_identities->setText(locale.toString(a.identities));
    m_escrow->setText(cybouAmountText(a.storage_escrow));

    ClearLayout(m_queue);
    if (a.candidate_ids.isEmpty()) {
        m_queue->addWidget(MutedText(tr("No candidate operations. The pool is empty."), m_queue->parentWidget()));
    }
    for (int i = 0; i < a.candidate_ids.size(); ++i) {
        if (i == kQueueRows) {
            m_queue->addWidget(MutedText(tr("and %1 more").arg(a.candidate_ids.size() - kQueueRows), m_queue->parentWidget()));
            break;
        }
        Row(m_queue, ShortHex(a.candidate_ids.at(i).toStdString()), tr("waiting for the next block"), true);
    }

    // The runtime's recent status history: finalized operations with their block.
    ClearLayout(m_recent);
    int recent = 0;
    for (auto it = d.operations.rbegin(); it != d.operations.rend() && recent < kQueueRows; ++it) {
        if (it->state != static_cast<std::uint32_t>(cybou::OperationStatusKind::FINALIZED)) continue;
        Row(m_recent, ShortHex(it->operation_id),
            tr("block %1").arg(locale.toString(static_cast<qulonglong>(it->finalized_height))), true);
        ++recent;
    }
    if (recent == 0) m_recent->addWidget(MutedText(tr("Nothing finalized in this session yet."), m_recent->parentWidget()));

    ClearLayout(m_totals);
    Row(m_totals, tr("Spendable Balance"), cybouAmountText(a.total_balance));
    Row(m_totals, tr("System Balance"), cybouAmountText(a.total_system_balance));
    Row(m_totals, tr("Storage escrow"), cybouAmountText(a.storage_escrow));
    Row(m_totals, tr(".cybou names"), tr("%1  ·  %2 commits pending")
        .arg(locale.toString(a.names), locale.toString(a.pending_name_commits)));

    ClearLayout(m_peer_rows);
    if (d.peers.empty()) m_peer_rows->addWidget(MutedText(tr("No peers connected."), m_peer_rows->parentWidget()));
    for (const auto& peer : d.peers) {
        const quint64 lag = d.height > peer.advertised_height ? d.height - peer.advertised_height : 0;
        QString text = lag == 0 ? tr("height %1  ·  in step").arg(locale.toString(static_cast<qulonglong>(peer.advertised_height)))
                                : tr("height %1  ·  %2 behind").arg(locale.toString(static_cast<qulonglong>(peer.advertised_height)))
                                      .arg(locale.toString(lag));
        if (!peer.storage_id.empty()) text += tr("  ·  StorageId %1").arg(ShortHex(peer.storage_id));
        Row(m_peer_rows, QString::fromStdString(peer.endpoint), text);
    }

    ClearLayout(m_chain);
    Row(m_chain, tr("Tip"), QString::fromStdString(d.tip), true);
    Row(m_chain, tr("State root"), QString::fromStdString(d.state_root), true);
    Row(m_chain, tr("Network ID"), QString::fromStdString(d.network_binding), true);
    Row(m_chain, tr("Connection"), cybouConnectionText(m_model->status()));
}
