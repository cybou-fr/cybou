// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/recoveryphrasedialog.h>

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
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
    auto* save_button = new QPushButton{tr("Save to file…"), this};
    save_button->setObjectName(QStringLiteral("secondaryButton"));
    connect(save_button, &QPushButton::clicked, this, [this] { saveWordsToFile(); });
    export_row->addWidget(copy_button);
    export_row->addWidget(save_button);
    export_row->addStretch();
    layout->addLayout(export_row);

    m_feedback = noteLabel({}, this);
    m_feedback->setVisible(false);
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
    // Best-effort scrub of the confirmation fields; the caller owns the word list.
    for (auto* edit : {m_first_edit, m_second_edit}) {
        if (edit) edit->clear();
    }
    QDialog::done(result);
}

void RecoveryPhraseDialog::copyWordsToClipboard()
{
    QGuiApplication::clipboard()->setText(m_words.join(QLatin1Char(' ')));
    m_feedback->setText(tr("Words copied to the clipboard. Clear the clipboard when you are done."));
    m_feedback->setVisible(true);
}

bool RecoveryPhraseDialog::saveWordsToFile()
{
    const auto consent = QMessageBox::warning(this, tr("Save recovery words"),
        tr("The file will contain your 24 recovery words in plain text. "
           "Anyone who can read that file can take over your identity.\n\nSave anyway?"),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
    if (consent != QMessageBox::Save) return false;

    const QString path = QFileDialog::getSaveFileName(this, tr("Save recovery words"),
        QDir::home().filePath(QStringLiteral("cybou-recovery-words.txt")),
        tr("Text file (*.txt)"));
    if (path.isEmpty()) return false;

    QFile file{path};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Cannot save"),
            tr("The recovery words could not be written to:\n%1").arg(path));
        return false;
    }
    file.write(m_words.join(QLatin1Char(' ')).toUtf8());
    file.write("\n");
    file.close();
    m_feedback->setText(tr("Recovery words saved to:\n%1").arg(path));
    m_feedback->setVisible(true);
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
