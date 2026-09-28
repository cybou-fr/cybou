// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/storagepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QBrush>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

/** Remove all items from a layout, deleting nested row layouts and their widgets. */
void clearLayoutDeep(QLayout* layout)
{
    if (!layout) return;
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QLayout* sub = item->layout()) {
            // For a sub-layout the returned item IS the layout itself.
            while (QLayoutItem* sub_item = sub->takeAt(0)) {
                if (QWidget* widget = sub_item->widget()) widget->deleteLater();
                if (sub_item->layout()) sub_item->layout()->deleteLater();
                else delete sub_item;
            }
            sub->deleteLater();
        } else {
            if (QWidget* widget = item->widget()) widget->deleteLater();
            delete item;
        }
    }
}

} // namespace

StoragePage::StoragePage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent},
      m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    // ---- Hero + usage panel ------------------------------------------------
    auto* hero_row = new QHBoxLayout;
    hero_row->setSpacing(16);

    auto* hero = new QFrame{this};
    hero->setObjectName(QStringLiteral("heroHeader"));
    auto* hero_layout = new QVBoxLayout{hero};
    hero_layout->setContentsMargins(30, 26, 30, 26);
    hero_layout->setSpacing(10);
    hero_layout->addWidget(Eyebrow(tr("FILES"), hero));
    hero_layout->addWidget(HeroTitle(tr("Your files, encrypted\nand under your control."), hero));
    hero_layout->addWidget(HeroSubtitle(tr("The Files client will show your names and folders. Storage providers receive encrypted content and opaque identifiers only."), hero));
    auto* chips = new QHBoxLayout;
    chips->setSpacing(8);
    chips->addWidget(Pill(tr("Encrypted objects"), Tint::Mint, hero));
    chips->addWidget(Pill(tr("Private catalog"), Tint::Blue, hero));
    chips->addWidget(Pill(tr("Client setup in progress"), Tint::Neutral, hero));
    chips->addStretch();
    hero_layout->addLayout(chips);
    hero_layout->addStretch();
    hero_row->addWidget(hero, 3);

    auto* usage = new QFrame{this};
    usage->setObjectName(QStringLiteral("heroPanel"));
    auto* usage_layout = new QVBoxLayout{usage};
    usage_layout->setContentsMargins(24, 20, 24, 20);
    usage_layout->setSpacing(8);
    {
        auto* header = new QHBoxLayout;
        auto* icon = new QLabel{usage};
        icon->setPixmap(glyphPixmap(Glyph::Database, {20, 20}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
        header->addWidget(icon, 0, Qt::AlignVCenter);
        auto* title = new QLabel{tr("Storage"), usage};
        title->setObjectName(QStringLiteral("serviceTitle"));
        header->addWidget(title, 0, Qt::AlignVCenter);
        header->addStretch();
        usage_layout->addLayout(header);
        m_usage_value = new QLabel{usage};
        m_usage_value->setObjectName(QStringLiteral("metric"));
        usage_layout->addWidget(m_usage_value);
        auto* meter = new QProgressBar{usage};
        meter->setObjectName(QStringLiteral("usageMeter"));
        meter->setRange(0, 100);
        meter->setValue(0);
        meter->setTextVisible(false);
        usage_layout->addWidget(meter);
        m_usage_caption = MutedText(tr("Files is not connected to the desktop storage service in this build."), usage);
        usage_layout->addWidget(m_usage_caption);
    }
    hero_row->addWidget(usage, 2);
    root->addLayout(hero_row);

    // ---- Three-pane browser -------------------------------------------------
    auto* panes = new QHBoxLayout;
    panes->setSpacing(14);

    // Left rail: Files navigation. Catalog-backed views are not wired yet.
    auto* rail = new QFrame{this};
    rail->setObjectName(QStringLiteral("card"));
    rail->setFixedWidth(224);
    auto* rail_layout = new QVBoxLayout{rail};
    rail_layout->setContentsMargins(14, 14, 14, 14);
    rail_layout->setSpacing(2);
    {
        auto* all_row = new QHBoxLayout;
        all_row->setContentsMargins(8, 6, 8, 6);
        auto* all_icon = new QLabel{rail};
        all_icon->setPixmap(glyphPixmap(Glyph::Folder, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
        all_row->addWidget(all_icon, 0, Qt::AlignVCenter);
        auto* all_label = new QLabel{tr("My files"), rail};
        all_label->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
            .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
        all_row->addWidget(all_label, 1);
        m_all_files_count = new QLabel{rail};
        m_all_files_count->setObjectName(QStringLiteral("pill"));
        m_all_files_count->setProperty("tint", "neutral");
        all_row->addWidget(m_all_files_count, 0, Qt::AlignVCenter);
        rail_layout->addLayout(all_row);

        auto add_source = [this, rail, rail_layout](Glyph glyph, const QString& name, const QString& note) {
            auto* row_widget = new QWidget{rail};
            auto* row = new QHBoxLayout{row_widget};
            row->setContentsMargins(8, 6, 8, 6);
            row->setSpacing(10);
            auto* icon = new QLabel{row_widget};
            icon->setPixmap(glyphPixmap(glyph, {16, 16}, CybouTheme::color(CybouTheme::TEXT_MUTED)));
            row->addWidget(icon, 0, Qt::AlignVCenter);
            auto* label = new QLabel{name, row_widget};
            label->setObjectName(QStringLiteral("mutedText"));
            row->addWidget(label, 1);
            row_widget->setToolTip(note);
            rail_layout->addWidget(row_widget);
        };
        add_source(Glyph::Clock, tr("Recent"), tr("Recent files will be listed here when the Files catalog is connected."));
        add_source(Glyph::Star, tr("Starred"), tr("Starred files will be listed here when the Files catalog is connected."));
        add_source(Glyph::Trash, tr("Trash"), tr("Trashed files will be listed here when the Files catalog is connected."));

        rail_layout->addSpacing(10);
        auto* folders_header = new QLabel{tr("MY FOLDERS"), rail};
        folders_header->setObjectName(QStringLiteral("eyebrow"));
        rail_layout->addWidget(folders_header);
        rail_layout->addWidget(MutedText(tr("Folders appear here when the encrypted Files catalog is connected."), rail));
        rail_layout->addStretch();
    }
    panes->addWidget(rail);

    // Middle: toolbar + object list.
    auto* middle = new QFrame{this};
    middle->setObjectName(QStringLiteral("card"));
    auto* middle_layout = new QVBoxLayout{middle};
    middle_layout->setContentsMargins(14, 14, 14, 14);
    middle_layout->setSpacing(10);
    {
        auto* toolbar = new QHBoxLayout;
        toolbar->setSpacing(8);
        m_search = new QLineEdit{middle};
        m_search->setPlaceholderText(tr("Search files\u2026"));
        m_search->setClearButtonEnabled(true);
        connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildList(); });
        toolbar->addWidget(m_search, 1);
        toolbar->addWidget(IconButton(Glyph::Sliders, middle, tr("Filter (planned)")), 0, Qt::AlignVCenter);
        toolbar->addWidget(IconButton(Glyph::ListView, middle, tr("List view")), 0, Qt::AlignVCenter);
        m_upload = new QPushButton{tr("Upload file\u2026"), middle};
        m_upload->setObjectName(QStringLiteral("primaryButton"));
        m_upload->setIcon(QIcon{glyphPixmap(Glyph::Upload, {16, 16}, QColor{0xffffff})});
        m_upload->setEnabled(false);
        m_upload->setToolTip(tr("The Files catalog and client transfer controller are not connected yet."));
        m_pin = new QPushButton{tr("New folder"), middle};
        m_pin->setObjectName(QStringLiteral("secondaryButton"));
        m_pin->setEnabled(false);
        m_pin->setToolTip(tr("Folder changes require the encrypted Files catalog."));
        toolbar->addWidget(m_upload, 0, Qt::AlignVCenter);
        toolbar->addWidget(m_pin, 0, Qt::AlignVCenter);
        middle_layout->addLayout(toolbar);

        m_list = new QListWidget{middle};
        m_list->setObjectName(QStringLiteral("messageList"));
        m_list->setFocusPolicy(Qt::NoFocus);
        connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
            auto* item = m_list->currentItem();
            if (!item) return;
            const int index = item->data(Qt::UserRole).toInt();
            if (index < 0 || index >= m_objects.size()) return;
            m_selected = index;
            showDetails(index);
        });
        middle_layout->addWidget(m_list, 1);
    }
    panes->addWidget(middle, 3);

    // Right: details panel for the selected object.
    m_details = new QFrame{this};
    m_details->setObjectName(QStringLiteral("card"));
    m_details->setFixedWidth(280);
    auto* details_layout = new QVBoxLayout{m_details};
    details_layout->setContentsMargins(20, 18, 20, 18);
    details_layout->setSpacing(10);
    details_layout->addStretch();
    panes->addWidget(m_details);

    root->addLayout(panes, 1);

    m_gate_hint = MutedText({}, this);
    root->addWidget(m_gate_hint);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });

    refresh();
}

