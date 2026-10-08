// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/pages/storagepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouconsoledialog.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QAction>
#include <QContextMenuEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFile>
#include <QScopedValueRollback>
#include <QFileInfo>
#include <QSettings>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyle>
#include <QApplication>
#include <QFrame>
#include <QMouseEvent>
#include <QInputDialog>
#include <QItemSelectionModel>
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
#include <QProgressDialog>
#include <QThread>
#include <QElapsedTimer>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QPointer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QSet>
#include <QToolButton>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <vector>
#include <utility>

using namespace CybouUi;

namespace {
QStringList LocalDropPaths(const QMimeData* data)
{
    QStringList paths;
    if (!data || data->hasFormat(CybouUi::fileIdsMime())) return paths;
    for (const auto& url : data->urls()) if (url.isLocalFile()) paths.append(url.toLocalFile());
    return paths;
}

constexpr int kIdRole = Qt::UserRole + 1;
constexpr int kOrderRole = Qt::UserRole + 2;
constexpr int kGlyphRole = Qt::UserRole + 3;

class FileRow final : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override
    {
        return data(0, kOrderRole).toInt() < other.data(0, kOrderRole).toInt();
    }
};

class FileTile final : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;
    bool operator<(const QListWidgetItem& other) const override
    {
        return data(kOrderRole).toInt() < other.data(kOrderRole).toInt();
    }
};

constexpr int kStateRole = Qt::UserRole + 4;
constexpr int kOperationRole = Qt::UserRole + 5;
enum Column { NameColumn = 0, SizeColumn, ModifiedColumn, StatusColumn, ColumnCount };

class FileStatusDelegate final : public QStyledItemDelegate {
public:
    explicit FileStatusDelegate(QObject* parent)
        : QStyledItemDelegate{parent}, m_lock{glyphPixmap(Glyph::Lock, {12, 12},
              CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))} {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QStyleOptionViewItem background{option};
        initStyleOption(&background, index);
        const QString text = background.text;
        background.text.clear();
        const auto* style = background.widget ? background.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &background, painter, background.widget);
        if (text.isEmpty()) return;
        const auto state = static_cast<CybouContentState>(index.data(kStateRole).toInt());
        const auto operation = static_cast<CybouOperationState>(index.data(kOperationRole).toInt());
        const bool settled = !CybouProduct::itemPending(state, operation) &&
            (state == CybouContentState::Protected || state == CybouContentState::Received);
        painter->save();
        painter->setClipRect(option.rect);
        const int x = option.rect.left() + 3;
        const int center = option.rect.center().y();
        if (settled) painter->drawPixmap(x, center - 6, m_lock);
        else {
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(CybouTheme::color(stateColor(state, operation)));
            painter->drawEllipse(QPointF{x + 5.0, static_cast<double>(center)}, 3.0, 3.0);
        }
        QFont font{option.font};
        font.setPixelSize(12);
        painter->setFont(font);
        painter->setPen(option.state & QStyle::State_Selected ? option.palette.highlightedText().color()
            : CybouTheme::color(state == CybouContentState::NeedsAttention ? CybouTheme::ROSE : CybouTheme::TEXT_SECONDARY));
        const QRect caption{x + 17, option.rect.top(), std::max(0, option.rect.width() - 23), option.rect.height()};
        painter->drawText(caption, Qt::AlignLeft | Qt::AlignVCenter,
            QFontMetrics{font}.elidedText(text, Qt::ElideRight, caption.width()));
        painter->restore();
    }
private:
    const QPixmap m_lock;
};

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
    return QLocale{}.toString(when, QStringLiteral("MMM d"));
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
    m_nav->viewport()->setAcceptDrops(true);
    m_nav->viewport()->installEventFilter(this);
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
    m_empty_trash = new QPushButton{tr("Empty Trash"), main};
    m_empty_trash->setObjectName(QStringLiteral("secondaryButton"));
    m_empty_trash->setProperty("cybouId", QStringLiteral("filesEmptyTrash"));
    m_empty_trash->setIcon(QIcon{glyphPixmap(Glyph::Trash, {16, 16}, CybouTheme::color(CybouTheme::ROSE))});
    m_empty_trash->hide();
    connect(m_empty_trash, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Empty Trash"),
                tr("Delete everything in Trash from your catalog? After eligible publication revocation is finalized, providers are instructed to purge unshared chunks. Other copies may remain.")) != QMessageBox::Yes) return;
        m_model->requestEmptyTrash();
        m_model->notify(tr("Emptying Trash. It is done once the network confirms it."));
    });
    toolbar->addWidget(m_empty_trash);
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
    const auto selection_action = [this, selection_layout](Glyph glyph, const QString& text, auto&& handler) {
        auto* button = IconButton(glyph, m_selection_bar, text, IconButtonSize::Toolbar);
        connect(button, &QPushButton::clicked, this, std::forward<decltype(handler)>(handler));
        selection_layout->addWidget(button);
        return button;
    };
    m_selection_star = selection_action(Glyph::Star, tr("Star"), [this] {
        const auto ids = selectedIds();
        const bool remove = std::all_of(ids.begin(), ids.end(), [this](const auto& id) {
            const auto* item = m_model->fileItem(id); return item && item->starred;
        });
        for (const auto& id : ids) m_model->requestFileStarred(id, !remove);
    });
    m_selection_trash = selection_action(Glyph::Trash, tr("Move to Trash"), [this] {
        const auto ids = selectedIds();
        moveFilesTo(ids, {}, true);
    });
    m_selection_restore = selection_action(Glyph::History, tr("Restore"), [this] {
        for (const auto& id : selectedIds()) m_model->requestRestoreFile(id);
    });
    m_selection_star->setObjectName(QStringLiteral("filesSelectionStar"));
    m_selection_trash->setObjectName(QStringLiteral("filesSelectionTrash"));
    m_selection_restore->setObjectName(QStringLiteral("filesSelectionRestore"));
    selection_action(Glyph::Info, tr("Details"), [this] {
        const auto ids = selectedIds();
        if (ids.size() == 1) showDetails(ids.front());
    })->setObjectName(QStringLiteral("filesSelectionDetails"));
    auto* clear_selection = IconButton(Glyph::Close, m_selection_bar, tr("Clear selection"), IconButtonSize::Toolbar);
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
    m_table->setItemDelegateForColumn(StatusColumn, new FileStatusDelegate{m_table});
    m_table->setHeaderLabels({tr("Name"), tr("Size"), tr("Modified"), tr("Status")});
    m_table->setRootIsDecorated(false);
    // Rename is an explicit action (F2 / context menu), never an inline editor on click.
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setUniformRowHeights(true);
    m_table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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
    m_table->header()->resizeSection(StatusColumn, 220);
    m_table->setStyleSheet(QStringLiteral("QTreeWidget::item { height: 40px; }"));
    m_table->header()->setSectionsClickable(true);
    m_table->header()->setSortIndicatorShown(true);
    m_table->header()->setSortIndicator(NameColumn, Qt::AscendingOrder);
    connect(m_table->header(), &QHeaderView::sectionClicked, this, [this](int column) {
        if (column == StatusColumn) return;
        sortBy(column, column == m_sort_column ? !m_sort_descending : column == ModifiedColumn);
    });
    m_views->addWidget(m_table);
    // Column visibility follows the list's own width (the page may be resized while hidden).
    m_views->installEventFilter(this);

    m_tiles = new QListWidget{m_views};
    m_tiles->setObjectName(QStringLiteral("filesGrid"));
    m_tiles->setAccessibleName(tr("Files"));
    m_tiles->setViewMode(QListView::IconMode);
    m_tiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tiles->setResizeMode(QListView::Adjust);
    m_tiles->setMovement(QListView::Static);
    m_tiles->setWrapping(true);
    m_tiles->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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
    auto* details_frame = new QVBoxLayout{m_details};
    details_frame->setContentsMargins(0, 0, 0, 0);
    auto* details_scroll = new QScrollArea{m_details};
    details_scroll->setWidgetResizable(true);
    details_scroll->setFrameShape(QFrame::NoFrame);
    details_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_details_body = new QWidget{details_scroll};
    // The card paints the background; the scroll area and its body stay transparent over it.
    details_scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"));
    new QVBoxLayout{m_details_body};
    details_scroll->setWidget(m_details_body);
    details_frame->addWidget(details_scroll);
    m_details->setVisible(false);
    root->addWidget(m_details);

    for (QAbstractItemView* view : {static_cast<QAbstractItemView*>(m_table), static_cast<QAbstractItemView*>(m_tiles)}) {
        view->viewport()->setAcceptDrops(true);
        view->viewport()->installEventFilter(this);
        view->installEventFilter(this);
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
    // Selection shortcuts belong to the file views, not navigation or editors.
    for (QWidget* view : {static_cast<QWidget*>(m_table), static_cast<QWidget*>(m_tiles)}) {
        auto* trash_key = new QShortcut{QKeySequence::Delete, view};
        trash_key->setContext(Qt::WidgetWithChildrenShortcut);
        connect(trash_key, &QShortcut::activated, this, [this] {
            const auto ids = selectedIds();
            if (m_view == View::Trash || ids.isEmpty()) return;
            moveFilesTo(ids, {}, true);
        });
        auto* rename_key = new QShortcut{QKeySequence{Qt::Key_F2}, view};
        rename_key->setContext(Qt::WidgetWithChildrenShortcut);
        connect(rename_key, &QShortcut::activated, this, [this] {
            const auto ids = selectedIds();
            if (m_view != View::Trash && ids.size() == 1) promptRename(ids.first());
        });
    }
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
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refreshChrome(); if (m_import) advanceImport(); });
    connect(m_model, &CybouDesktopModel::featureAvailabilityChanged, this, [this] { refreshChrome(); if (m_import) advanceImport(); });
    connect(m_model, &CybouDesktopModel::applicationLoadChanged, this,
        [this, previous = m_model->applicationLoadState()]() mutable {
            const auto state = m_model->applicationLoadState();
            if (state != previous) { previous = state; refreshChrome(); }
        });

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

