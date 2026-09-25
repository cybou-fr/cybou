// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/recoveryphrasedialog.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QLabel* noteLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setObjectName(QStringLiteral("mutedText"));
    label->setWordWrap(true);
    return label;
}

QLabel* warningLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: #b45309;"));
    return label;
}

QLabel* boldLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel{text, parent};
    label->setStyleSheet(QStringLiteral("font-weight: 700;"));
    label->setWordWrap(true);
    return label;
}

} // namespace

RecoveryPhraseDialog::RecoveryPhraseDialog(Mode mode, const QStringList& words, QWidget* parent)
    : QDialog{parent}, m_mode{mode}, m_words{words}
{
    setWindowTitle(mode == Mode::Create
        ? tr("Save your recovery words")
        : tr("Your recovery words"));
    setMinimumWidth(520);
    // Never grow past the usable screen — the action buttons at the bottom
    // must stay reachable no matter how small the display is.
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size();
        setMaximumSize(available.width() * 4 / 5, available.height() * 9 / 10);
    }

    auto* layout = new QVBoxLayout{this};
    layout->setSpacing(12);

    layout->addWidget(boldLabel(
        mode == Mode::Create
            ? tr("Write down these 24 words in order")
            : tr("Your 24 recovery words in order"), this));
    layout->addWidget(warningLabel(
        mode == Mode::Create
            ? tr("These 24 words are the only way to restore your identity on a new device. "
                 "Anyone who sees them controls your identity. Write them down on paper and store them offline.")
            : tr("Anyone who sees these words can take over your identity. Keep them private."), this));

    // Selectable and copyable: a read-only plain text edit, never a message box.
    auto* words_view = new QTextEdit{this};
    m_words_view = words_view;
    words_view->setReadOnly(true);
    words_view->setMinimumHeight(250);
    words_view->setStyleSheet(QStringLiteral(
        "QTextEdit { font-family: 'Cascadia Mono', 'Consolas', monospace; font-size: 13px; }"));
    QStringList numbered;
    for (int i = 0; i < m_words.size(); ++i) {
        numbered << QStringLiteral("%1. %2").arg(i + 1, 2).arg(m_words.at(i));
    }
    words_view->setPlainText(numbered.join(QLatin1Char('\n')));
    layout->addWidget(words_view);

    auto* export_row = new QHBoxLayout;
    export_row->setSpacing(10);
    auto* copy_button = new QPushButton{tr("Copy words"), this};
    copy_button->setObjectName(QStringLiteral("secondaryButton"));
    connect(copy_button, &QPushButton::clicked, this, [this] { copyWordsToClipboard(); });
    auto* save_button = new QPushButton{tr("Advanced: save unencrypted file…"), this};
    save_button->setObjectName(QStringLiteral("secondaryButton"));
    save_button->setToolTip(tr("Opens a save dialog, then reports the exact file path here."));
    connect(save_button, &QPushButton::clicked, this, [this] { saveWordsToFile(); });
    // Revealed only after a successful save, so the result is one click away.
    auto* reveal_button = new QPushButton{tr("Show in folder"), this};
    reveal_button->setObjectName(QStringLiteral("secondaryButton"));
    reveal_button->setVisible(false);
    reveal_button->setToolTip(tr("Opens the folder that contains the saved file."));
    connect(reveal_button, &QPushButton::clicked, this, [this] {
        if (m_saved_path.isEmpty()) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo{m_saved_path}.absolutePath()));
    });
    m_reveal_button = reveal_button;
    export_row->addWidget(copy_button);
    export_row->addWidget(save_button);
    export_row->addWidget(reveal_button);
    export_row->addStretch();
    layout->addLayout(export_row);

    // Always-visible status line: the user must never have to guess whether a
    // file was written, where, and under which name.
    m_feedback = noteLabel(tr("Nothing has been saved to a file yet."), this);
    m_feedback->setVisible(true);
    layout->addWidget(m_feedback);

    if (mode == Mode::Create) {
        // Two distinct random words must be retyped before creation continues.
        m_first_index = QRandomGenerator::global()->bounded(m_words.size());
        do {
            m_second_index = QRandomGenerator::global()->bounded(m_words.size());
        } while (m_second_index == m_first_index);

        layout->addSpacing(6);
        layout->addWidget(boldLabel(tr("Confirm you wrote the words down"), this));

        auto* first_row = new QHBoxLayout;
        auto* first_label = new QLabel{tr("Word #%1:").arg(m_first_index + 1), this};
        first_row->addWidget(first_label);
        m_first_edit = new QLineEdit{this};
        m_first_edit->setPlaceholderText(tr("type the word"));
        first_row->addWidget(m_first_edit, 1);
        layout->addLayout(first_row);

        auto* second_row = new QHBoxLayout;
        auto* second_label = new QLabel{tr("Word #%1:").arg(m_second_index + 1), this};
        second_row->addWidget(second_label);
        m_second_edit = new QLineEdit{this};
        m_second_edit->setPlaceholderText(tr("type the word"));
        second_row->addWidget(m_second_edit, 1);
        layout->addLayout(second_row);

        connect(m_first_edit, &QLineEdit::textChanged, this, [this] { updateAcceptState(); });
        connect(m_second_edit, &QLineEdit::textChanged, this, [this] { updateAcceptState(); });

        auto* actions = new QHBoxLayout;
        m_accept_button = new QPushButton{tr("Create identity"), this};
        m_accept_button->setObjectName(QStringLiteral("primaryButton"));
        m_accept_button->setEnabled(false);
        connect(m_accept_button, &QPushButton::clicked, this, [this] { accept(); });
        auto* cancel_button = new QPushButton{tr("Cancel"), this};
        cancel_button->setObjectName(QStringLiteral("secondaryButton"));
        connect(cancel_button, &QPushButton::clicked, this, [this] { reject(); });
        actions->addWidget(m_accept_button);
        actions->addWidget(cancel_button);
        actions->addStretch();
        layout->addLayout(actions);

        layout->addWidget(noteLabel(
            tr("Canceling discards this identity — nothing is created and nothing is sent to the network."), this));
    } else {
        auto* actions = new QHBoxLayout;
        auto* close_button = new QPushButton{tr("Close"), this};
        close_button->setObjectName(QStringLiteral("primaryButton"));
        connect(close_button, &QPushButton::clicked, this, [this] { accept(); });
        actions->addWidget(close_button);
        actions->addStretch();
        layout->addLayout(actions);
    }
}

