// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/storagepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QAction>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QInputDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QShortcut>
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
    auto* new_menu = new QMenu{m_new};
    new_menu->addAction(QIcon{glyphPixmap(Glyph::Folder, {16, 16}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))},
        tr("New folder"), this, [this] { promptNewFolder(); });
    new_menu->addSeparator();
    new_menu->addAction(QIcon{glyphPixmap(Glyph::Upload, {16, 16}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))},
        tr("Upload files"), this, [this] { uploadFiles(QFileDialog::getOpenFileNames(this, tr("Upload files"))); });
    new_menu->addAction(QIcon{glyphPixmap(Glyph::Folder, {16, 16}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))},
        tr("Upload folder"), this, [this] { promptUploadFolder(); });
    m_new->setMenu(new_menu);
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
    m_title->hide();
    m_up->hide();
    m_crumbs = new QWidget{main};
    m_crumbs->setObjectName(QStringLiteral("filesBreadcrumb"));
    m_crumbs->setAccessibleName(tr("Location"));
    auto* crumbs_layout = new QHBoxLayout{m_crumbs};
    crumbs_layout->setContentsMargins(0, 0, 0, 0);
    crumbs_layout->setSpacing(2);
    toolbar->addWidget(m_crumbs, 1);
    m_search = new QLineEdit{main};
    m_search->setObjectName(QStringLiteral("filesSearch"));
    m_search->setPlaceholderText(tr("Search files"));
    m_search->setAccessibleName(tr("Search files"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(QIcon{glyphPixmap(Glyph::Search, {16, 16}, CybouTheme::color(CybouTheme::TEXT_MUTED))},
        QLineEdit::LeadingPosition);
    m_search->setMinimumHeight(36);
    m_search->setMaximumWidth(340);
    m_search->setMinimumWidth(260);
    toolbar->addWidget(m_search, 0);
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

    m_selection_bar = new QFrame{main};
    m_selection_bar->setObjectName(QStringLiteral("selectionBar"));
    m_selection_bar->setStyleSheet(QStringLiteral("QFrame#selectionBar { background: %1; border-radius: 10px; }")
        .arg(CybouTheme::color(CybouTheme::MINT_GHOST).name()));
    auto* selection_layout = new QHBoxLayout{m_selection_bar};
    selection_layout->setContentsMargins(12, 6, 8, 6);
    selection_layout->setSpacing(6);
    m_selection_text = new QLabel{m_selection_bar};
    m_selection_text->setObjectName(QStringLiteral("rowTitle"));
    selection_layout->addWidget(m_selection_text);
    selection_layout->addStretch();
    const auto selection_action = [this, selection_layout](const QString& text, auto&& handler) {
        auto* button = new QPushButton{text, m_selection_bar};
        button->setObjectName(QStringLiteral("secondaryButton"));
        connect(button, &QPushButton::clicked, this, std::forward<decltype(handler)>(handler));
        selection_layout->addWidget(button);
        return button;
    };
    selection_action(tr("Star"), [this] {
        for (const auto& id : selectedIds()) m_model->setFileStarred(id, true);
    });
    selection_action(tr("Move to Trash"), [this] {
        const auto ids = selectedIds();
        for (const auto& id : ids) m_model->trashFile(id);
        m_model->notify(tr("%1 items moved to Trash").arg(ids.size()), tr("Undo"),
            [model = m_model, ids] { for (const auto& i : ids) model->restoreFile(i); });
    });
    auto* clear_selection = new QToolButton{m_selection_bar};
    clear_selection->setObjectName(QStringLiteral("iconButton"));
    clear_selection->setText(QStringLiteral("✕"));
    clear_selection->setToolTip(tr("Clear selection"));
    clear_selection->setAccessibleName(tr("Clear selection"));
    connect(clear_selection, &QToolButton::clicked, this, [this] {
        m_table->clearSelection();
        m_tiles->clearSelection();
    });
    selection_layout->addWidget(clear_selection);
    m_selection_bar->hide();
    main_layout->addWidget(m_selection_bar);

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
    m_table->header()->setSectionsClickable(true);
    m_table->header()->setSortIndicatorShown(true);
    m_table->header()->setSortIndicator(NameColumn, Qt::AscendingOrder);
    connect(m_table->header(), &QHeaderView::sectionClicked, this, [this](int column) {
        if (column == StatusColumn) return;
        sortBy(column, column == m_sort_column ? !m_sort_descending : column == ModifiedColumn);
    });
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
    m_empty_box = new QWidget{main};
    auto* empty_layout = new QVBoxLayout{m_empty_box};
    empty_layout->setSpacing(10);
    empty_layout->addStretch();
    auto* empty_icon = new QLabel{m_empty_box};
    empty_icon->setPixmap(glyphPixmap(Glyph::CloudUp, {64, 64}, CybouTheme::color(CybouTheme::BORDER_MEDIUM)));
    empty_icon->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_icon);
    m_empty = MutedText({}, m_empty_box);
    m_empty->setObjectName(QStringLiteral("bodyText"));
    m_empty->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(m_empty);
    m_empty_actions = new QWidget{m_empty_box};
    auto* empty_buttons = new QHBoxLayout{m_empty_actions};
    empty_buttons->addStretch();
    auto* empty_upload = new QPushButton{tr("Upload files"), m_empty_actions};
    empty_upload->setObjectName(QStringLiteral("primaryButton"));
    connect(empty_upload, &QPushButton::clicked, this, [this] { uploadFiles(QFileDialog::getOpenFileNames(this, tr("Upload files"))); });
    auto* empty_folder = new QPushButton{tr("New folder"), m_empty_actions};
    empty_folder->setObjectName(QStringLiteral("secondaryButton"));
    connect(empty_folder, &QPushButton::clicked, this, [this] { promptNewFolder(); });
    empty_buttons->addWidget(empty_upload);
    empty_buttons->addWidget(empty_folder);
    empty_buttons->addStretch();
    empty_layout->addWidget(m_empty_actions);
    empty_layout->addStretch();
    main_layout->addWidget(m_empty_box, 1);
    root->addWidget(main, 1);

    m_details = new QFrame{this};
    m_details->setObjectName(QStringLiteral("card"));
    m_details->setAccessibleName(tr("Details"));
    m_details->setFixedWidth(300);
    new QVBoxLayout{m_details};
    m_details->setVisible(false);
    root->addWidget(m_details);

    for (QAbstractItemView* view : {static_cast<QAbstractItemView*>(m_table), static_cast<QAbstractItemView*>(m_tiles)}) {
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, [this, view](const QPoint& pos) {
            showContextMenu(view->viewport()->mapToGlobal(pos));
        });
    }
    connect(m_tiles, &QListWidget::itemSelectionChanged, this, [this] { refreshSelectionBar(); });
    connect(m_table, &QTreeWidget::itemSelectionChanged, this, [this] {
        refreshSelectionBar();
        const auto ids = selectedIds();
        if (m_details->isVisible() && ids.size() == 1) showDetails(ids.first());
    });
    auto* trash_key = new QShortcut{QKeySequence::Delete, this};
    trash_key->setContext(Qt::WidgetWithChildrenShortcut);
    connect(trash_key, &QShortcut::activated, this, [this] {
        const auto ids = selectedIds();
        if (m_view == View::Trash || ids.isEmpty()) return;
        for (const auto& id : ids) m_model->trashFile(id);
        m_model->notify(ids.size() == 1 ? tr("Moved to Trash") : tr("%1 items moved to Trash").arg(ids.size()),
            tr("Undo"), [model = m_model, ids] { for (const auto& i : ids) model->restoreFile(i); });
    });
    auto* rename_key = new QShortcut{QKeySequence{Qt::Key_F2}, this};
    rename_key->setContext(Qt::WidgetWithChildrenShortcut);
    connect(rename_key, &QShortcut::activated, this, [this] {
        const auto ids = selectedIds();
        if (ids.size() == 1) promptRename(ids.first());
    });
    auto* search_key = new QShortcut{QKeySequence{QStringLiteral("Ctrl+K")}, this};
    search_key->setContext(Qt::WidgetWithChildrenShortcut);
    connect(search_key, &QShortcut::activated, m_search, [this] { m_search->setFocus(); });
    auto* close_key = new QShortcut{QKeySequence{Qt::Key_Escape}, this};
    close_key->setContext(Qt::WidgetWithChildrenShortcut);
    connect(close_key, &QShortcut::activated, this, [this] { showDetails({}); });
    setAcceptDrops(true);

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
    connect(m_model, &CybouDesktopModel::filesChanged, this, [this] {
        rebuild();
        rebuildDetails();
    });
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
        // Folders first, then by the chosen column (familiar file-manager order).
        const int column = m_sort_column;
        const bool descending = m_sort_descending;
        std::stable_sort(items.begin(), items.end(), [column, descending](const CybouFileItem& a, const CybouFileItem& b) {
            if (a.folder != b.folder) return a.folder;
            int order = 0;
            if (column == SizeColumn) order = a.logical_size < b.logical_size ? -1 : a.logical_size > b.logical_size ? 1 : 0;
            else if (column == ModifiedColumn) order = a.modified < b.modified ? -1 : a.modified > b.modified ? 1 : 0;
            if (column == NameColumn) order = a.name.compare(b.name, Qt::CaseInsensitive);
            if (order == 0) return a.name.compare(b.name, Qt::CaseInsensitive) < 0; // stable tie-break
            return descending ? order > 0 : order < 0;
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
        if (file.folder) {
            int children = 0;
            for (const auto& other : m_model->fileItems()) {
                if (!other.trashed && other.parent_id == file.id) ++children;
            }
            row->setText(SizeColumn, children == 1 ? tr("1 item") : tr("%1 items").arg(children));
        } else {
            row->setText(SizeColumn, CybouProduct::sizeText(file.logical_size));
        }
        row->setText(ModifiedColumn, ModifiedText(file.modified));
        // Protected is the norm; only other states draw attention.
        if (!status.isEmpty() && (file.state != CybouContentState::Protected || file.retrieval != CybouRetrievalState::Idle))
            m_table->setItemWidget(row, StatusColumn, StateChip(file.state, status, m_table));
        row->setData(StatusColumn, Qt::AccessibleTextRole, status);
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
    m_empty_box->setVisible(!empty.isEmpty());
    m_empty_actions->setVisible(m_view == View::MyFiles && m_search->text().trimmed().isEmpty() && m_new->isEnabled());
    m_views->setVisible(!items.isEmpty());
    m_title->setText(!m_search->text().trimmed().isEmpty() ? tr("Search results") : ViewName(m_view));
    rebuildCrumbs();
    refreshSelectionBar();
}

void StoragePage::rebuildCrumbs()
{
    auto* layout = static_cast<QHBoxLayout*>(m_crumbs->layout());
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    const auto add_label = [this, layout](const QString& text) {
        auto* label = new QLabel{text, m_crumbs};
        label->setObjectName(QStringLiteral("sectionTitle"));
        layout->addWidget(label);
    };
    const bool searching = !m_search->text().trimmed().isEmpty();
    if (searching || m_view != View::MyFiles || m_folder.isEmpty()) {
        add_label(searching ? tr("Search results") : ViewName(m_view));
        layout->addStretch();
        return;
    }
    // My files › Parent › Current, each ancestor clickable.
    QVector<QPair<QString, QString>> path;
    for (QString id = m_folder; !id.isEmpty();) {
        const auto* item = m_model->fileItem(id);
        if (!item) break;
        path.prepend({item->id, item->name});
        id = item->parent_id;
    }
    path.prepend({QString{}, ViewName(View::MyFiles)});
    for (int i = 0; i < path.size(); ++i) {
        if (i > 0) {
            auto* separator = new QLabel{QStringLiteral("›"), m_crumbs};
            separator->setObjectName(QStringLiteral("mutedText"));
            layout->addWidget(separator);
        }
        if (i == path.size() - 1) {
            add_label(path.at(i).second);
            break;
        }
        auto* crumb = new QPushButton{path.at(i).second, m_crumbs};
        crumb->setFlat(true);
        crumb->setCursor(Qt::PointingHandCursor);
        crumb->setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; color: %1; font-size: 17px;"
                                            " font-weight: 600; padding: 2px 6px; min-height: 0; }"
                                            "QPushButton:hover { color: %2; text-decoration: underline; }")
            .arg(CybouTheme::color(CybouTheme::TEXT_SECONDARY).name(), CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
        connect(crumb, &QPushButton::clicked, this, [this, id = path.at(i).first] { openFolder(id); });
        layout->addWidget(crumb);
    }
    layout->addStretch();
}

void StoragePage::refreshSelectionBar()
{
    const int count = selectedIds().size();
    m_selection_bar->setVisible(count > 1 && m_view != View::Trash);
    m_selection_text->setText(tr("%1 selected").arg(count));
}

void StoragePage::sortBy(int column, bool descending)
{
    m_sort_column = column;
    m_sort_descending = descending;
    m_table->header()->setSortIndicator(column, descending ? Qt::DescendingOrder : Qt::AscendingOrder);
    rebuild();
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
    const bool nearly_full = quota > 0 && status.storage_used * 10 >= quota * 8;
    m_usage_text->setStyleSheet(nearly_full ? QStringLiteral("color: %1;").arg(CybouTheme::color(CybouTheme::AMBER).name()) : QString{});
    m_usage_text->setText(nearly_full ? tr("Storage almost full · %1 used of %2")
            .arg(CybouProduct::sizeText(status.storage_used), CybouProduct::sizeText(quota))
        : quota > 0
        ? tr("%1 used of %2").arg(CybouProduct::sizeText(status.storage_used), CybouProduct::sizeText(quota))
        : tr("%1 used").arg(CybouProduct::sizeText(status.storage_used)));
    rebuild();
}

void StoragePage::activate(const QString& id)
{
    for (const auto& item : m_model->fileItems()) {
        if (item.id != id) continue;
        if (item.folder && !item.trashed) openFolder(id);
        else showDetails(id);
        return;
    }
}

void StoragePage::updateColumns()
{
    // Hide secondary columns before any horizontal scrolling can appear.
    const int width = m_views->width();
    m_table->setColumnHidden(ModifiedColumn, width < 700);
    m_table->setColumnHidden(SizeColumn, width < 560);
}

void StoragePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Measure after the layout has settled.
    QTimer::singleShot(0, this, [this] { updateColumns(); });
}

