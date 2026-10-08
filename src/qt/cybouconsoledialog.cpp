#include <qt/networkobservationtext.h>
// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cybouconsoledialog.h>

#include <qt/cyboudesktopmodel.h>
#include <qt/cybouproduct.h>
#include <qt/cyboutheme.h>
#include <qt/cybouui.h>

#include <QHBoxLayout>
#include <QDateTime>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTextDocument>
#include <QTimeZone>
#include <QCompleter>
#include <QStandardItemModel>
#include <QAbstractItemView>
#include <QShortcut>
#include <QSettings>
#include <QCloseEvent>
#include <QToolButton>
#include <QTextCursor>

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
    {"health", "health", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Local runtime observation and candidate pool"), false, false},
    {"metrics", "metrics", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Local traffic counters and measured transfer rates"), false, false},
    {"capacity", "capacity", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Local storage limits, headroom and available disk"), false, false},
    {"network", "network", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Network binding, chain tip and state root"), false, false},
    {"storage", "storage", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Physical storage usage and capacity policy"), false, false},
    {"peers", "peers", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Observed sessions and unverified peer heights"), false, false},
    {"block", "block <height|hash>", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Inspect verified block header and operations"), false, false},
    {"op", "op <id>", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Inspect finalized or candidate operation"), false, false},
    {"history", "history [page]", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Paginated verified blockchain history"), false, false},
    {"operations", "operations", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Locally tracked operations"), false, false},
    {"identity", "identity", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own AccountID, name and key epoch"), true, false},
    {"wallet", "wallet", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own Balance and System Balance"), true, false},
    {"files", "files [filter]", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own files and replica status"), true, false},
    {"file", "file <id|name>", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own file metadata and retrieval state"), true, false},
    {"chunks", "chunks <id|name>", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Real chunk tree, integrity and retrieval diagnostics"), true, false},
    {"jobs", "jobs", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Own active application tasks"), true, false},
    {"authority", "authority [status|candidates|totals|settle]", QT_TRANSLATE_NOOP("CybouConsoleDialog", "Central Authority status, candidates, totals and settlement preview"), true, true},
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

    auto* header = new QHBoxLayout;
    header->addWidget(SectionTitle(tr("CYBOU Console"), this));
    header->addStretch();
    for (const auto& action : QList<QPair<Glyph, QString>>{{Glyph::Info, tr("Help")}, {Glyph::Search, tr("Search output (Ctrl+F)")}, {Glyph::Copy, tr("Copy selection")}, {Glyph::Trash, tr("Clear (Ctrl+L)")}}) {
        auto* button = IconButton(action.first, this, action.second, IconButtonSize::Toolbar);
        header->addWidget(button);
        connect(button, &QToolButton::clicked, this, [this, glyph = action.first] {
            if (glyph == Glyph::Info) executeCommand(QStringLiteral("help"));
            else if (glyph == Glyph::Search) { m_find_bar->show(); m_find->setFocus(); }
            else if (glyph == Glyph::Copy) m_output->copy();
            else clearOutput();
        });
    }
    layout->addLayout(header);
    m_scope = MutedText({}, this);
    m_scope->setObjectName(QStringLiteral("consoleScope"));
    layout->addWidget(m_scope);
    m_find_bar = new QWidget{this};
    auto* find_layout = new QHBoxLayout{m_find_bar};
    find_layout->setContentsMargins(0, 0, 0, 0);
    m_find = new QLineEdit{m_find_bar};
    m_find->setObjectName(QStringLiteral("consoleSearch"));
    m_find->setPlaceholderText(tr("Search output…"));
    m_find->setAccessibleName(tr("Search output…"));
    m_find->setMaxLength(256);
    m_find->installEventFilter(this);
    m_matches = MutedText({}, m_find_bar);
    find_layout->addWidget(m_find, 1);
    find_layout->addWidget(m_matches);
    for (bool backward : {true, false}) {
        auto* button = new QPushButton{backward ? tr("Previous") : tr("Next"), m_find_bar};
        find_layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, backward] { findOutput(backward); });
    }
    connect(m_find, &QLineEdit::textChanged, this, [this] { findOutput(); });
    layout->addWidget(m_find_bar);
    m_find_bar->hide();

    // Output area
    m_output = new QPlainTextEdit{this};
    m_output->setObjectName(QStringLiteral("consoleOutput"));
    m_output->setReadOnly(true);
    m_output->setAccessibleName(tr("CYBOU Console"));
    m_output->installEventFilter(this);
    m_output->setMaximumBlockCount(kMaxLines);
    m_output->setFont(QFont(QStringLiteral("Consolas"), 9));
    m_output->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: %1; color: %2; border: 2px solid %3; border-radius: 6px; padding: 8px; }"
        "QPlainTextEdit:focus { border-color: %2; }"
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
    m_input->setAccessibleName(m_input->placeholderText());
    m_input->setMaxLength(1024);
    m_input->installEventFilter(this);
    input_bar->addWidget(m_input, 1);

    auto* run = IconButton(Glyph::ArrowRight, this, tr("Run command (Enter)"), IconButtonSize::Toolbar);
    run->setObjectName(QStringLiteral("consoleRun"));
    connect(run, &QToolButton::clicked, this, [this] { handleRun(); });
    input_bar->addWidget(run);

    layout->addLayout(input_bar);
    m_suggestions = new QStandardItemModel{this};
    m_completer = new QCompleter{m_suggestions, this};
    m_completer->setWidget(m_input);
    m_completer->setCompletionRole(Qt::UserRole);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    connect(m_completer, qOverload<const QModelIndex&>(&QCompleter::activated), this, [this](const QModelIndex& index) {
        m_input->setText(index.data(Qt::UserRole).toString());
        m_input->setFocus();
    });
    connect(m_input, &QLineEdit::textEdited, this, [this] { completeInput(); });
    auto* search = new QShortcut{QKeySequence{Qt::CTRL | Qt::Key_F}, this};
    connect(search, &QShortcut::activated, this, [this] { m_find_bar->show(); m_find->setFocus(); m_find->selectAll(); });
    auto* clear = new QShortcut{QKeySequence{Qt::CTRL | Qt::Key_L}, this};
    connect(clear, &QShortcut::activated, this, [this] { clearOutput(); m_input->setFocus(); });
    if (qEnvironmentVariableIsEmpty("CYBOU_SCREENSHOT_DIR")) restoreGeometry(QSettings{}.value(QStringLiteral("console/geometry")).toByteArray());
    appendOutput(tr("CYBOU · %1\nLocal read-only diagnostics. Peer announcements are unverified.\nQuick commands: status · peers · storage · history\nType help for all commands. Tab or Ctrl+Space opens suggestions.").arg(m_model->status().network_name));

    m_private_session = m_model->status().identity_state == CybouIdentityState::Active;
    m_authority_session = m_model->isNetworkAuthority();
    m_account = m_model->status().account_id;
    connect(m_model, &CybouDesktopModel::statusChanged, this, [this] { checkVaultLock(); });
    connect(m_model, &CybouDesktopModel::networkAuthorityChanged, this, [this] { checkVaultLock(); });
    checkVaultLock();
}

void CybouConsoleDialog::closeEvent(QCloseEvent* event)
{
    if (qEnvironmentVariableIsEmpty("CYBOU_SCREENSHOT_DIR")) QSettings{}.setValue(QStringLiteral("console/geometry"), saveGeometry());
    QDialog::closeEvent(event);
}

void CybouConsoleDialog::completeInput()
{
    m_suggestions->clear();
    const auto text = m_input->text();
    auto add = [this](const QString& value, const QString& description = {}) {
        if (m_suggestions->rowCount() >= kMaxRows || value.size() > 1024) return;
        auto* item = new QStandardItem{description.isEmpty() ? value : value + QStringLiteral(" — ") + description};
        item->setToolTip(description);
        item->setData(value, Qt::UserRole);
        m_suggestions->appendRow(item);
    };
    const bool unlocked = m_model->status().identity_state == CybouIdentityState::Active;
    if (!text.contains(QLatin1Char{' '})) {
        for (const auto& command : commands) {
            if (command.private_data && !unlocked) continue;
            if (command.authority && !m_model->isNetworkAuthority()) continue;
            add(QLatin1String{command.name}, tr(command.description));
        }
    } else {
        const auto command = text.section(QLatin1Char{' '}, 0, 0).toLower();
        if ((command == QLatin1String{"file"} || command == QLatin1String{"chunks"}) && unlocked) {
            for (const auto& file : m_model->fileItems()) if (!file.folder) {
                if (m_suggestions->rowCount() >= kMaxRows) break;
                add(command + QLatin1Char{' '} + file.id, file.name);
                if (file.name != file.id) add(command + QLatin1Char{' '} + file.name);
            }
        } else if (command == QLatin1String{"op"}) {
            for (const auto& op : m_model->networkDiagnostics().operations) {
                if (m_suggestions->rowCount() >= kMaxRows) break;
                add(command + QLatin1Char{' '} + QString::fromStdString(op.operation_id));
            }
        } else if (command == QLatin1String{"block"}) {
            const auto& d = m_model->networkDiagnostics();
            if (d.initialized) add(command + QLatin1Char{' '} + QString::number(d.height), tr("Locally verified tip"));
        } else if (command == QLatin1String{"authority"} && unlocked && m_model->isNetworkAuthority()) {
            for (const auto& argument : {"status", "candidates", "totals", "settle"}) add(command + QLatin1Char{' '} + QLatin1String{argument});
        }
    }
    m_completer->setCompletionPrefix(text);
    if (m_completer->completionCount()) m_completer->complete();
    else m_completer->popup()->hide();
}

void CybouConsoleDialog::findOutput(bool backward)
{
    const auto query = m_find->text();
    if (query.isEmpty()) { m_matches->clear(); return; }
    const auto flags = backward ? QTextDocument::FindBackward : QTextDocument::FindFlags{};
    if (!m_output->find(query, flags)) {
        auto cursor = m_output->textCursor();
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_output->setTextCursor(cursor);
        m_output->find(query, flags);
    }
    const auto text = m_output->toPlainText();
    int count = 0;
    int position = 0;
    int offset = 0;
    while ((offset = text.indexOf(query, offset, Qt::CaseInsensitive)) >= 0) {
        ++count;
        offset += query.size();
        if (offset <= m_output->textCursor().selectionEnd()) position = count;
    }
    m_matches->setText(tr("%1 / %2").arg(count ? position : 0).arg(count));
}

bool CybouConsoleDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() & Qt::ControlModifier) {
            if (key->key() == Qt::Key_F) { m_find_bar->show(); m_find->setFocus(); m_find->selectAll(); return true; }
            if (key->key() == Qt::Key_L) { clearOutput(); m_input->setFocus(); return true; }
        }
        if (key->key() == Qt::Key_Escape && (m_find_bar->isVisible() || m_completer->popup()->isVisible())) {
            m_completer->popup()->hide(); m_find_bar->hide(); m_input->setFocus(); return true;
        }
        if (watched == m_find && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) { findOutput(key->modifiers() & Qt::ShiftModifier); return true; }
        if (watched == m_input && key->key() == Qt::Key_Tab && m_completer->popup()->isVisible()) {
            const auto index = m_completer->popup()->currentIndex().isValid() ? m_completer->popup()->currentIndex() : m_completer->completionModel()->index(0,0);
            m_input->setText(index.data(Qt::UserRole).toString()); m_completer->popup()->hide(); return true;
        }
        if (watched == m_input && ((key->key() == Qt::Key_Tab && !m_input->text().trimmed().isEmpty()) || (key->key() == Qt::Key_Space && key->modifiers() & Qt::ControlModifier))) { completeInput(); return true; }
        if (watched == m_input && m_completer->popup()->isVisible()) return false;
    }
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
    m_suggestions->clear();
    m_completer->popup()->hide();
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
    m_find->clear();
    m_suggestions->clear();
    m_completer->popup()->hide();
}

QString CybouConsoleDialog::outputText() const
{
    return m_output->toPlainText();
}

void CybouConsoleDialog::appendOutput(const QString& text)
{
    auto cursor = m_output->textCursor();
    cursor.movePosition(QTextCursor::End);
    if (!m_output->document()->isEmpty()) cursor.insertBlock();
    QTextCharFormat format;
    if (text.startsWith(QStringLiteral("> "))) format.setForeground(CybouTheme::color(CybouTheme::BRAND_TEAL));
    if (text == tr("NODE") || text == tr("CHAIN") || text == tr("YOUR DATA") || text == tr("AUTHORITY") || text == tr("UTILITY")) format.setFontWeight(QFont::Bold);
    cursor.insertText(text.left(8192), format);
    m_output->setTextCursor(cursor);
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
    const auto& status = m_model->status();
    m_scope->setText(tr("%1 · %2 · Height %3 · %4 peers · %5 · Read only")
        .arg(status.network_name, status.online ? tr("Online") : tr("Offline"))
        .arg(status.finality_known ? QString::number(status.finalized_height) : tr("Unknown"))
        .arg(status.peer_count).arg(active && authority ? tr("Authority diagnostics") : active ? tr("Own Identity") : tr("Public")));
    if (m_completer->popup()->isVisible()) completeInput();
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
    for (const auto& candidate : commands) {
        if (cmd == QLatin1String{candidate.name} ||
            (cmd == QLatin1String{"?"} && QLatin1String{candidate.name} == QLatin1String{"help"}) ||
            (cmd == QLatin1String{"operation"} && QLatin1String{candidate.name} == QLatin1String{"op"})) {
            spec = &candidate;
            break;
        }
    }
    if (!spec || (spec->authority && (!unlocked || !m_model->isNetworkAuthority()))) {
        appendOutput(tr("Error: Command '%1' is not recognized or not permitted. Type help for available commands.").arg(cmd));
        return;
    }
    if (spec->private_data && !unlocked) { appendOutput(tr("Unlock your Identity to use this command.")); return; }
    const bool accepts_arg = (cmd == QLatin1String{"files"} || cmd == QLatin1String{"file"} ||
        cmd == QLatin1String{"chunks"} || cmd == QLatin1String{"authority"} ||
        cmd == QLatin1String{"block"} || cmd == QLatin1String{"op"} ||
        cmd == QLatin1String{"operation"} || cmd == QLatin1String{"history"} ||
        cmd == QLatin1String{"operations"});
    if (!arg.isEmpty() && !accepts_arg) {
        appendOutput(tr("Usage: %1").arg(QLatin1String{spec->syntax})); return;
    }
    if (cmd == QLatin1String{"help"} || cmd == QLatin1String{"?"}) {
        appendOutput(tr("Available read-only commands:"));
        const QStringList headings{tr("NODE"), tr("CHAIN"), tr("YOUR DATA"), tr("AUTHORITY"), tr("UTILITY")};
        for (int group = 0; group < headings.size(); ++group) {
            bool heading = false;
            for (const auto& entry : commands) {
                if (entry.private_data && !unlocked) continue;
                if (entry.authority && !m_model->isNetworkAuthority()) continue;
                const QString name = QLatin1String{entry.name};
                const int category = entry.authority ? 3 : entry.private_data ? 2 :
                    (name == QLatin1String{"help"} || name == QLatin1String{"clear"}) ? 4 :
                    (name == QLatin1String{"block"} || name == QLatin1String{"op"} || name == QLatin1String{"history"} || name == QLatin1String{"operations"}) ? 1 : 0;
                if (category != group) continue;
                if (!heading) { appendOutput(headings[group]); heading = true; }
                appendOutput(QStringLiteral("  %1 — %2").arg(QLatin1String{entry.syntax}, tr(entry.description)));
            }
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
    } else if (cmd == QLatin1String{"capacity"}) {
        const auto& d = m_model->networkDiagnostics();
        const bool measured = d.observed_unix_ms != 0;
        const bool policy = measured && d.local_storage_capacity;
        appendOutput(tr("Source: Local storage snapshot\nObserved: %1\nStored encrypted bytes: %2 / %3 bytes (%4 % of V)\nPolicy headroom: %5 bytes\nAdmitted provider bytes: %6 / %7 bytes; budget headroom: %8 bytes\nOS available disk: %9 bytes\nStored byte lengths exclude filesystem overhead. Disk space is shared and precedes admission reserve. Headroom is not promised admission or network capacity.")
            .arg(measured ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(d.observed_unix_ms), QTimeZone::UTC).toString(Qt::ISODateWithMs) : tr("Unknown"))
            .arg(policy ? QString::number(d.local_storage_used) : tr("Unknown"))
            .arg(policy ? QString::number(d.local_storage_capacity) : tr("Unknown"))
            .arg(policy ? QLocale{}.toString(d.local_storage_used * 100.0 / d.local_storage_capacity, 'f', 1) : tr("Unknown"))
            .arg(policy ? QString::number(d.local_storage_capacity - std::min(d.local_storage_used, d.local_storage_capacity)) : tr("Unknown"))
            .arg(measured && d.storage_capacity ? QString::number(d.storage_used) : tr("Unknown"))
            .arg(measured && d.storage_capacity ? QString::number(d.storage_capacity) : tr("Unknown"))
            .arg(measured && d.storage_capacity ? QString::number(d.storage_capacity - std::min(d.storage_used, d.storage_capacity)) : tr("Unknown"))
            .arg(measured && d.storage_disk_available ? QString::number(*d.storage_disk_available) : tr("Unknown")));
        const auto observed = cybouNetworkObservationText(d);
        appendOutput(observed.capacity_title + QStringLiteral("\n") + observed.capacity + QStringLiteral("\n") + observed.storage_detail + QStringLiteral("\n") + observed.coverage);
    } else if (cmd == QLatin1String{"metrics"}) {
        const auto& d = m_model->networkDiagnostics();
        const auto observed = cybouNetworkObservationText(d);
        appendOutput(observed.traffic + QStringLiteral("\n") + observed.traffic_detail + QStringLiteral("\n") + observed.cpu + QStringLiteral("\n") + observed.cpu_detail + QStringLiteral("\n") + observed.coverage);
        const auto& t = d.traffic;
        const bool measured = d.observed_unix_ms != 0;
        appendOutput(tr("Source: Local CYBOU frames, excluding TLS/TCP overhead\nReceived since runtime start: %1 bytes\nSent since runtime start: %2 bytes\nReceived rate: %3 B/s\nSent rate: %4 B/s\nWindow: 60 complete seconds; retries and service frames included.\nObserved: %5\nThis is local traffic, not unique delivery or network transaction throughput.")
            .arg(measured ? QString::number(t.received_bytes) : tr("Unknown"))
            .arg(measured ? QString::number(t.sent_bytes) : tr("Unknown"))
            .arg(measured && t.window_ms ? QLocale{}.toString(t.window_received_bytes * 1000.0 / t.window_ms, 'f', 1) : tr("Unknown"))
            .arg(measured && t.window_ms ? QLocale{}.toString(t.window_sent_bytes * 1000.0 / t.window_ms, 'f', 1) : tr("Unknown"))
            .arg(measured ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(d.observed_unix_ms), QTimeZone::UTC).toString(Qt::ISODateWithMs) : tr("Unknown")));
        const auto& f = d.finalization;
        const auto& cpu = d.process_cpu;
        appendOutput(tr("Local process CPU: %1 % over %2 ms\nOS online logical processors: %3\nLast completed CPU mean: %4 % over %5 ms / %6 intervals; age %7 ms\nNormalized process time, including GUI; not host load, quota utilization or network capacity.")
            .arg(measured && cpu.interval_percent ? QLocale{}.toString(*cpu.interval_percent, 'f', 1) : tr("Unknown"))
            .arg(measured && cpu.interval_percent ? QString::number(cpu.interval_ms) : tr("Unknown"))
            .arg(measured && cpu.processors ? QString::number(cpu.processors) : tr("Unknown"))
            .arg(measured && cpu.mean_percent ? QLocale{}.toString(*cpu.mean_percent, 'f', 1) : tr("Unknown"))
            .arg(measured && cpu.mean_percent ? QString::number(cpu.mean_window_ms) : tr("Unknown"))
            .arg(measured && cpu.mean_percent ? QString::number(cpu.mean_intervals) : tr("Unknown"))
            .arg(measured && cpu.mean_percent ? QString::number(cpu.mean_age_ms) : tr("Unknown")));
        appendOutput(tr("Local CYBOU process resident memory: %1 bytes\nSource: OS working set / RSS, instantaneous; includes GUI and shared pages. Not host or network memory.")
            .arg(measured && d.process_resident_bytes ? QString::number(*d.process_resident_bytes) : tr("Unknown")));
        appendOutput(tr("Verified local observations: %1 operations (%2 locally produced); history imports: %3. Totals since observation reset.")
            .arg(measured && d.initialized ? QString::number(f.observed_total) : tr("Unknown"))
            .arg(measured && d.initialized ? QString::number(f.local_produced_total) : tr("Unknown"))
            .arg(measured && d.initialized ? QString::number(f.history_total) : tr("Unknown")));
        for (const auto& window : f.windows) {
            const bool ready = measured && d.initialized && window.complete && window.window_ms;
            appendOutput(tr("%1 min: observed %2 op/min · locally produced %3 op/min · history %4 operations")
                .arg(window.window_ms / 60000)
                .arg(ready ? QLocale{}.toString(window.observed_operations * 60000.0 / window.window_ms, 'f', 1) : tr("Unknown"))
                .arg(ready ? QLocale{}.toString(window.local_produced_operations * 60000.0 / window.window_ms, 'f', 1) : tr("Unknown"))
                .arg(ready ? QString::number(window.history_operations) : tr("Unknown")));
        }
        appendOutput(tr("Arrival-time windows, excluding partial seconds. Announced blocks have no production timestamp; these observations do not prove global freshness or a network capacity limit."));
    } else if (cmd == QLatin1String{"health"}) {
        const auto& d = m_model->networkDiagnostics();
        const bool measured = d.observed_unix_ms != 0;
        appendOutput(tr("Source: Local node snapshot\nObserved: %1\nNode uptime: %2\nInitialized: %3\nSafety halt: %4\nLocal candidate pool: %5 operations / %6 bytes\nThis is local load, not network throughput or global health.")
            .arg(measured ? QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(d.observed_unix_ms), QTimeZone::UTC).toString(Qt::ISODateWithMs) : tr("Unknown"))
            .arg(measured ? tr("%1 s").arg(d.uptime_ms / 1000) : tr("Unknown"))
            .arg(measured ? (d.initialized ? tr("Yes") : tr("No")) : tr("Unknown"))
            .arg(measured ? (d.safety_halted ? tr("Yes") : tr("No")) : tr("Unknown"))
            .arg(measured && d.initialized ? QString::number(d.pending_operations) : tr("Unknown"))
            .arg(measured && d.initialized ? QString::number(d.pending_operation_bytes) : tr("Unknown")));
    } else if (cmd == QLatin1String{"network"}) {
        const auto& d = m_model->networkDiagnostics();
        appendOutput(tr("Network: %1\nNetwork binding: %2\nLocally verified height: %3\nTip: %4\nState root: %5\nSafety halt: %6")
            .arg(m_model->status().network_name, QString::fromStdString(d.network_binding))
            .arg(d.initialized ? QString::number(d.height) : tr("Unknown"))
            .arg(QString::fromStdString(d.tip), QString::fromStdString(d.state_root), d.safety_halted ? tr("Yes") : tr("No")));
    } else if (cmd == QLatin1String{"storage"}) {
        const auto& d = m_model->networkDiagnostics();
        appendOutput(tr("Physical storage (encrypted bytes): %1 / %2\nAdmitted provider bytes: %3 / %4 bytes\nCapacity limits are local policy; admitted provider bytes are accounted storage, not lease obligations or consensus rights.")
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
    } else if (cmd == QLatin1String{"block"}) {
        if (arg.isEmpty()) {
            appendOutput(tr("Usage: block <height|hash>"));
            return;
        }
        const auto info = m_model->inspectBlock(arg);
        if (!info.found) {
            appendOutput(tr("Block not found: %1").arg(arg));
            return;
        }
        appendOutput(tr(
            "Block Height %1:\n"
            "  Block ID: %2\n"
            "  Parent Block ID: %3\n"
            "  State Root: %4\n"
            "  Operations Root: %5\n"
            "  PoA Certificate: %6\n"
            "  Operations (%7):")
            .arg(info.height)
            .arg(info.block_id.isEmpty() ? QStringLiteral("—") : info.block_id)
            .arg(info.parent_block_id.isEmpty() ? QStringLiteral("—") : info.parent_block_id)
            .arg(info.state_root.isEmpty() ? QStringLiteral("—") : info.state_root)
            .arg(info.operations_root.isEmpty() ? QStringLiteral("—") : info.operations_root)
            .arg(info.has_poa_certificate ? tr("Verified PoA signature") : tr("None / Unverified"))
            .arg(info.operation_count));
        int op_idx = 0;
        for (const auto& op_id : info.operation_ids) {
            if (op_idx == kMaxRows) {
                appendOutput(tr("Output limited to 100 rows."));
                break;
            }
            ++op_idx;
            appendOutput(QStringLiteral("    [%1] %2").arg(op_idx).arg(op_id));
        }
        if (info.operation_count == 0) {
            appendOutput(tr("    (No operations in this block)"));
        }
    } else if (cmd == QLatin1String{"op"} || cmd == QLatin1String{"operation"}) {
        if (arg.isEmpty()) {
            appendOutput(tr("Usage: op <id>"));
            return;
        }
        const auto info = m_model->inspectOperation(arg);
        if (!info.found) {
            appendOutput(tr("Operation not found: %1").arg(arg));
            return;
        }
        appendOutput(tr(
            "Operation %1:\n"
            "  Status: %2\n"
            "  Height: %3\n"
            "  Block ID: %4\n"
            "  Index in Block: %5\n"
            "  Kind: %6\n"
            "  Author: %7")
            .arg(info.operation_id)
            .arg(info.state)
            .arg(info.height > 0 || info.state == tr("Finalized") ? QString::number(info.height) : QStringLiteral("—"))
            .arg(info.block_id.isEmpty() ? QStringLiteral("—") : info.block_id)
            .arg(info.state == tr("Finalized") ? QString::number(info.index) : QStringLiteral("—"))
            .arg(info.kind.isEmpty() ? QStringLiteral("—") : info.kind)
            .arg(info.author.isEmpty() ? QStringLiteral("—") : info.author));
    } else if (cmd == QLatin1String{"history"}) {
        int page = 1;
        if (!arg.isEmpty()) {
            bool ok = false;
            page = arg.toInt(&ok);
            if (!ok || page < 1) {
                appendOutput(tr("Usage: history [page] (page must be a positive integer)"));
                return;
            }
        }
        const auto items = m_model->inspectHistory(page, 10);
        if (items.isEmpty()) {
            appendOutput(tr("No history entries found for page %1.").arg(page));
            return;
        }
        appendOutput(tr("Blockchain History (Page %1):").arg(page));
        for (const auto& item : items) {
            if (!item.summary.isEmpty()) {
                appendOutput(QStringLiteral("  Height %1 | %2").arg(item.height).arg(item.summary));
            } else {
                appendOutput(QStringLiteral("  Height %1 | Block: %2 | State Root: %3 | Operations: %4")
                    .arg(item.height)
                    .arg(ShortHex(item.block_id))
                    .arg(ShortHex(item.state_root))
                    .arg(item.operation_count));
            }
        }
        appendOutput(tr("Type 'history %1' for next page, or 'block <height>' to inspect a specific block.").arg(page + 1));
    } else if (cmd == QLatin1String{"authority"}) {
        const auto& a = m_model->networkAuthority();
        const QString sub = tokens.size() > 1 ? tokens[1].toLower() : QStringLiteral("status");
        if (sub == QLatin1String{"status"}) {
            const QStringList states{tr("Signer unavailable"), tr("Finalizing"), tr("Paused"), tr("Safety halt")};
            QString settlement_text;
            if (a.settlement_due) {
                settlement_text = tr("Due now (period %1, %2 payouts, %3)")
                    .arg(a.next_settlement_period).arg(a.preview_payouts_count).arg(cybouAmountText(a.preview_payouts_amount));
            } else if (a.next_settlement_due_utc > 0) {
                settlement_text = tr("Due %1 UTC (period %2)")
                    .arg(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(a.next_settlement_due_utc), QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm")))
                    .arg(a.next_settlement_period);
            } else {
                settlement_text = tr("Unknown");
            }
            appendOutput(tr(
                "Local finalizer: %1\n"
                "Signer enabled: %2\n"
                "Candidates: %3 (queue age: %4 s)\n"
                "Locally verified height: %5\n"
                "Signing safety: %6\n"
                "Storage settlement: %7")
                .arg(states[int(a.finalizer)], a.signer_enabled ? tr("Yes") : tr("No"))
                .arg(a.candidates).arg(a.oldest_candidate_age_seconds)
                .arg(a.finalized_height)
                .arg(a.safety_journal_status.isEmpty() ? tr("Fail-closed durable append-only journal active.") : a.safety_journal_status)
                .arg(settlement_text));
        } else if (sub == QLatin1String{"candidates"}) {
            appendOutput(tr("Locally executed candidates: %1 (volatile pool, queue age: %2 s)")
                .arg(a.candidates).arg(a.oldest_candidate_age_seconds));
            for (int i = 0; i < qMin(kMaxRows, int(a.candidate_ids.size())); ++i) {
                const quint64 wait = i < a.candidate_wait_seconds.size() ? a.candidate_wait_seconds[i] : 0;
                appendOutput(QStringLiteral("  [%1] %2 (waiting %3 s)").arg(i + 1).arg(a.candidate_ids[i]).arg(wait));
            }
            if (a.candidate_ids.size() > kMaxRows) appendOutput(tr("Output limited to 100 rows."));
        } else if (sub == QLatin1String{"totals"}) {
            appendOutput(tr("Finalized state at height %1\nIdentities: %2\nNames: %3\nPending name commits: %4\nBalance total: %5 CYBOU\nSystem Balance total: %6 CYBOU\nStorage escrow: %7 CYBOU")
                .arg(a.finalized_height).arg(a.identities).arg(a.names).arg(a.pending_name_commits)
                .arg(a.total_balance).arg(a.total_system_balance).arg(a.storage_escrow));
        } else if (sub == QLatin1String{"settle"}) {
            // Preview only: the console never signs. Settlement is submitted from the Central Authority page.
            if (!a.settlement_due && a.next_settlement_due_utc > 0) {
                appendOutput(tr("The next storage settlement is due %1 UTC (period %2). Settlement cannot be submitted before the period ends.")
                    .arg(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(a.next_settlement_due_utc), QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm")))
                    .arg(a.next_settlement_period));
            } else {
                appendOutput(tr(
                    "Storage settlement preview for period %1:\n"
                    "  Status: Due now\n"
                    "  Eligible payouts: %2\n"
                    "  Total amount: %3\n"
                    "Submit it from the Central Authority page.")
                    .arg(a.next_settlement_period).arg(a.preview_payouts_count).arg(cybouAmountText(a.preview_payouts_amount)));
            }
        } else if (sub == QLatin1String{"pause"} || sub == QLatin1String{"resume"} || sub == QLatin1String{"finalize"}) {
            appendOutput(tr("This console is read-only. Pause, resume and finalize from the Central Authority page."));
        } else {
            appendOutput(tr("Usage: authority [status|candidates|totals|settle]"));
        }
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
        const auto diag = m_model->inspectFileChunks(target->id);
        if (!diag.available) {
            appendOutput(tr("Chunk evidence: %1\nContent root: %2\nActual chunk list and verification results are not exposed by the application model. No integrity check was performed.")
                .arg(target->name, target->content_root_id.isEmpty() ? tr("Not reported") : target->content_root_id));
            return;
        }
        appendOutput(tr(
            "Chunk Tree & Integrity Diagnostics:\n"
            "  File: %1\n"
            "  Root Chunk ID: %2\n"
            "  Total Chunks: %3\n"
            "  Local Chunks: %4\n"
            "  Verified (BLAKE3-256): %5\n"
            "  Missing Locally: %6\n"
            "  Corrupt: %7")
            .arg(target->name,
                 diag.root_chunk_id.isEmpty() ? (target->content_root_id.isEmpty() ? tr("Not reported") : target->content_root_id) : diag.root_chunk_id)
            .arg(diag.chunk_count)
            .arg(diag.local_count)
            .arg(diag.verified_count)
            .arg(diag.missing_count)
            .arg(diag.corrupt_count));
        if (!diag.retrieval_diagnosis.isEmpty()) {
            appendOutput(tr("  Retrieval diagnosis: %1").arg(diag.retrieval_diagnosis));
        }
        if (!diag.chunks.isEmpty()) {
            appendOutput(tr("Chunks:"));
            int count = 0;
            for (const auto& entry : diag.chunks) {
                if (count == kMaxRows) {
                    appendOutput(tr("Output limited to 100 rows."));
                    break;
                }
                ++count;
                QString status_str;
                if (!entry.present_locally) {
                    status_str = tr("Missing locally");
                } else if (entry.integrity_verified) {
                    status_str = tr("Verified (BLAKE3-256)");
                } else {
                    status_str = tr("Corrupt (hash mismatch)");
                }
                appendOutput(QStringLiteral("  [%1] %2 — %3").arg(count).arg(entry.chunk_id, status_str));
            }
        }
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
        const auto tasks = m_model->applicationTasks();
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
            appendOutput(tr("  [%1 local task] %2").arg(t.scope == CybouTaskScope::Mail ? tr("Mail") : tr("Files"), t.title));
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
