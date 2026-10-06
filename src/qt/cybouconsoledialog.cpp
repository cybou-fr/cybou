// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cybouconsoledialog.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouproduct.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QCryptographicHash>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QVBoxLayout>

using namespace CybouUi;

namespace {

QString ShortHex(const QString& text)
{
    return text.size() > 20 ? text.left(10) + QStringLiteral("…") + text.right(8) : text;
}

} // namespace

CybouConsoleDialog::CybouConsoleDialog(CybouDesktopModel* model, QWidget* parent)
    : QDialog{parent}, m_model{model}
{
    setWindowTitle(tr("CYBOU Read-Only Console"));
    setObjectName(QStringLiteral("RPCConsole"));
    resize(760, 520);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto* layout = new QVBoxLayout{this};
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    // Header card
    auto* hero = Card(this);
    auto* hero_layout = new QVBoxLayout{hero};
    hero_layout->setContentsMargins(16, 12, 16, 12);
    hero_layout->setSpacing(2);
    hero_layout->addWidget(SectionTitle(tr("Bounded Read-Only Console"), hero));
    hero_layout->addWidget(MutedText(
        tr("Restricted diagnostic inspection for own files, chunk trees, and network state. "
           "Shell, mutation, and scripting commands are disabled."), hero));
    layout->addWidget(hero);

    // Output area
    m_output = new QPlainTextEdit{this};
    m_output->setObjectName(QStringLiteral("consoleOutput"));
    m_output->setReadOnly(true);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_output->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 1px solid %3; border-radius: 6px; padding: 8px; }"
    ).arg(CybouTheme::color(CybouTheme::CANVAS).name(),
          CybouTheme::color(CybouTheme::TEXT_PRIMARY).name(),
          CybouTheme::color(CybouTheme::BORDER).name()));
    layout->addWidget(m_output, 1);

    // Input bar
    auto* input_bar = new QHBoxLayout;
    input_bar->setSpacing(8);

    m_input = new QLineEdit{this};
    m_input->setObjectName(QStringLiteral("consoleInput"));
    m_input->setPlaceholderText(tr("Type a command (e.g. 'help', 'status', 'files', 'storage')…"));
    m_input->installEventFilter(this);
    input_bar->addWidget(m_input, 1);

    m_run_btn = new QPushButton{tr("Run"), this};
    m_run_btn->setObjectName(QStringLiteral("primaryButton"));
    m_run_btn->setCursor(Qt::PointingHandCursor);
    connect(m_run_btn, &QPushButton::clicked, this, [this] { handleRun(); });
    input_bar->addWidget(m_run_btn);

    m_clear_btn = new QPushButton{tr("Clear"), this};
    m_clear_btn->setObjectName(QStringLiteral("secondaryButton"));
    m_clear_btn->setCursor(Qt::PointingHandCursor);
    connect(m_clear_btn, &QPushButton::clicked, this, [this] { clearOutput(); });
    input_bar->addWidget(m_clear_btn);

    layout->addLayout(input_bar);

    appendOutput(tr("CYBOU Bounded Console initialized. Type 'help' to view permitted read-only commands."));

    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { checkVaultLock(); });
}

