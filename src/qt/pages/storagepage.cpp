// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/storagepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace CybouUi;

namespace {

constexpr int kIdRole = Qt::UserRole + 1;

enum Column { NameColumn = 0, SizeColumn, ModifiedColumn, StatusColumn, ColumnCount };

QString ViewName(StoragePage::View view)
{
    switch (view) {
    case StoragePage::View::MyFiles: return StoragePage::tr("My files");
    case StoragePage::View::Recent: return StoragePage::tr("Recent");
    case StoragePage::View::Starred: return StoragePage::tr("Starred");
    case StoragePage::View::Trash: return StoragePage::tr("Trash");
    }
    return {};
}

Glyph ViewGlyph(StoragePage::View view)
{
    switch (view) {
    case StoragePage::View::MyFiles: return Glyph::Folder;
    case StoragePage::View::Recent: return Glyph::Clock;
    case StoragePage::View::Starred: return Glyph::Star;
    case StoragePage::View::Trash: return Glyph::Trash;
    }
    return Glyph::Folder;
}

Glyph FileGlyph(const CybouFileItem& item)
{
    if (item.folder) return Glyph::Folder;
    const QString lower = item.name.toLower();
    for (const char* ext : {".jpg", ".jpeg", ".png", ".gif", ".webp", ".heic"}) {
        if (lower.endsWith(QLatin1String{ext})) return Glyph::Image;
    }
    return Glyph::FileText;
}

QRgb StateColor(CybouContentState state)
{
    switch (state) {
    case CybouContentState::Protected: return CybouTheme::BRAND_TEAL_DARK;
    case CybouContentState::Preparing:
    case CybouContentState::WaitingForConfirmation:
    case CybouContentState::Securing: return CybouTheme::AMBER;
    case CybouContentState::TemporarilyUnavailable:
    case CybouContentState::NeedsAttention: return CybouTheme::ROSE;
    case CybouContentState::Local: return CybouTheme::TEXT_MUTED;
    }
    return CybouTheme::TEXT_MUTED;
}

QString ModifiedText(const QDateTime& when)
{
    const QDateTime now = QDateTime::currentDateTime();
    if (!when.isValid()) return {};
    if (when.date() == now.date()) return StoragePage::tr("Today");
    if (when.date() == now.date().addDays(-1)) return StoragePage::tr("Yesterday");
    return QLocale{QLocale::English}.toString(when, QStringLiteral("MMM d"));
}

QToolButton* ToggleButton(Glyph glyph, const QString& tooltip, QWidget* parent)
{
    auto* button = new QToolButton{parent};
    button->setObjectName(QStringLiteral("iconButton"));
    button->setCheckable(true);
    button->setAutoRaise(true);
    button->setFixedSize(36, 32);
    button->setToolTip(tooltip);
    button->setAccessibleName(tooltip);
    button->setIcon(QIcon{glyphPixmap(glyph, {18, 18}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))});
    return button;
}

} // namespace