bool StoragePage::canUseFolder(const QString& parent) const
{
    if (m_model->status().identity_state != CybouIdentityState::Active || !m_model->featureAvailability().files) return false;
    if (parent.isEmpty()) return true;
    const auto* folder = m_model->fileItem(parent);
    return folder && folder->folder && !folder->trashed;
}

bool StoragePage::canChangeFiles(const QStringList& ids) const
{
    return canUseFolder({}) && !ids.isEmpty() && std::all_of(ids.begin(), ids.end(),
        [this](const QString& id) { return m_model->fileItem(id) != nullptr; });
}

bool StoragePage::canMoveFilesTo(const QStringList& ids, const QString& folder_id, bool trash) const
{
    if (!canChangeFiles(ids) || (!trash && !canUseFolder(folder_id))) return false;
    // A folder cannot move into itself or one of its descendants.
    QSet<QString> visited;
    for (QString cursor = folder_id; !cursor.isEmpty();) {
        if (ids.contains(cursor) || visited.contains(cursor)) return false;
        visited.insert(cursor);
        const auto* folder = m_model->fileItem(cursor);
        if (!folder || !folder->folder || folder->trashed) return false;
        cursor = folder->parent_id;
    }
    return true;
}

bool StoragePage::moveFilesTo(const QStringList& ids, const QString& folder_id, bool trash)
{
    if (!canMoveFilesTo(ids, folder_id, trash)) return false;
    QVector<QPair<QString, QString>> before;
    QSet<QString> seen;
    for (const auto& id : ids) {
        if (seen.contains(id)) continue;
        seen.insert(id);
        const auto* item = m_model->fileItem(id);
        if (!item) continue;
        before.append({id, item->parent_id});
    }
    if (before.isEmpty()) return false;
    struct Batch { int remaining; int failed{0}; QVector<QPair<QString, QString>> saved; };
    auto batch = std::make_shared<Batch>(Batch{static_cast<int>(before.size())});
    const QPointer<CybouDesktopModel> model{m_model};
    m_model->notify(tr("Saving file changes…"));
    for (const auto& entry : before) {
        auto done = [model, batch, entry](bool ok, const QString&) {
            if (!model) return;
            if (ok) batch->saved.append(entry); else ++batch->failed;
            if (--batch->remaining) return;
            const auto text = batch->failed
                ? tr("%1 changes saved; %2 failed. Try again.").arg(batch->saved.size()).arg(batch->failed)
                : tr("%1 changes saved locally; awaiting network confirmation.").arg(batch->saved.size());
            model->notify(text, batch->saved.isEmpty() ? QString{} : tr("Undo"), [model, saved = batch->saved] {
                if (!model) return;
                for (const auto& [id, parent] : saved) model->requestMoveFile(id, parent);
            });
        };
        if (trash) m_model->requestTrashFile(entry.first, std::move(done));
        else m_model->requestMoveFile(entry.first, folder_id, std::move(done));
    }
    return true;
}

QString StoragePage::itemIdAt(QWidget* viewport, const QPoint& pos) const
{
    if (viewport == m_table->viewport()) {
        const auto* row = m_table->itemAt(pos);
        return row ? row->data(NameColumn, kIdRole).toString() : QString{};
    }
    const auto* tile = m_tiles->itemAt(pos);
    return tile ? tile->data(kIdRole).toString() : QString{};
}