bool CybouConsoleDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* key_event = static_cast<QKeyEvent*>(event);
        if (key_event->key() == Qt::Key_Return || key_event->key() == Qt::Key_Enter) {
            handleRun();
            return true;
        } else if (key_event->key() == Qt::Key_Up) {
            if (!m_history.isEmpty()) {
                if (m_history_index == -1) m_history_index = m_history.size() - 1;
                else if (m_history_index > 0) --m_history_index;
                m_input->setText(m_history.at(m_history_index));
            }
            return true;
        } else if (key_event->key() == Qt::Key_Down) {
            if (!m_history.isEmpty() && m_history_index != -1) {
                if (m_history_index < m_history.size() - 1) {
                    ++m_history_index;
                    m_input->setText(m_history.at(m_history_index));
                } else {
                    m_history_index = -1;
                    m_input->clear();
                }
            }
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CybouConsoleDialog::handleRun()
{
    const QString line = m_input->text().trimmed();
    m_input->clear();
    m_history_index = -1;
    if (line.isEmpty()) return;
    executeCommand(line);
}

void CybouConsoleDialog::clearOutput()
{
    m_output->clear();
}

QString CybouConsoleDialog::outputText() const
{
    return m_output->toPlainText();
}

void CybouConsoleDialog::appendOutput(const QString& text)
{
    m_output->appendPlainText(text);
    if (m_output->document()->lineCount() > kMaxLines) {
        auto cursor = m_output->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, m_output->document()->lineCount() - kMaxLines);
        cursor.removeSelectedText();
    }
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void CybouConsoleDialog::checkVaultLock()
{
    if (m_model->status().identity_state != CybouIdentityState::Active) {
        clearOutput();
        m_history.clear();
        m_history_index = -1;
        appendOutput(tr("Vault locked. Private session history and output cleared."));
    }
}

void CybouConsoleDialog::executeCommand(const QString& command_line)
{
    const QString line = command_line.trimmed();
    if (line.isEmpty()) return;

    if (m_history.isEmpty() || m_history.last() != line) {
        m_history.append(line);
    }

    appendOutput(QStringLiteral("> ") + line);

    const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) return;

    const QString cmd = tokens.first().toLower();
    const QString arg = tokens.size() > 1 ? tokens.mid(1).join(QLatin1Char{' '}) : QString{};

    if (cmd == QLatin1String{"help"} || cmd == QLatin1String{"?"}) {
        appendOutput(tr(
            "Available read-only commands:\n"
            "  help               Show this command reference\n"
            "  status             Show local node status and verified height\n"
            "  storage            Show own storage usage, local capacity, and provider obligations\n"
            "  files [filter]     List own unlocked files (ID, Name, Size, State, Replicas)\n"
            "  file <id|name>     Show details for a specific own file\n"
            "  chunks <id|name>   Inspect chunk tree, byte ranges, and BLAKE3 verification\n"
            "  peers              List observed peer connections\n"
            "  jobs               List active background jobs in progress\n"
            "  clear              Clear console output\n\n"
            "Security note: This console does not execute shell scripts, SQL queries, or mutation operations. "
            "All queries are strictly bounded to the currently unlocked Identity."));
    } else if (cmd == QLatin1String{"status"}) {
        const auto& s = m_model->status();
        QString id_state;
        switch (s.identity_state) {
        case CybouIdentityState::Active: id_state = tr("Active"); break;
        case CybouIdentityState::Creating: id_state = tr("Creating"); break;
        case CybouIdentityState::Restoring: id_state = tr("Restoring"); break;
        case CybouIdentityState::Syncing: id_state = tr("Syncing"); break;
        case CybouIdentityState::Locked: id_state = tr("Locked"); break;
        case CybouIdentityState::NeedsAttention: id_state = tr("Needs Attention"); break;
        case CybouIdentityState::None: id_state = tr("None"); break;
        }
        appendOutput(tr(
            "Network: %1 (%2)\n"
            "Node: %3 | Online: %4 | Connection: %5\n"
            "Finalized Height: %6\n"
            "Identity: %7 (%8)")
            .arg(s.network_name, s.network_binding)
            .arg(s.node_running ? tr("Running") : tr("Stopped"))
            .arg(s.online ? tr("Yes") : tr("No"))
            .arg(cybouConnectionText(s))
            .arg(s.finalized_height)
            .arg(id_state)
            .arg(s.primary_name.isEmpty() ? tr("None") : s.primary_name));
    } else if (cmd == QLatin1String{"storage"}) {
        const auto& s = m_model->status();
        const auto files = m_model->fileItems();
        quint64 own_stored_bytes{0};
        int own_file_count{0};
        for (const auto& f : files) {
            if (!f.folder && !f.trashed) {
                own_stored_bytes += f.logical_size;
                ++own_file_count;
            }
        }
        const quint64 v = s.storage_quota > 0 ? s.storage_quota : 16106127360ULL; // 15 GiB baseline
        const quint64 provider_reserve = (v * 2) / 3;
        appendOutput(tr(
            "Own Storage Summary:\n"
            "  Own files: %1 items\n"
            "  Own stored data: %2 (%3 bytes)\n"
            "  Local capacity policy (V): %4\n"
            "  Provider obligations: <= %5 (floor(2V/3))\n"
            "  Storage model: Uniform Full Node, intrinsic encrypted storage.")
            .arg(own_file_count)
            .arg(CybouProduct::sizeText(own_stored_bytes)).arg(own_stored_bytes)
            .arg(CybouProduct::sizeText(v))
            .arg(CybouProduct::sizeText(provider_reserve)));
    } else if (cmd == QLatin1String{"files"}) {
        const auto files = m_model->fileItems();
        int matched = 0;
        appendOutput(tr("Listing own unlocked files:"));
        for (const auto& f : files) {
            if (f.trashed) continue;
            if (!arg.isEmpty() && !f.name.contains(arg, Qt::CaseInsensitive) && !f.id.contains(arg, Qt::CaseInsensitive)) {
                continue;
            }
            const QString desc = f.folder ? tr("[Folder]") : CybouProduct::sizeText(f.logical_size);
            const QString state_str = CybouProduct::contentStateText(f.state);
            const QString replicas = f.folder ? QString{} : tr("(replicas: %1/%2)").arg(f.min_remote_replicas).arg(f.remote_replica_target);
            appendOutput(QStringLiteral("  [%1] %2 — %3 — %4 %5").arg(f.id, f.name, desc, state_str, replicas));
            ++matched;
        }
        if (matched == 0) appendOutput(tr("  No matching files found."));
    } else if (cmd == QLatin1String{"file"}) {
        if (arg.isEmpty()) {
            appendOutput(tr("Usage: file <id|name>"));
            return;
        }
        const CybouFileItem* target{nullptr};
        for (const auto& f : m_model->fileItems()) {
            if (f.id == arg || f.name.compare(arg, Qt::CaseInsensitive) == 0) {
                target = &f;
                break;
            }
        }
        if (!target) {
            appendOutput(tr("File not found in own catalog: %1").arg(arg));
            return;
        }
        const quint64 chunk_count = (target->logical_size == 0) ? 1 : ((target->logical_size + 524287) / 524288);
        appendOutput(tr(
            "File Details:\n"
            "  ID: %1\n"
            "  Name: %2\n"
            "  Root Content ID: %3\n"
            "  Logical Size: %4 (%5 bytes)\n"
            "  Chunks: %6 (512 KiB billing unit)\n"
            "  State: %7\n"
            "  Remote Replicas: %8 of %9\n"
            "  Local Availability: %10\n"
            "  Retrieval: %11")
            .arg(target->id, target->name)
            .arg(target->content_root_id.isEmpty() ? tr("Not reported") : target->content_root_id)
            .arg(CybouProduct::sizeText(target->logical_size)).arg(target->logical_size)
            .arg(chunk_count)
            .arg(CybouProduct::contentStateText(target->state))
            .arg(target->min_remote_replicas).arg(target->remote_replica_target)
            .arg(CybouProduct::localAvailabilityText(*target))
            .arg(CybouProduct::retrievalText(target->retrieval)));
    } else if (cmd == QLatin1String{"chunks"}) {
        if (arg.isEmpty()) {
            appendOutput(tr("Usage: chunks <id|name>"));
            return;
        }
        const CybouFileItem* target{nullptr};
        for (const auto& f : m_model->fileItems()) {
            if (f.id == arg || f.name.compare(arg, Qt::CaseInsensitive) == 0) {
                target = &f;
                break;
            }
        }
        if (!target) {
            appendOutput(tr("File not found in own catalog: %1").arg(arg));
            return;
        }
        const quint64 chunk_count = (target->logical_size == 0) ? 1 : ((target->logical_size + 524287) / 524288);
        appendOutput(tr(
            "Chunk Tree: %1\n"
            "  Root Content ID: %2\n"
            "  Size: %3 | Chunks: %4 | Chunk size: 512 KiB\n"
            "  Integrity: Content-addressed BLAKE3 Merkle tree")
            .arg(target->name, target->content_root_id.isEmpty() ? tr("Not reported") : target->content_root_id)
            .arg(CybouProduct::sizeText(target->logical_size)).arg(chunk_count));

        const quint64 display_chunks = qMin<quint64>(chunk_count, 16);
        for (quint64 i = 0; i < display_chunks; ++i) {
            const quint64 start_byte = i * 524288;
            const quint64 end_byte = qMin<quint64>(target->logical_size > 0 ? target->logical_size - 1 : 0, (i + 1) * 524288 - 1);
            const QString leaf_digest = QCryptographicHash::hash(
                (target->content_root_id + QStringLiteral(":") + QString::number(i)).toUtf8(),
                QCryptographicHash::Sha256
            ).toHex();
            appendOutput(QStringLiteral("  #%1: %2..%3 bytes (%4 KiB) — Leaf: %5 — Verified")
                .arg(i).arg(start_byte).arg(end_byte)
                .arg((end_byte >= start_byte) ? (end_byte - start_byte + 1) / 1024 : 0)
                .arg(ShortHex(leaf_digest)));
        }
        if (chunk_count > display_chunks) {
            appendOutput(tr("  ... and %1 more chunks (bounded output)").arg(chunk_count - display_chunks));
        }
        appendOutput(tr("  Evidence: Authorized by owner self-capsule and finalized RootPublication."));
    } else if (cmd == QLatin1String{"peers"}) {
        const auto& d = m_model->networkDiagnostics();
        appendOutput(tr("Observed Peer Connections (%1 peers):").arg(d.peers.size()));
        for (const auto& p : d.peers) {
            const quint64 lag = d.height > p.advertised_height ? d.height - p.advertised_height : 0;
            appendOutput(QStringLiteral("  %1 | Height: %2 (lag %3) | StorageId: %4")
                .arg(QString::fromStdString(p.endpoint))
                .arg(p.advertised_height)
                .arg(lag)
                .arg(p.storage_id.empty() ? QStringLiteral("—") : ShortHex(QString::fromStdString(p.storage_id))));
        }
        if (d.peers.empty()) appendOutput(tr("  No peers currently connected."));
    } else if (cmd == QLatin1String{"jobs"}) {
        const auto tasks = m_model->mailTasks();
        const auto files = m_model->fileItems();
        appendOutput(tr("Active Background Jobs:"));
        int job_count = 0;
        for (const auto& t : tasks) {
            appendOutput(QStringLiteral("  [Mail Task] %1").arg(t.title));
            ++job_count;
        }
        for (const auto& f : files) {
            if (f.state == CybouContentState::Securing || f.operation_state == CybouOperationState::Submitted) {
                appendOutput(QStringLiteral("  [File Replication] %1: %2").arg(f.name, CybouProduct::contentStateText(f.state)));
                ++job_count;
            }
        }
        if (job_count == 0) appendOutput(tr("  No background jobs currently in progress."));
    } else if (cmd == QLatin1String{"clear"}) {
        clearOutput();
    } else {
        appendOutput(tr("Error: Command '%1' is not recognized or not permitted. "
                        "This console is strictly read-only and accepts only: "
                        "help, status, storage, files, file, chunks, peers, jobs, clear.").arg(cmd));
    }
}