QString StoragePage::replicationText(Replication replication)
{
    switch (replication) {
    case Replication::Planned: return tr("Replication planned");
    case Replication::Placed: return tr("Placed on peers");
    }
    return {};
}

void StoragePage::refresh()
{
    const auto& status = m_model->status();

    qint64 total = 0;
    qint64 prunable = 0;
    for (const StoredObject& object : m_objects) {
        total += object.size;
        if (!object.pinned) prunable += object.size;
    }
    m_all_files_count->setText(QString::number(m_objects.size()));
    m_usage_value->setText(m_objects.isEmpty() ? tr("0 bytes") : tr("%1 bytes").arg(QLocale{}.toString(total)));
    m_usage_caption->setText(m_objects.isEmpty()
        ? tr("Files is not connected to the desktop storage service in this build.")
        : tr("%1 stored locally \u00b7 desktop nodes may drop unpinned copies.").arg(QLocale{}.toString(prunable)));

    // Node-side CAP_STORAGE means this node can serve provider requests. It
    // does not mean the desktop Files catalog or transfer controller exists.
    m_upload->setEnabled(false);
    m_pin->setEnabled(false);
    m_gate_hint->setText(status.account_id.isEmpty()
        ? tr("Create an identity before using Files. The desktop Files client is not connected yet.")
        : tr("Account: %1 \u00b7 desktop Files catalog is not connected yet.").arg(status.account_id));
    rebuildList();
}

