// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboucoreapplicationadapter_internal.h>

#include <cybou/identity_kem.h>
#include <cybou/identity_service.h>

using namespace cybou::qt_detail;

CybouCoreApplicationAdapter::CybouCoreApplicationAdapter(cybou::CybouNodeRuntime& runtime,
    cybou::CybouIdentityService& identity, std::filesystem::path data_directory, QObject* parent)
    : CybouApplicationBackend{parent}, m_runtime{runtime}, m_identity{identity},
      m_data_directory{std::move(data_directory)}
{
}

CybouCoreApplicationAdapter::~CybouCoreApplicationAdapter()
{
    m_session.reset();
}

void CybouCoreApplicationAdapter::setRefreshInterval(int ms)
{
    m_refresh_ms = std::max(10, ms);
}

void CybouCoreApplicationAdapter::openIdentity()
{
    if (m_session) return;
    m_initial_projection_ready = false;
    Q_EMIT applicationLoadChanged(CybouApplicationLoadState::Opening, 0, 0, {});
    const auto account = m_identity.GetAccountId();
    if (!account) {
        Q_EMIT applicationLoadChanged(CybouApplicationLoadState::Failed, 0, 0,
            tr("Your Identity is not available. Return to unlock and try again."));
        return;
    }
    // Separate durable local state and network index/journal per Identity.
    const auto root = cybou::IdentityDataDirectory(m_data_directory, *account);
    ++m_session_generation;
    m_session = std::make_unique<IdentitySession>(this, m_runtime, m_identity.GetKeyStore(), root, m_refresh_ms,
        m_transport_override);
}

void CybouCoreApplicationAdapter::prepareStorageSettlement(const std::uint64_t period,
    const std::int64_t verified_since_ms, std::function<void(cybou::StorageSettlement, QString)> done)
{
    if (!m_session) {
        done({}, tr("Storage settlement preparation requires an open Identity."));
        return;
    }
    m_session->Post([period, verified_since_ms, done = std::move(done)](IdentitySession& s) {
        cybou::StorageSettlement settlement{.period = period,
            .period_start_utc = static_cast<std::uint64_t>(verified_since_ms / 1000)};
        QString error;
        try {
            if (const auto retained = s.storage->PreparedSettlement(period)) settlement = *retained;
            else settlement.entries = s.storage_projection.Settlement(period, verified_since_ms);
        } catch (const std::length_error&) {
            error = tr("Too many storage payouts for one settlement. Nothing was submitted; obligations were not discarded.");
        } catch (const std::exception&) {
            error = tr("Storage settlement preparation failed. Nothing was submitted.");
        }
        done(std::move(settlement), std::move(error));
    });
}

void CybouCoreApplicationAdapter::submitStorageSettlement(const std::uint64_t period, const std::uint64_t start,
    std::vector<cybou::StorageSettlementEntry> entries, std::function<void(bool, QString)> done)
{
    if (!m_session) { done(false, tr("Storage settlement requires an open Identity.")); return; }
    m_session->Post([period, start, entries = std::move(entries), done = std::move(done)](IdentitySession& s) mutable {
        try {
            const auto result = s.storage->SubmitSettlement(period, start, std::move(entries));
            done(static_cast<bool>(result), {});
        } catch (const std::exception&) {
            done(false, tr("Storage settlement could not be saved or replayed. Its journal was retained; nothing was replaced."));
        }
    });
}

void CybouCoreApplicationAdapter::identityKeysChanged()
{
    if (!m_session || m_reopening) return;
    m_reopening = true;
    // Closing drains accepted local commits. Drafts survive in local.db under
    // the preserved data key; do not re-enqueue into an opening session whose
    // local executor has not been constructed yet.
    m_session.reset();
    setReady(false);
    openIdentity();
    m_reopening = false;
}

