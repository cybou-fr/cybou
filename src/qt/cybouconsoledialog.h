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
class QCompleter;
class QStandardItemModel;
class QLabel;
class QCloseEvent;

/**
 * Bounded read-only operator console.
 *
 * Implements a restricted command grammar for inspecting own unlocked
 * Identity metadata, storage counters, observed peers, jobs and local PoA status.
 *
 * Strict invariants (R8 / W7):
 * - Strictly read-only: no shell, arbitrary SQL/script, mutation or signing commands.
 * - Zero disclosure of private key material, mnemonics, or foreign ChunkStore objects.
 * - Memory-bounded: 500 output blocks, 100 rows/history entries, 1024-character input.
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
    void closeEvent(QCloseEvent* event) override;

private:
    CybouDesktopModel* const m_model;
    QPlainTextEdit* m_output{nullptr};
    QLineEdit* m_input{nullptr};
    QPushButton* m_run_btn{nullptr};
    QPushButton* m_clear_btn{nullptr};
    QLabel* m_scope{nullptr};
    QLineEdit* m_find{nullptr};
    QWidget* m_find_bar{nullptr};
    QLabel* m_matches{nullptr};
    QCompleter* m_completer{nullptr};
    QStandardItemModel* m_suggestions{nullptr};
    QStringList m_history;
    int m_history_index{-1};
    bool m_private_session{false};
    bool m_authority_session{false};
    QString m_account;
    static constexpr int kMaxLines = 500;

    void handleRun();
    void appendOutput(const QString& text);
    void checkVaultLock();
    void completeInput();
    void findOutput(bool backward = false);
};

#endif // CYBOU_QT_CYBOUCONSOLEDIALOG_H