StoragePage::StoragePage(CybouDesktopModel* model, std::function<void()> home_requested, QWidget* parent)
    : QWidget{parent}, m_model{model}, m_home_requested{std::move(home_requested)}
{
    setObjectName(QStringLiteral("filesPage"));
    setMinimumWidth(0);
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    // ---- Navigation rail -----------------------------------------------------
    auto* rail = new QWidget{this};
    rail->setFixedWidth(196);
    auto* rail_layout = new QVBoxLayout{rail};
    rail_layout->setContentsMargins(0, 0, 0, 0);
    rail_layout->setSpacing(10);
    m_new = new QPushButton{tr("New"), rail};
    m_new->setObjectName(QStringLiteral("primaryButton"));
    m_new->setProperty("cybouId", QStringLiteral("filesNew"));
    m_new->setIcon(QIcon{glyphPixmap(Glyph::Plus, {18, 18}, QColor{Qt::white})});
    m_new->setMinimumHeight(44);
    m_new->setCursor(Qt::PointingHandCursor);
    m_new->setMenu(new QMenu{m_new});
    rail_layout->addWidget(m_new);
    m_nav = new QListWidget{rail};
    m_nav->setObjectName(QStringLiteral("folderList"));
    m_nav->setAccessibleName(tr("Files navigation"));
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setFixedHeight(4 * 40 + 4);
    for (int i = 0; i <= static_cast<int>(View::Trash); ++i) {
        const auto view = static_cast<View>(i);
        auto* item = new QListWidgetItem{QIcon{glyphPixmap(ViewGlyph(view), {18, 18},
            CybouTheme::color(CybouTheme::TEXT_SECONDARY))}, ViewName(view), m_nav};
        item->setSizeHint(QSize{0, 40});
    }
    rail_layout->addWidget(m_nav);
    rail_layout->addSpacing(8);
    auto* storage_title = new QLabel{tr("Storage"), rail};
    storage_title->setObjectName(QStringLiteral("eyebrow"));
    rail_layout->addWidget(storage_title);
    m_usage_bar = new QProgressBar{rail};
    m_usage_bar->setObjectName(QStringLiteral("usageMeter"));
    m_usage_bar->setTextVisible(false);
    m_usage_bar->setFixedHeight(6);
    m_usage_bar->setRange(0, 1000);
    rail_layout->addWidget(m_usage_bar);
    m_usage_text = MutedText({}, rail);
    rail_layout->addWidget(m_usage_text);
    rail_layout->addStretch();
    root->addWidget(rail);

    // ---- Main area -------------------------------------------------------------
    auto* main = new QFrame{this};
    main->setObjectName(QStringLiteral("card"));
    main->setMinimumWidth(0);
    auto* main_layout = new QVBoxLayout{main};
    main_layout->setContentsMargins(16, 12, 16, 10);
    main_layout->setSpacing(10);

    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(8);
    m_up = new QPushButton{main};
    m_up->setObjectName(QStringLiteral("softButton"));
    m_up->setIcon(QIcon{glyphPixmap(Glyph::ChevronLeft, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    m_up->setToolTip(tr("Back to parent folder"));
    m_up->setAccessibleName(tr("Back to parent folder"));
    toolbar->addWidget(m_up);
    m_title = new QLabel{main};
    m_title->setObjectName(QStringLiteral("sectionTitle"));
    toolbar->addWidget(m_title);
    toolbar->addStretch();
    m_search = new QLineEdit{main};
    m_search->setObjectName(QStringLiteral("filesSearch"));
    m_search->setPlaceholderText(tr("Search files"));
    m_search->setAccessibleName(tr("Search files"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(QIcon{glyphPixmap(Glyph::Search, {16, 16}, CybouTheme::color(CybouTheme::TEXT_MUTED))},
        QLineEdit::LeadingPosition);
    m_search->setMinimumHeight(36);
    m_search->setMaximumWidth(340);
    m_search->setMinimumWidth(160);
    toolbar->addWidget(m_search, 1);
    m_list_toggle = ToggleButton(Glyph::ListView, tr("List view"), main);
    m_grid_toggle = ToggleButton(Glyph::GridView, tr("Grid view"), main);
    m_list_toggle->setChecked(true);
    toolbar->addWidget(m_list_toggle);
    toolbar->addWidget(m_grid_toggle);
    main_layout->addLayout(toolbar);

    m_banner = new QFrame{main};
    m_banner->setObjectName(QStringLiteral("identityBanner"));
    auto* banner_layout = new QHBoxLayout{m_banner};
    banner_layout->setContentsMargins(12, 8, 12, 8);
    m_banner_text = MutedText({}, m_banner);
    banner_layout->addWidget(m_banner_text, 1);
    main_layout->addWidget(m_banner);

    m_views = new QStackedWidget{main};
    m_table = new QTreeWidget{m_views};
    m_table->setObjectName(QStringLiteral("filesTable"));
    m_table->setAccessibleName(tr("Files"));
    m_table->setColumnCount(ColumnCount);
    m_table->setHeaderLabels({tr("Name"), tr("Size"), tr("Modified"), tr("Status")});
    m_table->setRootIsDecorated(false);
    m_table->setUniformRowHeights(true);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setIconSize({20, 20});
    m_table->header()->setStretchLastSection(false);
    m_table->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    m_table->header()->setSectionResizeMode(SizeColumn, QHeaderView::Fixed);
    m_table->header()->setSectionResizeMode(ModifiedColumn, QHeaderView::Fixed);
    m_table->header()->setSectionResizeMode(StatusColumn, QHeaderView::Fixed);
    m_table->header()->resizeSection(SizeColumn, 100);
    m_table->header()->resizeSection(ModifiedColumn, 120);
    m_table->header()->resizeSection(StatusColumn, 170);
    m_table->setStyleSheet(QStringLiteral("QTreeWidget::item { height: 40px; }"));
    m_views->addWidget(m_table);

    m_tiles = new QListWidget{m_views};
    m_tiles->setObjectName(QStringLiteral("filesGrid"));
    m_tiles->setAccessibleName(tr("Files"));
    m_tiles->setViewMode(QListView::IconMode);
    m_tiles->setResizeMode(QListView::Adjust);
    m_tiles->setMovement(QListView::Static);
    m_tiles->setWrapping(true);
    m_tiles->setSpacing(10);
    m_tiles->setIconSize({48, 48});
    m_tiles->setGridSize({164, 128});
    m_tiles->setWordWrap(true);
    m_tiles->setFrameShape(QFrame::NoFrame);
    m_tiles->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_tiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_views->addWidget(m_tiles);
    main_layout->addWidget(m_views, 1);
    m_empty = MutedText({}, main);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setMinimumHeight(120);
    main_layout->addWidget(m_empty);
    root->addWidget(main, 1);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) setView(static_cast<View>(row));
    });
    connect(m_up, &QPushButton::clicked, this, [this] {
        for (const auto& item : m_model->fileItems()) {
            if (item.id == m_folder) {
                openFolder(item.parent_id);
                return;
            }
        }
        openFolder({});
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuild(); });
    connect(m_list_toggle, &QToolButton::clicked, this, [this] { setGridMode(false); });
    connect(m_grid_toggle, &QToolButton::clicked, this, [this] { setGridMode(true); });
    connect(m_table, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        activate(item->data(NameColumn, kIdRole).toString());
    });
    connect(m_tiles, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        activate(item->data(kIdRole).toString());
    });
    connect(m_model, &CybouDesktopModel::filesChanged, this, [this] { rebuild(); });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refreshChrome(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refreshChrome(); });

    m_nav->setCurrentRow(0);
    refreshChrome();
    rebuild();
}

