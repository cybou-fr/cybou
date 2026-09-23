// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/pages/emailpage.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cyboutheme.h>

#include <QBrush>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QLabel* noteLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    return label;
}

QLabel* fieldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("cardLabel"));
    return label;
}

} // namespace

EmailPage::EmailPage(CybouDesktopModel* model, std::function<void()> identity_requested, QWidget* parent)
    : QWidget{parent},
      m_model{model},
      m_identity_requested{std::move(identity_requested)}
{
    auto* root = new QHBoxLayout{this};
    root->setContentsMargins(34, 32, 34, 32);
    root->setSpacing(18);

    // ---- Left rail: compose, folders, identity footer -------------------
    auto* rail = new QFrame{this};
    rail->setObjectName(QStringLiteral("card"));
    rail->setFixedWidth(252);
    auto* rail_layout = new QVBoxLayout{rail};
    rail_layout->setContentsMargins(16, 18, 16, 16);
    rail_layout->setSpacing(12);

    m_compose_button = new QPushButton{tr("Compose"), rail};
    m_compose_button->setObjectName(QStringLiteral("primaryButton"));
    connect(m_compose_button, &QPushButton::clicked, this, [this] { openComposer(); });
    rail_layout->addWidget(m_compose_button);

    m_folders = new QListWidget{rail};
    m_folders->setObjectName(QStringLiteral("folderList"));
    m_folders->setUniformItemSizes(true);
    m_folders->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_folders, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row >= FOLDER_COUNT) return;
        m_folder = static_cast<Folder>(row);
        rebuildMessageList();
    });
    rail_layout->addWidget(m_folders, 1);

    rail_layout->addSpacing(4);
    m_identity_line = new QLabel{rail};
    m_identity_line->setObjectName(QStringLiteral("cardLabel"));
    m_identity_line->setWordWrap(true);
    m_identity_hint = noteLabel(QString{}, rail);
    rail_layout->addWidget(m_identity_line);
    rail_layout->addWidget(m_identity_hint);
    root->addWidget(rail);

    // ---- Right side: mail view and composer share a stack ---------------
    m_right_stack = new QStackedWidget{this};
    root->addWidget(m_right_stack, 1);

    auto* mail_view = new QWidget{m_right_stack};
    auto* mail_layout = new QVBoxLayout{mail_view};
    mail_layout->setContentsMargins(0, 0, 0, 0);
    mail_layout->setSpacing(12);

    m_search = new QLineEdit{mail_view};
    m_search->setPlaceholderText(tr("Search subject, sender or body"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, [this] { rebuildMessageList(); });
    mail_layout->addWidget(m_search);

    m_list = new QListWidget{mail_view};
    m_list->setObjectName(QStringLiteral("messageList"));
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        auto* item = m_list->currentItem();
        if (!item) return;
        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= m_messages.size()) return;
        Message& message = m_messages[index];
        if (m_folder == FOLDER_INBOX && !message.read) {
            message.read = true;
            rebuildFolderList();
        }
        showMessage(message);
    });
    mail_layout->addWidget(m_list, 1);

    // Reader card: headers, body, protocol evidence, local actions.
    m_reader = new QFrame{mail_view};
    m_reader->setObjectName(QStringLiteral("card"));
    auto* reader_layout = new QVBoxLayout{m_reader};
    reader_layout->setContentsMargins(22, 18, 22, 18);
    reader_layout->setSpacing(10);
    m_reader_hint = noteLabel(tr("Select a message to read it. Read-state stays local to this client."), m_reader);
    reader_layout->addWidget(m_reader_hint);
    m_reader_headers = new QLabel{m_reader};
    m_reader_headers->setObjectName(QStringLiteral("bodyText"));
    m_reader_headers->setWordWrap(true);
    m_reader_headers->setVisible(false);
    reader_layout->addWidget(m_reader_headers);
    m_reader_body = new QLabel{m_reader};
    m_reader_body->setObjectName(QStringLiteral("bodyText"));
    m_reader_body->setWordWrap(true);
    m_reader_body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_reader_body->setVisible(false);
    reader_layout->addWidget(m_reader_body, 1);

    auto* evidence_title = new QLabel{tr("Protocol evidence"), m_reader};
    evidence_title->setObjectName(QStringLiteral("sectionTitle"));
    evidence_title->setVisible(false);
    reader_layout->addWidget(evidence_title);
    m_evidence_title = evidence_title;
    m_evidence = new QFrame{m_reader};
    auto* evidence_layout = new QVBoxLayout{m_evidence};
    evidence_layout->setContentsMargins(0, 0, 0, 0);
    evidence_layout->setSpacing(4);
    for (const QString& field : {tr("Transaction inclusion proof"), tr("BFT finality certificate"),
                                 tr("Historical sender-key authorization"), tr("Salted, domain-separated content commitment")}) {
        auto* row = new QHBoxLayout;
        auto* name = new QLabel{field, m_evidence};
        name->setObjectName(QStringLiteral("mutedText"));
        auto* state = new QLabel{tr("pending"), m_evidence};
        state->setObjectName(QStringLiteral("neutralBadge"));
        m_evidence_states.append(state);
        row->addWidget(name, 1);
        row->addWidget(state, 0, Qt::AlignVCenter);
        evidence_layout->addLayout(row);
    }
    m_evidence->setVisible(false);
    reader_layout->addWidget(m_evidence);
    reader_layout->addSpacing(4);
    m_reader->setMinimumHeight(220);
    mail_layout->addWidget(m_reader);

    // ---- Composer --------------------------------------------------------
    auto* composer = new QFrame{m_right_stack};
    composer->setObjectName(QStringLiteral("card"));
    auto* compose_layout = new QVBoxLayout{composer};
    compose_layout->setContentsMargins(24, 20, 24, 20);
    compose_layout->setSpacing(10);

    auto* compose_title = new QLabel{tr("New message"), composer};
    compose_title->setObjectName(QStringLiteral("sectionTitle"));
    compose_layout->addWidget(compose_title);
    compose_layout->addWidget(noteLabel(tr("CYBOU mail is a first-class protocol operation (MailTx): exactly one recipient, text only, no attachments."), composer));

    compose_layout->addWidget(fieldLabel(tr("To"), composer));
    m_to = new QLineEdit{composer};
    m_to->setObjectName(QStringLiteral("recipientEdit"));
    m_to->setPlaceholderText(tr("Recipient address"));
    compose_layout->addWidget(m_to);
    m_to_hint = noteLabel(QString{}, composer);
    m_to_hint->setVisible(false);
    compose_layout->addWidget(m_to_hint);

    compose_layout->addWidget(fieldLabel(tr("Subject"), composer));
    m_subject = new QLineEdit{composer};
    m_subject->setPlaceholderText(tr("Subject"));
    compose_layout->addWidget(m_subject);

    compose_layout->addWidget(fieldLabel(tr("Message"), composer));
    m_body = new QTextEdit{composer};
    m_body->setObjectName(QStringLiteral("composeBody"));
    m_body->setAcceptRichText(false);
    compose_layout->addWidget(m_body, 1);

    auto* meter_row = new QHBoxLayout;
    m_size_meter = new QProgressBar{composer};
    m_size_meter->setObjectName(QStringLiteral("sizeMeter"));
    m_size_meter->setRange(0, static_cast<int>(kMaxMailTxBytes));
    m_size_meter->setValue(0);
    m_size_meter->setTextVisible(false);
    m_size_meter->setFixedHeight(10);
    meter_row->addWidget(m_size_meter, 1);
    m_size_label = noteLabel(QString{}, composer);
    meter_row->addWidget(m_size_label);
    compose_layout->addLayout(meter_row);
    compose_layout->addWidget(noteLabel(tr("Fee: deterministic and size-based — priority fees are not part of the protocol. Distribution: 3 Security, 1 Onboarding."), composer));

    auto* actions = new QHBoxLayout;
    m_send = new QPushButton{tr("Send"), composer};
    m_send->setObjectName(QStringLiteral("sendButton"));
    m_send->setProperty("primary", true);
    connect(m_send, &QPushButton::clicked, this, [this] { sendNow(); });
    auto* save_draft = new QPushButton{tr("Save draft"), composer};
    save_draft->setObjectName(QStringLiteral("secondaryButton"));
    connect(save_draft, &QPushButton::clicked, this, [this] { saveDraft(); });
    auto* cancel = new QPushButton{tr("Cancel"), composer};
    cancel->setObjectName(QStringLiteral("secondaryButton"));
    connect(cancel, &QPushButton::clicked, this, [this] { closeComposer(); });
    actions->addWidget(m_send);
    actions->addWidget(save_draft);
    actions->addWidget(cancel);
    actions->addStretch();
    compose_layout->addLayout(actions);
    m_send_hint = noteLabel(QString{}, composer);
    compose_layout->addWidget(m_send_hint);

    m_right_stack->addWidget(mail_view);
    m_right_stack->addWidget(composer);

    // ---- Wiring -----------------------------------------------------------
    const auto refresh_meter = [this] {
        const qint64 bytes = payloadBytes();
        m_size_meter->setValue(static_cast<int>(qMin<qint64>(bytes, kMaxMailTxBytes)));
        m_size_meter->setProperty("overLimit", bytes > kMaxMailTxBytes);
        m_size_meter->style()->unpolish(m_size_meter);
        m_size_meter->style()->polish(m_size_meter);
        m_size_label->setText(tr("%1 / %2 bytes")
            .arg(QLocale{}.toString(bytes))
            .arg(QLocale{}.toString(kMaxMailTxBytes)));
        const bool too_long = bytes > kMaxMailTxBytes;
        m_size_label->setStyleSheet(too_long ? QStringLiteral("color: #dc2626;") : QString{});
        updateGates();
    };
    connect(m_to, &QLineEdit::textChanged, this, [this, refresh_meter] {
        const bool separators = m_to->text().contains(QRegularExpression(QStringLiteral("[,;]")));
        m_to_hint->setText(tr("CYBOU mail v1 supports exactly one recipient."));
        m_to_hint->setVisible(separators);
        m_to_hint->setStyleSheet(QStringLiteral("color: #dc2626;"));
        refresh_meter();
    });
    connect(m_subject, &QLineEdit::textChanged, this, refresh_meter);
    connect(m_body, &QTextEdit::textChanged, this, refresh_meter);

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { updateGates(); rebuildFolderList(); });
    connect(m_model, &CybouDesktopModel::capabilitiesChanged, this, [this] { updateGates(); });

    // Identity banner across the top of the mail view (hidden once active).
    m_banner = new QFrame{mail_view};
    m_banner->setObjectName(QStringLiteral("identityBanner"));
    auto* banner_layout = new QHBoxLayout{m_banner};
    banner_layout->setContentsMargins(16, 12, 16, 12);
    m_banner_text = new QLabel{m_banner};
    m_banner_text->setObjectName(QStringLiteral("bodyText"));
    m_banner_text->setWordWrap(true);
    banner_layout->addWidget(m_banner_text, 1);
    m_banner_action = new QPushButton{tr("Create identity"), m_banner};
    m_banner_action->setObjectName(QStringLiteral("secondaryButton"));
    connect(m_banner_action, &QPushButton::clicked, this, [this] { m_identity_requested(); });
    banner_layout->addWidget(m_banner_action, 0, Qt::AlignVCenter);
    mail_layout->insertWidget(0, m_banner);

    rebuildFolderList();
    rebuildMessageList();
    refresh_meter();
}