void RecoveryPhraseDialog::done(int result)
{
    // Best-effort scrub of text buffers owned by this dialog.
    for (auto* edit : {m_first_edit, m_second_edit}) {
        if (edit) edit->clear();
    }
    if (m_words_view) m_words_view->clear();
    for (auto& word : m_words) word.fill(QChar{0});
    m_words.clear();
    m_saved_path.clear();
    QDialog::done(result);
}

void RecoveryPhraseDialog::setFeedback(const QString& text, FeedbackTone tone)
{
    switch (tone) {
    case FeedbackTone::Success:
        m_feedback->setStyleSheet(QStringLiteral("color: #15803d;"));
        break;
    case FeedbackTone::Warning:
        m_feedback->setStyleSheet(QStringLiteral("color: #b45309;"));
        break;
    case FeedbackTone::Neutral:
        m_feedback->setStyleSheet(QString());
        break;
    }
    m_feedback->setText(text);
    m_feedback->setVisible(true);
}

void RecoveryPhraseDialog::copyWordsToClipboard()
{
    QString copied = m_words.join(QLatin1Char(' '));
    QGuiApplication::clipboard()->setText(copied);
    QTimer::singleShot(60000, qApp, [copied = std::move(copied)]() mutable {
        auto* clipboard = QGuiApplication::clipboard();
        if (clipboard->text() == copied) clipboard->clear();
        copied.fill(QChar{0});
    });
    setFeedback(tr("Words copied. The clipboard will be cleared after one minute if unchanged. Clipboard history may retain a copy."), FeedbackTone::Neutral);
}