bool StoragePage::handleItemDrag(QWidget* viewport, QEvent* event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        m_press_pos = mouse->position().toPoint();
        m_press_id = mouse->button() == Qt::LeftButton && m_view != View::Trash ? itemIdAt(viewport, m_press_pos) : QString{};
        return false;
    }
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (m_press_id.isEmpty() || !(mouse->buttons() & Qt::LeftButton) ||
            (mouse->position().toPoint() - m_press_pos).manhattanLength() < QApplication::startDragDistance()) return false;
        QStringList ids = selectedIds();
        const QString pressed = std::exchange(m_press_id, {});
        if (!ids.contains(pressed)) ids = QStringList{pressed};
        // Only files already downloaded can leave CYBOU by drag: their plaintext copy
        // exists where the user saved it. Nothing is decrypted for a drag.
        QList<QUrl> external;
        for (const auto& id : ids) {
            const QString path = downloadedPath(id);
            if (!path.isEmpty()) external << QUrl::fromLocalFile(path);
        }
        startIdDrag(viewport, fileIdsMime(), ids, ids.size() == 1 ? folderName(ids.first()) : tr("%1 items").arg(ids.size()),
            external);
        return true;
    }
    case QEvent::DragEnter:
    case QEvent::DragMove:
    case QEvent::Drop: {
        auto* drop = static_cast<QDropEvent*>(event);
        const auto* item = m_model->fileItem(itemIdAt(viewport, drop->position().toPoint()));
        const QString parent = item && item->folder ? item->id : (m_view == View::MyFiles ? m_folder : QString{});
        const bool destination = (item && item->folder) || m_view == View::MyFiles;
        // Internal identity is authoritative even when a downloaded URL is also
        // present. Never turn a refused internal move into a duplicate upload.
        const bool internal = drop->mimeData()->hasFormat(fileIdsMime());
        const QStringList ids = dragIds(drop->mimeData(), fileIdsMime());
        const QStringList paths = LocalDropPaths(drop->mimeData());
        bool accepted = internal ? destination && canMoveFilesTo(ids, parent)
            : !paths.isEmpty() && canUseFolder(parent);
        if (accepted && event->type() == QEvent::Drop) {
            if (internal) accepted = moveFilesTo(ids, parent);
            else uploadPaths(paths, parent);
        }
        if (accepted) drop->acceptProposedAction(); else drop->ignore();
        return true;
    }
    default:
        return false;
    }
}

bool StoragePage::eventFilter(QObject* watched, QEvent* event)
{
    for (QAbstractItemView* view : {static_cast<QAbstractItemView*>(m_table), static_cast<QAbstractItemView*>(m_tiles)}) {
        if (view && (watched == view || watched == view->viewport()) && event->type() == QEvent::ContextMenu &&
            static_cast<QContextMenuEvent*>(event)->reason() == QContextMenuEvent::Keyboard) {
            const auto current = view->currentIndex();
            if (current.isValid()) {
                if (!view->selectionModel()->isSelected(current))
                    view->selectionModel()->select(current, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                view->scrollTo(current);
                Q_EMIT view->customContextMenuRequested(view->visualRect(current).center());
            }
            event->accept();
            return true;
        }
    }
    if (watched == m_views) {
        // Only watched for its size; it also sees ChildAdded while its views are built.
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show) updateColumns();
        return QWidget::eventFilter(watched, event);
    }
    if (watched == m_table->viewport() || watched == m_tiles->viewport()) {
        if (handleItemDrag(static_cast<QWidget*>(watched), event)) return true;
        return QWidget::eventFilter(watched, event);
    }
    const bool nav = watched == m_nav->viewport();
    const bool crumb = !nav && watched->property("folderId").isValid();
    if ((nav || crumb) && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop)) {
        auto* drag = static_cast<QDropEvent*>(event);
        const QStringList ids = dragIds(drag->mimeData(), fileIdsMime());
        int row = -1;
        if (nav) {
            const auto* item = m_nav->itemAt(drag->position().toPoint());
            row = item ? m_nav->row(item) : -1;
        }
        const bool target_ok = crumb
            ? canMoveFilesTo(ids, watched->property("folderId").toString())
            : row == static_cast<int>(View::MyFiles) ? canMoveFilesTo(ids, {})
            : (row == static_cast<int>(View::Starred) || row == static_cast<int>(View::Trash)) && canChangeFiles(ids);
        if (!target_ok) {
            drag->ignore();
            return true;
        }
        if (event->type() != QEvent::Drop) {
            drag->acceptProposedAction();
            return true;
        }
        bool accepted = true;
        if (crumb) {
            accepted = moveFilesTo(ids, watched->property("folderId").toString());
        } else if (row == static_cast<int>(View::MyFiles)) {
            accepted = moveFilesTo(ids, {});
        } else if (row == static_cast<int>(View::Starred)) {
            for (const auto& id : ids) m_model->requestFileStarred(id, true);
        } else {
            accepted = moveFilesTo(ids, {}, true);
        }
        if (accepted) drag->acceptProposedAction(); else drag->ignore();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void StoragePage::setSearchText(const QString& text)
{
    m_search->setText(text);
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
    const QStringList selection = selectedIds();
    const QString current = m_grid
        ? (m_tiles->currentItem() ? m_tiles->currentItem()->data(kIdRole).toString() : QString{})
        : (m_table->currentItem() ? m_table->currentItem()->data(NameColumn, kIdRole).toString() : QString{});
    const QSignalBlocker table_blocker{m_table}, grid_blocker{m_tiles};
    m_grid = grid;
    if (grid) {
        m_tiles->clearSelection();
        if (m_grid_items.contains(current)) m_tiles->setCurrentItem(m_grid_items.value(current), QItemSelectionModel::NoUpdate);
        for (const auto& id : selection) if (m_grid_items.contains(id)) m_grid_items.value(id)->setSelected(true);
    } else {
        m_table->clearSelection();
        if (m_rows.contains(current)) m_table->setCurrentItem(m_rows.value(current), 0, QItemSelectionModel::NoUpdate);
        for (const auto& id : selection) if (m_rows.contains(id)) m_rows.value(id)->setSelected(true);
    }
    m_list_toggle->setChecked(!grid);
    m_grid_toggle->setChecked(grid);
    m_views->setCurrentWidget(grid ? static_cast<QWidget*>(m_tiles) : m_table);
    refreshSelectionBar();
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
    if (m_model->status().identity_state != CybouIdentityState::Active) return items;
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
    if (m_model->status().identity_state != CybouIdentityState::Active) {
        m_folder.clear();
        m_press_id.clear();
        m_details_id.clear();
        m_details_advanced = false;
        m_details->hide();
        const QSignalBlocker search_blocker{m_search};
        m_search->clear();
    }
    const auto items = collect();
    const bool online = m_model->status().online;
    const QSignalBlocker table_blocker{m_table}, grid_blocker{m_tiles};
    const int table_scroll = m_table->verticalScrollBar()->value();
    const int grid_scroll = m_tiles->verticalScrollBar()->value();
    auto* top = m_table->itemAt(QPoint{1, 0});
    const QString anchor = top ? top->data(NameColumn, kIdRole).toString() : QString{};
    const int anchor_offset = top ? m_table->visualItemRect(top).top() : 0;
    QStringList next_visible;
    QSet<QString> visible;
    for (const auto& file : items) { next_visible.append(file.id); visible.insert(file.id); }
    const bool reordered = next_visible != m_visible;
    QHash<QString, int> children;
    for (const auto& file : m_model->fileItems()) if (!file.trashed) ++children[file.parent_id];
    for (auto it = m_rows.begin(); it != m_rows.end();) {
        if (visible.contains(it.key())) { ++it; continue; }
        delete it.value();
        delete m_grid_items.take(it.key());
        it = m_rows.erase(it);
    }
    QHash<int, QPair<QIcon, QIcon>> icons;
    int position{0};
    for (const auto& file : items) {
        auto* row = m_rows.value(file.id);
        auto* tile = m_grid_items.value(file.id);
        if (!row) {
            row = new FileRow{m_table};
            row->setData(NameColumn, kIdRole, file.id);
            row->setForeground(SizeColumn, CybouTheme::color(CybouTheme::TEXT_SECONDARY));
            row->setForeground(ModifiedColumn, CybouTheme::color(CybouTheme::TEXT_SECONDARY));
            m_rows.insert(file.id, row);
            tile = new FileTile{m_tiles};
            tile->setData(kIdRole, file.id);
            tile->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            m_grid_items.insert(file.id, tile);
        }
        const int glyph = static_cast<int>(FileGlyph(file));
        if (row->data(NameColumn, kGlyphRole) != QVariant{glyph}) {
            if (!icons.contains(glyph)) {
                const QColor color = CybouTheme::color(file.folder ? CybouTheme::BLUE : CybouTheme::TEXT_SECONDARY);
                const QColor lock = CybouTheme::color(CybouTheme::BRAND_TEAL_DARK);
                icons.insert(glyph, {QIcon{withLockBadge(glyphPixmap(FileGlyph(file), {20, 20}, color), lock)},
                    QIcon{withLockBadge(glyphPixmap(FileGlyph(file), {48, 48}, color), lock)}});
            }
            row->setIcon(NameColumn, icons.value(glyph).first);
            tile->setIcon(icons.value(glyph).second);
            row->setData(NameColumn, kGlyphRole, glyph);
        }
        if (row->data(NameColumn, kOrderRole).toInt() != position || !row->data(NameColumn, kOrderRole).isValid())
            row->setData(NameColumn, kOrderRole, position);
        if (tile->data(kOrderRole).toInt() != position || !tile->data(kOrderRole).isValid())
            tile->setData(kOrderRole, position);
        ++position;
        const auto operation = m_model->displayedOperationState(file.operation_id, file.operation_state);
        const QString status = CybouProduct::fileStatusText(file, online, operation);
        const QString name = file.starred ? file.name + QStringLiteral("  ★") : file.name;
        const int count = children.value(file.id);
        const QString size = file.folder ? (count == 1 ? tr("1 item") : tr("%1 items").arg(count))
                                         : CybouProduct::sizeText(file.logical_size);
        const QString modified = ModifiedText(file.modified);
        if (row->text(NameColumn) != name) row->setText(NameColumn, name);
        if (row->toolTip(NameColumn) != file.name) row->setToolTip(NameColumn, file.name);
        if (row->text(SizeColumn) != size) row->setText(SizeColumn, size);
        if (row->text(ModifiedColumn) != modified) row->setText(ModifiedColumn, modified);
        if (row->text(StatusColumn) != status) row->setText(StatusColumn, status);
        if (row->data(StatusColumn, kStateRole).toInt() != static_cast<int>(file.state) || !row->data(StatusColumn, kStateRole).isValid())
            row->setData(StatusColumn, kStateRole, static_cast<int>(file.state));
        if (row->data(StatusColumn, kOperationRole).toInt() != static_cast<int>(operation) || !row->data(StatusColumn, kOperationRole).isValid())
            row->setData(StatusColumn, kOperationRole, static_cast<int>(operation));
        if (row->toolTip(StatusColumn) != status) row->setToolTip(StatusColumn, status);
        if (row->data(StatusColumn, Qt::AccessibleTextRole).toString() != status)
            row->setData(StatusColumn, Qt::AccessibleTextRole, status);
        const QString tile_text = status.isEmpty() ? file.name : QStringLiteral("%1\n%2").arg(file.name, status);
        if (tile->text() != tile_text) tile->setText(tile_text);
        if (tile->toolTip() != file.name) tile->setToolTip(file.name);
    }
    if (reordered) {
        // Sorting retained rows preserves Qt's current/selected persistent indexes.
        m_table->sortItems(NameColumn, Qt::AscendingOrder);
        m_table->header()->setSortIndicator(m_sort_column, m_sort_descending ? Qt::DescendingOrder : Qt::AscendingOrder);
        m_tiles->sortItems(Qt::AscendingOrder);
        m_table->doItemsLayout();
        m_tiles->doItemsLayout();
    }
    m_visible = std::move(next_visible);
    if (reordered && !anchor.isEmpty() && m_rows.contains(anchor)) {
        m_table->scrollToItem(m_rows.value(anchor), QAbstractItemView::PositionAtTop);
        m_table->verticalScrollBar()->setValue(m_table->verticalScrollBar()->value() - anchor_offset);
    } else m_table->verticalScrollBar()->setValue(table_scroll);
    m_tiles->verticalScrollBar()->setValue(grid_scroll);

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
    m_empty_trash->setVisible(m_view == View::Trash && !items.isEmpty() && m_new->isEnabled());
    m_title->setText(!m_search->text().trimmed().isEmpty() ? tr("Search results") : ViewName(m_view));
    rebuildCrumbs();
    refreshSelectionBar();
}

void StoragePage::rebuildCrumbs()
{
    QString key = QString::number(static_cast<int>(m_view)) + QLatin1Char('|') + m_search->text().trimmed();
    for (QString id = m_folder; !id.isEmpty();) {
        const auto* item = m_model->fileItem(id);
        if (!item) break;
        key += QLatin1Char('|') + item->id + QLatin1Char('/') + item->name;
        id = item->parent_id;
    }
    if (key == m_crumbs_key) return;
    m_crumbs_key = key;
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
        // Dropping items on a path segment moves them to that folder.
        crumb->setProperty("folderId", path.at(i).first);
        crumb->setAcceptDrops(true);
        crumb->installEventFilter(this);
        layout->addWidget(crumb);
    }
    layout->addStretch();
}