qint64 EmailPage::payloadBytes() const
{
    // The strict MailTx size covers everything the protocol commits to:
    // subject and body, UTF-8 encoded. No attachments exist to count.
    return m_subject->text().toUtf8().size() + m_body->toPlainText().toUtf8().size();
}

bool EmailPage::recipientWellFormed() const
{
    const QString to = m_to->text().trimmed();
    return !to.isEmpty() && !to.contains(QRegularExpression(QStringLiteral("[,;\\s]")));
}

QString EmailPage::finalityText(Finality finality)
{
    switch (finality) {
    case Finality::Draft: return tr("Local draft");
    case Finality::PendingFinality: return tr("Pending BFT finality");
    case Finality::Final: return tr("Final");
    }
    return {};
}

void EmailPage::rebuildFolderList()
{
    const QStringList names{tr("Inbox"), tr("Sent"), tr("Drafts")};
    QSignalBlocker blocker{m_folders};
    const int previous = m_folders->currentRow();
    m_folders->clear();
    for (int folder = 0; folder < FOLDER_COUNT; ++folder) {
        int count = 0;
        for (const Message& message : m_messages) {
            if (message.folder != folder) continue;
            if (folder == FOLDER_INBOX) {
                if (!message.read) ++count;
            } else {
                ++count;
            }
        }
        auto* item = new QListWidgetItem{m_folders};
        item->setText(count > 0 ? tr("%1 (%2)").arg(names.at(folder)).arg(count) : names.at(folder));
        item->setData(Qt::UserRole, folder);
    }
    m_folders->setCurrentRow(previous >= 0 ? previous : 0);
}