void StoragePage::rebuildList()
{
    const QString needle = m_search->text().trimmed().toLower();
    m_list->clear();
    bool has_rows = false;
    for (int i = 0; i < m_objects.size(); ++i) {
        const StoredObject& object = m_objects.at(i);
        if (!needle.isEmpty() && !object.cid.toLower().contains(needle)) continue;
        has_rows = true;
        auto* row = new QFrame{m_list};
        auto* row_layout = new QHBoxLayout{row};
        row_layout->setContentsMargins(10, 9, 10, 9);
        row_layout->setSpacing(10);
        row_layout->addWidget(Chip(Glyph::File, Tint::Blue, row, 34, 17), 0, Qt::AlignTop);

        auto* main = new QVBoxLayout;
        main->setSpacing(4);
        // Identifiers are deliberately truncated: the full CID is available
        // in core; the UI never invents a resolvable address.
        auto* cid = new QLabel{object.cid.left(20) + QStringLiteral("\u2026"), row};
        cid->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
            .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
        auto* badges = new QHBoxLayout;
        badges->setSpacing(6);
        auto* encrypted = new QLabel{tr("Encrypted"), row};
        encrypted->setObjectName(QStringLiteral("pill"));
        encrypted->setProperty("tint", "mint");
        badges->addWidget(encrypted);
        auto* retention = new QLabel{object.pinned ? tr("Pinned") : tr("Prunable"), row};
        retention->setObjectName(QStringLiteral("pill"));
        retention->setProperty("tint", object.pinned ? "blue" : "neutral");
        badges->addWidget(retention);
        auto* replication = new QLabel{replicationText(object.replication), row};
        replication->setObjectName(QStringLiteral("pill"));
        replication->setProperty("tint", object.replication == Replication::Placed ? "mint" : "neutral");
        badges->addWidget(replication);
        badges->addStretch();
        main->addWidget(cid);
        main->addLayout(badges);
        row_layout->addLayout(main, 1);

        auto* side = new QVBoxLayout;
        side->setSpacing(2);
        auto* size = new QLabel{tr("%1 bytes").arg(QLocale{}.toString(object.size)), row};
        size->setObjectName(QStringLiteral("rowMeta"));
        size->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        auto* when = new QLabel{QLocale{}.toString(object.at, QLocale::ShortFormat), row};
        when->setObjectName(QStringLiteral("rowMeta"));
        when->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        side->addWidget(size, 0, Qt::AlignRight);
        side->addWidget(when, 0, Qt::AlignRight);
        row_layout->addLayout(side);

        auto* item = new QListWidgetItem{m_list};
        item->setData(Qt::UserRole, i);
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 60}));
        m_list->setItemWidget(item, row);
    }
    if (!has_rows) {
        auto* item = new QListWidgetItem{m_list};
        item->setFlags(Qt::NoItemFlags);
        item->setText(tr("Your Files catalog is not connected yet.\nFiles will appear here after the encrypted catalog is available in the desktop."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 140});
        item->setData(Qt::UserRole, -1);
    }
}

