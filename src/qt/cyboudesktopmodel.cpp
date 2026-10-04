// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboudesktopmodel.h>

#include <qt/cybouapplicationbackend.h>

#include <cybou/identity_operation_coordinator.h>
#include <cybou/identity_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/name_registry.h>
#include <cybou/name_service.h>
#include <cybou/node_runtime.h>
#include <cybou/hex.h>
#include <cybou/protocol_limits.h>
#include <cybou/storage_economy.h>
#include <cybou/support_mail.h>
#include <cybou/identity_signer.h>
#include <cybou/wallet_service.h>

#include <cybou/crypto/cleanse.h>

#include <QCryptographicHash>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QRegularExpression>

#include <algorithm>
#include <filesystem>
#include <utility>

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

QString cybouConnectionText(const CybouDesktopStatus& status)
{
    if (!status.sync_error.isEmpty()) return CybouDesktopModel::tr("Needs attention");
    if (!status.node_running) return CybouDesktopModel::tr("Offline");
    if (!status.online) return CybouDesktopModel::tr("Connecting");
    if (status.syncing) return CybouDesktopModel::tr("Syncing");
    return CybouDesktopModel::tr("Synced");
}

namespace {
QString RememberedNameKey(const QString& data_directory)
{
    const auto digest = QCryptographicHash::hash(data_directory.toUtf8(), QCryptographicHash::Sha256).toHex().left(16);
    return QStringLiteral("identity/last_name/") + QString::fromLatin1(digest);
}
} // namespace

CybouDesktopModel::CybouDesktopModel(QString network_name, QObject* parent)
    : QObject{parent}
{
    m_status.network_name = std::move(network_name);
    connect(this, &CybouDesktopModel::filesChanged, this, &CybouDesktopModel::refreshStorageUsed);
    connect(this, &CybouDesktopModel::filesChanged, this, &CybouDesktopModel::rebuildActivity);
    connect(this, &CybouDesktopModel::mailChanged, this, &CybouDesktopModel::rebuildActivity);
    connect(this, &CybouDesktopModel::walletChanged, this, &CybouDesktopModel::rebuildActivity);
    connect(this, &CybouDesktopModel::mailChanged, this, &CybouDesktopModel::rebuildContacts);
    connect(this, &CybouDesktopModel::walletChanged, this, &CybouDesktopModel::rebuildContacts);
    // Yourself never appears; the own name may be learned after mail loaded.
    connect(this, &CybouDesktopModel::namesChanged, this, &CybouDesktopModel::rebuildContacts);
}

void CybouDesktopModel::rebuildActivity()
{
    if (fixtureMode()) return;
    const auto timed = [](const QDateTime& time) { return time.isValid() && time.toSecsSinceEpoch() > 0; };
    QVector<CybouActivityItem> items = m_extra_activity;
    for (const auto& mail : m_mail) {
        if (mail.draft || !timed(mail.time)) continue;
        const QString subject = mail.subject.isEmpty() ? tr("(no subject)") : mail.subject;
        if (mail.folder == CybouMailFolder::Sent) {
            items.append({CybouActivityKind::MailSent, tr("Mail to %1").arg(mail.to_name), subject, mail.time});
        } else if (mail.folder == CybouMailFolder::Inbox || mail.folder == CybouMailFolder::Archive) {
            items.append({CybouActivityKind::MailReceived, tr("Mail from %1").arg(mail.from_name), subject, mail.time});
        }
    }
    for (const auto& file : m_files) {
        if (file.folder || file.trashed || !timed(file.modified)) continue;
        items.append({CybouActivityKind::FileUploaded, tr("%1 added to Files").arg(file.name),
            CybouProduct::sizeText(file.logical_size), file.modified});
    }
    for (const auto& entry : m_wallet_entries) {
        if (!timed(entry.time)) continue;
        const QString amount = cybouAmountText(static_cast<quint64>(std::llabs(entry.amount)));
        if (entry.kind == CybouWalletEntryKind::Sent) {
            items.append({CybouActivityKind::PaymentSent, tr("%1 sent").arg(amount), entry.counterparty_name, entry.time});
        } else if (entry.kind == CybouWalletEntryKind::Received) {
            items.append({CybouActivityKind::PaymentReceived, tr("%1 received").arg(amount), entry.counterparty_name,
                entry.time});
        }
    }
    std::stable_sort(items.begin(), items.end(),
        [](const CybouActivityItem& a, const CybouActivityItem& b) { return a.time > b.time; });
    if (items.size() > 20) items.resize(20);
    m_activity = std::move(items);
    Q_EMIT activityChanged();
}

CybouDesktopModel::~CybouDesktopModel()
{
    if (m_name_service) m_name_service->Cancel();
    if (m_name_worker.joinable()) m_name_worker.join();
    if (m_recovery_rotation_worker.joinable()) m_recovery_rotation_worker.join();
    if (m_payment_worker.joinable()) m_payment_worker.join();
    if (m_vault_worker.joinable()) m_vault_worker.join();
}