QStringList StoragePage::selectedIds() const
{
    QStringList ids;
    if (m_grid) {
        for (const auto* item : m_tiles->selectedItems()) ids << item->data(kIdRole).toString();
    } else {
        for (const auto* item : m_table->selectedItems()) ids << item->data(NameColumn, kIdRole).toString();
    }
    return ids;
}

void StoragePage::uploadFiles(const QStringList& paths)
{
    const QString parent = m_view == View::MyFiles ? m_folder : QString{};
    for (const auto& path : paths) {
        if (!path.isEmpty() && QFileInfo{path}.isFile()) m_model->requestFileUpload(path, parent);
    }
}

void StoragePage::promptNewFolder()
{
    bool ok{false};
    const QString name = QInputDialog::getText(this, tr("New folder"), tr("Folder name"), QLineEdit::Normal,
        tr("Untitled folder"), &ok).trimmed();
    if (ok && !name.isEmpty() && !m_model->createFolder(name, m_view == View::MyFiles ? m_folder : QString{}).isEmpty())
        m_model->notify(tr("Folder “%1” created").arg(name));
}

void StoragePage::promptUploadFolder()
{
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Upload folder"));
    if (directory.isEmpty()) return;
    const QString root_id = m_model->createFolder(QFileInfo{directory}.fileName(),
        m_view == View::MyFiles ? m_folder : QString{});
    if (root_id.isEmpty()) return;
    QDirIterator it{directory, QDir::Files, QDirIterator::NoIteratorFlags};
    while (it.hasNext()) m_model->requestFileUpload(it.next(), root_id);
}

