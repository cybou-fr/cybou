// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#ifndef CYBOU_QT_CYBOUCONSOLEDIALOG_H
#define CYBOU_QT_CYBOUCONSOLEDIALOG_H

#include <QCoreApplication>
#include <QDialog>
#include <QStringList>

class CybouDesktopModel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/**
 * Bounded read-only operator console.
 *
 * Implements a restricted command grammar for inspecting own unlocked
 * Identity files, storage capacity, chunk trees, observed peers, and jobs.
 *
 * Strict invariants (R8 / W7):
 * - Strictly read-only: no shell, arbitrary SQL/script, mutation or signing commands.
 * - Zero disclosure of private key material, mnemonics, or foreign ChunkStore objects.
 * - Memory-bounded: capped output (500 lines).
 * - Safe cancellation: lock cleanup immediately flushes output and history.
 */
class CybouConsoleDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(CybouConsoleDialog)

public:
    explicit CybouConsoleDialog(CybouDesktopModel* model, QWidget* parent = nullptr);
    ~CybouConsoleDialog() override = default;

    void executeCommand(const QString& command_line);
    void clearOutput();
    QString outputText() const;
    int historyCount() const { return m_history.size(); }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    CybouDesktopModel* const m_model;
    QPlainTextEdit* m_output{nullptr};
    QLineEdit* m_input{nullptr};
    QPushButton* m_run_btn{nullptr};
    QPushButton* m_clear_btn{nullptr};
    QStringList m_history;
    int m_history_index{-1};
    static constexpr int kMaxLines = 500;

    void handleRun();
    void appendOutput(const QString& text);
    void checkVaultLock();
};

#endif // CYBOU_QT_CYBOUCONSOLEDIALOG_H
