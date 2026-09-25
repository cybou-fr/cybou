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
    Mode m_mode;
    QStringList m_words;
    int m_first_index{-1};
    int m_second_index{-1};
    QLineEdit* m_first_edit{nullptr};
    QLineEdit* m_second_edit{nullptr};
    QPushButton* m_accept_button{nullptr};
    QLabel* m_feedback{nullptr};

    void copyWordsToClipboard();
    bool saveWordsToFile();
    void updateAcceptState();
};

#endif // BITCOIN_QT_RECOVERYPHRASEDIALOG_H