void StoragePage::refreshSelectionBar()
{
    const auto ids = selectedIds();
    const int count = ids.size();
    m_selection_bar->setVisible(count > 0);
    m_selection_text->setText(tr("%1 selected").arg(count));
    const bool trash = m_view == View::Trash;
    m_selection_star->setVisible(!trash);
    m_selection_trash->setVisible(!trash);
    m_selection_restore->setVisible(trash);
    const bool starred = count && std::all_of(ids.begin(), ids.end(), [this](const auto& id) {
        const auto* item = m_model->fileItem(id); return item && item->starred;
    });
    m_selection_star->setToolTip(starred ? tr("Remove star") : tr("Star"));
    m_selection_star->setAccessibleName(m_selection_star->toolTip());
    const bool editable = m_model->status().identity_state == CybouIdentityState::Active &&
        m_model->featureAvailability().files;
    for (auto* button : {m_selection_star, m_selection_trash, m_selection_restore}) button->setEnabled(editable);
    m_selection_bar->findChild<QToolButton*>(QStringLiteral("filesSelectionDetails"))->setVisible(count == 1);
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
    const bool connected = m_model->featureAvailability().files;
    m_banner->setVisible(!identity || !connected);
    m_banner_text->setText(!identity ? tr("Files needs your CYBOU Identity. Create or restore it on Home.")
        : m_model->applicationLoadState() == CybouApplicationLoadState::Loading ||
          m_model->applicationLoadState() == CybouApplicationLoadState::Opening
            ? tr("Your local view is still being prepared. Items appear progressively.")
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

QString StoragePage::searchText() const
{
    return m_search ? m_search->text() : QString{};
}

bool StoragePage::isDetailsVisible() const
{
    return m_details && !m_details->isHidden();
}

void StoragePage::setDetailsAdvanced(bool adv)
{
    m_details_advanced = adv;
    if (m_details && m_details->isVisible()) rebuildDetails();
}

void StoragePage::updateColumns()
{
    if (!m_table) return;
    // Hide secondary columns before any horizontal scrolling can appear.
    // At 1040x720 window size with details open, m_views width is ~428px.
    // Modified is hidden below 560px, Size remains visible down to 400px.
    const int width = m_views->width();
    m_table->setColumnHidden(ModifiedColumn, width < 560);
    m_table->setColumnHidden(SizeColumn, width < 400);
}

void StoragePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_details) {
        const int target_details_width = width() < 1000 ? 250 : (width() < 1200 ? 275 : 300);
        m_details->setFixedWidth(target_details_width);
    }
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
    uploadPaths(paths, parent);
}