void StoragePage::showDetails(int index)
{
    // Rebuild the details panel for the selected object.
    clearLayoutDeep(m_details->layout());
    const StoredObject& object = m_objects.at(index);
    auto* layout = qobject_cast<QVBoxLayout*>(m_details->layout());
    layout->setSpacing(10);

    auto* header = new QHBoxLayout;
    header->addWidget(Chip(Glyph::File, Tint::Rose, m_details, 44, 22), 0, Qt::AlignTop);
    auto* title_column = new QVBoxLayout;
    auto* title = new QLabel{object.cid.left(14) + QStringLiteral("\u2026"), m_details};
    title->setStyleSheet(QStringLiteral("font-weight: 800; color: %1; background: transparent; border: none;")
        .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    title_column->addWidget(title);
    auto* kind = new QLabel{tr("Encrypted object"), m_details};
    kind->setObjectName(QStringLiteral("rowSub"));
    kind->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    title_column->addWidget(kind);
    header->addLayout(title_column, 1);
    auto* more = IconButton(Glyph::DotsV, m_details, tr("Object actions"));
    header->addWidget(more, 0, Qt::AlignTop);
    layout->addLayout(header);

    auto add_fact = [this](Glyph glyph, Tint tint, const QString& name, const QString& value) {
        auto* row = ActivityRow(glyph, tint, name, value, {}, m_details);
        return row;
    };
    layout->addWidget(add_fact(Glyph::Lock, Tint::Mint, tr("Encrypted"), tr("Storage providers receive ciphertext only. Recipient sharing is not available yet.")));
    layout->addWidget(add_fact(Glyph::Download, Tint::Blue, tr("Available offline"), object.pinned
        ? tr("Pinned \u2014 this node keeps a local copy.")
        : tr("Prunable \u2014 the local copy may be dropped.")));
    layout->addWidget(add_fact(Glyph::Refresh, Tint::Indigo, tr("Replication"), replicationText(object.replication)));

    auto* divider = new QFrame{m_details};
    divider->setFrameShape(QFrame::HLine);
    divider->setObjectName(QStringLiteral("separator"));
    layout->addWidget(divider);

    auto* meta_caption = new QLabel{tr("DETAILS"), m_details};
    meta_caption->setObjectName(QStringLiteral("eyebrow"));
    layout->addWidget(meta_caption);
    auto add_meta = [this](const QString& key, const QString& value) {
        auto* row = new QHBoxLayout;
        auto* k = new QLabel{key, m_details};
        k->setObjectName(QStringLiteral("rowSub"));
        k->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        k->setFixedWidth(70);
        auto* v = new QLabel{value, m_details};
        v->setObjectName(QStringLiteral("rowTitle"));
        v->setStyleSheet(QStringLiteral("font-weight: 600; background: transparent; border: none;"));
        v->setWordWrap(true);
        v->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(k);
        row->addWidget(v, 1);
        return row;
    };
    layout->addLayout(add_meta(tr("Type"), tr("Opaque object (content-addressed)")));
    layout->addLayout(add_meta(tr("Size"), tr("%1 bytes").arg(QLocale{}.toString(object.size))));
    layout->addLayout(add_meta(tr("Added"), QLocale{}.toString(object.at, QLocale::ShortFormat)));
    layout->addLayout(add_meta(tr("CID"), object.cid));
    layout->addLayout(add_meta(tr("Owner"), tr("You")));
    layout->addStretch();

    auto* actions = new QHBoxLayout;
    auto* share = new QPushButton{tr("Share"), m_details};
    share->setObjectName(QStringLiteral("secondaryButton"));
    share->setEnabled(false);
    share->setToolTip(tr("Recipient sharing is not implemented."));
    auto* download = new QPushButton{tr("Download"), m_details};
    download->setObjectName(QStringLiteral("secondaryButton"));
    download->setEnabled(false);
    download->setToolTip(tr("The desktop Files transfer controller is not connected yet."));
    actions->addWidget(share);
    actions->addWidget(download);
    actions->addStretch();
    layout->addLayout(actions);
}
