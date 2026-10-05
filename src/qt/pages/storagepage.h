// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_PAGES_STORAGEPAGE_H
#define CYBOU_QT_PAGES_STORAGEPAGE_H

#include <qt/cybouproduct.h>

#include <QCoreApplication>
#include <QHash>
#include <QWidget>

#include <functional>
#include <memory>

class CybouDesktopModel;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QListWidgetItem;
class QDragEnterEvent;
class QDropEvent;

/**
 * CYBOU Files: Google Drive-familiar file management over the product
 * model (docs/cybou/83_STORAGE_UI_UX.md). Users manage files and folders,
 * never chunks, providers or content identifiers.
 */
class StoragePage : public QWidget
{
    Q_DECLARE_TR_FUNCTIONS(StoragePage)

public:
    enum class View {
        MyFiles,
        Recent,
        Starred,
        Trash,
    };

    StoragePage(CybouDesktopModel* model, std::function<void()> home_requested = {}, QWidget* parent = nullptr);

    ~StoragePage() override;

    View view() const { return m_view; }
    void setView(View view);
    void openFolder(const QString& folder_id);
    void setSearchText(const QString& text);
    /** Moves items into a folder ("" = My files), refusing folder cycles. */
    bool moveFilesTo(const QStringList& ids, const QString& folder_id);
    QString currentFolder() const { return m_folder; }
    bool gridMode() const { return m_grid; }
    void setGridMode(bool grid);
    /** Item ids currently listed (after view, folder and search). */
    QStringList visibleIds() const;
    /** Opens the details drawer for a file or folder. */
    void showDetails(const QString& id);
    QString detailsId() const { return m_details_id; }
    /** Sort the list by Name (0), Size (1) or Modified (2); folders stay first. */
    void sortBy(int column, bool descending);
    /** Uploads local files into the current folder. */
    void uploadFiles(const QStringList& paths);
    /** Set by the shell: opens Mail compose with this file attached. */
    std::function<void(const QString& file_id)> onSendByMail;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    CybouDesktopModel* const m_model;
    const std::function<void()> m_home_requested;
    View m_view{View::MyFiles};
    QString m_folder;
    bool m_grid{false};
    QStringList m_visible;
    QHash<QString, QTreeWidgetItem*> m_rows;
    QHash<QString, QListWidgetItem*> m_grid_items;

    QPushButton* m_new{nullptr};
    /** Visible in Trash only: permanently deletes everything there as one change. */
    QPushButton* m_empty_trash{nullptr};
    QListWidget* m_nav{nullptr};
    QLabel* m_usage_text{nullptr};
    QProgressBar* m_usage_bar{nullptr};
    QLabel* m_title{nullptr};
    QPushButton* m_up{nullptr};
    QLineEdit* m_search{nullptr};
    QToolButton* m_list_toggle{nullptr};
    QToolButton* m_grid_toggle{nullptr};
    QFrame* m_banner{nullptr};
    QLabel* m_banner_text{nullptr};
    QStackedWidget* m_views{nullptr};
    QTreeWidget* m_table{nullptr};
    QListWidget* m_tiles{nullptr};
    QLabel* m_empty{nullptr};
    QWidget* m_empty_box{nullptr};
    QWidget* m_empty_actions{nullptr};
    QWidget* m_crumbs{nullptr};
    /** What the crumbs currently show; unchanged crumbs are not rebuilt (a rebuild mid-drag drops the target). */
    QString m_crumbs_key;
    QFrame* m_selection_bar{nullptr};
    QLabel* m_selection_text{nullptr};
    int m_sort_column{0};
    QPoint m_press_pos;
    QString m_press_id;
    QWidget* m_drop_highlight{nullptr};

    QString itemIdAt(QWidget* viewport, const QPoint& pos) const;
    bool handleItemDrag(QWidget* viewport, QEvent* event);
    bool m_sort_descending{false};
    QFrame* m_details{nullptr};
    /** Scrollable body of the details panel: a tall Advanced section never squeezes rows. */
    QWidget* m_details_body{nullptr};
    QString m_details_id;
    bool m_details_advanced{false};

    struct FolderImport;
    std::shared_ptr<FolderImport> m_import;
    void advanceImport();
    void finishImport(const QString& message);

    QStringList selectedIds() const;
    void showContextMenu(const QPoint& global_pos);
    void promptNewFolder();
    void promptUploadFolder();
    /** Where the user saved this file with Download, if that copy is still complete; else empty. */
    QString downloadedPath(const QString& id) const;
    /** Uploads files and discovers bounded folder trees in the background under `parent`. */
    void uploadPaths(const QStringList& paths, const QString& parent);
    void promptRename(const QString& id);
    void promptMove(const QString& id);
    void download(const QString& id);
    void rebuildDetails();
    void rebuildCrumbs();
    void refreshSelectionBar();

    QVector<CybouFileItem> collect() const;
    QString folderName(const QString& id) const;
    void rebuild();
    void refreshChrome();
    void updateColumns();
    void activate(const QString& id);
};

#endif // CYBOU_QT_PAGES_STORAGEPAGE_H
