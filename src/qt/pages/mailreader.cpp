// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/mailreader.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

QRgb PeerColor(const QString& peer)
{
    static const QRgb palette[] = {
        CybouTheme::BRAND_TEAL, CybouTheme::BLUE, CybouTheme::INDIGO,
        CybouTheme::VIOLET, CybouTheme::AMBER, CybouTheme::ROSE,
    };
    uint hash = 0;
    for (const QChar ch : peer) hash = (hash * 31) ^ ch.unicode();
    return palette[hash % std::size(palette)];
}

QString DisplayName(const CybouDesktopModel& model, const QString& name)
{
    if (!name.isEmpty() && name == model.status().primary_name) return MailReader::tr("Me");
    for (const auto& contact : model.contacts()) {
        if (contact.name == name) return contact.display_name;
    }
    return name.section(QLatin1Char{'.'}, 0, 0);
}

QToolButton* Action(Glyph glyph, const QString& tooltip, QWidget* parent)
{
    auto* button = IconButton(glyph, parent, tooltip);
    button->setAccessibleName(tooltip);
    button->setFixedSize(34, 34);
    button->setCursor(Qt::PointingHandCursor);
    return button;
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

void AddDetailRow(QVBoxLayout* layout, const QString& key, const QString& value, QWidget* parent)
{
    auto* row = new QHBoxLayout;
    auto* k = new QLabel{key, parent};
    k->setObjectName(QStringLiteral("rowSub"));
    k->setMinimumWidth(190);
    auto* v = new QLabel{value, parent};
    v->setObjectName(QStringLiteral("rowTitle"));
    v->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->setWordWrap(true);
    row->addWidget(k);
    row->addWidget(v, 1);
    layout->addLayout(row);
}

Glyph AttachmentGlyph(const QString& name)
{
    const QString lower = name.toLower();
    for (const char* ext : {".jpg", ".jpeg", ".png", ".gif", ".webp", ".heic"}) {
        if (lower.endsWith(QLatin1String{ext})) return Glyph::Image;
    }
    return Glyph::FileText;
}

} // namespace