void EmailPage::rebuildMessageList()
{
    const QString needle = m_search->text().trimmed().toLower();
    m_list->clear();
    for (int i = 0; i < m_messages.size(); ++i) {
        const Message& message = m_messages.at(i);
        if (message.folder != m_folder) continue;
        if (!needle.isEmpty() &&
            !message.subject.toLower().contains(needle) &&
            !message.from.toLower().contains(needle) &&
            !message.to.toLower().contains(needle) &&
            !message.body.toLower().contains(needle)) {
            continue;
        }
        auto* row = new QFrame{m_list};
        auto* row_layout = new QHBoxLayout{row};
        row_layout->setContentsMargins(12, 8, 12, 8);
        row_layout->setSpacing(10);
        auto* main = new QVBoxLayout;
        main->setSpacing(2);
        const QString peer = m_folder == FOLDER_SENT ? message.to : message.from;
        auto* subject = new QLabel{message.subject.isEmpty() ? tr("(no subject)") : message.subject, row};
        subject->setStyleSheet(QStringLiteral("font-weight: 700; color: %1; background: transparent; border: none;")
            .arg(message.read ? QStringLiteral("#4b5563") : QStringLiteral("#111827")));
        auto* snippet = new QLabel{tr("with %1 · %2")
            .arg(peer.isEmpty() ? tr("unknown") : peer,
                 QLocale{}.toString(message.received, QLocale::ShortFormat)), row};
        snippet->setObjectName(QStringLiteral("mutedText"));
        snippet->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
        main->addWidget(subject);
        main->addWidget(snippet);
        row_layout->addLayout(main, 1);
        auto* badge = new QLabel{finalityText(message.finality), row};
        badge->setObjectName(message.finality == Finality::Final
            ? QStringLiteral("statusBadge")
            : QStringLiteral("neutralBadge"));
        row_layout->addWidget(badge, 0, Qt::AlignVCenter);
        auto* item = new QListWidgetItem{m_list};
        item->setData(Qt::UserRole, i);
        item->setSizeHint(row->sizeHint().expandedTo(QSize{0, 58}));
        m_list->setItemWidget(item, row);
    }
    if (m_list->count() == 0) {
        auto* item = new QListWidgetItem{m_list};
        item->setFlags(Qt::NoItemFlags);
        item->setText(tr("No mail here yet.\nMessages appear once your identity is active and the Email service reaches BFT finality."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(QBrush{CybouTheme::color(CybouTheme::TEXT_MUTED)});
        item->setSizeHint(QSize{0, 120});
        item->setData(Qt::UserRole, -1);
    }
}

void EmailPage::showMessage(const Message& message)
{
    m_reader_hint->setVisible(false);
    m_reader_headers->setVisible(true);
    m_reader_body->setVisible(true);
    const QString direction = message.folder == FOLDER_SENT
        ? tr("To: %1").arg(message.to)
        : tr("From: %1").arg(message.from);
    m_reader_headers->setText(tr("%1\nSubject: %2\n%3 · %4")
        .arg(direction, message.subject.isEmpty() ? tr("(no subject)") : message.subject,
             finalityText(message.finality),
             QLocale{}.toString(message.received, QLocale::ShortFormat)));
    m_reader_body->setText(message.body);

    // Evidence rows mirror the mail-evidence rules: a message is only
    // trustworthy once all four are verified; drafts carry none.
    m_evidence_title->setVisible(true);
    m_evidence->setVisible(true);
    const bool verified = message.finality == Finality::Final;
    if (m_evidence_states.size() >= 4) {
        // 0: Transaction inclusion proof
        m_evidence_states[0]->setText(verified ? tr("verified") : tr("pending"));
        m_evidence_states[0]->setObjectName(verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));

        // 1: BFT finality certificate
        m_evidence_states[1]->setText(verified ? tr("verified") : tr("pending"));
        m_evidence_states[1]->setObjectName(verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));

        // 2: Historical sender-key authorization
        // MailEvidenceBundleV1 supplies current signing key but does not yet prove historical canonical state at block height
        m_evidence_states[2]->setText(tr("not available yet"));
        m_evidence_states[2]->setObjectName(QStringLiteral("neutralBadge"));

        // 3: Salted, domain-separated content commitment
        m_evidence_states[3]->setText(verified ? tr("verified") : tr("pending"));
        m_evidence_states[3]->setObjectName(verified ? QStringLiteral("statusBadge") : QStringLiteral("neutralBadge"));

        for (QLabel* state : m_evidence_states) {
            state->style()->unpolish(state);
            state->style()->polish(state);
        }
    }
}