bool RecoveryPhraseDialog::saveWordsToFile()
{
    // The default name is announced up front, so "under which name?" is
    // answered before the dialog even opens.
    const QString default_name = QStringLiteral("cybou-recovery-words.txt");
    const auto consent = QMessageBox::warning(this, tr("Save recovery words"),
        tr("The file will contain your 24 recovery words in plain text. "
           "Anyone who can read that file can take over your identity.\n\n"
           "A save dialog will open, proposing the file name \"%1\" in your home folder. "
           "After saving, the exact path will be shown in this window.\n\nSave anyway?")
            .arg(default_name),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
    if (consent != QMessageBox::Save) {
        setFeedback(tr("Saving canceled before the file dialog — nothing was written."), FeedbackTone::Warning);
        return false;
    }

    // The platform-native save dialog can open behind this application-modal
    // dialog (the window is not the foreground window), which looks exactly
    // like "the dialog never opened". The Qt-owned dialog always opens on top.
    QFileDialog dialog{this, tr("Save recovery words"), QDir::homePath(), tr("Text file (*.txt)")};
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    dialog.setDefaultSuffix(QStringLiteral("txt"));
    dialog.selectFile(default_name);
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) {
        setFeedback(tr("Saving canceled in the file dialog — no file was written."), FeedbackTone::Warning);
        return false;
    }
    const QString path = dialog.selectedFiles().first();

    // Recovery words must never overwrite an existing file.
    if (QFileInfo::exists(path)) {
        setFeedback(tr("This file already exists. Choose a new file name; nothing was overwritten."), FeedbackTone::Warning);
        return false;
    }

    QByteArray payload = (m_words.join(QLatin1Char(' ')) + QLatin1Char('\n')).toUtf8();
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::NewOnly)) {
        payload.fill(0);
        QMessageBox::warning(this, tr("Cannot save"),
            tr("The recovery words could not be written to:\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(path), file.errorString()));
        setFeedback(tr("Save failed — the file was not created."), FeedbackTone::Warning);
        return false;
    }
    // Reject an export if owner-only file permissions cannot be applied.
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
        file.close();
        file.remove();
        payload.fill(0);
        setFeedback(tr("Save failed — file access could not be restricted to the current user."), FeedbackTone::Warning);
        return false;
    }
    const qint64 written = file.write(payload);
    payload.fill(0);
    const bool write_failed = written != payload.size() || !file.flush();
    const QString error_string = file.errorString();
    file.close();
    if (write_failed) {
        file.remove();
        QMessageBox::warning(this, tr("Cannot save"),
            tr("Writing the recovery words failed and the incomplete file was removed:\n%1\n\n%2")
                .arg(QDir::toNativeSeparators(path), error_string));
        setFeedback(tr("Save failed — the incomplete file was removed."), FeedbackTone::Warning);
        return false;
    }

    m_saved_path = path;
    if (m_reveal_button) m_reveal_button->setVisible(true);
    setFeedback(tr("Recovery words saved to file:\n%1")
        .arg(QDir::toNativeSeparators(path)), FeedbackTone::Success);
    return true;
}

void RecoveryPhraseDialog::updateAcceptState()
{
    if (m_mode != Mode::Create) return;
    const auto matches = [this](QLineEdit* edit, int index) {
        return edit && edit->text().trimmed().compare(m_words.at(index), Qt::CaseInsensitive) == 0;
    };
    const bool first_ok = matches(m_first_edit, m_first_index);
    const bool second_ok = matches(m_second_edit, m_second_index);
    m_accept_button->setEnabled(first_ok && second_ok);
}
