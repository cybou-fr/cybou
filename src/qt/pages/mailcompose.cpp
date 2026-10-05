// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/mailcompose.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QAbstractItemView>
#include <QCompleter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMimeData>
#include <QMenu>
#include <QHeaderView>
#include <QTreeWidget>
#include <QSet>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPointer>
#include <QTimer>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

using namespace CybouUi;

namespace {

constexpr int kNameRole = Qt::UserRole + 1;

QLabel* FieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("rowSub"));
    label->setFixedWidth(64);
    return label;
}

} // namespace

MailCompose::MailCompose(CybouDesktopModel* model, QWidget* parent)
    : QFrame{parent}, m_model{model}
{
    setObjectName(QStringLiteral("card"));
    setAccessibleName(tr("New message"));
    setMinimumWidth(0);
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(22, 14, 22, 18);
    root->setSpacing(10);

    auto* title_row = new QHBoxLayout;
    m_back = IconButton(Glyph::ChevronLeft, this, tr("Back to list"));
    m_back->setFixedSize(34, 34);
    connect(m_back, &QToolButton::clicked, this, [this] { saveDraftAndClose(); });
    title_row->addWidget(m_back);
    auto* title = new QLabel{tr("New message"), this};
    title->setObjectName(QStringLiteral("pageTitle"));
    title_row->addWidget(title, 1);
    auto* close = new QToolButton{this};
    close->setObjectName(QStringLiteral("iconButton"));
    close->setProperty("cybouId", QStringLiteral("keepDraftAndClose"));
    close->setText(QStringLiteral("✕"));
    close->setFixedSize(34, 34);
    close->setToolTip(tr("Close and keep the draft"));
    close->setAccessibleName(tr("Close and keep the draft"));
    connect(close, &QToolButton::clicked, this, [this] { saveDraftAndClose(); });
    title_row->addWidget(close);
    root->addLayout(title_row);

    // To (one .cybou recipient with autocomplete).
    auto* to_row = new QHBoxLayout;
    to_row->addWidget(FieldLabel(tr("To"), this));
    m_to = new QLineEdit{this};
    m_to->setObjectName(QStringLiteral("recipientEdit"));
    m_to->setPlaceholderText(tr("name.cybou"));
    m_to->setAccessibleName(tr("To"));
    m_to->setMinimumHeight(36);
    to_row->addWidget(m_to, 1);
    root->addLayout(to_row);
    m_to_hint = new QLabel{this};
    m_to_hint->setObjectName(QStringLiteral("rowSub"));
    m_to_hint->setContentsMargins(74, 0, 0, 0);
    root->addWidget(m_to_hint);

    m_completer = new QCompleter{this};
    m_completer->setModel(new QStandardItemModel{m_completer});
    m_completer->setCompletionRole(kNameRole);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_to->setCompleter(m_completer);
    connect(m_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this, [this](const QModelIndex& index) {
        const QString address = index.data(kNameRole).toString();
        m_reply_address = QRegularExpression{QStringLiteral("^[0-9a-f]{64}$")}.match(address).hasMatch() ? address : QString{};
        m_to->setText(m_reply_address.isEmpty() ? address : CybouProduct::shortId(address));
        updateGates();
    });

    auto* subject_row = new QHBoxLayout;
    subject_row->addWidget(FieldLabel(tr("Subject"), this));
    m_subject = new QLineEdit{this};
    m_subject->setObjectName(QStringLiteral("subjectEdit"));
    m_subject->setAccessibleName(tr("Subject"));
    m_subject->setMinimumHeight(36);
    subject_row->addWidget(m_subject, 1);
    root->addLayout(subject_row);

    m_body = new QTextEdit{this};
    m_body->setObjectName(QStringLiteral("composeBody"));
    m_body->setAccessibleName(tr("Message"));
    m_body->setAcceptRichText(false);
    m_body->setPlaceholderText(tr("Write your message"));
    m_body->setTabChangesFocus(true);
    root->addWidget(m_body, 1);

    // Attachments: local until Send, then one RootPublication with the text.
    m_attachment_area = new QWidget{this};
    m_attachment_rows = new QVBoxLayout{m_attachment_area};
    m_attachment_rows->setContentsMargins(0, 0, 0, 0);
    m_attachment_rows->setSpacing(6);
    root->addWidget(m_attachment_area);
    m_drop_hint = MutedText(tr("Drop files to attach them"), this);
    m_drop_hint->setAlignment(Qt::AlignCenter);
    m_drop_hint->setStyleSheet(QStringLiteral("border: 2px dashed %1; border-radius: 10px; padding: 18px; color: %2;")
        .arg(CybouTheme::color(CybouTheme::MINT).name(), CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
    m_drop_hint->setVisible(false);
    root->addWidget(m_drop_hint);
    setAcceptDrops(true);

    auto* actions = new QHBoxLayout;
    actions->setSpacing(10);
    m_send = new QPushButton{tr("Send"), this};
    m_send->setObjectName(QStringLiteral("sendButton"));
    m_send->setProperty("primary", true);
    m_send->setStyleSheet(QStringLiteral("QPushButton#sendButton { background: %1; color: white; border: none; border-radius: 10px;"
                                         " padding: 9px 22px; font-weight: 700; }"
                                         "QPushButton#sendButton:disabled { background: %2; color: %3; }")
        .arg(CybouTheme::color(CybouTheme::MINT).name(), CybouTheme::color(CybouTheme::SURFACE).name(),
            CybouTheme::color(CybouTheme::DIM).name()));
    m_send->setIcon(QIcon{glyphPixmap(Glyph::Send, {16, 16}, QColor{Qt::white})});
    m_send->setShortcut(QKeySequence{QStringLiteral("Ctrl+Return")});
    m_send->setToolTip(tr("Send (Ctrl+Enter)"));
    actions->addWidget(m_send);
    auto* attach = new QPushButton{tr("Attach file"), this};
    attach->setObjectName(QStringLiteral("secondaryButton"));
    attach->setProperty("cybouId", QStringLiteral("attachFile"));
    attach->setIcon(QIcon{glyphPixmap(Glyph::File, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    auto* sources = new QMenu{attach};
    auto* local_files = sources->addAction(tr("From this computer"));
    local_files->setObjectName(QStringLiteral("attachLocalFile"));
    connect(local_files, &QAction::triggered, this, [this] {
        addAttachments(QFileDialog::getOpenFileNames(this, tr("Attach files")));
    });
    auto* cybou_files = sources->addAction(tr("From CYBOU Files"));
    cybou_files->setObjectName(QStringLiteral("attachCybouFile"));
    connect(cybou_files, &QAction::triggered, this, [this] { chooseCybouFiles(); });
    attach->setMenu(sources);
    actions->addWidget(attach);
    m_send_hint = MutedText({}, this);
    actions->addWidget(m_send_hint, 1);
    auto* discard_button = new QPushButton{tr("Discard"), this};
    discard_button->setObjectName(QStringLiteral("secondaryButton"));
    discard_button->setToolTip(tr("Discard this draft"));
    connect(discard_button, &QPushButton::clicked, this, [this] { discard(); });
    actions->addWidget(discard_button);
    root->addLayout(actions);
    m_save_hint = MutedText({}, this);
    m_save_hint->setObjectName(QStringLiteral("draftSaveStatus"));
    root->addWidget(m_save_hint);
    m_autosave = new QTimer{this};
    m_autosave->setSingleShot(true);
    m_autosave->setInterval(700);
    connect(m_autosave, &QTimer::timeout, this, [this] { saveDraft(false); });

    setTabOrder(m_to, m_subject);
    setTabOrder(m_subject, m_body);
    setTabOrder(m_body, m_send);
    connect(m_to, &QLineEdit::textChanged, this, [this] {
        if (!m_reply_address.isEmpty() && m_to->text()!=CybouProduct::shortId(m_reply_address)) m_reply_address.clear();
        updateGates();
        edited();
    });
    connect(m_subject, &QLineEdit::textChanged, this, [this] { updateGates(); edited(); });
    connect(m_body, &QTextEdit::textChanged, this, [this] { updateGates(); edited(); });
    connect(m_send, &QPushButton::clicked, this, [this] { send(); });
    connect(m_model, &CybouDesktopModel::mailTasksChanged, this, [this] {
        if (!m_following_send || m_draft_id.isEmpty()) return;
        for (const auto& task : m_model->mailTasks()) {
            if (task.kind != CybouMailTaskKind::Send || task.item_id != m_draft_id) continue;
            if (task.state == CybouCommandState::Committed) {
                const auto id = m_model->resolvedMailId(task.related_id);
                clearCompose();
                if (onSent) onSent(id);
            } else if (task.state == CybouCommandState::Failed) {
                m_following_send = m_sending = false;
                updateGates();
                m_send_hint->setText(task.error);
            }
            return;
        }
    });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { updateGates(); });
    connect(m_model, &CybouDesktopModel::featureAvailabilityChanged, this, [this] { updateGates(); });
    updateGates();
}

void MailCompose::setBackVisible(bool visible)
{
    m_back->setVisible(visible);
}

void MailCompose::rebuildCompleter()
{
    auto* model = static_cast<QStandardItemModel*>(m_completer->model());
    model->clear();
    for (const auto& contact : m_model->contacts()) {
        const QString label = contact.name.endsWith(QStringLiteral(".cybou")) ? contact.name : contact.display_name;
        const QString text = contact.name.endsWith(QStringLiteral(".cybou"))
            ? QStringLiteral("%1  ·  %2").arg(contact.display_name, label) : label;
        auto* item = new QStandardItem{text + (contact.verified ? tr("  ·  Verified identity") : QString{})};
        item->setData(contact.name, kNameRole);
        model->appendRow(item);
    }
}

void MailCompose::start(const CybouMailItem& draft)
{
    m_loading = true;
    ++m_compose_generation;
    m_autosave->stop();
    m_saving = m_sending = m_close_requested = m_following_send = false;
    m_revision = m_saved_revision = 0;
    m_save_hint->clear();
    rebuildCompleter();
    m_draft_id = draft.draft ? draft.id : QString{};
    m_reply_address.clear();
    if (QRegularExpression{QStringLiteral("^[0-9a-fA-F]{64}$")}.match(draft.to_name).hasMatch())
        m_reply_address=draft.to_name.toLower();
    m_to->setText(m_reply_address.isEmpty() ? draft.to_name : CybouProduct::shortId(m_reply_address));
    m_subject->setText(draft.subject);
    m_body->setPlainText(draft.body);
    m_attachments = draft.attachments;
    m_loading = false;
    m_following_send = false;
    for (const auto& task : m_model->mailTasks()) {
        if (task.kind == CybouMailTaskKind::Send && task.item_id == m_draft_id &&
            (task.state == CybouCommandState::Queued || task.state == CybouCommandState::Running)) {
            m_sending = m_following_send = true;
        }
    }
    rebuildAttachments();
    updateGates();
    if (!m_sending && hasContent() && m_draft_id.isEmpty()) edited();
    if (draft.to_name.isEmpty()) {
        m_to->setFocus();
    } else {
        m_body->setFocus();
        m_body->moveCursor(QTextCursor::Start);
    }
}

bool MailCompose::hasContent() const
{
    return !m_to->text().trimmed().isEmpty() || !m_subject->text().trimmed().isEmpty() ||
        !m_body->toPlainText().trimmed().isEmpty() || !m_attachments.isEmpty();
}

void MailCompose::addAttachments(const QStringList& paths)
{
    if (m_sending || m_close_requested) return;
    for (const auto& path : paths) {
        if (path.isEmpty()) continue;
        m_attachments.append(m_model->localAttachment(path));
    }
    rebuildAttachments();
    updateGates();
    edited();
}

void MailCompose::addProtectedAttachment(const CybouAttachmentItem& attachment)
{
    if (m_sending || m_close_requested) return;
    m_attachments.append(attachment);
    rebuildAttachments();
    updateGates();
    edited();
}

void MailCompose::chooseCybouFiles()
{
    QDialog dialog{this};
    dialog.setObjectName(QStringLiteral("cybouFilePicker"));
    dialog.setWindowTitle(tr("Attach from CYBOU Files"));
    dialog.resize(640, 420);
    auto* layout = new QVBoxLayout{&dialog};
    auto* hint = new QLabel{tr("Choose protected files. Their encrypted content is reused without uploading it again."), &dialog};
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* files = new QTreeWidget{&dialog};
    files->setObjectName(QStringLiteral("cybouFileChoices"));
    files->setHeaderLabels({tr("File"), tr("Size"), tr("State")});
    files->setRootIsDecorated(false);
    files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    files->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    layout->addWidget(files, 1);
    for (const auto& file : m_model->fileItems()) {
        if (file.folder || file.trashed) continue;
        QStringList path{file.name};
        QSet<QString> seen{file.id};
        auto parent = file.parent_id;
        bool hidden{false};
        while (!parent.isEmpty()) {
            if (seen.contains(parent)) { hidden=true; break; }
            seen.insert(parent);
            const auto* folder=m_model->fileItem(parent);
            if (!folder || folder->trashed) { hidden=true; break; }
            path.prepend(folder->name);
            parent=folder->parent_id;
        }
        if (hidden) continue;
        const bool ready=m_model->attachmentFromFile(file.id).has_value();
        auto* row = new QTreeWidgetItem{files, {path.join(QLatin1Char{'/'}),
            CybouProduct::sizeText(file.logical_size), ready ? tr("Protected") : tr("Not protected yet")}};
        row->setData(0, Qt::UserRole, file.id);
        if (!ready) row->setFlags(row->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
    }
    files->sortItems(0, Qt::AscendingOrder);
    if (files->topLevelItemCount()==0) hint->setText(tr("There are no files to attach in CYBOU Files."));
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    auto* confirm=buttons->button(QDialogButtonBox::Ok);
    confirm->setText(tr("Attach"));
    confirm->setEnabled(false);
    connect(files, &QTreeWidget::itemSelectionChanged, &dialog, [files,confirm] {
        confirm->setEnabled(!files->selectedItems().isEmpty());
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec()!=QDialog::Accepted) return;
    for (const auto* row : files->selectedItems()) {
        if (const auto attachment=m_model->attachmentFromFile(row->data(0,Qt::UserRole).toString())) {
            if (std::none_of(m_attachments.begin(),m_attachments.end(),[&](const auto& current) { return current.id==attachment->id; }))
                addProtectedAttachment(*attachment);
        }
    }
}

void MailCompose::rebuildAttachments()
{
    while (QLayoutItem* item = m_attachment_rows->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_attachment_area->setVisible(!m_attachments.isEmpty());
    for (int i = 0; i < m_attachments.size(); ++i) {
        const auto& attachment = m_attachments.at(i);
        auto* chip = new QFrame{m_attachment_area};
        chip->setObjectName(QStringLiteral("attachmentChip"));
        chip->setStyleSheet(QStringLiteral("QFrame#attachmentChip { border: 1px solid %1; border-radius: 10px; }")
            .arg(CybouTheme::color(CybouTheme::BORDER).name()));
        auto* layout = new QHBoxLayout{chip};
        layout->setContentsMargins(10, 6, 6, 6);
        layout->setSpacing(10);
        layout->addWidget(Chip(Glyph::FileText, Tint::Blue, chip, 28, 16));
        auto* name = new ElidedLabel{attachment.name, chip};
        name->setObjectName(QStringLiteral("rowTitle"));
        layout->addWidget(name, 1);
        auto* size = new QLabel{CybouProduct::sizeText(attachment.logical_size), chip};
        size->setObjectName(QStringLiteral("rowSub"));
        layout->addWidget(size);
        // Before Send an attachment exists only on this device (or is
        // already protected content being reused from Files).
        auto* state = new QLabel{attachment.state == CybouContentState::Protected ? tr("Protected")
            : attachment.state == CybouContentState::Received ? tr("Received") : tr("On this device"), chip};
        state->setObjectName(QStringLiteral("rowMeta"));
        layout->addWidget(state);
        auto* remove = IconButton(Glyph::Trash, chip, tr("Remove %1").arg(attachment.name));
        remove->setAccessibleName(tr("Remove %1").arg(attachment.name));
        connect(remove, &QToolButton::clicked, this, [this, id = attachment.id] {
            if (m_sending || m_close_requested) return;
            m_attachments.removeIf([&id](const CybouAttachmentItem& item) { return item.id == id; });
            edited();
            rebuildAttachments();
            updateGates();
        });
        layout->addWidget(remove);
        m_attachment_rows->addWidget(chip);
    }
}

void MailCompose::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        m_drop_hint->setVisible(true);
        event->acceptProposedAction();
    }
}

void MailCompose::dragLeaveEvent(QDragLeaveEvent* event)
{
    m_drop_hint->setVisible(false);
    QFrame::dragLeaveEvent(event);
}

void MailCompose::dropEvent(QDropEvent* event)
{
    m_drop_hint->setVisible(false);
    QStringList paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) paths << url.toLocalFile();
    }
    addAttachments(paths);
    event->acceptProposedAction();
}

const CybouContact* MailCompose::resolvedContact() const
{
    const QString to = m_to->text().trimmed().toLower();
    for (const auto& contact : m_model->contacts()) {
        if (contact.name == to) return &contact;
    }
    return nullptr;
}

QString MailCompose::recipientProblem() const
{
    if (!m_reply_address.isEmpty()) return {};
    const QString to = m_to->text().trimmed().toLower();
    if (to.isEmpty()) return tr("Add a recipient.");
    if (to.contains(QRegularExpression{QStringLiteral("[,;\\s]")})) return tr("Send to one recipient at a time.");
    if (QRegularExpression{QStringLiteral("^[0-9a-f]{64}$")}.match(to).hasMatch()) return {};
    if (!to.endsWith(QStringLiteral(".cybou"))) return tr("Use a CYBOU name, for example alice.cybou.");
    const QString problem = m_model->recipientNameProblem(to.chopped(6));
    if (!problem.isEmpty()) return problem;
    return {};
}

void MailCompose::updateGates()
{
    const auto& status = m_model->status();
    const QString to_problem = recipientProblem();
    if (m_to->text().trimmed().isEmpty()) {
        m_to_hint->clear();
    } else if (!to_problem.isEmpty()) {
        m_to_hint->setText(to_problem);
    } else if (m_to->text().trimmed().toLower() == CybouDesktopModel::supportName()) {
        // Support mail pays a higher network fee on purpose (anti-spam); the network never sees the recipient.
        const auto fee = m_model->supportMailFee();
        m_to_hint->setText(fee ? tr("CYBOU Support  ·  a message here costs about %1 from System Balance (support rate)")
                                     .arg(cybouAmountText(*fee))
                               : tr("CYBOU Support  ·  messages here cost more than usual (support rate)"));
    } else if (const auto* contact = resolvedContact()) {
        m_to_hint->setText(contact->verified ? tr("%1  ·  Verified identity").arg(contact->display_name) : contact->display_name);
    } else {
        m_to_hint->setText(!m_reply_address.isEmpty() || m_to->text().trimmed().size()==64 ? tr("The Identity is checked when you send.")
                                                          : tr("The name is checked when you send."));
    }

    QString reason;
    if (status.identity_state != CybouIdentityState::Active) {
        reason = tr("Mail needs your CYBOU Identity.");
    } else if (!m_model->featureAvailability().mail) {
        reason = tr("This feature is not connected yet.");
    } else if (!to_problem.isEmpty()) {
        reason = m_to->text().trimmed().isEmpty() ? QString{} : to_problem;
    } else if (m_body->toPlainText().trimmed().isEmpty()) {
        reason = tr("Write a message.");
    }
    m_to->setEnabled(!m_sending && !m_close_requested);
    m_subject->setEnabled(!m_sending && !m_close_requested);
    m_body->setEnabled(!m_sending && !m_close_requested);
    m_send->setEnabled(!m_sending && !m_close_requested && to_problem.isEmpty() && !m_body->toPlainText().trimmed().isEmpty() &&
        status.identity_state == CybouIdentityState::Active && m_model->featureAvailability().mail);
    m_send_hint->setText(reason);
}

CybouMailItem MailCompose::currentMessage() const
{
    CybouMailItem item;
    item.id = m_draft_id;
    item.to_name = m_reply_address.isEmpty() ? m_to->text().trimmed().toLower() : m_reply_address;
    item.subject = m_subject->text().trimmed();
    item.body = m_body->toPlainText();
    item.attachments = m_attachments;
    return item;
}

CybouMailItem MailCompose::snapshotForRebuild() const
{
    auto item = currentMessage();
    item.draft = true;
    return item;
}

void MailCompose::edited()
{
    if (m_loading || m_sending || m_close_requested) return;
    ++m_revision;
    m_save_hint->setText(tr("Unsaved changes"));
    m_autosave->start();
}

void MailCompose::clearCompose()
{
    m_loading = true;
    ++m_compose_generation;
    m_autosave->stop();
    m_draft_id.clear();
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    m_attachments.clear();
    m_saving = m_sending = m_close_requested = m_following_send = false;
    m_revision = m_saved_revision = 0;
    m_save_hint->clear();
    rebuildAttachments();
    m_loading = false;
    updateGates();
}

void MailCompose::saveDraft(bool close)
{
    if (m_sending) return;
    m_close_requested = m_close_requested || close;
    m_autosave->stop();
    if (!hasContent() && m_draft_id.isEmpty()) {
        if (close) { clearCompose(); if (onClosed) onClosed(); }
        return;
    }
    if (m_saving) { updateGates(); return; }
    m_saving = true;
    const auto revision = m_revision;
    const auto generation = m_compose_generation;
    m_save_hint->setText(tr("Saving draft…"));
    updateGates();
    const QPointer<MailCompose> guard{this};
    const auto id = m_model->requestSaveMailDraft(currentMessage(), [guard, revision, generation](bool ok, const QString& error) {
        if (!guard || guard->m_compose_generation != generation) return;
        guard->m_saving = false;
        if (!ok) {
            guard->m_close_requested = false;
            guard->m_save_hint->setText(tr("Save failed. Your text is kept here; close again to retry."));
            guard->m_model->notify(error);
            guard->updateGates();
            return;
        }
        guard->m_saved_revision = revision;
        if (guard->m_revision != revision) {
            if (guard->m_close_requested) guard->saveDraft(true);
            else guard->m_autosave->start();
            return;
        }
        guard->m_save_hint->setText(tr("Draft saved on this computer"));
        if (guard->m_close_requested) {
            guard->clearCompose();
            if (guard->onClosed) guard->onClosed();
        }
        guard->updateGates();
    });
    if (id.isEmpty()) {
        m_saving = m_close_requested = false;
        m_save_hint->setText(tr("Draft not saved. Mail is unavailable; your text is kept here."));
        updateGates();
    } else m_draft_id = id;
}

void MailCompose::send()
{
    if (!m_send->isEnabled()) return;
    // Assign a durable draft identity before handing off, also for a new compose.
    if (m_draft_id.isEmpty()) saveDraft(false);
    if (m_draft_id.isEmpty()) return;
    m_sending = true;
    m_autosave->stop();
    updateGates();
    m_send_hint->setText(tr("Preparing message. Your draft is kept until it is saved."));
    const QPointer<MailCompose> guard{this};
    const auto generation = m_compose_generation;
    const auto outgoing_id = std::make_shared<QString>();
    *outgoing_id = m_model->requestSendMail(currentMessage(), [guard, generation, outgoing_id](bool ok, const QString& error) {
        if (!guard || guard->m_compose_generation != generation) return;
        guard->m_sending = false;
        if (!ok) {
            guard->updateGates();
            guard->m_send_hint->setText(error);
            return;
        }
        guard->clearCompose();
        if (guard->onSent) guard->onSent(guard->m_model->resolvedMailId(*outgoing_id));
    });
    if (outgoing_id->isEmpty()) {
        m_sending = false;
        updateGates();
        m_send_hint->setText(tr("This message has not been sent. Your text is kept here."));
    }
}

void MailCompose::saveDraftAndClose()
{
    saveDraft(true);
}

void MailCompose::discard()
{
    if (m_sending || m_close_requested) return;
    if (!m_draft_id.isEmpty()) m_model->requestDeleteMail(m_draft_id);
    clearCompose();
    if (onClosed) onClosed();
}
