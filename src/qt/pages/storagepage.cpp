// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/storagepage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace CybouUi;

StoragePage::StoragePage(CybouDesktopModel* model, QWidget* parent)
    : QWidget{parent}, m_model{model}
{
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(24, 22, 24, 22);
    root->setSpacing(16);

    auto* header = new QHBoxLayout;
    auto* title_column = new QVBoxLayout;
    title_column->addWidget(HeroTitle(tr("Files"), this));
    m_gate_hint = MutedText(tr("Your file names are kept in an encrypted local index."), this);
    title_column->addWidget(m_gate_hint);
    header->addLayout(title_column, 1);
    m_refresh = new QPushButton{tr("Refresh"), this};
    m_refresh->setObjectName(QStringLiteral("secondaryButton"));
    header->addWidget(m_refresh, 0, Qt::AlignVCenter);
    m_upload = new QPushButton{tr("Upload"), this};
    m_upload->setObjectName(QStringLiteral("primaryButton"));
    header->addWidget(m_upload, 0, Qt::AlignVCenter);
    root->addLayout(header);

    auto* panes = new QHBoxLayout;
    panes->setSpacing(14);
    auto* rail = new QFrame{this};
    rail->setObjectName(QStringLiteral("card"));
    rail->setFixedWidth(176);
    auto* rail_layout = new QVBoxLayout{rail};
    rail_layout->setContentsMargins(14, 14, 14, 14);
    rail_layout->addWidget(Chip(Glyph::Folder, Tint::Blue, rail, 36, 18));
    auto* files_label = new QLabel{tr("My files"), rail};
    files_label->setObjectName(QStringLiteral("serviceTitle"));
    rail_layout->addWidget(files_label);
    rail_layout->addSpacing(8);
    rail_layout->addWidget(MutedText(tr("Recent files are shown in this local view."), rail));
    rail_layout->addStretch();
    panes->addWidget(rail);

    auto* middle = new QFrame{this};
    middle->setObjectName(QStringLiteral("card"));
    auto* middle_layout = new QVBoxLayout{middle};
    middle_layout->setContentsMargins(16, 16, 16, 16);
    middle_layout->setSpacing(10);
    auto* toolbar = new QHBoxLayout;
    m_search = new QLineEdit{middle};
    m_search->setPlaceholderText(tr("Search files…"));
    m_search->setClearButtonEnabled(true);
    toolbar->addWidget(m_search, 1);
    middle_layout->addLayout(toolbar);
    m_list = new QListWidget{middle};
    m_list->setObjectName(QStringLiteral("messageList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    middle_layout->addWidget(m_list, 1);
    panes->addWidget(middle, 3);

    m_details = new QFrame{this};
    m_details->setObjectName(QStringLiteral("card"));
    m_details->setMinimumWidth(220);
    m_details->setMaximumWidth(300);
    auto* detail_layout = new QVBoxLayout{m_details};
    detail_layout->setContentsMargins(18, 18, 18, 18);
    detail_layout->setSpacing(10);
    detail_layout->addWidget(new QLabel{tr("File details"), m_details});
    detail_layout->addWidget(MutedText(tr("Select a file to see its local metadata and download it."), m_details));
    detail_layout->addStretch();
    panes->addWidget(m_details);
    root->addLayout(panes, 1);

    m_usage_value = new QLabel{this};
    m_usage_value->setObjectName(QStringLiteral("rowMeta"));
    m_usage_caption = MutedText({}, this);
    root->addWidget(m_usage_value);
    root->addWidget(m_usage_caption);

    m_refresh->hide();
    m_refresh->setEnabled(false);
    connect(m_upload, &QPushButton::clicked, this, [this] {
        if (m_model->status().identity_state != CybouIdentityState::Active) {
            QMessageBox::information(this, tr("Identity required"), tr("Unlock your Identity before using Files."));
            return;
        }
        const auto source = QFileDialog::getOpenFileName(this, tr("Upload file"));
        if (!source.isEmpty()) m_model->requestFileUpload(source);
    });
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildList(); });
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        const auto* item = m_list->currentItem();
        if (!item) return;
        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= m_objects.size()) return;
        showDetails(index);
    });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::filesChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { refresh(); });
    refresh();
}

void StoragePage::refresh()
{
    m_objects.clear();
    for (const auto& file : m_model->fileItems()) {
        if (!file.trashed) m_objects.append(file);
    }
    const bool active = m_model->status().identity_state == CybouIdentityState::Active;
    m_upload->setEnabled(active && m_model->capabilities().files);
    m_gate_hint->setText(!active ? tr("Unlock your Identity to open Files.")
        : m_model->capabilities().files ? QString{} : tr("This feature is not connected yet."));
    m_usage_value->setText(tr("%1 used").arg(CybouProduct::sizeText(m_model->status().storage_used)));
    m_usage_caption->clear();
    rebuildList();
}

void StoragePage::rebuildList()
{
    const QString needle = m_search->text().trimmed().toCaseFolded();
    m_list->clear();
    bool visible{false};
    for (int i = 0; i < m_objects.size(); ++i) {
        const auto& file = m_objects.at(i);
        if (!needle.isEmpty() && !file.name.toCaseFolded().contains(needle)) continue;
        auto* item = new QListWidgetItem{QStringLiteral("%1\n%2  ·  %3")
            .arg(file.name, file.folder ? QStringLiteral("—") : CybouProduct::sizeText(file.logical_size),
                CybouProduct::contentStateText(file.state)), m_list};
        item->setData(Qt::UserRole, i);
        item->setSizeHint(QSize{0, 62});
        visible = true;
    }
    if (!visible) {
        auto* item = new QListWidgetItem{m_objects.isEmpty()
            ? tr("No files yet.")
            : tr("No files match your search."), m_list};
        item->setFlags(Qt::NoItemFlags);
        item->setTextAlignment(Qt::AlignCenter);
        item->setSizeHint(QSize{0, 110});
    }
}

void StoragePage::showDetails(const int index)
{
    const auto file = m_objects.at(index);
    auto* layout = qobject_cast<QVBoxLayout*>(m_details->layout());
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    auto* title = new QLabel{file.name, m_details};
    title->setObjectName(QStringLiteral("serviceTitle"));
    title->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(MutedText(CybouProduct::sizeText(file.logical_size), m_details));
    layout->addWidget(MutedText(CybouProduct::contentStateText(file.state), m_details));
    layout->addStretch();
    auto* download = new QPushButton{tr("Download"), m_details};
    download->setObjectName(QStringLiteral("primaryButton"));
    download->setEnabled(file.state == CybouContentState::Protected);
    connect(download, &QPushButton::clicked, this, [this, file] {
        const auto destination = QFileDialog::getSaveFileName(this, tr("Download file"), file.name);
        if (!destination.isEmpty()) m_model->requestFileDownload(file.id, destination);
    });
    layout->addWidget(download);
}