void StoragePage::promptRename(const QString& id)
{
    const auto* item = m_model->fileItem(id);
    if (!item) return;
    bool ok{false};
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name"), QLineEdit::Normal, item->name, &ok);
    if (ok) m_model->renameFile(id, name);
}

void StoragePage::promptMove(const QString& id)
{
    QStringList labels{tr("My files")};
    QStringList ids{QString{}};
    for (const auto& item : m_model->fileItems()) {
        if (item.folder && !item.trashed && item.id != id) {
            labels << item.name;
            ids << item.id;
        }
    }
    bool ok{false};
    const QString choice = QInputDialog::getItem(this, tr("Move"), tr("Move to"), labels, 0, false, &ok);
    if (ok) m_model->moveFile(id, ids.at(labels.indexOf(choice)));
}

void StoragePage::download(const QString& id)
{
    const auto* item = m_model->fileItem(id);
    if (!item || item->folder) return;
    const QString destination = QFileDialog::getSaveFileName(this, tr("Download"), item->name);
    if (!destination.isEmpty()) m_model->requestFileDownload(id, destination);
}

void StoragePage::showContextMenu(const QPoint& global_pos)
{
    const auto ids = selectedIds();
    if (ids.isEmpty()) return;
    const auto* item = m_model->fileItem(ids.first());
    if (!item) return;
    const QString id = item->id;
    QMenu menu{this};
    if (m_view == View::Trash) {
        menu.addAction(tr("Restore"), this, [this, ids] {
            for (const auto& i : ids) m_model->restoreFile(i);
            m_model->notify(ids.size() == 1 ? tr("Restored") : tr("%1 items restored").arg(ids.size()));
        });
        menu.addAction(tr("Delete forever"), this, [this, ids] {
            if (QMessageBox::question(this, tr("Delete forever"),
                    tr("Remove from your Files? CYBOU releases retained storage according to the Storage retention policy."))
                == QMessageBox::Yes) {
                for (const auto& i : ids) m_model->deleteFileForever(i);
            }
        });
        menu.exec(global_pos);
        return;
    }
    menu.addAction(tr("Open"), this, [this, id] { activate(id); });
    if (!item->folder) {
        auto* dl = menu.addAction(tr("Download"), this, [this, id] { download(id); });
        dl->setEnabled(item->state == CybouContentState::Protected);
    }
    menu.addSeparator();
    menu.addAction(tr("Rename"), this, [this, id] { promptRename(id); })->setShortcut(QKeySequence{Qt::Key_F2});
    menu.addAction(tr("Move"), this, [this, id] { promptMove(id); });
    menu.addAction(item->starred ? tr("Remove star") : tr("Star"), this,
        [this, id, starred = item->starred] { m_model->setFileStarred(id, !starred); });
    if (!item->folder) {
        auto* send = menu.addAction(tr("Send by CYBOU Mail"), this, [this, id] { if (onSendByMail) onSendByMail(id); });
        send->setEnabled(item->state == CybouContentState::Protected && onSendByMail && m_model->capabilities().mail);
    }
    menu.addSeparator();
    menu.addAction(tr("Move to Trash"), this, [this, ids] {
        for (const auto& i : ids) m_model->trashFile(i);
        m_model->notify(ids.size() == 1 ? tr("Moved to Trash") : tr("%1 items moved to Trash").arg(ids.size()),
            tr("Undo"), [model = m_model, ids] { for (const auto& i : ids) model->restoreFile(i); });
    })
        ->setShortcut(QKeySequence::Delete);
    menu.addAction(tr("Details"), this, [this, id] { showDetails(id); });
    menu.exec(global_pos);
}

