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
#include <QFileDialog>
#include <QMimeData>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

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
        m_to->setText(index.data(kNameRole).toString());
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
    connect(attach, &QPushButton::clicked, this, [this] {
        addAttachments(QFileDialog::getOpenFileNames(this, tr("Attach files")));
    });
    actions->addWidget(attach);
    m_send_hint = MutedText({}, this);
    actions->addWidget(m_send_hint, 1);
    auto* discard_button = new QPushButton{tr("Discard"), this};
    discard_button->setObjectName(QStringLiteral("secondaryButton"));
    discard_button->setToolTip(tr("Discard this draft"));
    connect(discard_button, &QPushButton::clicked, this, [this] { discard(); });
    actions->addWidget(discard_button);
    root->addLayout(actions);

    setTabOrder(m_to, m_subject);
    setTabOrder(m_subject, m_body);
    setTabOrder(m_body, m_send);
    connect(m_to, &QLineEdit::textChanged, this, [this] { updateGates(); });
    connect(m_subject, &QLineEdit::textChanged, this, [this] { updateGates(); });
    connect(m_body, &QTextEdit::textChanged, this, [this] { updateGates(); });
    connect(m_send, &QPushButton::clicked, this, [this] { send(); });
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { updateGates(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { updateGates(); });
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
        auto* item = new QStandardItem{QStringLiteral("%1  ·  %2%3").arg(contact.display_name, contact.name,
            contact.verified ? tr("  ·  Verified identity") : QString{})};
        item->setData(contact.name, kNameRole);
        model->appendRow(item);
    }
}

void MailCompose::start(const CybouMailItem& draft)
{
    rebuildCompleter();
    m_draft_id = draft.draft ? draft.id : QString{};
    m_to->setText(draft.to_name);
    m_subject->setText(draft.subject);
    m_body->setPlainText(draft.body);
    m_attachments = draft.attachments;
    rebuildAttachments();
    updateGates();
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
    for (const auto& path : paths) {
        if (path.isEmpty()) continue;
        m_attachments.append(m_model->localAttachment(path));
    }
    rebuildAttachments();
    updateGates();
}

void MailCompose::addProtectedAttachment(const CybouAttachmentItem& attachment)
{
    m_attachments.append(attachment);
    rebuildAttachments();
    updateGates();
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
            m_attachments.removeIf([&id](const CybouAttachmentItem& item) { return item.id == id; });
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
    const QString to = m_to->text().trimmed().toLower();
    if (to.isEmpty()) return tr("Add a recipient.");
    if (to.contains(QRegularExpression{QStringLiteral("[,;\\s]")})) return tr("Send to one recipient at a time.");
    if (!to.endsWith(QStringLiteral(".cybou"))) return tr("Use a CYBOU name, for example alice.cybou.");
    const QString problem = m_model->nameLabelProblem(to.chopped(6));
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
    } else if (const auto* contact = resolvedContact()) {
        m_to_hint->setText(contact->verified ? tr("%1  ·  Verified identity").arg(contact->display_name) : contact->display_name);
    } else {
        m_to_hint->setText(tr("The name is checked when you send."));
    }

    QString reason;
    if (status.identity_state != CybouIdentityState::Active) {
        reason = tr("Mail needs your CYBOU Identity.");
    } else if (!m_model->capabilities().mail) {
        reason = tr("This feature is not connected yet.");
    } else if (!to_problem.isEmpty()) {
        reason = m_to->text().trimmed().isEmpty() ? QString{} : to_problem;
    } else if (m_body->toPlainText().trimmed().isEmpty()) {
        reason = tr("Write a message.");
    }
    m_send->setEnabled(to_problem.isEmpty() && !m_body->toPlainText().trimmed().isEmpty() &&
        status.identity_state == CybouIdentityState::Active && m_model->capabilities().mail);
    m_send_hint->setText(reason);
}

CybouMailItem MailCompose::currentMessage() const
{
    CybouMailItem item;
    item.id = m_draft_id;
    item.to_name = m_to->text().trimmed().toLower();
    item.subject = m_subject->text().trimmed();
    item.body = m_body->toPlainText();
    item.attachments = m_attachments;
    return item;
}

void MailCompose::send()
{
    if (!m_send->isEnabled()) return;
    const QString id = m_model->requestSendMail(currentMessage());
    if (id.isEmpty()) {
        m_send_hint->setText(tr("This message has not been sent. It stays in Drafts."));
        return;
    }
    m_draft_id.clear();
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    m_attachments.clear();
    rebuildAttachments();
    if (onSent) onSent(id);
}

void MailCompose::saveDraftAndClose()
{
    if (hasContent() && m_model->status().identity_state == CybouIdentityState::Active) {
        m_draft_id = m_model->requestSaveMailDraft(currentMessage());
        m_model->notify(tr("Draft saved"));
    }
    m_draft_id.clear();
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    m_attachments.clear();
    rebuildAttachments();
    if (onClosed) onClosed();
}

void MailCompose::discard()
{
    if (!m_draft_id.isEmpty()) m_model->requestDeleteMail(m_draft_id);
    if (hasContent()) m_model->notify(tr("Draft discarded"));
    m_draft_id.clear();
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    m_attachments.clear();
    rebuildAttachments();
    if (onClosed) onClosed();
}