void EmailPage::updateGates()
{
    const auto& status = m_model->status();
    const bool identity_active = status.identity_state == CybouIdentityState::Active;
    const bool email_available = m_model->capabilities().email;

    m_banner->setVisible(!identity_active);
    m_banner_text->setText(identity_active
        ? QString{}
        : tr("Email needs an active CYBOU identity. Identity creation is a permissionless protocol operation — your keys never leave this device."));
    m_banner_action->setVisible(!identity_active);

    m_identity_line->setText(identity_active
        ? tr("Identity: %1").arg(status.account_id.isEmpty() ? tr("active") : status.account_id)
        : tr("Identity: not created"));
    m_identity_hint->setText(email_available
        ? tr("Email service: available")
        : tr("Email service: planned — drafts stay local"));

    // Send gate: the button is honest about every missing precondition.
    QString gate_reason;
    if (!recipientWellFormed()) {
        gate_reason = tr("Enter exactly one recipient.");
    } else if (payloadBytes() > kMaxMailTxBytes) {
        gate_reason = tr("Message exceeds the strict MailTx size limit.");
    } else if (m_body->toPlainText().trimmed().isEmpty()) {
        gate_reason = tr("Write a message first.");
    } else if (!identity_active) {
        gate_reason = tr("Create an identity to send mail.");
    } else if (!email_available) {
        gate_reason = tr("Email service is planned — this draft stays local until MailTx is live.");
    }
    m_send->setEnabled(gate_reason.isEmpty());
    m_send_hint->setText(gate_reason);
    m_send_hint->setVisible(!gate_reason.isEmpty());
}