void CybouCoreApplicationAdapter::closeIdentity()
{
    m_session.reset();
    finishRotation(false, tr("CYBOU was locked before your data was secured. The current recovery phrase stays active."));
    m_pending_drafts.clear();
    m_known_drafts.clear();
    m_deleted_drafts.clear();
    m_deleted_mail.clear();
    m_pending_sends.clear();
    m_send_drafts.clear();
    m_last_mail.clear();
    m_last_files.clear();
    m_pending_files.clear();
    m_pending_stars.clear();
    setReady(false);
}

void CybouCoreApplicationAdapter::setReady(bool ready)
{
    if (m_mail_ready == ready) return;
    m_mail_ready = ready;
    Q_EMIT availabilityChanged();
}

void CybouCoreApplicationAdapter::applySnapshot(QVector<CybouMailItem> items, QVector<CybouFileItem> files,
    bool ready, CybouRestoreStepState restore, bool initial_complete)
{
    if (!m_session) return;
    // Drafts: the latest local edit wins until the worker has stored it; a
    // deleted draft stays hidden until the stored copy is gone.
    QSet<QString> present;
    QSet<QString> present_deleted;
    for (auto it = items.begin(); it != items.end();) {
        if (!it->draft) {
            // Deleted forever: hidden until the worker's copy is gone too.
            if (m_deleted_mail.contains(it->id)) {
                present_deleted.insert(it->id);
                it = items.erase(it);
                continue;
            }
            ++it;
            continue;
        }
        present.insert(it->id);
        if (m_deleted_drafts.contains(it->id)) {
            it = items.erase(it);
            continue;
        }
        if (const auto pending = m_pending_drafts.find(it->id); pending != m_pending_drafts.end()) {
            if (pending->time == it->time) m_pending_drafts.erase(pending);
            else *it = *pending;
        }
        ++it;
    }
    for (auto it = m_deleted_drafts.begin(); it != m_deleted_drafts.end();) {
        it = present.contains(*it) ? std::next(it) : m_deleted_drafts.erase(it);
    }
    for (auto it = m_deleted_mail.begin(); it != m_deleted_mail.end();) {
        it = present_deleted.contains(*it) ? std::next(it) : m_deleted_mail.erase(it);
    }
    for (const auto& pending : std::as_const(m_pending_drafts)) {
        if (!present.contains(pending.id)) items.append(pending);
    }
    for (const auto& pending : std::as_const(m_pending_sends)) items.append(pending);
    // Drafts live only on this device. One the store no longer returns (a
    // projection rebuilt under rotated keys) is written back, never dropped;
    // only an explicit delete forgets it.
    QVector<CybouMailItem> lost;
    for (const auto& known : std::as_const(m_known_drafts)) {
        if (!present.contains(known.id) && !m_pending_drafts.contains(known.id) && !m_deleted_drafts.contains(known.id)) {
            lost.append(known);
        }
    }
    for (const auto& item : std::as_const(items)) {
        if (item.draft) m_known_drafts.insert(item.id, item);
    }
    setReady(ready);
    for (const auto& draft : std::as_const(lost)) items.append(draft);
    m_last_mail.clear();
    for (const auto& item : items) m_last_mail.insert(item.id, item);
    Q_EMIT mailSnapshot(items);
    for (const auto& draft : std::as_const(lost)) saveMailDraft(draft);
    m_last_files = std::move(files);
    emitFiles();
    Q_EMIT restoreProgressChanged(restore, restore);
    if (!m_initial_projection_ready && (initial_complete || !items.isEmpty() || !m_last_files.isEmpty())) {
        m_initial_projection_ready = true;
        Q_EMIT applicationLoadChanged(CybouApplicationLoadState::Ready, 0, 0, {});
    }
}

void CybouCoreApplicationAdapter::showPendingFile(const CybouFileItem& item)
{
    m_pending_files.insert(item.id, item);
    emitFiles();
}