void StoragePage::setView(View view)
{
    m_view = view;
    m_folder.clear();
    if (m_nav->currentRow() != static_cast<int>(view)) m_nav->setCurrentRow(static_cast<int>(view));
    rebuild();
}

void StoragePage::openFolder(const QString& folder_id)
{
    m_view = View::MyFiles;
    m_folder = folder_id;
    if (m_nav->currentRow() != 0) {
        const QSignalBlocker blocker{m_nav};
        m_nav->setCurrentRow(0);
    }
    m_search->clear();
    rebuild();
}

void StoragePage::setGridMode(bool grid)
{
    m_grid = grid;
    m_list_toggle->setChecked(!grid);
    m_grid_toggle->setChecked(grid);
    m_views->setCurrentWidget(grid ? static_cast<QWidget*>(m_tiles) : m_table);
}

QStringList StoragePage::visibleIds() const
{
    return m_visible;
}

QString StoragePage::folderName(const QString& id) const
{
    for (const auto& item : m_model->fileItems()) {
        if (item.id == id) return item.name;
    }
    return {};
}

QVector<CybouFileItem> StoragePage::collect() const
{
    const QString needle = m_search->text().trimmed();
    QVector<CybouFileItem> items;
    for (const auto& item : m_model->fileItems()) {
        if (!needle.isEmpty()) {
            // Search covers the whole private catalog, not just this folder.
            if (!item.trashed && item.name.contains(needle, Qt::CaseInsensitive)) items.append(item);
            continue;
        }
        switch (m_view) {
        case View::MyFiles:
            if (!item.trashed && item.parent_id == m_folder) items.append(item);
            break;
        case View::Recent:
            if (!item.trashed && !item.folder) items.append(item);
            break;
        case View::Starred:
            if (!item.trashed && item.starred) items.append(item);
            break;
        case View::Trash:
            if (item.trashed) items.append(item);
            break;
        }
    }
    if (m_view == View::Recent && needle.isEmpty()) {
        std::sort(items.begin(), items.end(), [](const CybouFileItem& a, const CybouFileItem& b) { return a.modified > b.modified; });
        if (items.size() > 20) items.resize(20);
    } else {
        // Folders first, then by name (familiar file-manager order).
        std::sort(items.begin(), items.end(), [](const CybouFileItem& a, const CybouFileItem& b) {
            if (a.folder != b.folder) return a.folder;
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
    }
    return items;
}

void StoragePage::rebuild()
{
    const auto items = collect();
    const bool online = m_model->status().online;
    m_visible.clear();
    m_table->clear();
    m_tiles->clear();
    for (const auto& file : items) {
        m_visible << file.id;
        const QIcon icon{glyphPixmap(FileGlyph(file), {20, 20},
            CybouTheme::color(file.folder ? CybouTheme::BLUE : CybouTheme::TEXT_SECONDARY))};
        const QString status = file.folder ? QString{}
            : file.retrieval != CybouRetrievalState::Idle && file.retrieval != CybouRetrievalState::Ready
                ? CybouProduct::retrievalText(file.retrieval)
                : CybouProduct::progressText(file.state, file.progress_percent, online);

        auto* row = new QTreeWidgetItem{m_table};
        row->setIcon(NameColumn, icon);
        row->setText(NameColumn, file.starred ? file.name + QStringLiteral("  ★") : file.name);
        row->setData(NameColumn, kIdRole, file.id);
        row->setToolTip(NameColumn, file.name);
        row->setText(SizeColumn, file.folder ? QStringLiteral("—") : CybouProduct::sizeText(file.logical_size));
        row->setText(ModifiedColumn, ModifiedText(file.modified));
        row->setText(StatusColumn, status);
        row->setForeground(StatusColumn, CybouTheme::color(StateColor(file.state)));
        row->setForeground(SizeColumn, CybouTheme::color(CybouTheme::TEXT_SECONDARY));
        row->setForeground(ModifiedColumn, CybouTheme::color(CybouTheme::TEXT_SECONDARY));

        auto* tile = new QListWidgetItem{QIcon{glyphPixmap(FileGlyph(file), {48, 48},
            CybouTheme::color(file.folder ? CybouTheme::BLUE : CybouTheme::TEXT_SECONDARY))},
            status.isEmpty() ? file.name : QStringLiteral("%1\n%2").arg(file.name, status), m_tiles};
        tile->setData(kIdRole, file.id);
        tile->setToolTip(file.name);
        tile->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    }

    const bool identity = m_model->status().identity_state == CybouIdentityState::Active;
    QString empty;
    if (identity && items.isEmpty()) {
        if (!m_search->text().trimmed().isEmpty()) empty = tr("No files match “%1”.").arg(m_search->text().trimmed());
        else if (m_view == View::Trash) empty = tr("Trash is empty.");
        else if (m_view == View::Starred) empty = tr("Star files to find them here quickly.");
        else if (m_view == View::Recent) empty = tr("Files you add or open appear here.");
        else empty = tr("No files yet. Drag files here or use New to upload.");
    }
    m_empty->setText(empty);
    m_empty->setVisible(!empty.isEmpty());
    m_views->setVisible(!items.isEmpty());

    const bool in_folder = m_view == View::MyFiles && !m_folder.isEmpty() && m_search->text().trimmed().isEmpty();
    m_up->setVisible(in_folder);
    m_title->setText(!m_search->text().trimmed().isEmpty() ? tr("Search results")
        : in_folder ? tr("My files  ›  %1").arg(folderName(m_folder)) : ViewName(m_view));
}

void StoragePage::refreshChrome()
{
    const auto& status = m_model->status();
    const bool identity = status.identity_state == CybouIdentityState::Active;
    const bool connected = m_model->capabilities().files;
    m_banner->setVisible(!identity || !connected);
    m_banner_text->setText(!identity ? tr("Files needs your CYBOU Identity. Create or restore it on Home.")
                                     : tr("Files is not connected yet. Your files will appear here once it is."));
    m_new->setEnabled(identity && connected);
    const quint64 quota = status.storage_quota;
    m_usage_bar->setVisible(quota > 0);
    m_usage_bar->setValue(quota > 0 ? static_cast<int>(qMin<quint64>(1000, status.storage_used * 1000 / quota)) : 0);
    m_usage_text->setText(quota > 0
        ? tr("%1 used of %2").arg(CybouProduct::sizeText(status.storage_used), CybouProduct::sizeText(quota))
        : tr("%1 used").arg(CybouProduct::sizeText(status.storage_used)));
    rebuild();
}

void StoragePage::activate(const QString& id)
{
    for (const auto& item : m_model->fileItems()) {
        if (item.id != id) continue;
        if (item.folder && !item.trashed) openFolder(id);
        return;
    }
}

void StoragePage::updateColumns()
{
    // Hide secondary columns before any horizontal scrolling can appear.
    const int width = m_views->width();
    m_table->setColumnHidden(ModifiedColumn, width < 620);
    m_table->setColumnHidden(SizeColumn, width < 480);
}

void StoragePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Measure after the layout has settled.
    QTimer::singleShot(0, this, [this] { updateColumns(); });
}
