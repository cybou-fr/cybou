// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/networkauthoritypage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouui.h>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QTimer>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

QLabel* Tile(QGridLayout* grid, int row, int column, const QString& caption, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(4);
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

QVBoxLayout* Section(QVBoxLayout* root, const QString& title, QWidget* parent)
{
    auto* card = Card(parent);
    auto* layout = new QVBoxLayout{card};
    layout->setContentsMargins(22, 18, 22, 18);
    layout->setSpacing(8);
    layout->addWidget(SectionTitle(title, card));
    auto* body = new QWidget{card};
    auto* rows = new QVBoxLayout{body};
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(8);
    layout->addWidget(body);
    root->addWidget(card);
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

void Row(QVBoxLayout* layout, const QString& key, const QString& value)
{
    QWidget* parent = layout->parentWidget();
    auto* row = new QHBoxLayout;
    row->setSpacing(12);
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("rowSub"));
    k->setFixedWidth(210);
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    row->addWidget(k, 0, Qt::AlignTop);
    row->addWidget(v, 1);
    layout->addLayout(row);
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
    auto* proof = MutedText(tr("This Identity derives this network's genesis PoA key. When its vault is unlocked, "
                               "this desktop can operate the PoA finalizer through a vault-backed signer. "
                               "This page reports finalized state independently validated by this node."), this);
    proof->setObjectName(QStringLiteral("networkAuthorityProof"));
    root->addWidget(proof);

    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    m_height = Tile(grid, 0, 0, tr("Finalized height"), this);
    m_last_block = Tile(grid, 0, 1, tr("Height change observed"), this);
    m_safety = Tile(grid, 0, 2, tr("Safety halt"), this);
    m_identities = Tile(grid, 1, 0, tr("Identities"), this);
    m_names = Tile(grid, 1, 1, tr(".cybou names"), this);
    m_peers = Tile(grid, 1, 2, tr("Connected peers"), this);
    for (int column = 0; column < 3; ++column) grid->setColumnStretch(column, 1);
    root->addLayout(grid);

    m_finality = Section(root, tr("Finality"), this);
    m_economy = Section(root, tr("Supply and pools"), this);
    m_providers = Section(root, tr("Connected peers"), this);
    root->addStretch();

    connect(m_model, &CybouDesktopModel::networkAuthorityChanged, this, [this] { refresh(); });
    auto* ticker = new QTimer{this};
    connect(ticker, &QTimer::timeout, this, [this] { refresh(); });
    ticker->start(5000);
    refresh();
}

void NetworkAuthorityPage::refresh()
{
    const auto& a = m_model->networkAuthority();
    const auto& d = m_model->networkDiagnostics();
    const QLocale locale;
    const auto now = QDateTime::currentDateTimeUtc();
    if (!a.proven) {
        m_seen_authority_height = false;
        m_height_advanced_in_view = false;
        m_seen_at = {};
    } else if (!m_seen_authority_height) {
        m_seen_height = a.finalized_height;
        m_seen_at = now;
        m_seen_authority_height = true;
    } else if (a.finalized_height != m_seen_height) {
        m_seen_height = a.finalized_height;
        m_seen_at = now;
        m_height_advanced_in_view = true;
    }
    const qint64 age = m_seen_at.isValid() ? m_seen_at.secsTo(now) : 0;

    m_height->setText(a.proven ? locale.toString(a.finalized_height) : QStringLiteral("—"));
    m_last_block->setText(a.proven
        ? (m_height_advanced_in_view ? tr("%1 s ago").arg(age) : tr("Not observed yet"))
        : QStringLiteral("—"));
    m_safety->setText(d.safety_halted ? tr("HALTED") : tr("No"));
    m_identities->setText(locale.toString(a.identities));
    m_names->setText(locale.toString(a.names));
    m_peers->setText(QString::number(d.peers.size()));

    ClearLayout(m_finality);
    Row(m_finality, tr("Local PoA signer"), a.signer_enabled ? tr("Enabled in the unlocked vault") : tr("Not enabled"));
    Row(m_finality, tr("Model"), tr("Genesis-bound single-operator hybrid-PQ PoA (centralized finality, not BFT)"));
    Row(m_finality, tr("Finalizer key"), tr("Matches this Identity's recovery phrase (proven from genesis)"));
    Row(m_finality, tr("P2P connection"), cybouConnectionText(m_model->status()));
    Row(m_finality, tr("Height tracking"), m_height_advanced_in_view
        ? tr("Height changed %1 s ago in this view").arg(age)
        : tr("Waiting to observe a height change in this view"));
    Row(m_finality, tr("Tip"), QString::fromStdString(d.tip));
    Row(m_finality, tr("State root"), QString::fromStdString(d.state_root));
    Row(m_finality, tr("Network ID"), QString::fromStdString(d.network_binding));
    Row(m_finality, tr("Pending .cybou name commits"), locale.toString(a.pending_name_commits));

    ClearLayout(m_economy);
    Row(m_economy, tr("Spendable Balance (all Identities)"), cybouAmountText(a.total_balance));
    Row(m_economy, tr("System Balance (all Identities)"), cybouAmountText(a.total_system_balance));
    Row(m_economy, tr("Onboarding pool"), cybouAmountText(a.onboarding_pool));

    ClearLayout(m_providers);
    if (d.peers.empty()) Row(m_providers, tr("Peers"), tr("None connected"));
    for (const auto& peer : d.peers) {
        const quint64 lag = d.height > peer.advertised_height ? d.height - peer.advertised_height : 0;
        const QString provider = peer.storage_id.empty()
            ? tr("No StorageId verified")
            : tr("StorageId verified: %1").arg(QString::fromStdString(peer.storage_id));
        Row(m_providers, QString::fromStdString(peer.endpoint),
            tr("%1  ·  height %2  ·  lag %3").arg(provider, locale.toString(static_cast<quint64>(peer.advertised_height)))
                .arg(lag));
    }
}