void CybouCoreApplicationAdapter::emitFiles()
{
    QVector<CybouFileItem> files = m_last_files;
    for (const auto& file : std::as_const(m_last_files)) m_pending_files.remove(file.id);
    for (const auto& pending : std::as_const(m_pending_files)) files.append(pending);
    for (auto& file : files) {
        const auto pending = m_pending_stars.find(file.id);
        if (pending == m_pending_stars.end()) continue;
        const bool indexed = std::any_of(m_last_files.begin(), m_last_files.end(),
            [&](const CybouFileItem& f) { return f.id == file.id; });
        if (indexed && file.starred == *pending) {
            m_pending_stars.erase(pending);
            continue;
        }
        // Starred before it was indexed: store it now that the item exists.
        if (indexed) postFileStar(file.id, *pending);
        file.starred = *pending;
    }
    Q_EMIT filesSnapshot(files);
}

void CybouCoreApplicationAdapter::finishRotation(bool ok, const QString& error)
{
    if (!m_rotation_done) return;
    auto done = std::move(m_rotation_done);
    m_rotation_done = nullptr;
    done(ok, error);
}

void CybouCoreApplicationAdapter::prepareIdentityRotation(const QStringList& new_words,
    std::function<void(bool, const QString&)> done)
{
    if (!m_session || !m_mail_ready || m_rotation_done) {
        done(false, tr("Your data cannot be secured for a new recovery phrase right now."));
        return;
    }
    cybou::RecoveryWords words{};
    if (new_words.size() != static_cast<int>(words.size())) {
        done(false, tr("The new recovery phrase is invalid."));
        return;
    }
    for (std::size_t i{0}; i < words.size(); ++i) words[i] = new_words.at(static_cast<int>(i)).toStdString();
    m_rotation_done = std::move(done);
    m_session->Post([words = std::move(words)](IdentitySession& s) mutable {
        auto entropy = cybou::DecodeRecoveryWords(words);
        for (auto& word : words) cybou::crypto::CleanseMemory(word.data(), word.size());
        auto seed = entropy ? cybou::DeriveIdentityXWingSeed(*entropy) : std::nullopt;
        const auto future = seed ? cybou::DeriveXWingPublicKey(*seed) : std::nullopt;
        if (seed) cybou::crypto::CleanseMemory(seed->data(), seed->size());
        if (!entropy || !future) {
            if (entropy) cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
            s.ToGui([owner = s.owner] { owner->finishRotation(false, tr("The new recovery phrase is invalid.")); });
            return;
        }
        // Stable per new phrase (public key fingerprint), so a retry resumes the same bridge.
        std::string job_id{"bridge-"};
        for (std::size_t i{0}; i < 16; ++i) {
            job_id.push_back(HEX[(*future)[i] >> 4]);
            job_id.push_back(HEX[(*future)[i] & 0x0f]);
        }
        auto prep = std::make_unique<IdentitySession::RotationPrep>();
        prep->job_id = job_id;
        prep->entropy = *entropy;
        cybou::crypto::CleanseMemory(entropy->data(), entropy->size());
        if (!s.local_db->PrepareKeyRotation(prep->entropy) || !s.db->PrepareKeyRotation(prep->entropy)) {
            s.ToGui([owner = s.owner] { owner->finishRotation(false, tr("Local data could not be secured for rotation. Existing keys remain active.")); });
            return;
        }
        const auto result = s.publication->PublishRecoveryBridge(job_id,
            std::span<const unsigned char, 32>{prep->entropy.data(), 32});
        s.storage_projection.jobs[job_id] = result;
        if (result.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
            s.ToGui([owner = s.owner, error = QString::fromStdString(result.error)] {
                owner->finishRotation(false, error.isEmpty() ? tr("Your recovery data could not be secured.") : error);
            });
            return;
        }
        s.rotation = std::move(prep);
    });
}

void CybouCoreApplicationAdapter::notAvailable()
{
    Q_EMIT commandFailed(tr("This action is not available yet."));
}
