// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_RECOVERYPHRASEDIALOG_H
#define BITCOIN_QT_RECOVERYPHRASEDIALOG_H

#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

/**
 * Dialog that presents the 24 identity recovery words.
 *
 * Create mode: the words are shown in a selectable, copyable form, can be
 * saved to a file, and the user must confirm two random words before the
 * dialog accepts. Canceling means the identity is NOT created — the caller
 * discards the prepared identity material.
 *
 * View mode: read-only display of the words of an already unlocked identity
 * (recovery card on the identity page). No confirmation step.
 *
 * File export is deliberately explicit about its outcome:
 *  - the consent warning names the proposed default file up front;
 *  - the save dialog always opens on top of this modal (the non-native Qt
 *    dialog is used — the platform-native one can open behind an
 *    application-modal parent and look like "nothing happened");
 *  - the always-visible status line afterwards states exactly what happened:
 *    the saved path, a cancel that wrote nothing, or the concrete write error.
 */
class RecoveryPhraseDialog : public QDialog
{
public:
    enum class Mode {
        Create,
        View,
    };

    RecoveryPhraseDialog(Mode mode, const QStringList& words, QWidget* parent = nullptr);

protected:
    void done(int result) override;

private:
    enum class FeedbackTone {
        Neutral,
        Success,
        Warning,
    };

    Mode m_mode;
    QStringList m_words;
    QString m_saved_path;
    int m_first_index{-1};
    int m_second_index{-1};
    QLineEdit* m_first_edit{nullptr};
    QLineEdit* m_second_edit{nullptr};
    QPushButton* m_accept_button{nullptr};
    QPushButton* m_reveal_button{nullptr};
    QLabel* m_feedback{nullptr};
    QTextEdit* m_words_view{nullptr};

    void copyWordsToClipboard();
    bool saveWordsToFile();
    void updateAcceptState();
    void setFeedback(const QString& text, FeedbackTone tone);
};

#endif // BITCOIN_QT_RECOVERYPHRASEDIALOG_H