void StoragePage::showDetails(const QString& id)
{
    m_details_id = id;
    m_details->setVisible(!id.isEmpty());
    rebuildDetails();
    QTimer::singleShot(0, this, [this] { updateColumns(); });
}

namespace {
QString TypeText(const CybouFileItem& item)
{
    if (item.folder) return StoragePage::tr("Folder");
    const QString suffix = QFileInfo{item.name}.suffix().toUpper();
    if (suffix == QLatin1String{"PDF"}) return StoragePage::tr("PDF document");
    if (suffix == QLatin1String{"JPG"} || suffix == QLatin1String{"JPEG"} || suffix == QLatin1String{"PNG"})
        return StoragePage::tr("Image");
    if (suffix == QLatin1String{"ZIP"}) return StoragePage::tr("Archive");
    return suffix.isEmpty() ? StoragePage::tr("File") : StoragePage::tr("%1 file").arg(suffix);
}

void DetailPair(QVBoxLayout* layout, const QString& key, const QString& value, QWidget* parent)
{
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("metricCaption"));
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(k);
    layout->addWidget(v);
}
} // namespace

void StoragePage::rebuildDetails()
{
    if (!m_details->isVisible()) return;
    auto* layout = static_cast<QVBoxLayout*>(m_details->layout());
    while (QLayoutItem* entry = layout->takeAt(0)) {
        if (entry->layout()) {
            while (QLayoutItem* inner = entry->layout()->takeAt(0)) {
                if (inner->widget()) { inner->widget()->hide(); inner->widget()->deleteLater(); }
                delete inner;
            }
        }
        if (entry->widget()) { entry->widget()->hide(); entry->widget()->deleteLater(); }
        delete entry;
    }
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setSpacing(6);
    const auto* item = m_model->fileItem(m_details_id);
    if (!item) {
        m_details->setVisible(false);
        return;
    }
    auto* head = new QHBoxLayout;
    auto* title = new QLabel{item->name, m_details};
    title->setObjectName(QStringLiteral("sectionTitle"));
    title->setWordWrap(true);
    head->addWidget(title, 1);
    auto* close = new QPushButton{tr("Close"), m_details};
    close->setObjectName(QStringLiteral("softButton"));
    connect(close, &QPushButton::clicked, this, [this] { showDetails({}); });
    head->addWidget(close, 0, Qt::AlignTop);
    layout->addLayout(head);
    auto* icon = new QLabel{m_details};
    icon->setPixmap(glyphPixmap(FileGlyph(*item), {56, 56}, CybouTheme::color(item->folder ? CybouTheme::BLUE : CybouTheme::TEXT_SECONDARY)));
    icon->setAlignment(Qt::AlignCenter);
    icon->setMinimumHeight(90);
    layout->addWidget(icon);
    layout->addWidget(MutedText(item->folder ? TypeText(*item)
        : QStringLiteral("%1  ·  %2").arg(TypeText(*item), CybouProduct::sizeText(item->logical_size)), m_details));
    layout->addWidget(MutedText(tr("Modified %1").arg(ModifiedText(item->modified).toLower()), m_details));
    layout->addSpacing(8);
    if (!item->folder) {
        const bool online = m_model->status().online;
        const QString retrieval = CybouProduct::retrievalText(item->retrieval);
        DetailPair(layout, tr("Status"), retrieval.isEmpty()
            ? CybouProduct::progressText(item->state, item->progress_percent, online) : retrieval, m_details);
    }
    const auto& status = m_model->status();
    DetailPair(layout, tr("Owner"), status.primary_name.isEmpty() ? tr("You") : status.primary_name, m_details);
    layout->addSpacing(8);
    if (!item->folder && !item->trashed) {
        auto* dl = new QPushButton{tr("Download"), m_details};
        dl->setObjectName(QStringLiteral("primaryButton"));
        dl->setProperty("cybouId", QStringLiteral("fileDownload"));
        dl->setEnabled(item->state == CybouContentState::Protected &&
            (item->retrieval == CybouRetrievalState::Idle || item->retrieval == CybouRetrievalState::Ready));
        connect(dl, &QPushButton::clicked, this, [this, id = item->id] { download(id); });
        layout->addWidget(dl);
        auto* send = new QPushButton{tr("Send by Mail"), m_details};
        send->setObjectName(QStringLiteral("secondaryButton"));
        send->setProperty("cybouId", QStringLiteral("fileSendByMail"));
        send->setEnabled(item->state == CybouContentState::Protected && onSendByMail && m_model->capabilities().mail);
        connect(send, &QPushButton::clicked, this, [this, id = item->id] { if (onSendByMail) onSendByMail(id); });
        layout->addWidget(send);
    }
    layout->addStretch();
    if (!item->folder) {
        auto* advanced = new QToolButton{m_details};
        advanced->setObjectName(QStringLiteral("sectionLink"));
        advanced->setText(tr("Advanced"));
        advanced->setCheckable(true);
        advanced->setAutoRaise(true);
        layout->addWidget(advanced, 0, Qt::AlignLeft);
        auto* box = new QWidget{m_details};
        auto* box_layout = new QVBoxLayout{box};
        box_layout->setContentsMargins(0, 0, 0, 0);
        box_layout->setSpacing(4);
        const QString none = tr("Not reported yet");
        DetailPair(box_layout, tr("Root content identifier"), item->content_root_id.isEmpty() ? none : item->content_root_id, box);
        DetailPair(box_layout, tr("Finalized height"), item->finalized_height > 0 ? QString::number(item->finalized_height) : none, box);
        DetailPair(box_layout, tr("Protection status"), CybouProduct::contentStateText(item->state), box);
        DetailPair(box_layout, tr("Retrieval status"), item->retrieval == CybouRetrievalState::Idle
            ? tr("Not retrieved on this computer") : CybouProduct::retrievalText(item->retrieval), box);
        box->setVisible(false);
        connect(advanced, &QToolButton::toggled, box, &QWidget::setVisible);
        layout->addWidget(box);
    }
}

void StoragePage::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls() && m_new->isEnabled()) event->acceptProposedAction();
}

void StoragePage::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths << url.toLocalFile();
    }
    uploadFiles(paths);
    event->acceptProposedAction();
}