void StoragePage::promptNewFolder()
{
    bool ok{false};
    const QString name = QInputDialog::getText(this, tr("New folder"), tr("Folder name"), QLineEdit::Normal,
        tr("Untitled folder"), &ok).trimmed();
    if (ok && !name.isEmpty())
        m_model->requestCreateFolder(name, m_view == View::MyFiles ? m_folder : QString{});
}

void StoragePage::promptUploadFolder()
{
    const QString directory = QFileDialog::getExistingDirectory(this, tr("Upload folder"));
    if (directory.isEmpty()) return;
    uploadPaths({directory}, m_view == View::MyFiles ? m_folder : QString{});
}

struct StoragePage::FolderImport {
    struct Entry { QString path; QString name; int parent{-1}; bool folder{false}; };
    std::atomic<bool> cancelled{false};
    std::atomic<bool> discovered{false};
    std::atomic<int> count{0};
    // Only the discovery worker writes these until discovered's release/acquire handoff.
    std::vector<Entry> entries;
    QString error;
    QString account;
    QString destination;
    QVector<QString> ids;
    int queued{0};
    bool advancing{false};
    int skipped_links{0};
    QPointer<QProgressDialog> dialog;
    QPointer<QTimer> timer;
};

StoragePage::~StoragePage()
{
    if (m_import) m_import->cancelled.store(true);
}

void StoragePage::finishImport(const QString& message)
{
    auto job = std::exchange(m_import, {});
    if (!job) return;
    job->cancelled.store(true);
    if (job->timer) { job->timer->stop(); job->timer->deleteLater(); }
    if (job->dialog) { job->dialog->hide(); job->dialog->deleteLater(); }
    if (!message.isEmpty()) m_model->notify(message);
}

void StoragePage::advanceImport()
{
    const auto job = m_import;
    if (!job) return;
    if (m_model->status().identity_state != CybouIdentityState::Active ||
        m_model->status().account_id != job->account || !m_model->featureAvailability().files) {
        finishImport(tr("Import stopped. %1 items already queued; remaining items were not uploaded.").arg(job->queued));
        return;
    }
    if (job->advancing) return;
    QScopedValueRollback<bool> advancing{job->advancing, true};
    if (!job->destination.isEmpty()) {
        const auto* destination = m_model->fileItem(job->destination);
        if (!destination || !destination->folder || destination->trashed) {
            finishImport(tr("Import stopped. %1 items already queued; remaining items were not uploaded.").arg(job->queued));
            return;
        }
    }
    if (!job->discovered.load(std::memory_order_acquire)) {
        job->dialog->setLabelText(tr("Finding files and folders: %1 found. Nothing uploaded yet.").arg(job->count.load()));
        return;
    }
    if (!job->error.isEmpty()) { finishImport(job->error); return; }
    const int total = static_cast<int>(job->entries.size());
    job->dialog->setRange(0, std::max(1, total));
    QElapsedTimer budget;
    budget.start();
    QPointer<StoragePage> guard{this};
    for (int batch{0}; job->queued < total && batch < 8 && budget.elapsed() < 8; ++batch) {
        const auto entry = job->entries[job->queued];
        const QString parent = entry.parent < 0 ? job->destination : job->ids[entry.parent];
        ++job->queued; // Count ownership handoff before reentrant model signals.
        const QString id = entry.folder ? m_model->requestCreateFolder(entry.name, parent)
                                       : m_model->requestFileUpload(entry.path, parent);
        if (!guard || job->cancelled.load() || guard->m_import != job) return;
        if (id.isEmpty()) {
            --job->queued;
            finishImport(tr("Import stopped. %1 items already queued; remaining items were not uploaded.").arg(job->queued));
            return;
        }
        job->ids.append(id);
    }
    job->dialog->setValue(job->queued);
    if (!guard || job->cancelled.load() || guard->m_import != job) return;
    job->dialog->setLabelText(tr("Preparing upload: %1 of %2 items queued. Network protection continues separately.").arg(job->queued).arg(total));
    if (job->queued == total) {
        finishImport(tr("%1 items queued for upload. %2 symbolic links skipped. Follow protection in Files.").arg(total).arg(job->skipped_links));
    } else job->timer->setInterval(0);
}