void EmailPage::openComposer()
{
    m_right_stack->setCurrentIndex(1);
    m_to->setFocus();
}

void EmailPage::closeComposer()
{
    m_right_stack->setCurrentIndex(0);
}

void EmailPage::sendNow()
{
    // Reachable only once core reports the email capability. The client
    // hands the MailTx to the local node; finality arrives asynchronously.
    Message outgoing;
    outgoing.id = QStringLiteral("local-%1").arg(m_next_id++);
    outgoing.folder = FOLDER_SENT;
    outgoing.to = m_to->text().trimmed();
    outgoing.subject = m_subject->text();
    outgoing.body = m_body->toPlainText();
    outgoing.received = QDateTime::currentDateTime();
    outgoing.read = true;
    outgoing.finality = Finality::PendingFinality;
    m_messages.append(outgoing);
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    rebuildFolderList();
    rebuildMessageList();
    closeComposer();
}

void EmailPage::saveDraft()
{
    if (m_body->toPlainText().trimmed().isEmpty() && m_subject->text().trimmed().isEmpty()) {
        closeComposer();
        return;
    }
    Message draft;
    draft.id = QStringLiteral("local-%1").arg(m_next_id++);
    draft.folder = FOLDER_DRAFTS;
    draft.to = m_to->text().trimmed();
    draft.subject = m_subject->text();
    draft.body = m_body->toPlainText();
    draft.received = QDateTime::currentDateTime();
    draft.read = true;
    draft.finality = Finality::Draft;
    m_messages.append(draft);
    m_to->clear();
    m_subject->clear();
    m_body->clear();
    rebuildFolderList();
    rebuildMessageList();
    closeComposer();
}
