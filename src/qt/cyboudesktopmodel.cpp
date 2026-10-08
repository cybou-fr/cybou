// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboudesktopmodel.h>

#include <qt/cybouactivity.h>
#include <qt/cybouapplicationbackend.h>

#include <cybou/identity_operation_coordinator.h>
#include <cybou/identity_service.h>
#include <cybou/recovery_phrase.h>
#include <cybou/name_registry.h>
#include <cybou/name_service.h>
#include <cybou/block.h>
#include <cybou/protocol_operation.h>
#include <cybou/node_runtime.h>
#include <cybou/hex.h>
#include <cybou/publication_service.h>
#include <cybou/protocol_limits.h>
#include <cybou/storage_economy.h>
#include <cybou/support_mail.h>
#include <cybou/identity_signer.h>
#include <cybou/wallet_service.h>

#include <cybou/crypto/cleanse.h>

#include <QCryptographicHash>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QLocale>
#include <QTimer>
#include <QUuid>
#include <QSettings>
#include <QRegularExpression>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace {
void NormalizeActivityIds(QVector<CybouActivityItem>& items)
{
    QHash<QString, int> occurrences;
    for (auto& item : items) {
        if (item.id.isEmpty()) {
            const auto digest = [](const QString& text) {
                return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
            };
            item.id = QStringLiteral("event:%1:%2:%3:%4").arg(static_cast<int>(item.kind))
                .arg(item.time.toMSecsSinceEpoch()).arg(digest(item.title), digest(item.subtitle));
        }
        const auto occurrence = occurrences[item.id]++;
        if (occurrence) item.id += QStringLiteral(":%1").arg(occurrence);
    }
}

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
    if (status.finality_stall_minutes > 0)
        return CybouDesktopModel::tr("Network not confirming for %n min", nullptr, status.finality_stall_minutes);
    if (status.syncing) {
        // Peers' announced heights are unverified; they only estimate how far there is to go.
        if (status.finality_known && status.sync_target_height > status.finalized_height + 10) {
            const auto percent = static_cast<int>(status.finalized_height * 100 / status.sync_target_height);
            return CybouDesktopModel::tr("Syncing %1% (%2 blocks left)").arg(percent)
                .arg(QLocale{}.toString(status.sync_target_height - status.finalized_height));
        }
        return CybouDesktopModel::tr("Syncing");
    }
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
    auto* finality_watch = new QTimer{this};
    connect(finality_watch, &QTimer::timeout, this, &CybouDesktopModel::updateFinalityStall);
    finality_watch->start(5000);
    connect(this, &CybouDesktopModel::filesChanged, this, &CybouDesktopModel::refreshStorageUsed);
    connect(this, &CybouDesktopModel::filesChanged, this, &CybouDesktopModel::scheduleRebuildActivity);
    connect(this, &CybouDesktopModel::mailChanged, this, &CybouDesktopModel::scheduleRebuildActivity);
    connect(this, &CybouDesktopModel::walletChanged, this, &CybouDesktopModel::scheduleRebuildActivity);
    connect(this, &CybouDesktopModel::mailChanged, this, &CybouDesktopModel::scheduleRebuildContacts);
    connect(this, &CybouDesktopModel::walletChanged, this, &CybouDesktopModel::scheduleRebuildContacts);
    // Yourself never appears; the own name may be learned after mail loaded.
    connect(this, &CybouDesktopModel::namesChanged, this, &CybouDesktopModel::scheduleRebuildContacts);
}

void CybouDesktopModel::scheduleRebuildActivity()
{
    if (m_activity_update_scheduled) return;
    m_activity_update_scheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_activity_update_scheduled = false;
        rebuildActivity();
    });
}

void CybouDesktopModel::scheduleRebuildContacts()
{
    if (m_contacts_update_scheduled) return;
    m_contacts_update_scheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_contacts_update_scheduled = false;
        rebuildContacts();
    });
}

void CybouDesktopModel::rebuildActivity()
{
    if (fixtureMode()) return;
    auto items = CybouBuildActivityItems(*this, m_extra_activity);
    if (items == m_activity) return;
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
    m_last_finality_change = QDateTime::currentDateTimeUtc();
    m_status.finality_stall_minutes = 0;
    Q_EMIT statusChanged();
}