MailReader::MailReader(CybouDesktopModel* model, QWidget* parent)
    : QFrame{parent}, m_model{model}
{
    setObjectName(QStringLiteral("card"));
    setAccessibleName(tr("Message"));
    setMinimumWidth(0);
    auto* root = new QVBoxLayout{this};
    root->setContentsMargins(22, 14, 22, 18);
    root->setSpacing(12);

    // Toolbar: Back (narrow layouts) + message actions.
    auto* toolbar = new QHBoxLayout;
    toolbar->setSpacing(4);
    m_back = new QToolButton{this};
    m_back->setObjectName(QStringLiteral("readerBack"));
    m_back->setText(tr("Back"));
    m_back->setToolTip(tr("Back to list (Esc)"));
    m_back->setAccessibleName(tr("Back to list"));
    m_back->setIcon(QIcon{glyphPixmap(Glyph::ChevronLeft, {18, 18}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    m_back->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_back->setCursor(Qt::PointingHandCursor);
    connect(m_back, &QToolButton::clicked, this, [this] { if (onBack) onBack(); });
    toolbar->addWidget(m_back);
    toolbar->addStretch();
    m_archive = Action(Glyph::Archive, tr("Archive"), this);
    auto* trash = Action(Glyph::Trash, tr("Move to Trash"), this);
    m_star = Action(Glyph::Star, tr("Star"), this);
    m_star->setCheckable(true);
    toolbar->addWidget(m_archive);
    toolbar->addWidget(trash);
    toolbar->addWidget(m_star);
    root->addLayout(toolbar);
    connect(m_archive, &QToolButton::clicked, this, [this] {
        const auto* item = m_model->mailItem(m_id);
        if (!item) return;
        const QString id = m_id;
        const auto from = item->folder;
        const auto to = from == CybouMailFolder::Archive ? CybouMailFolder::Inbox : CybouMailFolder::Archive;
        m_model->requestMoveMail(id, to);
        m_model->notify(to == CybouMailFolder::Archive ? tr("Conversation archived") : tr("Moved to Inbox"),
            tr("Undo"), [model = m_model, id, from] { model->requestMoveMail(id, from); });
        if (onBack) onBack();
    });
    connect(trash, &QToolButton::clicked, this, [this] {
        const auto* item = m_model->mailItem(m_id);
        if (!item) return;
        const QString id = m_id;
        const auto from = item->folder;
        m_model->requestMoveMail(id, CybouMailFolder::Trash);
        m_model->notify(tr("Moved to Trash"), tr("Undo"), [model = m_model, id, from] { model->requestMoveMail(id, from); });
        if (onBack) onBack();
    });
    connect(m_star, &QToolButton::clicked, this, [this](bool on) { m_model->requestMailStarred(m_id, on); });

    m_subject = new QLabel{this};
    m_subject->setObjectName(QStringLiteral("pageTitle"));
    m_subject->setWordWrap(true);
    m_subject->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_subject);

    // Sender header: Alice / alice.cybou / To: stan.cybou ........ 10:42
    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    m_avatar = new QLabel{this};
    m_avatar->setFixedSize(40, 40);
    header->addWidget(m_avatar, 0, Qt::AlignTop);
    auto* who = new QVBoxLayout;
    who->setSpacing(0);
    m_sender_name = new QLabel{this};
    m_sender_name->setObjectName(QStringLiteral("rowTitle"));
    m_sender = new QLabel{this};
    m_sender->setObjectName(QStringLiteral("rowSub"));
    m_sender->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_recipient = new QLabel{this};
    m_recipient->setObjectName(QStringLiteral("rowSub"));
    who->addWidget(m_sender_name);
    who->addWidget(m_sender);
    who->addWidget(m_recipient);
    header->addLayout(who, 1);
    m_time = new QLabel{this};
    m_time->setObjectName(QStringLiteral("rowMeta"));
    header->addWidget(m_time, 0, Qt::AlignTop);
    root->addLayout(header);

    // Compact security line; details on demand.
    auto* security_row = new QHBoxLayout;
    security_row->setSpacing(8);
    auto* shield = new QLabel{this};
    shield->setPixmap(glyphPixmap(Glyph::ShieldCheck, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK)));
    security_row->addWidget(shield);
    m_security = new QLabel{this};
    m_security->setObjectName(QStringLiteral("securityLine"));
    m_security->setStyleSheet(QStringLiteral("color: %1;").arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
    m_security->setWordWrap(true);
    security_row->addWidget(m_security, 1);
    auto* details = new QPushButton{tr("Details"), this};
    details->setFlat(true);
    details->setToolTip(tr("Security details"));
    details->setAccessibleName(tr("Security details"));
    details->setStyleSheet(QStringLiteral("QPushButton { border: none; background: transparent; color: %1; padding: 0 4px;"
                                          " min-height: 0; text-decoration: underline; font-weight: 600; }")
        .arg(CybouTheme::color(CybouTheme::BRAND_TEAL_DARK).name()));
    details->setProperty("cybouId", QStringLiteral("securityDetails"));
    details->setCursor(Qt::PointingHandCursor);
    connect(details, &QPushButton::clicked, this, [this] { showSecurityDetails(); });
    security_row->addWidget(details);
    root->addLayout(security_row);

    // Outgoing delivery state (finality-first; finalized is not Sent).
    m_delivery = new QFrame{this};
    m_delivery->setObjectName(QStringLiteral("deliveryBanner"));
    auto* delivery_layout = new QHBoxLayout{m_delivery};
    delivery_layout->setContentsMargins(14, 10, 10, 10);
    m_delivery_text = new QLabel{m_delivery};
    m_delivery_text->setWordWrap(true);
    delivery_layout->addWidget(m_delivery_text, 1);
    m_retry = new QPushButton{tr("Retry"), m_delivery};
    m_retry->setObjectName(QStringLiteral("secondaryButton"));
    m_retry->setProperty("cybouId", QStringLiteral("retrySend"));
    connect(m_retry, &QPushButton::clicked, this, [this] { m_model->requestRetryMail(m_id); });
    delivery_layout->addWidget(m_retry);
    root->addWidget(m_delivery);

    auto* separator = new QFrame{this};
    separator->setFrameShape(QFrame::HLine);
    separator->setObjectName(QStringLiteral("separator"));
    root->addWidget(separator);

    // Body + attachments scroll together.
    auto* scroll = new QScrollArea{this};
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName(QStringLiteral("readerScroll"));
    scroll->setStyleSheet(QStringLiteral("QScrollArea#readerScroll, QScrollArea#readerScroll > QWidget > QWidget { background: transparent; }"));
    auto* content = new QWidget{scroll};
    auto* content_layout = new QVBoxLayout{content};
    content_layout->setContentsMargins(0, 0, 6, 0);
    content_layout->setSpacing(16);
    m_body = new QLabel{content};
    m_body->setObjectName(QStringLiteral("readerBody"));
    m_body->setWordWrap(true);
    m_body->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    m_body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_body->setStyleSheet(QStringLiteral("QLabel#readerBody { color: %1; background: transparent; border: none; padding: 0; font-size: 14px; }")
        .arg(CybouTheme::color(CybouTheme::TEXT_PRIMARY).name()));
    content_layout->addWidget(m_body);
    m_attachments = new QWidget{content};
    m_attachment_rows = new QVBoxLayout{m_attachments};
    m_attachment_rows->setContentsMargins(0, 0, 0, 0);
    m_attachment_rows->setSpacing(8);
    content_layout->addWidget(m_attachments);
    content_layout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto* replies = new QHBoxLayout;
    replies->setSpacing(10);
    m_reply = new QPushButton{tr("Reply"), this};
    m_reply->setObjectName(QStringLiteral("secondaryButton"));
    m_reply->setIcon(QIcon{glyphPixmap(Glyph::Reply, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    m_forward = new QPushButton{tr("Forward"), this};
    m_forward->setObjectName(QStringLiteral("secondaryButton"));
    m_forward->setIcon(QIcon{glyphPixmap(Glyph::Forward, {16, 16}, CybouTheme::color(CybouTheme::BRAND_TEAL_DARK))});
    replies->addWidget(m_reply);
    replies->addWidget(m_forward);
    replies->addStretch();
    root->addLayout(replies);
    connect(m_reply, &QPushButton::clicked, this, [this] { if (onReply) onReply(m_id); });
    connect(m_forward, &QPushButton::clicked, this, [this] { if (onForward) onForward(m_id); });

    connect(m_model, &CybouDesktopModel::mailChanged, this, [this] { refresh(); });
    connect(m_model, &CybouDesktopModel::mailIdReplaced, this, [this](const QString& old_id, const QString& new_id) {
        if (m_id != old_id) return;
        m_id = new_id;
        refresh();
    });
}

void MailReader::setBackVisible(bool visible)
{
    m_back->setVisible(visible);
}

void MailReader::showMessage(const QString& id)
{
    m_id = id;
    refresh();
}

void MailReader::refresh()
{
    const auto* item = m_model->mailItem(m_id);
    if (!item) return;
    const bool outgoing = item->folder == CybouMailFolder::Sent || item->folder == CybouMailFolder::Drafts;
    m_subject->setText(item->subject.isEmpty() ? tr("(no subject)") : item->subject);
    m_avatar->setPixmap(avatarPixmap(item->from_name.left(1), PeerColor(item->from_name), 40));
    m_sender_name->setText(DisplayName(*m_model, item->from_name));
    m_sender->setText(item->from_name);
    m_recipient->setText(tr("To: %1").arg(item->to_name));
    m_time->setText(QLocale{}.toString(item->time, QStringLiteral("MMM d, HH:mm")));
    m_star->setChecked(item->starred);
    m_star->setIcon(QIcon{glyphPixmap(Glyph::Star, {18, 18}, CybouTheme::color(item->starred ? CybouTheme::AMBER : CybouTheme::TEXT_SECONDARY))});
    m_archive->setToolTip(item->folder == CybouMailFolder::Archive ? tr("Move to Inbox") : tr("Archive"));

    if (item->state == CybouContentState::Received) {
        // Finalized and opened here; the sender's storage durability is not claimed.
        m_security->setText(tr("Protected end to end  •  Post-quantum protected  •  Network confirmed"));
    } else if (item->state == CybouContentState::Protected) {
        m_security->setText(tr("Protected end to end  •  Post-quantum protected  •  Stored on the network"));
    } else if (outgoing) {
        m_security->setText(tr("Protected end to end  •  %1").arg(item->draft ? CybouProduct::mailStateText(*item)
            : CybouProduct::contentWithOperationText(item->state,
                m_model->displayedOperationState(item->operation_id, item->operation_state), m_model->status().online)));
    } else {
        m_security->setText(CybouProduct::contentStateText(item->state));
    }
    const bool online = m_model->status().online;
    const bool pending = outgoing && !item->draft && item->state != CybouContentState::Protected;
    m_delivery->setVisible(pending);
    if (pending) {
        QString text;
        const auto operation = m_model->displayedOperationState(item->operation_id, item->operation_state);
        switch (item->state) {
        case CybouContentState::Local:
            // Before finality the operation tells where the message is.
            if (!online) {
                text = tr("Waiting for network. Your message is saved and will be sent when CYBOU reconnects.");
            } else if (operation == CybouOperationState::Submitted) {
                text = tr("Waiting for confirmation… The network is confirming your message.");
            } else if (operation == CybouOperationState::Validated) {
                text = tr("Validated… Network validators checked your message; waiting for final confirmation.");
            } else {
                text = tr("Preparing… Your message is being encrypted on this computer.");
            }
            break;
        case CybouContentState::Securing:
            text = tr("Securing… Confirmed by the network. Keep CYBOU open until your message is stored securely.");
            break;
        case CybouContentState::NeedsAttention:
            text = tr("Needs attention. Your message could not be secured yet. It is kept on this computer.");
            break;
        case CybouContentState::TemporarilyUnavailable:
            text = tr("Temporarily unavailable — retrying.");
            break;
        case CybouContentState::Protected:
        case CybouContentState::Received:
            break;
        }
        m_delivery_text->setText(text);
        const bool attention = item->state == CybouContentState::NeedsAttention;
        m_delivery->setStyleSheet(QStringLiteral("QFrame#deliveryBanner { background: %1; border-radius: 10px; } QLabel { color: %2; }")
            .arg(CybouTheme::color(attention ? CybouTheme::ROSE_SOFT : CybouTheme::AMBER_SOFT).name(),
                CybouTheme::color(attention ? CybouTheme::ROSE : CybouTheme::TEXT_PRIMARY).name()));
        m_retry->setVisible(attention);
    }
    m_body->setText(item->body);
    m_reply->setEnabled(!item->draft);
    m_forward->setEnabled(!item->draft);

    ClearLayout(m_attachment_rows);
    m_attachments->setVisible(!item->attachments.isEmpty());
    if (!item->attachments.isEmpty()) {
        m_attachment_rows->addWidget(SectionTitle(tr("Attachments"), m_attachments));
    }
    for (const auto& attachment : item->attachments) {
        auto* chip = new QFrame{m_attachments};
        chip->setObjectName(QStringLiteral("attachmentChip"));
        chip->setStyleSheet(QStringLiteral("QFrame#attachmentChip { border: 1px solid %1; border-radius: 10px; }")
            .arg(CybouTheme::color(CybouTheme::BORDER).name()));
        auto* layout = new QHBoxLayout{chip};
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(10);
        layout->addWidget(Chip(AttachmentGlyph(attachment.name), Tint::Blue, chip, 34, 18));
        auto* text = new QVBoxLayout;
        text->setSpacing(0);
        auto* name = new ElidedLabel{attachment.name, chip};
        name->setObjectName(QStringLiteral("rowTitle"));
        text->addWidget(name);
        const QString retrieval = CybouProduct::retrievalText(attachment.retrieval);
        auto* meta = new QLabel{QStringLiteral("%1  ·  %2").arg(CybouProduct::sizeText(attachment.logical_size),
            !retrieval.isEmpty() ? retrieval : !attachment.saved_file_id.isEmpty() ? tr("Saved to Files")
                : CybouProduct::progressText(attachment.state, attachment.progress_percent, online,
                    m_model->displayedOperationState(item->operation_id, item->operation_state))), chip};
        meta->setObjectName(QStringLiteral("rowSub"));
        text->addWidget(meta);
        layout->addLayout(text, 1);
        const bool available = attachment.state == CybouContentState::Protected ||
            attachment.state == CybouContentState::Received;
        const bool saved = !attachment.saved_file_id.isEmpty();
        auto* download = new QPushButton{tr("Download"), chip};
        download->setObjectName(QStringLiteral("secondaryButton"));
        download->setProperty("cybouId", QStringLiteral("downloadAttachment"));
        download->setEnabled(available && attachment.retrieval == CybouRetrievalState::Idle);
        auto* more = new QToolButton{chip};
        more->setObjectName(QStringLiteral("iconButton"));
        more->setIcon(QIcon{glyphPixmap(Glyph::DotsV, {18, 18}, CybouTheme::color(CybouTheme::TEXT_SECONDARY))});
        more->setToolTip(tr("More actions"));
        more->setAccessibleName(tr("More actions for %1").arg(attachment.name));
        more->setPopupMode(QToolButton::InstantPopup);
        more->setFixedSize(34, 34);
        more->setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0; }"));
        auto* menu = new QMenu{more};
        auto* save = menu->addAction(saved ? tr("Saved to Files") : tr("Save to Files"));
        save->setObjectName(QStringLiteral("saveToFiles"));
        save->setEnabled(available && !outgoing && !saved && m_model->featureAvailability().files);
        more->setMenu(menu);
        layout->addWidget(download);
        layout->addWidget(more);
        const QString attachment_id = attachment.id;
        const QString attachment_name = attachment.name;
        connect(download, &QPushButton::clicked, this, [this, attachment_id, attachment_name] {
            if (onDownloadAttachment) {
                onDownloadAttachment(m_id, attachment_id);
                return;
            }
            const QString destination = QFileDialog::getSaveFileName(this, tr("Download attachment"), attachment_name);
            if (!destination.isEmpty()) m_model->requestAttachmentDownload(m_id, attachment_id, destination);
        });
        connect(save, &QAction::triggered, this, [this, attachment_id] {
            if (onSaveAttachment) {
                onSaveAttachment(m_id, attachment_id);
                return;
            }
            if (!m_model->requestSaveAttachmentToFiles(m_id, attachment_id).isEmpty()) m_model->notify(tr("Saved to Files"));
        });
        m_attachment_rows->addWidget(chip);
    }
}

void MailReader::showSecurityDetails()
{
    const auto* item = m_model->mailItem(m_id);
    if (!item) return;
    QDialog dialog{this};
    dialog.setObjectName(QStringLiteral("mailSecurityDetails"));
    dialog.setWindowTitle(tr("Security details"));
    dialog.setMinimumWidth(520);
    auto* layout = new QVBoxLayout{&dialog};
    layout->setContentsMargins(24, 20, 24, 16);
    layout->setSpacing(8);
    layout->addWidget(SectionTitle(tr("Security details"), &dialog));
    const bool confirmed = item->state == CybouContentState::Protected || item->state == CybouContentState::Received ||
        item->state == CybouContentState::Securing;
    AddDetailRow(layout, tr("Sender identity"), tr("Verified"), &dialog);
    AddDetailRow(layout, tr("Identity authorization"), tr("Valid"), &dialog);
    const auto operation = m_model->displayedOperationState(item->operation_id, item->operation_state);
    AddDetailRow(layout, tr("Network confirmation"), confirmed ? tr("Finalized") : tr("Waiting"), &dialog);
    AddDetailRow(layout, tr("Content protection"), tr("Verified"), &dialog);
    AddDetailRow(layout, tr("Content availability"), CybouProduct::contentStateText(item->state), &dialog);
    AddDetailRow(layout, tr("Post-quantum authorization"), tr("Ed25519 + ML-DSA"), &dialog);
    AddDetailRow(layout, tr("Post-quantum key encapsulation"), tr("X25519 + ML-KEM"), &dialog);
    layout->addSpacing(8);
    layout->addWidget(Eyebrow(tr("ADVANCED"), &dialog));
    const QString none = tr("Not reported yet");
    AddDetailRow(layout, tr("Finalized height"), item->finalized_height > 0 ? QString::number(item->finalized_height) : none, &dialog);
    AddDetailRow(layout, tr("Operation ID"), item->operation_id.isEmpty() ? none : item->operation_id, &dialog);
    AddDetailRow(layout, tr("Root content ID"), item->root_chunk_id.isEmpty() ? none : item->root_chunk_id, &dialog);
    auto* buttons = new QDialogButtonBox{QDialogButtonBox::Close, &dialog};
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}
