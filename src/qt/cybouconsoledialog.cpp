// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cybouconsoledialog.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouproduct.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTextDocument>

using namespace CybouUi;

namespace {

QString ShortHex(const QString& text)
{
    return text.size() > 20 ? text.left(10) + QStringLiteral("…") + text.right(8) : text;
}

struct Command {
    const char* name;
    const char* syntax;
    const char* description;
    bool private_data;
    bool authority;
};
const Command commands[] = {
    {"help", "help", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Show available commands"), false, false},
    {"status", "status", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Local node and Identity status"), false, false},
    {"network", "network", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Network binding, chain tip and state root"), false, false},
    {"storage", "storage", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Physical storage usage and capacity policy"), false, false},
    {"peers", "peers", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Observed sessions and unverified peer heights"), false, false},
    {"operations", "operations", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Locally tracked operations"), false, false},
    {"identity", "identity", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own AccountID, name and key epoch"), true, false},
    {"wallet", "wallet", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own Balance and System Balance"), true, false},
    {"files", "files [filter]", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own files and replica status"), true, false},
    {"file", "file <id|name>", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own file metadata and retrieval state"), true, false},
    {"chunks", "chunks <id|name>", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Chunk evidence availability for an own file"), true, false},
    {"jobs", "jobs", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own active application tasks"), true, false},
    {"authority", "authority [status|candidates|totals]", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Local finalizer and finalized state totals"), true, true},
    {"clear", "clear", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Clear output and command history"), false, false},
};
constexpr int kMaxRows = 100;
QString Count(int count) { return count < 0 ? QStringLiteral("—") : QString::number(count); }

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
        tr("Inspect local network state, own file metadata and available operator diagnostics. "
           "Shell, mutation, and scripting commands are disabled."), hero));
    layout->addWidget(hero);

    // Output area
    m_output = new QPlainTextEdit{this};
    m_output->setObjectName(QStringLiteral("consoleOutput"));
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(kMaxLines);
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
    m_input->setMaxLength(1024);
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

    m_private_session = m_model->status().identity_state == CybouIdentityState::Active;
    m_authority_session = m_model->isNetworkAuthority();
    m_account = m_model->status().account_id;
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { checkVaultLock(); });
    connect(m_model, &CybouDesktopModel::networkAuthorityChanged, this, [this] { checkVaultLock(); });
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
    m_history.clear();
    m_history_index = -1;
    m_input->clear();
}

QString CybouConsoleDialog::outputText() const
{
    return m_output->toPlainText();
}

void CybouConsoleDialog::appendOutput(const QString& text)
{
    m_output->appendPlainText(text.left(8192));
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void CybouConsoleDialog::checkVaultLock()
{
    const bool active = m_model->status().identity_state == CybouIdentityState::Active;
    const bool authority = m_model->isNetworkAuthority();
    const auto account = m_model->status().account_id;
    if ((m_private_session && !active) || (m_authority_session && !authority) || account != m_account) {
        clearOutput();
        appendOutput(tr("Vault locked. Private session history and output cleared."));
    }
    m_private_session = active;
    m_authority_session = authority;
    m_account = account;
}

void CybouConsoleDialog::executeCommand(const QString& command_line)
{
    checkVaultLock();
    const QString line = command_line.trimmed();
    if (line.isEmpty()) return;
    if (line.size() > 1024) { appendOutput(tr("Command too long (maximum 1024 characters).")); return; }

    if (m_history.isEmpty() || m_history.last() != line) {
        m_history.append(line);
        if (m_history.size() > 100) m_history.removeFirst();
    }

    appendOutput(QStringLiteral("> ") + line);

    const QStringList tokens = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (tokens.isEmpty()) return;

    const QString cmd = tokens.first().toLower();
    const QString arg = tokens.size() > 1 ? tokens.mid(1).join(QLatin1Char{' '}) : QString{};

    const bool unlocked = m_model->status().identity_state == CybouIdentityState::Active;
    const Command* spec = nullptr;
    for (const auto& candidate : commands) if (cmd == QLatin1String{candidate.name} ||
        (cmd == QLatin1String{"?"} && QLatin1String{candidate.name} == QLatin1String{"help"})) spec = &candidate;
    if (!spec || (spec->authority && (!unlocked || !m_model->isNetworkAuthority()))) {
        appendOutput(tr("Error: Command '%1' is not recognized or not permitted. Type help for available commands.").arg(cmd));
        return;
    }
    if (spec->private_data && !unlocked) { appendOutput(tr("Unlock your Identity to use this command.")); return; }
    if (!arg.isEmpty() && cmd != QLatin1String{"files"} && cmd != QLatin1String{"file"} &&
        cmd != QLatin1String{"chunks"} && cmd != QLatin1String{"authority"}) {
        appendOutput(tr("Usage: %1").arg(QLatin1String{spec->syntax})); return;
    }
    if (cmd == QLatin1String{"help"} || cmd == QLatin1String{"?"}) {
        appendOutput(tr("Available read-only commands:"));
        for (const auto& entry : commands) {
            if (entry.private_data && !unlocked) continue;
            if (entry.authority && !m_model->isNetworkAuthority()) continue;
            appendOutput(QStringLiteral("  %1 — %2").arg(QLatin1String{entry.syntax}, tr(entry.description)));
        }
        if (!unlocked) appendOutput(tr("Unlock your Identity for private catalog commands."));
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
            .arg(s.network_name, ShortHex(s.network_binding))
            .arg(s.node_running ? tr("Running") : tr("Stopped"))
            .arg(s.online ? tr("Yes") : tr("No"))
            .arg(cybouConnectionText(s))
            .arg(s.finality_known ? QString::number(s.finalized_height) : tr("Unknown"))
            .arg(id_state)
            .arg(s.primary_name.isEmpty() ? tr("None") : s.primary_name));
    } else if (cmd == QLatin1String{"network"}) {
        const auto& d = m_model->networkDiagnostics();
        appendOutput(tr("Network: %1\nNetwork binding: %2\nLocally verified height: %3\nTip: %4\nState root: %5\nSafety halt: %6")
            .arg(m_model->status().network_name, QString::fromStdString(d.network_binding))
            .arg(d.initialized ? QString::number(d.height) : tr("Unknown"))
            .arg(QString::fromStdString(d.tip), QString::fromStdString(d.state_root), d.safety_halted ? tr("Yes") : tr("No")));
    } else if (cmd == QLatin1String{"storage"}) {
        const auto& d = m_model->networkDiagnostics();
        appendOutput(tr("Physical storage (encrypted bytes): %1 / %2\nProvider obligations: %3 / %4 bytes\nCapacity and obligations are local policy, not consensus rights.")
            .arg(d.initialized ? QString::number(d.local_storage_used) : tr("Unknown"))
            .arg(d.initialized ? QString::number(d.local_storage_capacity) : tr("Unknown"))
            .arg(d.initialized ? QString::number(d.storage_used) : tr("Unknown"))
            .arg(d.initialized ? QString::number(d.storage_capacity) : tr("Unknown")));
        if (unlocked) {
            quint64 logical = 0; int count = 0;
            for (const auto& file : m_model->fileItems()) if (!file.folder && !file.trashed) { logical += file.logical_size; ++count; }
            appendOutput(tr("Own Storage Summary: %1 items, %2 logical bytes (not physical storage usage).")
                .arg(count).arg(logical));
        }
    } else if (cmd == QLatin1String{"identity"}) {
        const auto& s = m_model->status();
        appendOutput(tr("AccountID: %1\nName: %2\nKey epoch: %3\nCreation height: %4")
            .arg(s.account_id, s.primary_name).arg(s.key_epoch).arg(s.creation_height));
    } else if (cmd == QLatin1String{"wallet"}) {
        const auto& s = m_model->status();
        appendOutput(tr("Balance: %1 CYBOU\nSystem Balance: %2 CYBOU (non-transferable service budget)")
            .arg(s.balance).arg(s.system_balance));
    } else if (cmd == QLatin1String{"operations"}) {
        const auto& rows = m_model->networkDiagnostics().operations;
        appendOutput(tr("Locally tracked operations: %1 (maximum 100 rows)").arg(rows.size()));
        const QStringList states{tr("Unknown"), tr("Local pending"), tr("Accepted remotely"), tr("Finalized"), tr("Rejected"), tr("History unavailable")};
        int count = 0;
        for (const auto& op : rows) {
            if (count++ == kMaxRows) { appendOutput(tr("Output limited to 100 rows.")); break; }
            appendOutput(QStringLiteral("  %1 | %2 | %3").arg(QString::fromStdString(op.operation_id),
                op.state < unsigned(states.size()) ? states[op.state] : tr("Unknown"), QString::number(op.finalized_height)));
        }
    } else if (cmd == QLatin1String{"authority"}) {
        const auto& a = m_model->networkAuthority();
        if (arg.isEmpty() || arg == QLatin1String{"status"}) {
            const QStringList states{tr("Signer unavailable"), tr("Finalizing"), tr("Paused"), tr("Safety halt")};
            appendOutput(tr("Local finalizer: %1\nSigner enabled: %2\nCandidates: %3\nLocally verified height: %4")
                .arg(states[int(a.finalizer)], a.signer_enabled ? tr("Yes") : tr("No")).arg(a.candidates).arg(a.finalized_height));
        } else if (arg == QLatin1String{"candidates"}) {
            appendOutput(tr("Locally executed candidates: %1 (volatile pool)").arg(a.candidates));
            for (int i = 0; i < qMin(kMaxRows, int(a.candidate_ids.size())); ++i) appendOutput(a.candidate_ids[i]);
            if (a.candidate_ids.size() > kMaxRows) appendOutput(tr("Output limited to 100 rows."));
        } else if (arg == QLatin1String{"totals"}) {
            appendOutput(tr("Finalized state at height %1\nIdentities: %2\nNames: %3\nPending name commits: %4\nBalance total: %5 CYBOU\nSystem Balance total: %6 CYBOU\nStorage escrow: %7 CYBOU")
                .arg(a.finalized_height).arg(a.identities).arg(a.names).arg(a.pending_name_commits)
                .arg(a.total_balance).arg(a.total_system_balance).arg(a.storage_escrow));
        } else appendOutput(tr("Usage: authority [status|candidates|totals]"));
    } else if (cmd == QLatin1String{"files"}) {
        const auto files = m_model->fileItems();
        int matched = 0;
        appendOutput(tr("Listing own unlocked files:"));
        for (const auto& f : files) {
            if (f.trashed) continue;
            if (!arg.isEmpty() && !f.name.contains(arg, Qt::CaseInsensitive) && !f.id.contains(arg, Qt::CaseInsensitive)) {
                continue;
            }
            if (matched == kMaxRows) { appendOutput(tr("Output limited to 100 rows.")); break; }
            const QString desc = f.folder ? tr("[Folder]") : CybouProduct::sizeText(f.logical_size);
            const QString state_str = CybouProduct::contentStateText(f.state);
            const QString replicas = f.folder ? QString{} : tr("(replicas: %1/%2)").arg(Count(f.min_remote_replicas), Count(f.remote_replica_target));
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
            if (!f.trashed && (f.id == arg || f.name.compare(arg, Qt::CaseInsensitive) == 0)) {
                target = &f;
                break;
            }
        }
        if (!target) {
            appendOutput(tr("File not found in own catalog: %1").arg(arg));
            return;
        }
        const quint64 chunk_count = target->logical_size / 524288 + (target->logical_size % 524288 != 0);
        appendOutput(tr(
            "File Details:\n"
            "  ID: %1\n"
            "  Name: %2\n"
            "  Root Content ID: %3\n"
            "  Logical Size: %4 (%5 bytes)\n"
            "  Billing units: %6 (512 KiB, not a measured chunk count)\n"
            "  State: %7\n"
            "  Remote Replicas: %8 of %9\n"
            "  Local Availability: %10\n"
            "  Retrieval: %11")
            .arg(target->id, target->name)
            .arg(target->content_root_id.isEmpty() ? tr("Not reported") : target->content_root_id)
            .arg(CybouProduct::sizeText(target->logical_size)).arg(target->logical_size)
            .arg(chunk_count)
            .arg(CybouProduct::contentStateText(target->state))
            .arg(Count(target->min_remote_replicas), Count(target->remote_replica_target))
            .arg(CybouProduct::localAvailabilityText(*target))
            .arg(CybouProduct::retrievalText(target->retrieval)));
    } else if (cmd == QLatin1String{"chunks"}) {
        if (arg.isEmpty()) {
            appendOutput(tr("Usage: chunks <id|name>"));
            return;
        }
        const CybouFileItem* target{nullptr};
        for (const auto& f : m_model->fileItems()) {
            if (!f.trashed && (f.id == arg || f.name.compare(arg, Qt::CaseInsensitive) == 0)) {
                target = &f;
                break;
            }
        }
        if (!target) {
            appendOutput(tr("File not found in own catalog: %1").arg(arg));
            return;
        }
        appendOutput(tr("Chunk evidence: %1\nContent root: %2\nActual chunk list and verification results are not exposed by the application model. No integrity check was performed.")
            .arg(target->name, target->content_root_id.isEmpty() ? tr("Not reported") : target->content_root_id));
    } else if (cmd == QLatin1String{"peers"}) {
        const auto& d = m_model->networkDiagnostics();
        appendOutput(tr("Observed Peer Connections (%1 peers):").arg(d.peers.size()));
        int displayed = 0;
        for (const auto& p : d.peers) {
            if (displayed++ == kMaxRows) { appendOutput(tr("Output limited to 100 rows.")); break; }
            const quint64 lag = d.height > p.advertised_height ? d.height - p.advertised_height : 0;
            appendOutput(QStringLiteral("  %1 | Advertised height: %2 (unverified, lag %3) | StorageId: %4")
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
        const auto loading = m_model->applicationLoadState();
        if (loading == CybouApplicationLoadState::Opening || loading == CybouApplicationLoadState::Loading) {
            appendOutput(tr("Application preparation: %1 / %2 publications scanned")
                .arg(m_model->applicationLoadScanned()).arg(m_model->applicationLoadTotal()));
            ++job_count;
        }
        for (const auto& t : tasks) {
            if (t.state != CybouCommandState::Queued && t.state != CybouCommandState::Running) continue;
            if (job_count == kMaxRows) break;
            appendOutput(QStringLiteral("  [Mail Task] %1").arg(t.title));
            ++job_count;
        }
        for (const auto& f : files) {
            if (f.trashed || job_count == kMaxRows) continue;
            if (f.state == CybouContentState::Securing || f.operation_state == CybouOperationState::Submitted ||
                (f.retrieval != CybouRetrievalState::Idle && f.retrieval != CybouRetrievalState::Ready)) {
                appendOutput(tr("  [File task] %1: %2 | Retrieval: %3").arg(f.name,
                    CybouProduct::contentStateText(f.state), CybouProduct::retrievalText(f.retrieval)));
                ++job_count;
            }
        }
        if (job_count == kMaxRows) appendOutput(tr("Output limited to 100 rows."));
        if (job_count == 0) appendOutput(tr("  No background jobs currently in progress."));
    } else if (cmd == QLatin1String{"clear"}) {
        clearOutput();
    }
}