void StoragePage::uploadPaths(const QStringList& paths, const QString& parent)
{
    if (paths.isEmpty() || !m_model->featureAvailability().files ||
        m_model->status().identity_state != CybouIdentityState::Active) return;
    if (m_import) { m_model->notify(tr("A folder import is already in progress.")); return; }
    // Small plain-file selections retain their immediate existing behavior.
    if (paths.size() <= 64 && std::none_of(paths.begin(), paths.end(), [](const QString& path) { return QFileInfo{path}.isDir(); })) {
        for (const auto& path : paths) if (!QFileInfo{path}.isSymLink()) m_model->requestFileUpload(path, parent);
        return;
    }
    if (paths.size() > 10000) {
        m_model->notify(tr("This import exceeds 10,000 items or 64 folder levels. Select smaller folders. No items were uploaded."));
        return;
    }
    const auto job = std::make_shared<FolderImport>();
    job->account = m_model->status().account_id;
    job->destination = parent;
    job->dialog = new QProgressDialog(tr("Finding files and folders. Nothing uploaded yet."), tr("Cancel"), 0, 0, this);
    job->dialog->setProperty("cybouId", QStringLiteral("folderImportProgress"));
    job->dialog->setWindowTitle(tr("Upload folder"));
    job->dialog->setWindowModality(Qt::NonModal);
    job->dialog->setAutoClose(false);
    job->dialog->setAutoReset(false);
    job->dialog->setMinimumDuration(0);
    m_import = job;
    connect(job->dialog, &QProgressDialog::canceled, this, [this, job] {
        if (m_import == job) finishImport(tr("Import cancelled. %1 items already queued; remaining items were not uploaded.").arg(job->queued));
    });
    job->timer = new QTimer(this);
    job->timer->setInterval(100);
    connect(job->timer, &QTimer::timeout, this, [this] { advanceImport(); });
    job->timer->start();
    // Lifetime belongs to the worker, not the page. Destroying a page cancels
    // without joining a potentially slow filesystem call on the GUI thread.
    auto* worker = QThread::create([job, paths] {
        try {
            struct Pending { QString path; int parent; int depth; };
            std::vector<Pending> pending;
            for (auto it = paths.crbegin(); it != paths.crend(); ++it) pending.push_back({*it, -1, 0});
            constexpr int limit{10000};
            while (!pending.empty() && !job->cancelled.load()) {
                const auto next = pending.back();
                pending.pop_back();
#ifdef Q_OS_WIN
                const std::filesystem::path path{QDir::toNativeSeparators(next.path).toStdWString()};
#else
                const std::filesystem::path path{QFile::encodeName(next.path).constData()};
#endif
                std::error_code ec;
                const auto status = std::filesystem::symlink_status(path, ec);
                if (ec) { job->error = tr("The folder could not be read. No items were uploaded."); break; }
                if (std::filesystem::is_symlink(status)) { ++job->skipped_links; continue; }
                const bool folder = std::filesystem::is_directory(status);
                if (!folder && !std::filesystem::is_regular_file(status)) {
                    job->error = tr("The folder contains an unreadable or unsupported item. No items were uploaded."); break;
                }
                if (job->entries.size() >= limit || next.depth > 64) {
                    job->error = tr("This import exceeds 10,000 items or 64 folder levels. Select smaller folders. No items were uploaded."); break;
                }
                const int index = static_cast<int>(job->entries.size());
                job->entries.push_back({next.path, QFileInfo{next.path}.fileName(), next.parent, folder});
                job->count.store(index + 1);
                if (!folder) continue;
                std::filesystem::directory_iterator cursor{path, ec}, end;
                if (ec) { job->error = tr("The folder could not be read. No items were uploaded."); break; }
                for (; cursor != end && !job->cancelled.load(); cursor.increment(ec)) {
                    if (ec) break;
#ifdef Q_OS_WIN
                    const QString child = QString::fromStdWString(cursor->path().wstring());
#else
                    const QString child = QFile::decodeName(cursor->path().string().c_str());
#endif
                    pending.push_back({child, index, next.depth + 1});
                    if (pending.size() + job->entries.size() > limit) {
                        job->error = tr("This import exceeds 10,000 items or 64 folder levels. Select smaller folders. No items were uploaded."); break;
                    }
                }
                if (ec) job->error = tr("The folder could not be read. No items were uploaded.");
                if (!job->error.isEmpty()) break;
            }
        } catch (const std::exception&) {
            job->error = tr("The folder could not be read. No items were uploaded.");
        }
        job->discovered.store(true, std::memory_order_release);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
    job->dialog->show();
}

void StoragePage::promptRename(const QString& id)
{
    const auto* item = m_model->fileItem(id);
    if (!item) return;
    bool ok{false};
    const QString name = QInputDialog::getText(this, tr("Rename"), tr("New name"), QLineEdit::Normal, item->name, &ok);
    if (ok) m_model->requestRenameFile(id, name);
}

void StoragePage::promptMove(const QString& id)
{
    QDialog dialog{this};
    dialog.setWindowTitle(tr("Move"));
    auto* layout = new QVBoxLayout{&dialog};
    layout->addWidget(new QLabel{tr("Move to"), &dialog});
    auto* folders = new QComboBox{&dialog};
    folders->setAccessibleName(tr("Move to"));
    folders->addItem(tr("My files"), QString{});
    for (const auto& item : m_model->fileItems()) {
        if (!item.folder || item.trashed || item.id == id) continue;
        QStringList path{item.name};
        QString parent = item.parent_id;
        QSet<QString> visited{item.id};
        bool valid = true;
        while (!parent.isEmpty()) {
            if (parent == id || visited.contains(parent)) { valid = false; break; }
            visited.insert(parent);
            const auto* ancestor = m_model->fileItem(parent);
            if (!ancestor || ancestor->trashed) { valid = false; break; }
            path.prepend(ancestor->name);
            parent = ancestor->parent_id;
        }
        if (valid) folders->addItem(path.join(QStringLiteral(" / ")), item.id);
    }
    layout->addWidget(folders);
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) moveFilesTo({id}, folders->currentData().toString());
}

void StoragePage::download(const QString& id)
{
    const auto* item = m_model->fileItem(id);
    if (!item || item->folder) return;
    const QString destination = QFileDialog::getSaveFileName(this, tr("Download"), item->name);
    if (destination.isEmpty()) return;
    QSettings{}.setValue(QStringLiteral("files/downloaded/") + id, destination);
    m_model->requestFileDownload(id, destination);
}

QString StoragePage::downloadedPath(const QString& id) const
{
    const auto* item = m_model->fileItem(id);
    if (!item || item->folder) return {};
    const QString path = QSettings{}.value(QStringLiteral("files/downloaded/") + id).toString();
    const QFileInfo info{path};
    // The saved copy must still be there, complete, and not currently being written.
    if (path.isEmpty() || !info.isFile() ||
        (item->retrieval != CybouRetrievalState::Idle && item->retrieval != CybouRetrievalState::Ready)) return {};
    if (item->logical_size > 0 && static_cast<quint64>(info.size()) != item->logical_size) return {};
    return info.absoluteFilePath();
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
            for (const auto& i : ids) m_model->requestRestoreFile(i);
        });
        menu.addAction(tr("Delete forever"), this, [this, ids] {
            if (QMessageBox::question(this, tr("Delete forever"),
                    tr("Delete from your catalog? After eligible publication revocation is finalized, providers are instructed to purge unshared chunks. Other copies may remain."))
                == QMessageBox::Yes) {
                for (const auto& i : ids) m_model->requestDeleteFile(i);
            }
        });
        menu.exec(global_pos);
        return;
    }
    menu.addAction(tr("Open"), this, [this, id] { activate(id); });
    if (!item->folder) {
        auto* dl = menu.addAction(tr("Download"), this, [this, id] { download(id); });
        dl->setEnabled(item->state == CybouContentState::Protected || item->available_offline);
    }
    menu.addSeparator();
    menu.addAction(tr("Rename"), this, [this, id] { promptRename(id); })->setShortcut(QKeySequence{Qt::Key_F2});
    menu.addAction(tr("Move"), this, [this, id] { promptMove(id); });
    if (!item->folder) {
        menu.addAction(tr("Make a copy"), this, [this, id, parent = item->parent_id] {
            m_model->requestCopyFile(id, parent);
        });
    }
    menu.addAction(item->starred ? tr("Remove star") : tr("Star"), this,
        [this, id, starred = item->starred] { m_model->requestFileStarred(id, !starred); });
    if (!item->folder) {
        auto* send = menu.addAction(tr("Send by CYBOU Mail"), this, [this, id] { if (onSendByMail) onSendByMail(id); });
        send->setEnabled(item->state == CybouContentState::Protected && onSendByMail && m_model->featureAvailability().mail);
        send->setToolTip(!m_model->featureAvailability().mail || !onSendByMail ? tr("Mail is unavailable.")
            : item->state != CybouContentState::Protected ? tr("This file must reach Protected before it can be attached by reference.") : QString{});
    }
    menu.addSeparator();
    menu.addAction(tr("Move to Trash"), this, [this, ids] {
        moveFilesTo(ids, {}, true);
    })
        ->setShortcut(QKeySequence::Delete);
    menu.addAction(tr("Details"), this, [this, id] { showDetails(id); });
    menu.exec(global_pos);
}

