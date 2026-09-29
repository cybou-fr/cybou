// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopmodel.h>

#include <cybou/identity_operation_coordinator.h>
#include <cybou/identity_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/name_registry.h>
#include <cybou/name_service.h>
#include <cybou/wallet_service.h>

#include <support/cleanse.h>

#include <QRegularExpression>

#include <algorithm>
#include <filesystem>
#include <utility>

QString cybouConnectionText(const CybouDesktopStatus& status)
{
    if (!status.sync_error.isEmpty()) return CybouDesktopModel::tr("Needs attention");
    if (!status.node_running) return CybouDesktopModel::tr("Offline");
    if (!status.online) return CybouDesktopModel::tr("Connecting");
    if (status.syncing) return CybouDesktopModel::tr("Syncing");
    return CybouDesktopModel::tr("Synced");
}

CybouDesktopModel::CybouDesktopModel(QString network_name, QObject* parent)
    : QObject{parent}
{
    m_status.network_name = std::move(network_name);
}

CybouDesktopModel::~CybouDesktopModel()
{
    if (m_name_service) m_name_service->Cancel();
    if (m_name_worker.joinable()) m_name_worker.join();
    if (m_recovery_rotation_worker.joinable()) m_recovery_rotation_worker.join();
}

void CybouDesktopModel::setNodeStatus(bool running, int peer_count, bool online,
    const QString& data_directory)
{
    if (m_status.node_running == running && m_status.peer_count == peer_count &&
        m_status.online == online &&
        (data_directory.isEmpty() || m_status.data_directory == data_directory)) return;
    m_status.node_running = running;
    m_status.peer_count = peer_count;
    m_status.online = online;
    if (!data_directory.isEmpty()) m_status.data_directory = data_directory;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setCapabilities(const CybouCapabilities& capabilities)
{
    if (m_capabilities.account_creation == capabilities.account_creation &&
        m_capabilities.payments == capabilities.payments &&
        m_capabilities.mail == capabilities.mail &&
        m_capabilities.files == capabilities.files &&
        m_capabilities.sharing == capabilities.sharing &&
        m_capabilities.version_history == capabilities.version_history) {
        return;
    }
    m_capabilities = capabilities;
    Q_EMIT capabilitiesChanged();
}

void CybouDesktopModel::setNetworkInfo(const QString& network_name, const QString& network_id)
{
    if (m_status.network_name == network_name && m_status.network_id == network_id) return;
    m_status.network_name = network_name;
    m_status.network_id = network_id;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setFinalizedHeight(quint64 finalized_height)
{
    refreshFinalizedName();
    if (m_status.finality_known && m_status.finalized_height == finalized_height) return;
    m_status.finalized_height = finalized_height;
    m_status.finality_known = true;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setPeerCount(int peer_count)
{
    if (m_status.peer_count == peer_count) return;
    m_status.peer_count = peer_count;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setSyncing(bool syncing)
{
    if (m_status.syncing == syncing) return;
    m_status.syncing = syncing;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setSyncError(const QString& error)
{
    if (m_status.sync_error == error) return;
    m_status.sync_error = error;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setLastSync(const QDateTime& when)
{
    m_last_sync = when;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setIdentityState(CybouIdentityState state, const QString& account_id,
    quint64 creation_height)
{
    if (m_status.identity_state == state && m_status.account_id == account_id &&
        m_status.creation_height == creation_height) {
        return;
    }
    m_status.identity_state = state;
    m_status.account_id = account_id;
    if (state == CybouIdentityState::None) m_status.primary_name.clear();
    m_status.creation_height = creation_height;
    if (state != CybouIdentityState::Creating && state != CybouIdentityState::Restoring) {
        m_identity_request_pending = false;
    }
    if (state == CybouIdentityState::Active && m_wallet_service && !m_capabilities.payments) {
        m_capabilities.payments = true;
        Q_EMIT capabilitiesChanged();
    }
    Q_EMIT statusChanged();
    if (state == CybouIdentityState::Active) refreshFinalizedName();
}

void CybouDesktopModel::setIdentityStep(CybouIdentityStep step)
{
    if (m_status.identity_step == step) return;
    m_status.identity_step = step;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setPrimaryName(const QString& name)
{
    if (m_status.primary_name == name) return;
    m_status.primary_name = name;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setBalances(quint64 balance, quint64 system_balance)
{
    if (m_status.balance == balance && m_status.system_balance == system_balance) return;
    m_status.balance = balance;
    m_status.system_balance = system_balance;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setStorageUsage(quint64 used, quint64 quota)
{
    if (m_status.storage_used == used && m_status.storage_quota == quota) return;
    m_status.storage_used = used;
    m_status.storage_quota = quota;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setNames(QVector<CybouNameItem> names)
{
    m_names = std::move(names);
    Q_EMIT namesChanged();
}

void CybouDesktopModel::setMailItems(QVector<CybouMailItem> items)
{
    m_mail = std::move(items);
    Q_EMIT mailChanged();
}

void CybouDesktopModel::upsertMailItem(const CybouMailItem& item)
{
    const auto it = std::find_if(m_mail.begin(), m_mail.end(),
        [&](const CybouMailItem& existing) { return existing.id == item.id; });
    if (it != m_mail.end()) *it = item;
    else m_mail.prepend(item);
    Q_EMIT mailChanged();
}

int CybouDesktopModel::unreadMailCount() const
{
    return static_cast<int>(std::count_if(m_mail.begin(), m_mail.end(), [](const CybouMailItem& item) {
        return item.folder == CybouMailFolder::Inbox && item.unread;
    }));
}

const CybouMailItem* CybouDesktopModel::mailItem(const QString& id) const
{
    for (const auto& item : m_mail) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

void CybouDesktopModel::setMailRead(const QString& id, bool read)
{
    for (auto& item : m_mail) {
        if (item.id == id && item.unread == read) {
            item.unread = !read;
            Q_EMIT mailChanged();
            return;
        }
    }
}

void CybouDesktopModel::setMailStarred(const QString& id, bool starred)
{
    for (auto& item : m_mail) {
        if (item.id == id && item.starred != starred) {
            item.starred = starred;
            Q_EMIT mailChanged();
            return;
        }
    }
}

void CybouDesktopModel::moveMail(const QString& id, CybouMailFolder folder)
{
    for (auto& item : m_mail) {
        if (item.id == id && item.folder != folder) {
            item.folder = folder;
            Q_EMIT mailChanged();
            return;
        }
    }
}

namespace {
QString NewLocalId(const char* prefix)
{
    static quint64 counter = 0;
    return QStringLiteral("%1-%2-%3").arg(QLatin1String{prefix})
        .arg(QDateTime::currentMSecsSinceEpoch()).arg(++counter);
}

QString PreviewOf(const QString& body)
{
    return body.simplified().left(90);
}
} // namespace

QString CybouDesktopModel::saveMailDraft(CybouMailItem draft)
{
    if (draft.id.isEmpty()) draft.id = NewLocalId("draft");
    draft.folder = CybouMailFolder::Drafts;
    draft.draft = true;
    draft.unread = false;
    draft.state = CybouContentState::Local;
    draft.from_name = m_status.primary_name;
    draft.time = QDateTime::currentDateTime();
    draft.preview = PreviewOf(draft.body);
    upsertMailItem(draft);
    return draft.id;
}

void CybouDesktopModel::deleteMail(const QString& id)
{
    const auto removed = m_mail.removeIf([&id](const CybouMailItem& item) { return item.id == id; });
    if (removed > 0) Q_EMIT mailChanged();
}

QString CybouDesktopModel::requestSendMail(CybouMailItem message)
{
    if (!m_capabilities.mail || m_status.identity_state != CybouIdentityState::Active) return {};
    // A sent draft becomes the outgoing message.
    if (!message.id.isEmpty()) deleteMail(message.id);
    message.id = NewLocalId("out");
    message.folder = CybouMailFolder::Sent;
    message.draft = false;
    message.unread = false;
    message.from_name = m_status.primary_name;
    message.time = QDateTime::currentDateTime();
    message.preview = PreviewOf(message.body);
    message.state = CybouContentState::Preparing;
    for (auto& attachment : message.attachments) {
        if (attachment.state != CybouContentState::Protected) attachment.state = CybouContentState::Preparing;
    }
    upsertMailItem(message);
    Q_EMIT mailSendRequested(message.id);
    return message.id;
}

void CybouDesktopModel::setMailState(const QString& id, CybouContentState state)
{
    for (auto& item : m_mail) {
        if (item.id != id) continue;
        item.state = state;
        // Reused, already protected content keeps its state.
        for (auto& attachment : item.attachments) {
            if (attachment.state != CybouContentState::Protected) attachment.state = state;
        }
        Q_EMIT mailChanged();
        return;
    }
}

void CybouDesktopModel::setFileItems(QVector<CybouFileItem> items)
{
    m_files = std::move(items);
    Q_EMIT filesChanged();
}

void CybouDesktopModel::upsertFileItem(const CybouFileItem& item)
{
    const auto it = std::find_if(m_files.begin(), m_files.end(),
        [&](const CybouFileItem& existing) { return existing.id == item.id; });
    if (it != m_files.end()) *it = item;
    else m_files.append(item);
    Q_EMIT filesChanged();
}

void CybouDesktopModel::setActivity(QVector<CybouActivityItem> items)
{
    m_activity = std::move(items);
    Q_EMIT activityChanged();
}

void CybouDesktopModel::addActivity(const CybouActivityItem& item)
{
    m_activity.prepend(item);
    Q_EMIT activityChanged();
}

void CybouDesktopModel::setWalletEntries(QVector<CybouWalletEntry> entries)
{
    m_wallet_entries = std::move(entries);
    Q_EMIT walletChanged();
}

void CybouDesktopModel::setContacts(QVector<CybouContact> contacts)
{
    m_contacts = std::move(contacts);
}

void CybouDesktopModel::setRestoreProgress(const CybouRestoreProgress& progress)
{
    m_restore_progress = progress;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setIdentityService(cybou::CybouIdentityService* identity_service)
{
    if (m_identity_service == identity_service) return;
    if (m_name_service) m_name_service->Cancel();
    if (m_name_worker.joinable()) m_name_worker.join();
    if (m_recovery_rotation_worker.joinable()) m_recovery_rotation_worker.join();
    m_recovery_rotation_pending = false;
    m_name_service.reset();
    m_identity_service = identity_service;
    if (!m_identity_service && m_capabilities.account_creation) {
        m_capabilities.account_creation = false;
        Q_EMIT capabilitiesChanged();
    }
    if (m_identity_service) {
        if (const auto path = m_identity_service->GetStoragePath()) {
            m_name_service = std::make_unique<cybou::CybouNameService>(
                m_identity_service->GetNodeRuntime(), m_identity_service->GetKeyStore(), *path);
        }
        m_capabilities.account_creation = true;
        Q_EMIT capabilitiesChanged();

        if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE &&
            m_identity_service->GetAccountId().has_value()) {
            // A vault exists but is not opened in this session yet.
            setIdentityState(CybouIdentityState::Locked,
                QString::fromStdString(m_identity_service->GetAccountId()->Value().GetHex()));
        }
        refreshFinalizedName();
    }
}

void CybouDesktopModel::setWalletService(cybou::CybouWalletService* wallet_service)
{
    m_wallet_service = wallet_service;
    const bool payments = m_wallet_service && m_status.identity_state == CybouIdentityState::Active;
    if (m_capabilities.payments != payments) {
        m_capabilities.payments = payments;
        Q_EMIT capabilitiesChanged();
    }
}

bool CybouDesktopModel::requestClaimName(const QString& label, const QString& vault_password)
{
    if (m_fixture_mode) {
        if (m_status.identity_state != CybouIdentityState::Active || m_status.name_claim_pending) return false;
        m_status.name_claim_pending = true;
        m_status.name_claim_status = tr("Claiming %1.cybou…").arg(label);
        Q_EMIT statusChanged();
        Q_EMIT nameClaimRequested(label);
        return true;
    }
    if (!m_name_service || m_status.identity_state != CybouIdentityState::Active ||
        m_status.name_claim_pending || !m_status.primary_name.isEmpty()) return false;
    if (m_name_worker.joinable()) m_name_worker.join();
    m_status.name_claim_pending = true;
    m_status.name_claim_status = tr("Saving encrypted name claim…");
    Q_EMIT statusChanged();
    m_name_worker = std::jthread([this, name = label.toStdString(), password = vault_password.toStdString()]() mutable {
        const auto result = m_name_service->ClaimSync(std::move(name), password,
            [this](cybou::NameClaimPhase, const std::string& detail) {
                QMetaObject::invokeMethod(this, [this, detail] {
                    m_status.name_claim_status = QString::fromStdString(detail);
                    Q_EMIT statusChanged();
                }, Qt::QueuedConnection);
            });
        memory_cleanse(password.data(), password.size());
        QMetaObject::invokeMethod(this, [this, result] {
            m_status.name_claim_pending = false;
            m_status.name_claim_status.clear();
            refreshFinalizedName();
            Q_EMIT statusChanged();
            if (!result.success) Q_EMIT nameClaimFailed(QString::fromStdString(result.message));
        }, Qt::QueuedConnection);
    });
    return true;
}

bool CybouDesktopModel::requestRecoveryRootRotation(const QStringList& new_phrase,
    const QString& vault_password, bool resume_pending)
{
    if (m_recovery_rotation_pending) return false;
    if (m_fixture_mode) {
        m_recovery_rotation_pending = true;
        Q_EMIT statusChanged();
        QMetaObject::invokeMethod(this, [this] {
            m_recovery_rotation_pending = false;
            setKeyEpoch(m_status.key_epoch + 1);
            Q_EMIT recoveryRotationFinished(CybouOperationOutcome::Finalized, {});
        }, Qt::QueuedConnection);
        return true;
    }
    if (!m_identity_service) return false;
    cybou::RecoveryWords words{};
    if (!resume_pending) {
        if (new_phrase.size() != static_cast<int>(words.size())) return false;
        for (size_t i = 0; i < words.size(); ++i) words[i] = new_phrase.at(static_cast<int>(i)).toStdString();
    }
    if (m_recovery_rotation_worker.joinable()) m_recovery_rotation_worker.join();
    m_recovery_rotation_pending = true;
    Q_EMIT statusChanged();
    auto password = vault_password.toStdString();
    m_recovery_rotation_worker = std::jthread([this, words = std::move(words), password = std::move(password), resume_pending]() mutable {
        auto result = resume_pending
            ? m_identity_service->ResumeIdentityRotationSync(password)
            : m_identity_service->RotateIdentitySync(words, password);
        memory_cleanse(password.data(), password.size());
        for (auto& word : words) memory_cleanse(word.data(), word.size());
        const auto outcome = result.phase == cybou::IdentityOperationPhase::FINALIZED ? CybouOperationOutcome::Finalized
            : result.phase == cybou::IdentityOperationPhase::ACCEPTED ||
                result.phase == cybou::IdentityOperationPhase::UNCERTAIN ? CybouOperationOutcome::Pending
            : CybouOperationOutcome::Failed;
        const auto error = QString::fromStdString(result.error);
        QMetaObject::invokeMethod(this, [this, outcome, error] {
            m_recovery_rotation_pending = false;
            Q_EMIT statusChanged();
            Q_EMIT recoveryRotationFinished(outcome, error);
        }, Qt::QueuedConnection);
    });
    return true;
}

bool CybouDesktopModel::requestUnlockIdentity(const QString& vault_password)
{
    if (!m_identity_service || !m_identity_service->LoadVault(vault_password.toStdString())) return false;
    const auto account_id = m_identity_service->GetAccountId();
    if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE && account_id) {
        const auto state = m_identity_service->GetFinalizedAccountState();
        setIdentityState(CybouIdentityState::Active,
            QString::fromStdString(account_id->Value().GetHex()),
            state ? state->creation_height : 0);
        if (state) setBalances(state->balance, state->system_balance);
    }
    Q_EMIT statusChanged();
    return true;
}

bool CybouDesktopModel::hasLocalVault() const
{
    if (!m_identity_service) return false;
    const auto path = m_identity_service->GetStoragePath();
    return path && std::filesystem::exists(*path);
}

namespace {
QStringList ToQStringList(const cybou::RecoveryWords& words)
{
    QStringList list;
    for (const auto& word : words) list << QString::fromStdString(word);
    return list;
}

/** Deterministic, clearly fake words used only by UI fixtures. */
QStringList FixtureWords()
{
    return QStringList{QStringLiteral("ocean"), QStringLiteral("lamp"), QStringLiteral("river"),
        QStringLiteral("stone"), QStringLiteral("cloud"), QStringLiteral("maple"), QStringLiteral("orbit"),
        QStringLiteral("violet"), QStringLiteral("anchor"), QStringLiteral("harbor"), QStringLiteral("pilot"),
        QStringLiteral("garden"), QStringLiteral("silver"), QStringLiteral("canyon"), QStringLiteral("ember"),
        QStringLiteral("meadow"), QStringLiteral("quartz"), QStringLiteral("lantern"), QStringLiteral("summit"),
        QStringLiteral("willow"), QStringLiteral("falcon"), QStringLiteral("copper"), QStringLiteral("island"),
        QStringLiteral("breeze")};
}
} // namespace

std::optional<QStringList> CybouDesktopModel::prepareNewIdentityWords()
{
    if (m_fixture_mode) return FixtureWords();
    if (!m_identity_service) return std::nullopt;
    const auto words = m_identity_service->PrepareNewIdentity();
    if (!words) return std::nullopt;
    return ToQStringList(*words);
}

void CybouDesktopModel::discardPreparedIdentity()
{
    if (m_identity_service) m_identity_service->DiscardPreparedIdentity();
}

std::optional<QStringList> CybouDesktopModel::revealRecoveryWords(const QString& vault_password)
{
    if (m_fixture_mode) {
        if (vault_password.isEmpty()) return std::nullopt;
        return FixtureWords();
    }
    if (!requestUnlockIdentity(vault_password)) return std::nullopt;
    const auto words = m_identity_service->GetKeyStore().GetRecoveryWords();
    if (!words) return std::nullopt;
    return ToQStringList(*words);
}

bool CybouDesktopModel::recoveryPhraseValid(const QString& phrase) const
{
    const auto parts = phrase.trimmed().split(QRegularExpression{QStringLiteral("\\s+")}, Qt::SkipEmptyParts);
    if (parts.size() != 24) return false;
    if (m_fixture_mode) return true;
    cybou::RecoveryWords words;
    for (int i{0}; i < parts.size(); ++i) words[i] = parts[i].toLower().toStdString();
    auto entropy = cybou::DecodeRecoveryWords(words);
    const bool valid = entropy.has_value();
    if (entropy) memory_cleanse(entropy->data(), entropy->size());
    for (auto& word : words) memory_cleanse(word.data(), word.size());
    return valid;
}

std::optional<QStringList> CybouDesktopModel::generateRotationWords()
{
    if (m_fixture_mode) return FixtureWords();
    auto entropy = cybou::GenerateRecoveryEntropy();
    if (!entropy) return std::nullopt;
    auto words = cybou::EncodeRecoveryWords(*entropy);
    memory_cleanse(entropy->data(), entropy->size());
    QStringList list = ToQStringList(words);
    for (auto& word : words) memory_cleanse(word.data(), word.size());
    return list;
}

bool CybouDesktopModel::hasPendingRecoveryRotation() const
{
    return !m_fixture_mode && m_identity_service && m_identity_service->HasPendingIdentityRotation();
}

void CybouDesktopModel::setKeyEpoch(quint32 key_epoch)
{
    if (m_status.key_epoch == key_epoch) return;
    m_status.key_epoch = key_epoch;
    Q_EMIT statusChanged();
}

QString CybouDesktopModel::nameLabelProblem(const QString& label) const
{
    using E = cybou::NameValidationError;
    switch (cybou::ValidateNameLabel(label.toStdString())) {
    case E::NONE: return {};
    case E::EMPTY: return tr("Enter a name.");
    case E::TOO_SHORT: return tr("Use at least 5 characters.");
    case E::TOO_LONG: return tr("Use at most 32 characters.");
    case E::INVALID_CHARACTER: return tr("Use lowercase letters a–z, digits and hyphens.");
    case E::INVALID_START_END: return tr("A name cannot start or end with a hyphen.");
    case E::CONSECUTIVE_HYPHENS: return tr("A name cannot contain two hyphens in a row.");
    case E::IDN_PREFIX: return tr("Names cannot start with \"xn--\".");
    case E::ALL_DIGITS: return tr("A name needs at least one letter.");
    case E::RESERVED_NAME: return tr("This name is reserved.");
    }
    return tr("This name is not valid.");
}

void CybouDesktopModel::setNameClaimFinished()
{
    m_status.name_claim_pending = false;
    m_status.name_claim_status.clear();
    Q_EMIT statusChanged();
}

void CybouDesktopModel::requestLockVault()
{
    if (m_status.identity_state != CybouIdentityState::Active) return;
    setIdentityState(CybouIdentityState::Locked, m_status.account_id, m_status.creation_height);
    Q_EMIT lockVaultRequested();
}

namespace {
CybouIdentityStep StepForPhase(cybou::IdentityCreationPhase phase)
{
    switch (phase) {
    case cybou::IdentityCreationPhase::CREATING_KEYS: return CybouIdentityStep::PreparingKeys;
    case cybou::IdentityCreationPhase::PERFORMING_WORK:
    case cybou::IdentityCreationPhase::BROADCASTING: return CybouIdentityStep::CreatingIdentity;
    default: return CybouIdentityStep::WaitingForConfirmation;
    }
}
} // namespace

void CybouDesktopModel::requestCreateIdentity(const QString& vault_password)
{
    // The UI boundary ends here: anti-Sybil work, operation construction and
    // finality handling belong to core. The flag is request bookkeeping only.
    m_identity_request_pending = true;
    Q_EMIT createIdentityRequested();
    Q_EMIT statusChanged();
    if (!m_identity_service) return;

    m_identity_service->CreateIdentityAsync(vault_password.toStdString(),
        [this](cybou::IdentityCreationPhase phase, const std::string&) {
            QMetaObject::invokeMethod(this, [this, phase] {
                if (phase == cybou::IdentityCreationPhase::FAILED) {
                    setIdentityState(CybouIdentityState::None);
                    return;
                }
                if (phase == cybou::IdentityCreationPhase::ACTIVE) return;
                setIdentityStep(StepForPhase(phase));
                setIdentityState(CybouIdentityState::Creating);
            }, Qt::QueuedConnection);
        },
        [this](const cybou::IdentityCreationResult& result) {
            QMetaObject::invokeMethod(this, [this, result] {
                if (result.success) {
                    setIdentityState(CybouIdentityState::Active,
                        QString::fromStdString(result.account_id.Value().GetHex()), result.creation_height);
                    setBalances(0, result.system_balance);
                } else {
                    setIdentityState(CybouIdentityState::None);
                    Q_EMIT identityCreationFailed(QString::fromStdString(result.error_message));
                }
            }, Qt::QueuedConnection);
        });
}

bool CybouDesktopModel::requestRestoreIdentity(const QString& recovery_phrase, const QString& vault_password)
{
    if (!recoveryPhraseValid(recovery_phrase)) return false;
    if (!m_identity_service) {
        // Fixture mode: hand the request to the fixture driver.
        m_identity_request_pending = true;
        CybouRestoreProgress progress;
        progress.identity = CybouRestoreStepState::Running;
        setRestoreProgress(progress);
        setIdentityState(CybouIdentityState::Restoring);
        Q_EMIT restoreIdentityRequested();
        return true;
    }
    const auto parts = recovery_phrase.trimmed().split(QRegularExpression{QStringLiteral("\\s+")}, Qt::SkipEmptyParts);
    if (parts.size() != 24) return false;
    cybou::RecoveryWords words;
    for (int i{0}; i < parts.size(); ++i) words[i] = parts[i].toStdString();
    if (!cybou::DecodeRecoveryWords(words)) return false;
    m_identity_request_pending = true;
    CybouRestoreProgress progress;
    progress.identity = CybouRestoreStepState::Running;
    setRestoreProgress(progress);
    setIdentityState(CybouIdentityState::Restoring);
    m_identity_service->RestoreIdentityAsync(std::move(words), vault_password.toStdString(),
        [this](cybou::IdentityCreationPhase phase, const std::string&) {
            QMetaObject::invokeMethod(this, [this, phase] {
                if (phase == cybou::IdentityCreationPhase::FAILED) setIdentityState(CybouIdentityState::None);
            }, Qt::QueuedConnection);
        },
        [this](const cybou::IdentityCreationResult& result) {
            QMetaObject::invokeMethod(this, [this, result] {
                if (result.success) {
                    // Identity, wallet and names come from finalized state.
                    // Mail and Files history reconstruction is not connected
                    // yet, so those rows stay honest instead of claiming Done.
                    CybouRestoreProgress done;
                    done.identity = CybouRestoreStepState::Done;
                    done.wallet = CybouRestoreStepState::Done;
                    done.names = CybouRestoreStepState::Done;
                    setRestoreProgress(done);
                    setIdentityState(CybouIdentityState::Active,
                        QString::fromStdString(result.account_id.Value().GetHex()), result.creation_height);
                    setBalances(0, result.system_balance);
                } else {
                    setIdentityState(CybouIdentityState::None);
                    Q_EMIT identityCreationFailed(QString::fromStdString(result.error_message));
                }
            }, Qt::QueuedConnection);
        });
    return true;
}

void CybouDesktopModel::requestFileUpload(const QString& source_path)
{
    if (m_status.identity_state != CybouIdentityState::Active) return;
    Q_EMIT fileUploadRequested(source_path);
}

void CybouDesktopModel::requestFileDownload(const QString& file_id, const QString& destination)
{
    if (m_status.identity_state != CybouIdentityState::Active) return;
    Q_EMIT fileDownloadRequested(file_id, destination);
}

void CybouDesktopModel::refreshFinalizedName()
{
    if (m_fixture_mode) return;
    const auto name = m_identity_service && m_status.identity_state == CybouIdentityState::Active
        ? m_identity_service->GetFinalizedPrimaryName() : std::nullopt;
    const QString finalized = name ? QString::fromStdString(*name) + QStringLiteral(".cybou") : QString{};
    if (m_status.primary_name == finalized) return;
    m_status.primary_name = finalized;
    QVector<CybouNameItem> names;
    if (!finalized.isEmpty()) names.append({finalized, true});
    m_names = names;
    Q_EMIT namesChanged();
    Q_EMIT statusChanged();
}