void CybouDesktopModel::notify(const QString& text, const QString& action_label, std::function<void()> action)
{
    Q_EMIT notificationRequested(text, action_label, std::move(action));
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

CybouFeatureAvailability CybouDesktopModel::honest(CybouFeatureAvailability featureAvailability) const
{
    // Never claim Mail or Files without a backend that can carry them out.
    featureAvailability.mail = featureAvailability.mail && m_backend && m_backend->mailAvailable();
    featureAvailability.files = featureAvailability.files && m_backend && m_backend->filesAvailable();
    return featureAvailability;
}

void CybouDesktopModel::setFeatureAvailability(const CybouFeatureAvailability& requested)
{
    m_requested_availability = requested;
    const CybouFeatureAvailability featureAvailability = honest(requested);
    if (m_availability.account_creation == featureAvailability.account_creation &&
        m_availability.payments == featureAvailability.payments &&
        m_availability.mail == featureAvailability.mail &&
        m_availability.files == featureAvailability.files &&
        m_availability.sharing == featureAvailability.sharing &&
        m_availability.version_history == featureAvailability.version_history) {
        return;
    }
    m_availability = featureAvailability;
    Q_EMIT featureAvailabilityChanged();
}

void CybouDesktopModel::setNetworkInfo(const QString& network_name, const QString& network_binding)
{
    if (m_status.network_name == network_name && m_status.network_binding == network_binding) return;
    m_status.network_name = network_name;
    m_status.network_binding = network_binding;
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

void CybouDesktopModel::setGeoAdmissionStatus(CybouGeoAdmissionStatus status)
{
    if (m_status.geo_admission == status) return;
    m_status.geo_admission = status;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setSyncing(bool syncing)
{
    if (m_status.syncing == syncing) return;
    m_status.syncing = syncing;
    Q_EMIT statusChanged();
    setFeatureAvailability(m_requested_availability);
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
    if (state == CybouIdentityState::Active && m_wallet_service && !m_availability.payments) {
        m_availability.payments = true;
        m_requested_availability.payments = true;
        Q_EMIT featureAvailabilityChanged();
    }
    syncIdentitySession();
    Q_EMIT statusChanged();
    if (state == CybouIdentityState::Active) refreshFinalizedName();
}

void CybouDesktopModel::syncIdentitySession()
{
    const auto state = m_status.identity_state;
    const bool open = state == CybouIdentityState::Active || state == CybouIdentityState::Syncing ||
        state == CybouIdentityState::NeedsAttention;
    if (open == m_session_open) return;
    m_session_open = open;
    if (!open) {
        // Private semantic data never outlives the unlocked Identity.
        const bool had_mail = !m_mail.isEmpty();
        const bool had_files = !m_files.isEmpty();
        m_mail.clear();
        m_files.clear();
        if (had_mail) Q_EMIT mailChanged();
        if (had_files) Q_EMIT filesChanged();
    }
    if (!m_backend) return;
    if (open) m_backend->openIdentity();
    else m_backend->closeIdentity();
}

void CybouDesktopModel::requestApplicationFeatureAvailability(bool mail, bool files)
{
    CybouFeatureAvailability requested = m_requested_availability;
    requested.mail = mail;
    requested.files = files;
    setFeatureAvailability(requested);
}

void CybouDesktopModel::setApplicationBackend(CybouApplicationBackend* backend)
{
    if (m_backend == backend) return;
    if (m_backend) {
        if (m_session_open) m_backend->closeIdentity();
        disconnect(m_backend, nullptr, this, nullptr);
    }
    m_backend = backend;
    if (m_backend) {
        using B = CybouApplicationBackend;
        connect(m_backend, &B::availabilityChanged, this, [this] { setFeatureAvailability(m_requested_availability); });
        connect(m_backend, &B::mailSnapshot, this, [this](const QVector<CybouMailItem>& items) {
            if (m_session_open) setMailItems(items);
        });
        connect(m_backend, &B::mailItemChanged, this, [this](const CybouMailItem& item) {
            if (m_session_open) upsertMailItem(item);
        });
        connect(m_backend, &B::mailItemRemoved, this, &CybouDesktopModel::removeMailItem);
        connect(m_backend, &B::mailItemReplaced, this, &CybouDesktopModel::mailIdReplaced);
        connect(m_backend, &B::mailStateChanged, this, &CybouDesktopModel::setMailState);
        connect(m_backend, &B::attachmentStateChanged, this, &CybouDesktopModel::setAttachmentState);
        connect(m_backend, &B::attachmentRetrievalChanged, this, &CybouDesktopModel::setAttachmentRetrieval);
        connect(m_backend, &B::filesSnapshot, this, [this](const QVector<CybouFileItem>& items) {
            if (m_session_open) setFileItems(items);
        });
        connect(m_backend, &B::fileItemChanged, this, [this](const CybouFileItem& item) {
            if (m_session_open) upsertFileItem(item);
        });
        connect(m_backend, &B::fileItemsRemoved, this, &CybouDesktopModel::removeFileItems);
        connect(m_backend, &B::fileStateChanged, this, &CybouDesktopModel::setFileState);
        connect(m_backend, &B::fileRetrievalChanged, this, &CybouDesktopModel::setFileRetrieval);
        connect(m_backend, &B::restoreProgressChanged, this,
            [this](CybouRestoreStepState mail, CybouRestoreStepState files) {
                CybouRestoreProgress progress = m_restore_progress;
                progress.mail = mail;
                progress.files = files;
                setRestoreProgress(progress);
            });
        connect(m_backend, &B::commandFailed, this, [this](const QString& text) { notify(text); });
        if (m_session_open) m_backend->openIdentity();
    }
    setFeatureAvailability(m_requested_availability);
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

void CybouDesktopModel::removeMailItem(const QString& id)
{
    if (m_mail.removeIf([&id](const CybouMailItem& item) { return item.id == id; }) > 0) Q_EMIT mailChanged();
}

bool CybouDesktopModel::mailReady() const
{
    return m_backend && m_availability.mail && m_status.identity_state == CybouIdentityState::Active;
}

bool CybouDesktopModel::filesReady() const
{
    return m_backend && m_availability.files && m_status.identity_state == CybouIdentityState::Active;
}

void CybouDesktopModel::requestMailRead(const QString& id, bool read)
{
    const auto* item = mailItem(id);
    if (mailReady() && item && item->unread == read) m_backend->setMailRead(id, read);
}

void CybouDesktopModel::requestMailStarred(const QString& id, bool starred)
{
    const auto* item = mailItem(id);
    if (mailReady() && item && item->starred != starred) m_backend->setMailStarred(id, starred);
}

void CybouDesktopModel::requestMoveMail(const QString& id, CybouMailFolder folder)
{
    const auto* item = mailItem(id);
    if (mailReady() && item && item->folder != folder) m_backend->moveMail(id, folder);
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

QString CybouDesktopModel::requestSaveMailDraft(CybouMailItem draft)
{
    if (!mailReady()) return {};
    if (draft.id.isEmpty()) draft.id = NewLocalId("draft");
    draft.folder = CybouMailFolder::Drafts;
    draft.draft = true;
    draft.unread = false;
    draft.state = CybouContentState::Local;
    draft.from_name = m_status.primary_name;
    draft.time = QDateTime::currentDateTime();
    draft.preview = PreviewOf(draft.body);
    m_backend->saveMailDraft(draft);
    return draft.id;
}

void CybouDesktopModel::requestDeleteMail(const QString& id)
{
    if (mailReady() && mailItem(id)) m_backend->deleteMail(id);
}

QString CybouDesktopModel::requestSendMail(CybouMailItem message)
{
    if (!mailReady()) return {};
    // A sent draft becomes the outgoing message.
    if (!message.id.isEmpty() && mailItem(message.id)) m_backend->deleteMail(message.id);
    message.id = NewLocalId("out");
    message.folder = CybouMailFolder::Sent;
    message.outgoing = true;
    message.draft = false;
    message.unread = false;
    message.from_name = m_status.primary_name;
    message.time = QDateTime::currentDateTime();
    message.preview = PreviewOf(message.body);
    message.state = CybouContentState::Local;
    message.operation_state = CybouOperationState::Preparing;
    for (auto& attachment : message.attachments) {
        if (!CybouProduct::contentOnNetwork(attachment.state)) attachment.state = CybouContentState::Local;
    }
    // Optimistic Preparing; the backend is the authority from here on and
    // never reports Sent before the content is Protected.
    upsertMailItem(message);
    m_backend->sendMail(message);
    return message.id;
}

void CybouDesktopModel::setMailState(const QString& id, CybouContentState state)
{
    for (auto& item : m_mail) {
        if (item.id != id) continue;
        item.state = state;
        // Storage durability exists only after PoA finality.
        if (state == CybouContentState::Securing || state == CybouContentState::Protected ||
            state == CybouContentState::Received) {
            item.operation_state = CybouOperationState::Finalized;
        }
        // Reused, already protected content keeps its state.
        for (auto& attachment : item.attachments) {
            if (!CybouProduct::contentOnNetwork(attachment.state)) attachment.state = state;
        }
        Q_EMIT mailChanged();
        return;
    }
}

namespace {
CybouAttachmentItem* FindAttachment(QVector<CybouMailItem>& mail, const QString& message_id,
    const QString& attachment_id)
{
    for (auto& item : mail) {
        if (item.id != message_id) continue;
        for (auto& attachment : item.attachments) {
            if (attachment.id == attachment_id) return &attachment;
        }
    }
    return nullptr;
}
} // namespace

void CybouDesktopModel::setAttachmentState(const QString& message_id, const QString& attachment_id,
    CybouContentState state, int progress_percent)
{
    if (auto* attachment = FindAttachment(m_mail, message_id, attachment_id)) {
        attachment->state = state;
        attachment->progress_percent = progress_percent;
        Q_EMIT mailChanged();
    }
}

void CybouDesktopModel::setAttachmentRetrieval(const QString& message_id, const QString& attachment_id,
    CybouRetrievalState retrieval)
{
    if (auto* attachment = FindAttachment(m_mail, message_id, attachment_id)) {
        attachment->retrieval = retrieval;
        Q_EMIT mailChanged();
    }
}

void CybouDesktopModel::requestRetryMail(const QString& id)
{
    const auto* item = mailItem(id);
    if (!mailReady() || !item || item->state != CybouContentState::NeedsAttention) return;
    CybouMailItem pending = *item;
    pending.state = CybouContentState::Local;
    pending.operation_state = CybouOperationState::Preparing;
    for (auto& attachment : pending.attachments) {
        if (!CybouProduct::contentOnNetwork(attachment.state)) attachment.state = CybouContentState::Local;
    }
    upsertMailItem(pending);
    m_backend->retryMail(id);
}

void CybouDesktopModel::requestAttachmentDownload(const QString& message_id, const QString& attachment_id,
    const QString& destination)
{
    if (!m_backend || m_status.identity_state != CybouIdentityState::Active) return;
    setAttachmentRetrieval(message_id, attachment_id, CybouRetrievalState::Downloading);
    m_backend->downloadAttachment(message_id, attachment_id, destination);
}

CybouAttachmentItem CybouDesktopModel::localAttachment(const QString& path) const
{
    const QFileInfo info{path};
    CybouAttachmentItem item;
    item.id = NewLocalId("att");
    item.name = info.fileName();
    item.logical_size = static_cast<quint64>(qMax<qint64>(0, info.size()));
    item.state = CybouContentState::Local;
    item.source_path = info.absoluteFilePath();
    return item;
}

void CybouDesktopModel::setFileItems(QVector<CybouFileItem> items)
{
    m_files = std::move(items);
    Q_EMIT filesChanged();
}

void CybouDesktopModel::refreshStorageUsed()
{
    // Live mode: "used" is this Identity's own files (Trash included until
    // emptied), never provider topology. Fixtures set their own figures.
    if (fixtureMode()) return;
    quint64 used{0};
    for (const auto& file : m_files) {
        if (!file.folder) used += file.logical_size;
    }
    setStorageUsage(used, m_status.storage_quota);
}

void CybouDesktopModel::upsertFileItem(const CybouFileItem& item)
{
    const auto it = std::find_if(m_files.begin(), m_files.end(),
        [&](const CybouFileItem& existing) { return existing.id == item.id; });
    if (it != m_files.end()) *it = item;
    else m_files.append(item);
    Q_EMIT filesChanged();
}

void CybouDesktopModel::removeFileItems(const QStringList& ids)
{
    if (m_files.removeIf([&ids](const CybouFileItem& item) { return ids.contains(item.id); }) > 0)
        Q_EMIT filesChanged();
}

void CybouDesktopModel::setActivity(QVector<CybouActivityItem> items)
{
    m_activity = std::move(items);
    Q_EMIT activityChanged();
}

void CybouDesktopModel::addActivity(const CybouActivityItem& item)
{
    if (fixtureMode()) {
        m_activity.prepend(item);
        Q_EMIT activityChanged();
        return;
    }
    m_extra_activity.prepend(item);
    if (m_extra_activity.size() > 20) m_extra_activity.resize(20);
    rebuildActivity();
}

void CybouDesktopModel::setWalletEntries(QVector<CybouWalletEntry> entries)
{
    // Counterparties arrive as AccountIDs; show their finalized .cybou name when they have one.
    if (m_identity_service && !m_fixture_mode) {
        std::optional<cybou::CybouState> state;
        for (auto& entry : entries) {
            if (entry.counterparty_name.size() != 64) continue;
            const auto raw = cybou::ParseHash256UserHex(entry.counterparty_name.toStdString());
            if (!raw) continue;
            if (!state) {
                const auto loaded = m_identity_service->GetNodeRuntime().GetStore().LoadState();
                if (!loaded || !loaded.state) break;
                state = *loaded.state;
            }
            if (const auto* name = state->names.PrimaryName(cybou::AccountId{*raw})) {
                entry.counterparty_name = QString::fromStdString(*name) + QStringLiteral(".cybou");
            } else {
                entry.counterparty_name = CybouProduct::shortId(entry.counterparty_name);
            }
        }
    }
    m_wallet_entries = std::move(entries);
    Q_EMIT walletChanged();
}

void CybouDesktopModel::setResourceUsage(quint64 quota_used)
{
    if (m_status.quota_used == quota_used) return;
    m_status.quota_used = quota_used;
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setNetworkDiagnostics(cybou::NodeDiagnosticsSnapshot snapshot)
{
    m_network_diagnostics = std::move(snapshot);
    Q_EMIT statusChanged();
}

void CybouDesktopModel::setNetworkAuthority(const CybouNetworkAuthorityStatus& status)
{
    const bool changed_role = status.proven != m_network_authority.proven;
    m_network_authority = status.proven ? status : CybouNetworkAuthorityStatus{};
    if (changed_role || status.proven) Q_EMIT networkAuthorityChanged();
}

void CybouDesktopModel::requestFinalizationPaused(bool paused)
{
    if (!m_network_authority.proven) return;
    Q_EMIT finalizationPauseRequested(paused);
}

void CybouDesktopModel::requestFinalizeNow()
{
    if (!m_network_authority.proven) return;
    Q_EMIT finalizeNowRequested();
}

void CybouDesktopModel::setOperationStatus(const CybouOperationStatus& status)
{
    if (status.operation_id.isEmpty()) return;
    auto& stored = m_operations[status.operation_id];
    // Finality is canonical. Failed is terminal for this exact operation;
    // retryable delivery stays Submitted and a new attempt needs a new id.
    if (stored.state == CybouOperationState::Finalized && status.state != CybouOperationState::Finalized) return;
    if (stored.state == CybouOperationState::Failed && status.state != CybouOperationState::Failed &&
        status.state != CybouOperationState::Finalized) return;
    // Explicit phase transitions prevent delayed asynchronous events from regressing validation.
    const auto allowed = [&] {
        if (status.state == stored.state || status.state == CybouOperationState::Finalized) return true;
        if (status.state == CybouOperationState::Failed) return stored.state != CybouOperationState::Finalized;
        switch (stored.state) {
        case CybouOperationState::Local: return status.state == CybouOperationState::Preparing || status.state == CybouOperationState::Submitted;
        case CybouOperationState::Preparing: return status.state == CybouOperationState::Submitted;
        case CybouOperationState::Submitted:
        case CybouOperationState::Finalized: case CybouOperationState::Failed: return false;
        }
        return false;
    };
    if (!allowed()) return;
    stored = status;
    Q_EMIT operationStatusChanged(status.operation_id);
    // Items reference operations by id; let their views re-render.
    Q_EMIT walletChanged();
    Q_EMIT mailChanged();
    Q_EMIT filesChanged();
}

std::optional<CybouOperationStatus> CybouDesktopModel::operationStatus(const QString& operation_id) const
{
    const auto it = m_operations.constFind(operation_id);
    if (it == m_operations.constEnd()) return std::nullopt;
    return *it;
}

CybouOperationState CybouDesktopModel::displayedOperationState(const QString& operation_id,
    CybouOperationState own) const
{
    if (own == CybouOperationState::Finalized) return own;
    auto state = own;
    if (const auto status = operationStatus(operation_id)) {
        if (status->state == CybouOperationState::Finalized || status->state == CybouOperationState::Failed) {
            return status->state;
        }
        if (own == CybouOperationState::Failed) return own;
        if (static_cast<int>(status->state) > static_cast<int>(state)) state = status->state;
    }
    return state;
}

bool CybouDesktopModel::requestPayment(const QString& to_name, quint64 amount)
{
    if (m_payment_pending || amount == 0 || m_status.identity_state != CybouIdentityState::Active) return false;
    const QString name = to_name.trimmed().toLower();
    if (!name.endsWith(QStringLiteral(".cybou")) || !nameLabelProblem(name.chopped(6)).isEmpty()) return false;
    if (name == m_status.primary_name) return false;
    m_payment_pending = true;
    Q_EMIT statusChanged();
    Q_EMIT paymentRequested(name, amount);
    if (m_fixture_mode) return true; // the fixture driver answers
    if (!m_wallet_service || !m_identity_service) {
        setPaymentFinished(false, tr("Payments are not connected yet."));
        return true;
    }
    if (m_payment_worker.joinable()) m_payment_worker.join();
    m_payment_worker = std::jthread([this, label = name.chopped(6).toStdString(), amount] {
        // Resolve the finalized name owner, then submit from Balance.
        std::optional<cybou::AccountId> recipient;
        {
            const auto loaded = m_identity_service->GetNodeRuntime().GetStore().LoadState();
            if (loaded && loaded.state) {
                if (const auto* owner = loaded.state->names.Resolve(label)) recipient = *owner;
            }
        }
        if (!recipient) {
            QMetaObject::invokeMethod(this, [this] {
                setPaymentFinished(false, tr("This name does not belong to a CYBOU Identity."));
            }, Qt::QueuedConnection);
            return;
        }
        const auto result = m_wallet_service->SendPayment(*recipient, amount);
        const bool ok = static_cast<bool>(result);
        QString error;
        switch (result.error) {
        case cybou::WalletOperationError::NONE: break;
        case cybou::WalletOperationError::INSUFFICIENT_BALANCE: error = tr("Not enough CYBOU available."); break;
        case cybou::WalletOperationError::INSUFFICIENT_SYSTEM_BALANCE: error = tr("Not enough System Balance for the network service fee."); break;
        case cybou::WalletOperationError::SELF_PAYMENT: error = tr("You cannot send CYBOU to yourself."); break;
        case cybou::WalletOperationError::SUBMIT_FAILED: error = tr("CYBOU could not reach the network. Try again."); break;
        default: error = tr("The payment could not be sent."); break;
        }
        QMetaObject::invokeMethod(this, [this, ok, error] { setPaymentFinished(ok, error); }, Qt::QueuedConnection);
    });
    return true;
}

bool CybouDesktopModel::requestLockToSystemBalance(quint64 amount)
{
    if (m_payment_pending || amount == 0 || amount > m_status.balance ||
        m_status.identity_state != CybouIdentityState::Active) return false;
    m_payment_pending = true;
    Q_EMIT statusChanged();
    const auto finish = [this](bool ok, const QString& error) {
        m_payment_pending = false;
        Q_EMIT statusChanged();
        Q_EMIT systemLockFinished(ok, error);
    };
    if (m_fixture_mode) {
        m_status.balance -= amount;
        m_status.system_balance += amount;
        finish(true, {});
        return true;
    }
    if (!m_wallet_service) {
        finish(false, tr("The wallet is not connected yet."));
        return true;
    }
    if (m_payment_worker.joinable()) m_payment_worker.join();
    m_payment_worker = std::jthread([this, amount, finish] {
        const auto result = m_wallet_service->LockToSystemBalance(amount);
        QString error;
        switch (result.error) {
        case cybou::WalletOperationError::NONE: break;
        case cybou::WalletOperationError::INSUFFICIENT_BALANCE: error = tr("Not enough CYBOU available."); break;
        case cybou::WalletOperationError::SUBMIT_FAILED: error = tr("CYBOU could not reach the network. Try again."); break;
        default: error = tr("CYBOU could not be moved to System Balance."); break;
        }
        QMetaObject::invokeMethod(this, [finish, ok = static_cast<bool>(result), error] { finish(ok, error); },
            Qt::QueuedConnection);
    });
    return true;
}

QString CybouDesktopModel::feePurpose(const QString& operation_id) const
{
    if (operation_id.isEmpty()) return {};
    for (const auto& mail : m_mail) {
        if (mail.operation_id == operation_id) {
            return tr("Mail: %1").arg(mail.subject.isEmpty() ? tr("(no subject)") : mail.subject);
        }
    }
    for (const auto& file : m_files) {
        if (file.operation_id == operation_id) {
            return file.folder ? tr("Folder: %1").arg(file.name) : tr("File: %1").arg(file.name);
        }
    }
    // Renames, moves and deletions republish the Files tree without a single owner item.
    return tr("Files and folders update");
}

void CybouDesktopModel::setPaymentFinished(bool ok, const QString& error)
{
    m_payment_pending = false;
    Q_EMIT statusChanged();
    Q_EMIT paymentFinished(ok, error);
}

void CybouDesktopModel::setContacts(QVector<CybouContact> contacts)
{
    m_contacts = std::move(contacts);
    Q_EMIT contactsChanged();
}

QString CybouDesktopModel::supportName()
{
    return QString::fromLatin1(cybou::SUPPORT_NAME_LABEL.data(), cybou::SUPPORT_NAME_LABEL.size()) + QStringLiteral(".cybou");
}

std::optional<quint64> CybouDesktopModel::supportMailFee() const
{
    if (!m_identity_service) return std::nullopt;
    return cybou::SupportMailMinimumFee(m_identity_service->GetNodeRuntime().GetNetworkGenesis().GetProtocolParameters());
}

void CybouDesktopModel::rebuildContacts()
{
    if (m_fixture_mode) return;
    // Mail counterparties retain full routing addresses even without a name.
    QHash<QString, QDateTime> last_seen;
    const auto seen = [&](const QString& raw, const QDateTime& when, const QString& address = QString{}) {
        QString name = raw.trimmed().toLower();
        if (name == m_status.primary_name.toLower() || (!address.isEmpty() && address == m_status.account_id)) return;
        if (!name.endsWith(QStringLiteral(".cybou"))) {
            name = address.toLower();
            if (!QRegularExpression{QStringLiteral("^[0-9a-f]{64}$")}.match(name).hasMatch()) return;
        }
        auto& latest = last_seen[name];
        if (!latest.isValid() || (when.isValid() && when > latest)) latest = when;
    };
    for (const auto& mail : m_mail) {
        if (mail.draft) continue;
        seen(mail.from_name, mail.time, mail.from_address);
        seen(mail.to_name, mail.time, mail.to_address);
    }
    for (const auto& entry : m_wallet_entries) {
        if (entry.kind == CybouWalletEntryKind::Sent || entry.kind == CybouWalletEntryKind::Received) {
            seen(entry.counterparty_name, entry.time);
        }
    }
    QVector<QPair<QString, QDateTime>> ordered;
    for (auto it = last_seen.cbegin(); it != last_seen.cend(); ++it) ordered.append({it.key(), it.value()});
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    QVector<CybouContact> contacts;
    // Everyone can reach support by default, once the authority claimed the name.
    bool support_available = false;
    if (m_identity_service) {
        const auto loaded = m_identity_service->GetNodeRuntime().GetStore().LoadState();
        support_available = loaded && loaded.state && cybou::SupportAccount(*loaded.state).has_value();
    }
    const QString support = supportName();
    if (support_available && m_status.primary_name.toLower() != support) {
        contacts.append({tr("CYBOU Support"), support, true});
    }
    for (const auto& [name, when] : ordered) {
        if (name != support) contacts.append({name.endsWith(QStringLiteral(".cybou")) ? name.chopped(6) : CybouProduct::shortId(name), name, true});
    }
    if (contacts.size() == m_contacts.size() && std::equal(contacts.begin(), contacts.end(), m_contacts.begin(),
            [](const CybouContact& a, const CybouContact& b) { return a.name == b.name; })) return;
    m_contacts = std::move(contacts);
    Q_EMIT contactsChanged();
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
    if (!m_identity_service && m_availability.account_creation) {
        m_availability.account_creation = false;
        m_requested_availability.account_creation = false;
        Q_EMIT featureAvailabilityChanged();
    }
    if (m_identity_service) {
        if (const auto path = m_identity_service->GetStoragePath()) {
            m_name_service = std::make_unique<cybou::CybouNameService>(
                m_identity_service->GetNodeRuntime(), m_identity_service->GetKeyStore(), *path);
        }
        m_availability.account_creation = true;
        m_requested_availability.account_creation = true;
        Q_EMIT featureAvailabilityChanged();

        if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE &&
            m_identity_service->GetAccountId().has_value()) {
            // A vault exists but is not opened in this session yet.
            setIdentityState(CybouIdentityState::Locked,
                QString::fromStdString(m_identity_service->GetAccountId()->Value().GetHex()));
        } else if (hasLocalVault() && m_status.identity_state == CybouIdentityState::None) {
            // An encrypted vault on disk is a returning user: ask for the
            // password (Unlock), never offer to create a second Identity.
            setIdentityState(CybouIdentityState::Locked);
        }
        refreshFinalizedName();
    }
}

void CybouDesktopModel::setWalletService(cybou::CybouWalletService* wallet_service)
{
    m_wallet_service = wallet_service;
    const bool payments = m_wallet_service && m_status.identity_state == CybouIdentityState::Active;
    if (m_availability.payments != payments) {
        m_availability.payments = payments;
        m_requested_availability.payments = payments;
        Q_EMIT featureAvailabilityChanged();
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
        cybou::crypto::CleanseMemory(password.data(), password.size());
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
    if (!resume_pending) {
        // Content published under the current keys must stay readable with the
        // new phrase: the backend secures that first, or the rotation stops here.
        if (!m_backend) {
            for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
            m_recovery_rotation_pending = false;
            Q_EMIT statusChanged();
            Q_EMIT recoveryRotationFinished(CybouOperationOutcome::Failed,
                tr("Your data cannot be secured for a new recovery phrase right now."));
            return true;
        }
        notify(tr("Securing your data for the new recovery phrase. Keep CYBOU open and unlocked."));
        auto phrase_words = std::make_shared<cybou::RecoveryWords>(std::move(words));
        m_backend->prepareIdentityRotation(new_phrase,
            [this, phrase_words, password = vault_password](bool ok, const QString& error) {
                if (!ok) {
                    for (auto& word : *phrase_words) cybou::crypto::CleanseMemory(word.data(), word.size());
                    m_recovery_rotation_pending = false;
                    Q_EMIT statusChanged();
                    Q_EMIT recoveryRotationFinished(CybouOperationOutcome::Failed, error);
                    return;
                }
                startRecoveryRotation(std::move(*phrase_words), password, false);
            });
        return true;
    }
    startRecoveryRotation(std::move(words), vault_password, true);
    return true;
}

void CybouDesktopModel::startRecoveryRotation(cybou::RecoveryWords words, const QString& vault_password,
    bool resume_pending)
{
    if (m_recovery_rotation_worker.joinable()) m_recovery_rotation_worker.join();
    if (!m_identity_service) {
        for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
        m_recovery_rotation_pending = false;
        Q_EMIT statusChanged();
        Q_EMIT recoveryRotationFinished(CybouOperationOutcome::Failed, {});
        return;
    }
    auto password = vault_password.toStdString();
    m_recovery_rotation_worker = std::jthread([this, words = std::move(words), password = std::move(password), resume_pending]() mutable {
        auto result = resume_pending
            ? m_identity_service->ResumeIdentityRotationSync(password)
            : m_identity_service->RotateIdentitySync(words, password);
        cybou::crypto::CleanseMemory(password.data(), password.size());
        for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
        const auto outcome = result.phase == cybou::IdentityOperationPhase::FINALIZED ? CybouOperationOutcome::Finalized
            : result.phase == cybou::IdentityOperationPhase::ACCEPTED ||
                result.phase == cybou::IdentityOperationPhase::UNCERTAIN ? CybouOperationOutcome::Pending
            : CybouOperationOutcome::Failed;
        const auto error = QString::fromStdString(result.error);
        QMetaObject::invokeMethod(this, [this, outcome, error] {
            m_recovery_rotation_pending = false;
            // New key material: reopen everything encrypted under the old keys.
            if (outcome == CybouOperationOutcome::Finalized && m_backend) m_backend->identityKeysChanged();
            Q_EMIT statusChanged();
            Q_EMIT recoveryRotationFinished(outcome, error);
        }, Qt::QueuedConnection);
    });
}

void CybouDesktopModel::finishUnlock()
{
    const auto account_id = m_identity_service->GetAccountId();
    if (m_identity_service->GetPhase() == cybou::IdentityCreationPhase::ACTIVE && account_id) {
        const auto state = m_identity_service->GetFinalizedAccountState();
        setIdentityState(CybouIdentityState::Active,
            QString::fromStdString(account_id->Value().GetHex()),
            state ? state->creation_height : 0);
        if (state) setBalances(state->balance, state->system_balance);
    }
    Q_EMIT statusChanged();
}

void CybouDesktopModel::requestUnlockIdentityAsync(const QString& vault_password, std::function<void(bool)> done)
{
    if (m_fixture_mode) {
        const bool ok = !vault_password.isEmpty();
        QMetaObject::invokeMethod(this, [this, ok, done = std::move(done)] {
            if (ok && m_status.identity_state == CybouIdentityState::Locked)
                setIdentityState(CybouIdentityState::Active, m_status.account_id, m_status.creation_height);
            if (done) done(ok);
        }, Qt::QueuedConnection);
        return;
    }
    if (!m_identity_service) {
        QMetaObject::invokeMethod(this, [done = std::move(done)] { if (done) done(false); }, Qt::QueuedConnection);
        return;
    }
    if (m_vault_worker.joinable()) m_vault_worker.join();
    m_vault_worker = std::jthread([this, password = vault_password.toStdString(), done = std::move(done)]() mutable {
        const bool ok = m_identity_service->LoadVault(password);
        cybou::crypto::CleanseMemory(password.data(), password.size());
        QMetaObject::invokeMethod(this, [this, ok, done = std::move(done)] {
            if (ok) finishUnlock();
            if (done) done(ok);
        }, Qt::QueuedConnection);
    });
}

void CybouDesktopModel::revealRecoveryWordsAsync(const QString& vault_password,
    std::function<void(std::optional<QStringList>)> done)
{
    requestUnlockIdentityAsync(vault_password, [this, done = std::move(done)](bool ok) {
        if (!ok) {
            done(std::nullopt);
            return;
        }
        if (m_fixture_mode) {
            done(FixtureWords());
            return;
        }
        const auto words = m_identity_service->GetKeyStore().GetRecoveryWords();
        done(words ? std::optional<QStringList>{ToQStringList(*words)} : std::nullopt);
    });
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

const QStringList& CybouDesktopModel::recoveryWordList()
{
    static const QStringList words = [] {
        static constexpr const char* kWords[] = {
#include <cybou/bip39_english.inc>
        };
        QStringList list;
        list.reserve(static_cast<qsizetype>(std::size(kWords)));
        for (const char* word : kWords) list << QString::fromLatin1(word);
        return list;
    }();
    return words;
}

bool CybouDesktopModel::isRecoveryWord(const QString& word)
{
    const auto& words = recoveryWordList();
    return std::binary_search(words.begin(), words.end(), word.trimmed().toLower());
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
    if (entropy) cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
    for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
    return valid;
}

std::optional<QStringList> CybouDesktopModel::generateRotationWords()
{
    if (m_fixture_mode) return FixtureWords();
    auto entropy = cybou::GenerateRecoveryEntropy();
    if (!entropy) return std::nullopt;
    auto words = cybou::EncodeRecoveryWords(*entropy);
    cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
    QStringList list = ToQStringList(words);
    for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
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
    case E::PROTECTED_NAME: return tr("This name is reserved.");
    }
    return tr("This name is not valid.");
}

QString CybouDesktopModel::recipientNameProblem(const QString& label) const
{
    // Reservation restricts claiming a name, not addressing its existing owner.
    if (cybou::ValidateNameLabel(label.toStdString()) == cybou::NameValidationError::PROTECTED_NAME) return {};
    return nameLabelProblem(label);
}

void CybouDesktopModel::setNameClaimFinished()
{
    m_status.name_claim_pending = false;
    m_status.name_claim_status.clear();
    Q_EMIT statusChanged();
}

void CybouDesktopModel::requestLockVault()
{
    if (m_status.identity_state != CybouIdentityState::Active || m_vault_locking) return;
    if (m_fixture_mode || !m_identity_service) {
        setIdentityState(CybouIdentityState::Locked, m_status.account_id, m_status.creation_height);
        return;
    }
    m_vault_locking = true;
    Q_EMIT lockVaultRequested();
}

bool CybouDesktopModel::beginVaultLock()
{
    if (!m_vault_locking || m_status.identity_state != CybouIdentityState::Active) return false;
    // Closing the backend joins its worker and clears the private Mail/Files
    // projection before the keystore is erased.
    setIdentityState(CybouIdentityState::Locked, m_status.account_id, m_status.creation_height);
    if (m_name_service) m_name_service->Cancel();
    if (m_name_worker.joinable()) m_name_worker.join();
    if (m_recovery_rotation_worker.joinable()) m_recovery_rotation_worker.join();
    if (m_payment_worker.joinable()) m_payment_worker.join();
    if (m_vault_worker.joinable()) m_vault_worker.join();
    m_recovery_rotation_pending = false;
    return true;
}

void CybouDesktopModel::completeVaultLock()
{
    m_status.primary_name.clear();
    m_status.balance = 0;
    m_status.system_balance = 0;
    m_status.storage_used = 0;
    m_status.storage_quota = 0;
    m_names.clear();
    m_contacts.clear();
    m_activity.clear();
    m_extra_activity.clear();
    m_wallet_entries.clear();
    m_operations.clear();
    m_network_authority = {};
    m_payment_fee.reset();
    m_payment_pending = false;
    m_recovery_rotation_pending = false;
    m_vault_locking = false;
    m_availability.payments = false;
    m_requested_availability.payments = false;
    Q_EMIT statusChanged();
    Q_EMIT namesChanged();
    Q_EMIT contactsChanged();
    Q_EMIT activityChanged();
    Q_EMIT walletChanged();
    Q_EMIT networkAuthorityChanged();
    Q_EMIT featureAvailabilityChanged();
}

namespace {
/** Creation and restore wait this long for PoA finality before reporting a timeout. */
constexpr std::chrono::minutes kIdentityFinalityTimeout{3};

QString PhaseText(cybou::IdentityCreationPhase phase)
{
    switch (phase) {
    case cybou::IdentityCreationPhase::CREATING_KEYS: return CybouDesktopModel::tr("Deriving your keys and checking the network…");
    case cybou::IdentityCreationPhase::PERFORMING_WORK: return CybouDesktopModel::tr("Computing the anti-spam proof-of-work…");
    case cybou::IdentityCreationPhase::BROADCASTING: return CybouDesktopModel::tr("Sending your Identity to the network…");
    case cybou::IdentityCreationPhase::WAITING_FOR_FINALITY: return CybouDesktopModel::tr("Waiting for the network to confirm it…");
    default: return {};
    }
}

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
                m_status.identity_progress = PhaseText(phase);
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
        }, kIdentityFinalityTimeout);
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
                // The completion reports failures with their reason.
                if (phase == cybou::IdentityCreationPhase::FAILED || phase == cybou::IdentityCreationPhase::ACTIVE) return;
                m_status.identity_progress = PhaseText(phase);
                // A step change also re-evaluates the local PoA signer (genesis Identity claim).
                setIdentityStep(StepForPhase(phase));
                Q_EMIT statusChanged();
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
                    Q_EMIT identityRestoreFailed(QString::fromStdString(result.error_message));
                }
            }, Qt::QueuedConnection);
        }, kIdentityFinalityTimeout);
    return true;
}