void StoragePage::showDetails(const QString& id)
{
    showDetails(id, m_details_id == id && m_details_advanced);
}

void StoragePage::showDetails(const QString& id, bool advanced)
{
    m_details_id = id;
    m_details_advanced = advanced;
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

/// Text width inside the 300 px details card after its margins and the scroll bar.
constexpr int DETAILS_TEXT_WIDTH{246};

void DetailPair(QVBoxLayout* layout, const QString& key, const QString& value, QWidget* parent)
{
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("metricCaption"));
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setWordWrap(true);
    // Wrapped text takes the panel width; an unbreakable word is clipped instead of widening it.
    v->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(k);
    layout->addWidget(v);
}
} // namespace

void StoragePage::rebuildDetails()
{
    if (!m_details->isVisible()) return;
    auto* scroll = m_details->findChild<QScrollArea*>();
    const int scroll_value = scroll ? scroll->verticalScrollBar()->value() : 0;
    auto* layout = static_cast<QVBoxLayout*>(m_details_body->layout());
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
        m_details_id.clear();
        m_details_advanced = false;
        m_details->setVisible(false);
        return;
    }
    auto* head = new QHBoxLayout;
    // A file name without spaces cannot wrap; elide it so it never widens the panel.
    auto* title = new QLabel{m_details};
    title->setObjectName(QStringLiteral("sectionTitle"));
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setText(title->fontMetrics().elidedText(item->name, Qt::ElideMiddle, qMax(120, m_details->width() - 54)));
    title->setToolTip(item->name);
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
        DetailPair(layout, tr("Status"), CybouProduct::fileStatusText(*item, online,
            m_model->displayedOperationState(item->operation_id, item->operation_state)), m_details);
        DetailPair(layout, tr("On this computer"), CybouProduct::localAvailabilityText(*item), m_details);
        if (const QString saved = downloadedPath(item->id); !saved.isEmpty()) {
            // Short form (full path in the tooltip); this copy can be dragged out of CYBOU.
            const QFileInfo info{saved};
            DetailPair(layout, tr("Downloaded to"), tr("%1 in %2").arg(info.fileName(), info.dir().dirName()), m_details);
            m_details->setToolTip(tr("Downloaded copy: %1 — drag the file out of CYBOU to copy it")
                .arg(QDir::toNativeSeparators(saved)));
        } else {
            m_details->setToolTip({});
        }
        QString replica_info = item->min_remote_replicas < 0
            ? tr("Remote copies have not been measured yet")
            : item->remote_replica_target > 0
                ? tr("Encrypted copies: %1 of %2").arg(item->min_remote_replicas).arg(item->remote_replica_target)
                : tr("Encrypted copies: %1").arg(item->min_remote_replicas);
        if (item->min_remote_replicas >= 0 && item->remote_replica_target > 0 && item->min_remote_replicas < item->remote_replica_target) {
            replica_info += QStringLiteral("  ·  ") + tr("Remote copy target not reached");
        }
        DetailPair(layout, tr("On the network"), replica_info, m_details);
        const auto observed = item->protection_observed_at;
        QString observation_time = observed.isValid()
            ? QLocale{}.toString(observed.toUTC(), QLocale::ShortFormat) + QStringLiteral(" UTC")
            : tr("Unknown — reading saved records does not check copies");
        if (observed.isValid() && observed.secsTo(QDateTime::currentDateTimeUtc()) > 24 * 60 * 60)
            observation_time = tr("Older than 24 hours · %1").arg(observation_time);
        DetailPair(layout, tr("Last local observation"), observation_time, m_details);
        DetailPair(layout, tr("Observation scope"), item->protection_observation_scope.isEmpty()
            ? tr("No local placement observation available") : item->protection_observation_scope, m_details);
        if (!item->protection_reason.isEmpty()) DetailPair(layout, tr("Last observed reason"), item->protection_reason, m_details);
        auto* observation_hint = MutedText(tr("Local observations do not prove continuous availability or independent remote machines."), m_details);
        observation_hint->setWordWrap(true);
        layout->addWidget(observation_hint);
    }
    DetailPair(layout, tr("Encryption"), tr("Encrypted before sending · Recoverable with your account recovery phrase"), m_details);
    const auto& status = m_model->status();
    DetailPair(layout, tr("Owner"), status.primary_name.isEmpty() ? tr("You") : status.primary_name, m_details);
    layout->addSpacing(8);
    if (!item->folder && !item->trashed && item->state == CybouContentState::NeedsAttention) {
        // A change the network did not take: resubmit it or drop it.
        auto* retry = new QPushButton{tr("Try again"), m_details};
        retry->setObjectName(QStringLiteral("primaryButton"));
        retry->setProperty("cybouId", QStringLiteral("fileRetry"));
        connect(retry, &QPushButton::clicked, this, [this, id = item->id] { m_model->requestRetryFile(id); });
        layout->addWidget(retry);
        auto* discard = new QPushButton{tr("Discard"), m_details};
        discard->setObjectName(QStringLiteral("secondaryButton"));
        discard->setProperty("cybouId", QStringLiteral("fileDiscard"));
        connect(discard, &QPushButton::clicked, this, [this, id = item->id] {
            m_model->requestDiscardFile(id);
            showDetails({});
        });
        layout->addWidget(discard);
    }
    if (item->trashed) {
        auto* trash_box = Card(m_details);
        trash_box->setObjectName(QStringLiteral("trashLifecycleCard"));
        auto* t_layout = new QVBoxLayout{trash_box};
        t_layout->setContentsMargins(12, 10, 12, 10);
        t_layout->setSpacing(4);
        auto* t_title = new QLabel{tr("Deletion lifecycle"), trash_box};
        t_title->setObjectName(QStringLiteral("metricCaption"));
        t_layout->addWidget(t_title);

        auto* t_desc = new QLabel{
            tr("• Phase 1: Local catalog removal (immediate)\n"
               "• Phase 2: Finalized publication revocation (stops new admissions)\n"
               "• Phase 3: Storage lease closure (after period close)\n"
               "• Phase 4: Provider chunk purge (remote acknowledgements unconfirmed)\n"
               "• Phase 5: Retained copies (downloaded or shared copies remain)"), trash_box};
        t_desc->setObjectName(QStringLiteral("rowTitle"));
        t_desc->setWordWrap(true);
        t_layout->addWidget(t_desc);
        layout->addWidget(trash_box);

        auto* restore = new QPushButton{tr("Restore"), m_details};
        restore->setObjectName(QStringLiteral("primaryButton"));
        restore->setProperty("cybouId", QStringLiteral("fileRestore"));
        connect(restore, &QPushButton::clicked, this, [this, id = item->id] {
            m_model->requestRestoreFile(id);
            showDetails({});
        });
        layout->addWidget(restore);
        auto* forever = new QPushButton{tr("Delete forever"), m_details};
        forever->setObjectName(QStringLiteral("secondaryButton"));
        forever->setProperty("cybouId", QStringLiteral("fileDeleteForever"));
        connect(forever, &QPushButton::clicked, this, [this, id = item->id] {
            if (QMessageBox::question(this, tr("Delete forever"),
                    tr("Delete this item from your catalog?\n\n"
                       "Deletion proceeds through 5 distinct phases:\n"
                       "1. Immediate removal from your local catalog view.\n"
                       "2. Finalized RootPublication revocation on the network, stopping new admissions.\n"
                       "3. Storage lease closure upon billing period expiration.\n"
                       "4. Provider chunk purge: compliant providers purge unshared chunks. Remote purge acknowledgements are not cryptographically notarized; network cannot prove erasure of uncooperative or offline copies.\n"
                       "5. Retained copies: any copies previously downloaded, shared with recipients, or backed up externally remain unaffected.\n\n"
                       "Proceed with permanent deletion?")) != QMessageBox::Yes) return;
            m_model->requestDeleteFile(id);
            showDetails({});
        });
        layout->addWidget(forever);
    }
    if (!item->folder && !item->trashed) {
        auto* dl = new QPushButton{tr("Download"), m_details};
        dl->setObjectName(QStringLiteral("primaryButton"));
        dl->setProperty("cybouId", QStringLiteral("fileDownload"));
        const bool can_download = (item->state == CybouContentState::Protected || item->available_offline) &&
            (item->retrieval == CybouRetrievalState::Idle || item->retrieval == CybouRetrievalState::Ready);
        dl->setEnabled(can_download);
        const QString download_reason = can_download ? QString{}
            : item->retrieval != CybouRetrievalState::Idle && item->retrieval != CybouRetrievalState::Ready
                ? tr("Wait for the current download to finish.")
                : tr("No verified local copy is available and remote protection is incomplete.");
        dl->setToolTip(download_reason);
        connect(dl, &QPushButton::clicked, this, [this, id = item->id] { download(id); });
        layout->addWidget(dl);
        if (!download_reason.isEmpty()) {
            auto* reason = MutedText(download_reason, m_details);
            reason->setWordWrap(true);
            layout->addWidget(reason);
        }
        auto* send = new QPushButton{tr("Send by Mail"), m_details};
        send->setObjectName(QStringLiteral("secondaryButton"));
        send->setProperty("cybouId", QStringLiteral("fileSendByMail"));
        send->setEnabled(item->state == CybouContentState::Protected && onSendByMail && m_model->featureAvailability().mail);
        send->setToolTip(!m_model->featureAvailability().mail || !onSendByMail ? tr("Mail is unavailable.")
            : item->state != CybouContentState::Protected ? tr("This file must reach Protected before it can be attached by reference.") : QString{});
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
        // A 64-hex identifier has no break point: show it shortened, full value selectable in the tooltip.
        const QString root_id = item->content_root_id;
        DetailPair(box_layout, tr("Root content identifier"), root_id.isEmpty() ? none
            : root_id.size() > 20 ? root_id.left(10) + QStringLiteral("…") + root_id.right(8) : root_id, box);
        box->setToolTip(root_id);
        const quint64 chunk_count = (item->logical_size == 0) ? 1 : ((item->logical_size + 524287) / 524288);
        DetailPair(box_layout, tr("Chunk count"), tr("%1 chunks (512 KiB unit)").arg(chunk_count), box);
        DetailPair(box_layout, tr("Confidentiality assurance"), tr("Hybrid post-quantum encryption before upload (ML-KEM-768 + X25519). Plaintext and filename never sent to network."), box);
        DetailPair(box_layout, tr("Integrity assurance"), tr("Content-addressed BLAKE3 Merkle tree. Each chunk verified on retrieval against authorized RootPublication commitment."), box);
        DetailPair(box_layout, tr("Availability & durability scope"), item->min_remote_replicas < 0 || item->remote_replica_target <= 0
            ? tr("Remote copy count is unknown; independent remote machines have not been established.")
            : tr("Measured copies: %1 of %2 target. Replica deduplication is by StorageId; does not prove independent physical host failure domains.").arg(item->min_remote_replicas).arg(item->remote_replica_target), box);
        DetailPair(box_layout, tr("Recovery assurance"), tr("Recoverable on any node using your account recovery phrase via owner self-capsule. Historical capsules preserved across rotation."), box);

        auto* inspect_chunks = new QPushButton{tr("Inspect chunk tree"), box};
        inspect_chunks->setObjectName(QStringLiteral("inspectChunkTreeButton"));
        inspect_chunks->setCursor(Qt::PointingHandCursor);
        connect(inspect_chunks, &QPushButton::clicked, this, [this, id = item->id] {
            auto* dialog = new CybouConsoleDialog{m_model, this};
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            dialog->executeCommand(QStringLiteral("chunks ") + id);
            dialog->show();
        });
        box_layout->addWidget(inspect_chunks);

        DetailPair(box_layout, tr("Finalized height"), item->finalized_height > 0 ? QString::number(item->finalized_height) : none, box);
        DetailPair(box_layout, tr("Protection status"), CybouProduct::contentStateText(item->state), box);
        DetailPair(box_layout, tr("Local availability"), CybouProduct::localAvailabilityText(*item), box);
        DetailPair(box_layout, tr("Retrieval status"), item->retrieval == CybouRetrievalState::Idle
            ? tr("Not retrieved on this computer") : CybouProduct::retrievalText(item->retrieval), box);
        advanced->setProperty("cybouId", QStringLiteral("fileAdvanced"));
        advanced->setChecked(m_details_advanced);
        box->setVisible(m_details_advanced);
        connect(advanced, &QToolButton::toggled, box, &QWidget::setVisible);
        connect(advanced, &QToolButton::toggled, this, [this](bool open) { m_details_advanced = open; });
        layout->addWidget(box);
    }
    QTimer::singleShot(0, this, [scroll = QPointer<QScrollArea>{scroll}, scroll_value] {
        if (scroll) scroll->verticalScrollBar()->setValue(scroll_value);
    });
}

void StoragePage::dragEnterEvent(QDragEnterEvent* event)
{
    const bool internal = event->mimeData()->hasFormat(fileIdsMime());
    const bool accepted = internal
        ? m_view == View::MyFiles && canMoveFilesTo(dragIds(event->mimeData(), fileIdsMime()), m_folder)
        : !LocalDropPaths(event->mimeData()).isEmpty() && canUseFolder(m_view == View::MyFiles ? m_folder : QString{});
    if (accepted) event->acceptProposedAction(); else event->ignore();
}

void StoragePage::dropEvent(QDropEvent* event)
{
    bool accepted = false;
    if (event->mimeData()->hasFormat(fileIdsMime())) {
        accepted = m_view == View::MyFiles && moveFilesTo(dragIds(event->mimeData(), fileIdsMime()), m_folder);
    } else {
        const QStringList paths = LocalDropPaths(event->mimeData());
        accepted = !paths.isEmpty() && canUseFolder(m_view == View::MyFiles ? m_folder : QString{});
        if (accepted) uploadFiles(paths);
    }
    if (accepted) event->acceptProposedAction(); else event->ignore();
}