bool CybouDesktopModel::hasUnconfirmedWork() const
{
    if (m_payment_pending) return true;
    const auto submitted = [](CybouOperationState state) { return state == CybouOperationState::Submitted; };
    // The displayed state merges canonical finality: a finalized operation never counts as waiting.
    for (const auto& item : m_mail) if (submitted(displayedOperationState(item.operation_id, item.operation_state))) return true;
    for (const auto& item : m_files) if (submitted(displayedOperationState(item.operation_id, item.operation_state))) return true;
    for (const auto& entry : m_wallet_entries) if (submitted(displayedOperationState(entry.operation_id, entry.operation_state))) return true;
    for (const auto& operation : m_operations) if (submitted(operation.state)) return true;
    return false;
}

void CybouDesktopModel::updateFinalityStall()
{
    checkFinalityStall(QDateTime::currentDateTimeUtc());
}

void CybouDesktopModel::checkFinalityStall(const QDateTime& now)
{
    // Blocks are produced only for pending operations, so an idle network is quiet:
    // only own work waiting without any new block means the network is not confirming.
    int minutes{0};
    if (m_status.online && !m_status.syncing && hasUnconfirmedWork()) {
        if (!m_unconfirmed_since.isValid()) m_unconfirmed_since = now;
        auto since = m_unconfirmed_since;
        if (m_last_finality_change.isValid() && m_last_finality_change > since) since = m_last_finality_change;
        const auto waited = since.secsTo(now);
        if (waited >= 120) minutes = static_cast<int>(waited / 60);
    } else {
        m_unconfirmed_since = {};
    }
    if (minutes == m_status.finality_stall_minutes) return;
    m_status.finality_stall_minutes = minutes;
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

void CybouDesktopModel::setApplicationLoad(CybouApplicationLoadState state, quint64 scanned, quint64 total, const QString& error)
{
    if (state == m_application_load_state && scanned == m_application_load_scanned &&
        total == m_application_load_total && error == m_application_load_error) return;
    m_application_load_state = state;
    m_application_load_scanned = scanned;
    m_application_load_total = total;
    m_application_load_error = error;
    Q_EMIT applicationLoadChanged();
}

void CybouDesktopModel::syncIdentitySession()
{
    const auto state = m_status.identity_state;
    const bool open = state == CybouIdentityState::Active || state == CybouIdentityState::Syncing ||
        state == CybouIdentityState::NeedsAttention;
    if (open == m_session_open) return;
    m_session_open = open;
    setApplicationLoad(open && m_backend ? CybouApplicationLoadState::Opening : CybouApplicationLoadState::Closed);
    ++m_mail_generation;
    m_mail_tasks.clear();
    m_mail_ids.clear();
    m_application_refreshing = false;
    m_last_application_refresh = {};
    m_application_refresh_error.clear();
    m_application_refresh_done = {};
    Q_EMIT applicationRefreshChanged();
    Q_EMIT mailTasksChanged();
    if (!open) {
        // Private semantic data never outlives the unlocked Identity.
        const bool had_mail = !m_mail.isEmpty();
        const bool had_files = !m_files.isEmpty();
        const bool had_activity = !m_activity.isEmpty();
        m_mail.clear();
        m_files.clear();
        m_activity.clear();
        m_extra_activity.clear();
        if (had_mail) Q_EMIT mailChanged();
        if (had_files) Q_EMIT filesChanged();
        if (had_activity) Q_EMIT activityChanged();
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
    if (!m_backend) setApplicationLoad(CybouApplicationLoadState::Closed);
    if (m_backend) {
        using B = CybouApplicationBackend;
        connect(m_backend, &B::applicationLoadChanged, this,
            [this](CybouApplicationLoadState state, quint64 scanned, quint64 total, const QString& error) {
                if (m_session_open) setApplicationLoad(state, scanned, total, error);
            });
        connect(m_backend, &B::availabilityChanged, this, [this] {
            setFeatureAvailability(m_requested_availability);
            if (m_application_refreshing && !m_backend->mailAvailable()) {
                const QPointer<CybouDesktopModel> guard{this};
                const auto generation = m_mail_generation;
                m_application_refreshing = false;
                ++m_application_refresh_id;
                m_application_refresh_error = tr("Local refresh was interrupted. Try again.");
                auto done = std::move(m_application_refresh_done);
                Q_EMIT applicationRefreshChanged();
                if (guard && m_mail_generation == generation && m_session_open && done)
                    done(false, m_application_refresh_error);
            }
        });
        connect(m_backend, &B::mailSnapshot, this, [this](const QVector<CybouMailItem>& items) {
            if (m_session_open) setMailItems(items);
        });
        connect(m_backend, &B::mailItemChanged, this, [this](const CybouMailItem& item) {
            if (m_session_open) upsertMailItem(item);
        });
        connect(m_backend, &B::mailItemRemoved, this, &CybouDesktopModel::removeMailItem);
        connect(m_backend, &B::mailItemReplaced, this, [this](const QString& old_id, const QString& new_id) {
            if (!m_session_open || old_id == new_id) return;
            m_mail_ids.insert(old_id, new_id);
            for (auto& task : m_mail_tasks) if (task.related_id == old_id) task.related_id = new_id;
            bool changed{false};
            for (auto it = m_mail.begin(); it != m_mail.end(); ++it) {
                if (it->id != old_id) continue;
                if (mailItem(new_id)) m_mail.erase(it);
                else it->id = new_id;
                changed = true;
                break;
            }
            Q_EMIT mailIdReplaced(old_id, new_id);
            if (changed) Q_EMIT mailChanged();
        });
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
        if (m_session_open) {
            setApplicationLoad(CybouApplicationLoadState::Opening);
            m_backend->openIdentity();
        }
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
    if (items == m_mail) return;
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

bool CybouDesktopModel::requestApplicationRefresh(CommandDone done)
{
    if (m_application_refreshing || !mailReady()) return false;
    const auto generation = m_mail_generation;
    const auto request_id = ++m_application_refresh_id;
    const QPointer<CybouDesktopModel> guard{this};
    m_application_refreshing = true;
    m_application_refresh_error.clear();
    m_application_refresh_done = std::move(done);
    Q_EMIT applicationRefreshChanged();
    if (!guard || m_mail_generation != generation || !m_session_open) return false;
    m_backend->refreshProjection([guard, generation, request_id](CybouCommandState state, const QString& error) {
        if (!guard) return;
        QTimer::singleShot(0, guard, [guard, generation, request_id, state, error] {
            if (!guard || guard->m_mail_generation != generation || !guard->m_session_open ||
                guard->m_application_refresh_id != request_id || !guard->m_application_refreshing) return;
            if (state != CybouCommandState::Committed && state != CybouCommandState::Failed) return;
            const bool ok = state == CybouCommandState::Committed;
            guard->m_application_refreshing = false;
            guard->m_application_refresh_error = error;
            auto done = std::move(guard->m_application_refresh_done);
            if (ok) {
                guard->m_last_application_refresh = QDateTime::currentDateTime();
                guard->rebuildActivity();
            }
            if (!guard || guard->m_mail_generation != generation || !guard->m_session_open) return;
            Q_EMIT guard->applicationRefreshChanged();
            if (!guard || guard->m_mail_generation != generation || !guard->m_session_open) return;
            if (done) done(ok, error);
        });
    });
    return true;
}

void CybouDesktopModel::requestMoveMail(const QString& id, CybouMailFolder folder, CommandDone done)
{
    const auto* item = mailItem(id);
    if (!mailReady() || !item) {
        if (done) QTimer::singleShot(0, this, [done] { done(false, tr("Mail is unavailable.")); });
        return;
    }
    // Do not skip an apparent no-op: an earlier queued move may still commit.
    const QString title = folder == CybouMailFolder::Archive ? tr("Archiving message")
        : folder == CybouMailFolder::Trash ? tr("Moving message to Trash") : tr("Moving message");
    const QPointer<CybouDesktopModel> guard{this};
    m_backend->moveMail(id, folder, mailCommand(id, title, [guard, id, folder, done](bool ok, const QString& error) {
        if (guard && ok) {
            if (const auto* current = guard->mailItem(id)) {
                auto changed = *current;
                changed.folder = folder;
                guard->upsertMailItem(changed);
            }
        }
        if (done) done(ok, error);
        else if (!ok && guard) guard->notify(error);
    }, CybouMailTaskKind::Move));
}

namespace {
QString NewLocalId(const char* prefix)
{
    return QStringLiteral("%1-%2").arg(QLatin1String{prefix}, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

QString NewFileId()
{
    const auto id = cybou::NewPrivateItemId();
    return id ? QString::fromStdString(cybou::HexEncode(*id)) : QString{};
}

QString PreviewOf(const QString& body)
{
    return body.simplified().left(90);
}
} // namespace

std::function<void(CybouCommandState, const QString&)> CybouDesktopModel::mailCommand(
    const QString& item_id, const QString& title, CommandDone done, CybouMailTaskKind kind, const QString& related_id)
{
    const QString command_id = NewLocalId("command");
    const auto generation = m_mail_generation;
    m_mail_tasks.removeIf([&](const auto& task) { return task.item_id == item_id && task.title == title &&
        (task.state == CybouCommandState::Committed || task.state == CybouCommandState::Failed); });
    m_mail_tasks.append({command_id, item_id, title, CybouCommandState::Queued, {}, QDateTime::currentDateTime(), kind, related_id});
    Q_EMIT mailTasksChanged();
    const QPointer<CybouDesktopModel> guard{this};
    return [guard, generation, command_id, done = std::move(done)](CybouCommandState state, const QString& error) {
        if (!guard) return;
        // Queued delivery also makes synchronous fixtures behave like the worker.
        QTimer::singleShot(0, guard, [guard, generation, command_id, state, error, done] {
            if (!guard || guard->m_mail_generation != generation || !guard->m_session_open) return;
            auto& tasks = guard->m_mail_tasks;
            const auto it = std::find_if(tasks.begin(), tasks.end(), [&](const auto& task) { return task.id == command_id; });
            if (it == tasks.end() || it->state == CybouCommandState::Committed || it->state == CybouCommandState::Failed) return;
            it->state = state;
            it->error = error;
            Q_EMIT guard->mailTasksChanged();
            // A listener may lock the Identity or destroy the model reentrantly.
            if (!guard || guard->m_mail_generation != generation || !guard->m_session_open) return;
            if (state == CybouCommandState::Committed || state == CybouCommandState::Failed) {
                if (done) done(state == CybouCommandState::Committed, error);
                else if (state == CybouCommandState::Failed) guard->notify(error);
                if (!guard || guard->m_mail_generation != generation) return;
                // Bound completed/failed task retention without dropping active work.
                int terminal = 0;
                for (auto i = tasks.end(); i != tasks.begin();) {
                    --i;
                    if ((i->state == CybouCommandState::Committed || i->state == CybouCommandState::Failed) && ++terminal > 32) i = tasks.erase(i);
                }
            }
        });
    };
}

QString CybouDesktopModel::requestSaveMailDraft(CybouMailItem draft, CommandDone done)
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
    m_backend->saveMailDraft(draft, mailCommand(draft.id, tr("Saving draft"), std::move(done)));
    return draft.id;
}

void CybouDesktopModel::requestDeleteMail(const QString& id)
{
    if (mailReady() && mailItem(id)) m_backend->deleteMail(id);
}

QString CybouDesktopModel::requestSendMail(CybouMailItem message, CommandDone done)
{
    if (!mailReady()) return {};
    // The backend saves the payload before preparing a publication.
    const QString draft_id = message.id.startsWith(QStringLiteral("draft-")) ? message.id : NewLocalId("draft");
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
    m_backend->sendMail(message, draft_id, mailCommand(draft_id, tr("Preparing message"), std::move(done), CybouMailTaskKind::Send, message.id));
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
    if (items == m_files) return;
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
    NormalizeActivityIds(items);
    if (items == m_activity) return;
    m_activity = std::move(items);
    Q_EMIT activityChanged();
}

void CybouDesktopModel::addActivity(const CybouActivityItem& item)
{
    auto event = item;
    if (event.id.isEmpty()) event.id = NewLocalId("event");
    if (fixtureMode()) {
        m_activity.prepend(event);
        Q_EMIT activityChanged();
        return;
    }
    m_extra_activity.prepend(event);
    if (m_extra_activity.size() > 20) m_extra_activity.resize(20);
    rebuildActivity();
}

void CybouDesktopModel::setWalletEntries(QVector<CybouWalletEntry> entries)
{
    // Counterparties arrive as AccountIDs; show their finalized .cybou name when they have one.
    if (m_identity_service && !m_fixture_mode) {
        cybou::StateSnapshotResult loaded;
        for (auto& entry : entries) {
            if (entry.counterparty_name.size() != 64) continue;
            const auto raw = cybou::ParseHash256UserHex(entry.counterparty_name.toStdString());
            if (!raw) continue;
            // Hold the shared snapshot: copying the whole state on the GUI thread stalls it.
            if (!loaded) {
                loaded = m_identity_service->GetNodeRuntime().GetStore().GetStateSnapshot();
                if (!loaded) break;
            }
            if (const auto* name = loaded.state->names.PrimaryName(cybou::AccountId{*raw})) {
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
    // Announced heights are unverified: the median of connected peers resists one peer
    // claiming an absurd height, where the maximum would follow it.
    std::vector<quint64> heights;
    for (const auto& peer : m_network_diagnostics.peers) heights.push_back(peer.advertised_height);
    std::sort(heights.begin(), heights.end());
    m_status.sync_target_height = heights.empty() ? 0 : heights[heights.size() / 2];
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
    if (!m_network_authority.proven || !m_network_authority.signer_enabled || m_status.identity_state != CybouIdentityState::Active ||
        (m_network_authority.finalizer != CybouFinalizerState::Finalizing && m_network_authority.finalizer != CybouFinalizerState::Paused)) return;
    Q_EMIT finalizationPauseRequested(paused);
}

void CybouDesktopModel::requestFinalizeNow()
{
    if (!m_network_authority.proven || !m_network_authority.signer_enabled || m_status.identity_state != CybouIdentityState::Active ||
        m_network_authority.finalizer != CybouFinalizerState::Paused) return;
    Q_EMIT finalizeNowRequested();
}

void CybouDesktopModel::requestStorageSettlement()
{
    if (!m_network_authority.proven || !m_network_authority.signer_enabled || !m_network_authority.settlement_due ||
        m_status.identity_state != CybouIdentityState::Active || m_network_authority.finalizer == CybouFinalizerState::SafetyHalt) return;
    Q_EMIT storageSettlementRequested();
}

void CybouDesktopModel::setBackgroundFinalizerActive(bool active)
{
    if (m_status.background_finalizer_active == active) return;
    m_status.background_finalizer_active = active;
    Q_EMIT statusChanged();
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
            const auto loaded = m_identity_service->GetNodeRuntime().GetStore().GetStateSnapshot();
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
        case cybou::WalletOperationError::INSUFFICIENT_SYSTEM_BALANCE: error = tr("Not enough Network balance for the network service fee."); break;
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
        default: error = tr("CYBOU could not be moved to Network balance."); break;
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
    if (fixtureMode()) return;
    auto contacts = CybouBuildContacts(*this);
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
    const QString id = NewFileId();
    if (id.isEmpty()) return {};
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
    const QString id = NewFileId();
    if (id.isEmpty()) return {};
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
    const QString copy_id = NewFileId();
    if (copy_id.isEmpty()) return {};
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
        const QString id = NewFileId();
        if (id.isEmpty()) return {};
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

namespace {
QString OperationKindString(const cybou::ProtocolOperation& op)
{
    if (std::holds_alternative<cybou::AccountCreateOp>(op)) return QStringLiteral("AccountCreate");
    if (std::holds_alternative<cybou::AuthorizedPayment>(op)) return QStringLiteral("Payment");
    if (std::holds_alternative<cybou::AuthorizedRootPublication>(op)) return QStringLiteral("RootPublication");
    if (std::holds_alternative<cybou::AuthorizedStorageLease>(op)) return QStringLiteral("StorageLease");
    if (std::holds_alternative<cybou::AuthorizedSystemLock>(op)) return QStringLiteral("SystemLock");
    if (std::holds_alternative<cybou::IdentityRotate>(op)) return QStringLiteral("IdentityRotate");
    if (std::holds_alternative<cybou::AuthorizedNameCommit>(op)) return QStringLiteral("NameCommit");
    if (std::holds_alternative<cybou::AuthorizedNameReveal>(op)) return QStringLiteral("NameReveal");
    if (std::holds_alternative<cybou::AuthorizedRevokePublication>(op)) return QStringLiteral("RevokePublication");
    if (std::holds_alternative<cybou::StorageSettlement>(op)) return QStringLiteral("StorageSettlement");
    return QStringLiteral("Unknown");
}
} // namespace

CybouFileChunkDiagnostics CybouDesktopModel::inspectFileChunks(const QString& file_id) const
{
    if (m_backend) {
        auto diag = m_backend->inspectFileChunks(file_id);
        if (diag.available) return diag;
    }
    auto it = m_fixture_chunk_diagnostics.find(file_id);
    if (it != m_fixture_chunk_diagnostics.end()) return it.value();
    return {};
}

void CybouDesktopModel::setFixtureChunkDiagnostics(const QString& file_id, CybouFileChunkDiagnostics diag)
{
    m_fixture_chunk_diagnostics[file_id] = std::move(diag);
}

CybouBlockExplorerInfo CybouDesktopModel::inspectBlock(const QString& id_or_height) const
{
    auto fit = m_fixture_blocks.find(id_or_height);
    if (fit != m_fixture_blocks.end()) return fit.value();

    CybouBlockExplorerInfo info;
    if (m_node_runtime) {
        bool ok = false;
        quint64 height = id_or_height.toULongLong(&ok);
        std::optional<cybou::FinalizedBlock> block_opt;
        if (ok) {
            block_opt = m_node_runtime->GetBlockAtHeight(height);
        } else if (id_or_height.size() == 64) {
            const auto hash_opt = cybou::Hash256::FromHex(id_or_height.toStdString());
            if (hash_opt) {
                block_opt = m_node_runtime->GetStore().GetBlock(*hash_opt);
            }
        }
        if (block_opt) {
            info.found = true;
            info.height = block_opt->block.height;
            info.block_id = QString::fromStdString(cybou::ComputeBlockId(block_opt->block).GetHex());
            info.parent_block_id = QString::fromStdString(block_opt->block.parent_block_id.GetHex());
            info.state_root = QString::fromStdString(block_opt->block.resulting_state_root.GetHex());
            info.operations_root = QString::fromStdString(cybou::ComputeOperationsRoot(block_opt->block.operations).GetHex());
            info.operation_count = static_cast<int>(block_opt->block.operations.size());
            for (const auto& op : block_opt->block.operations) {
                auto oid = cybou::ComputeOperationId(op);
                if (oid) info.operation_ids.append(QString::fromStdString(oid->GetHex()));
            }
            info.has_poa_certificate = (block_opt->certificate.height == block_opt->block.height);
            return info;
        }
    }

    if (m_network_diagnostics.initialized && m_network_diagnostics.height > 0) {
        if (id_or_height == QString::number(m_network_diagnostics.height) ||
            id_or_height.toStdString() == m_network_diagnostics.tip) {
            info.found = true;
            info.height = m_network_diagnostics.height;
            info.block_id = QString::fromStdString(m_network_diagnostics.tip);
            info.state_root = QString::fromStdString(m_network_diagnostics.state_root);
            info.operation_count = static_cast<int>(m_network_diagnostics.operations.size());
            info.has_poa_certificate = true;
            return info;
        }
    }
    return info;
}

void CybouDesktopModel::setFixtureBlock(const QString& key, CybouBlockExplorerInfo info)
{
    m_fixture_blocks[key] = std::move(info);
}

CybouOperationExplorerInfo CybouDesktopModel::inspectOperation(const QString& op_id) const
{
    auto fit = m_fixture_operations.find(op_id);
    if (fit != m_fixture_operations.end()) return fit.value();

    CybouOperationExplorerInfo info;
    info.operation_id = op_id;
    if (m_node_runtime) {
        const auto hash_opt = cybou::Hash256::FromHex(op_id.toStdString());
        if (hash_opt) {
            if (m_node_runtime->HasCandidateOperation(*hash_opt)) {
                info.found = true;
                info.state = tr("Candidate (volatile pool)");
                return info;
            }
            const auto res = m_node_runtime->FindFinalizedOperation(*hash_opt);
            if (res.status == cybou::FinalizedOperationLookupStatus::FOUND) {
                info.found = true;
                info.state = tr("Finalized");
                info.height = res.height;
                info.index = res.operation_index;
                info.block_id = QString::fromStdString(res.block_id.GetHex());
                const auto blk = m_node_runtime->GetBlockAtHeight(res.height);
                if (blk && res.operation_index < blk->block.operations.size()) {
                    const auto& op = blk->block.operations[res.operation_index];
                    info.kind = OperationKindString(op);
                    if (const auto author = cybou::AuthorizingAccount(op)) {
                        info.author = QString::fromStdString(author->Value().GetHex());
                    }
                }
                return info;
            }
        }
    }

    for (const auto& cid : m_network_authority.candidate_ids) {
        if (cid.compare(op_id, Qt::CaseInsensitive) == 0) {
            info.found = true;
            info.state = tr("Candidate (volatile pool)");
            return info;
        }
    }
    for (const auto& top : m_network_diagnostics.operations) {
        if (QString::fromStdString(top.operation_id).compare(op_id, Qt::CaseInsensitive) == 0) {
            info.found = true;
            info.state = (top.state == static_cast<std::uint32_t>(cybou::OperationStatusKind::FINALIZED))
                ? tr("Finalized") : tr("Submitted");
            info.height = top.finalized_height;
            return info;
        }
    }
    return info;
}

void CybouDesktopModel::setFixtureOperation(const QString& op_id, CybouOperationExplorerInfo info)
{
    m_fixture_operations[op_id] = std::move(info);
}

QVector<CybouHistoryItem> CybouDesktopModel::inspectHistory(int page, int page_size) const
{
    if (!m_fixture_history.isEmpty()) return m_fixture_history;

    page = qMax(1, page);
    page_size = qBound(1, page_size, 100);
    QVector<CybouHistoryItem> items;

    if (m_node_runtime) {
        const quint64 head = m_node_runtime->GetFinalizedHeight().value_or(0);
        const quint64 offset = static_cast<quint64>((page - 1) * page_size);
        if (head >= offset) {
            const quint64 start = head - offset;
            for (quint64 h = start; items.size() < page_size; --h) {
                const auto blk = m_node_runtime->GetBlockAtHeight(h);
                if (blk) {
                    CybouHistoryItem item;
                    item.height = h;
                    item.block_id = QString::fromStdString(cybou::ComputeBlockId(blk->block).GetHex());
                    item.state_root = QString::fromStdString(blk->block.resulting_state_root.GetHex());
                    item.operation_count = static_cast<int>(blk->block.operations.size());
                    items.append(item);
                }
                if (h == 0) break;
            }
        }
    } else {
        if (m_network_diagnostics.initialized && m_network_diagnostics.height > 0) {
            CybouHistoryItem tip_item;
            tip_item.height = m_network_diagnostics.height;
            tip_item.block_id = QString::fromStdString(m_network_diagnostics.tip);
            tip_item.state_root = QString::fromStdString(m_network_diagnostics.state_root);
            tip_item.operation_count = static_cast<int>(m_network_diagnostics.operations.size());
            items.append(tip_item);
        }
        for (const auto& op : m_network_diagnostics.operations) {
            if (items.size() >= page_size) break;
            CybouHistoryItem op_item;
            op_item.height = op.finalized_height;
            op_item.block_id = QString::fromStdString(op.operation_id);
            op_item.summary = tr("Tracked operation (%1)").arg(QString::fromStdString(op.operation_id));
            items.append(op_item);
        }
    }
    return items;
}

void CybouDesktopModel::setFixtureHistory(QVector<CybouHistoryItem> items)
{
    m_fixture_history = std::move(items);
}