QString CybouDesktopModel::requestFileUpload(const QString& source_path, const QString& parent_id)
{
    if (!filesReady() || !QFileInfo{source_path}.isFile()) return {};
    const QString id = NewLocalId("file");
    m_backend->uploadFile(id, source_path, parent_id);
    return id;
}

void CybouDesktopModel::requestFileDownload(const QString& file_id, const QString& destination)
{
    if (!m_backend || m_status.identity_state != CybouIdentityState::Active || !fileItem(file_id)) return;
    setFileRetrieval(file_id, CybouRetrievalState::Downloading);
    m_backend->downloadFile(file_id, destination);
}

const CybouFileItem* CybouDesktopModel::fileItem(const QString& id) const
{
    for (const auto& item : m_files) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

QString CybouDesktopModel::requestCreateFolder(const QString& name, const QString& parent_id)
{
    if (!filesReady() || name.trimmed().isEmpty()) return {};
    const QString id = NewLocalId("folder");
    m_backend->createFolder(id, name.trimmed(), parent_id);
    return id;
}

void CybouDesktopModel::requestRenameFile(const QString& id, const QString& name)
{
    const auto* item = fileItem(id);
    if (!filesReady() || !item || name.trimmed().isEmpty() || item->name == name.trimmed()) return;
    m_backend->renameFile(id, name.trimmed());
}

void CybouDesktopModel::requestMoveFile(const QString& id, const QString& parent_id)
{
    const auto* item = fileItem(id);
    if (!filesReady() || !item || id == parent_id || item->parent_id == parent_id) return;
    m_backend->moveFile(id, parent_id);
}

QString CybouDesktopModel::requestCopyFile(const QString& id, const QString& parent_id)
{
    const auto* item = fileItem(id);
    if (!filesReady() || !item || item->folder) return {};
    const QString copy_id = NewLocalId("copy");
    m_backend->copyFile(id, copy_id, parent_id);
    return copy_id;
}

void CybouDesktopModel::requestFileStarred(const QString& id, bool starred)
{
    const auto* item = fileItem(id);
    if (filesReady() && item && item->starred != starred) m_backend->setFileStarred(id, starred);
}

void CybouDesktopModel::requestTrashFile(const QString& id)
{
    if (filesReady() && fileItem(id)) m_backend->trashFile(id);
}

void CybouDesktopModel::requestRestoreFile(const QString& id)
{
    if (filesReady() && fileItem(id)) m_backend->restoreFile(id);
}

void CybouDesktopModel::requestDeleteMailForever(const QStringList& ids)
{
    QStringList trashed;
    for (const auto& id : ids) {
        const auto* item = mailItem(id);
        if (item && item->folder == CybouMailFolder::Trash && !item->draft) trashed << id;
    }
    if (trashed.isEmpty()) return;
    if (m_fixture_mode || !m_backend) {
        for (const auto& id : trashed) removeMailItem(id);
        return;
    }
    m_backend->deleteMailForever(trashed);
}

void CybouDesktopModel::requestDeleteFile(const QString& id)
{
    if (filesReady() && fileItem(id)) m_backend->deleteFile(id);
}

void CybouDesktopModel::requestEmptyTrash()
{
    if (!filesReady()) return;
    // Only top-level trashed items: deleting a folder takes its contents with it.
    QStringList ids;
    for (const auto& file : m_files) {
        if (!file.trashed) continue;
        const auto* parent = file.parent_id.isEmpty() ? nullptr : fileItem(file.parent_id);
        if (!parent || !parent->trashed) ids << file.id;
    }
    if (!ids.isEmpty()) m_backend->deleteFiles(ids);
}

void CybouDesktopModel::requestRetryFile(const QString& id)
{
    if (filesReady() && fileItem(id)) m_backend->retryFile(id);
}

void CybouDesktopModel::requestDiscardFile(const QString& id)
{
    if (filesReady() && fileItem(id)) m_backend->discardFile(id);
}

QString CybouDesktopModel::requestSaveAttachmentToFiles(const QString& message_id, const QString& attachment_id)
{
    if (!filesReady() || !m_backend->mailAvailable()) return {};
    const auto* message = mailItem(message_id);
    if (!message) return {};
    for (const auto& attachment : message->attachments) {
        if (attachment.id != attachment_id) continue;
        if (!CybouProduct::contentOnNetwork(attachment.state)) return {};
        if (!attachment.saved_file_id.isEmpty() && fileItem(attachment.saved_file_id)) return attachment.saved_file_id;
        const QString id = NewLocalId("saved");
        const QString name = attachment.name;
        m_backend->saveAttachmentToFiles(message_id, attachment_id, id);
        if (fileItem(id)) {
            addActivity({CybouActivityKind::FileUploaded, tr("%1 saved to Files").arg(name), tr("From Mail"),
                QDateTime::currentDateTime()});
        }
        return id;
    }
    return {};
}

namespace {
template <typename F>
bool MutateFile(QVector<CybouFileItem>& files, const QString& id, F mutate)
{
    for (auto& item : files) {
        if (item.id == id) {
            mutate(item);
            return true;
        }
    }
    return false;
}
} // namespace

std::optional<CybouAttachmentItem> CybouDesktopModel::attachmentFromFile(const QString& file_id) const
{
    const auto* file = fileItem(file_id);
    if (!file || file->folder || file->trashed || file->state != CybouContentState::Protected) return std::nullopt;
    CybouAttachmentItem attachment;
    attachment.id = QStringLiteral("ref-") + file->id;
    attachment.name = file->name;
    attachment.logical_size = file->logical_size;
    // Existing protected content: the Mail root only references it.
    attachment.state = CybouContentState::Protected;
    return attachment;
}

void CybouDesktopModel::setFileState(const QString& id, CybouContentState state, int progress_percent)
{
    if (MutateFile(m_files, id, [&](CybouFileItem& item) {
            item.state = state;
            item.progress_percent = progress_percent;
            // Storage durability exists only after PoA finality.
            if (state == CybouContentState::Securing || state == CybouContentState::Protected) {
                item.operation_state = CybouOperationState::Finalized;
            }
        }))
        Q_EMIT filesChanged();
}

void CybouDesktopModel::setFileRetrieval(const QString& id, CybouRetrievalState retrieval)
{
    if (MutateFile(m_files, id, [&](CybouFileItem& item) { item.retrieval = retrieval; })) Q_EMIT filesChanged();
}

QString CybouDesktopModel::rememberedName() const
{
    return QSettings{}.value(RememberedNameKey(m_status.data_directory)).toString();
}

void CybouDesktopModel::refreshFinalizedName()
{
    if (m_fixture_mode) return;
    const auto name = m_identity_service && m_status.identity_state == CybouIdentityState::Active
        ? m_identity_service->GetFinalizedPrimaryName() : std::nullopt;
    const QString finalized = name ? QString::fromStdString(*name) + QStringLiteral(".cybou") : QString{};
    if (m_status.primary_name == finalized) return;
    m_status.primary_name = finalized;
    // Names are public: remember the last one so the unlock screen can greet
    // a returning user before the vault is open.
    if (!finalized.isEmpty()) QSettings{}.setValue(RememberedNameKey(m_status.data_directory), finalized);
    QVector<CybouNameItem> names;
    if (!finalized.isEmpty()) names.append({finalized, true});
    m_names = names;
    Q_EMIT namesChanged();
    Q_EMIT statusChanged();
}
