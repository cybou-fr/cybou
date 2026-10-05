// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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
    const auto account = m_identity.GetAccountId();
    if (!account) return;
    // One encrypted, rebuildable Application DB per Identity.
    const auto root = cybou::IdentityDataDirectory(m_data_directory, *account);
    ++m_session_generation;
    m_session = std::make_unique<IdentitySession>(this, m_runtime, m_identity.GetKeyStore(), root, m_refresh_ms,
        m_transport_override);
}

void CybouCoreApplicationAdapter::prepareStorageSettlement(const std::uint64_t period,
    const std::int64_t verified_since_ms, std::function<void(std::vector<cybou::StorageSettlementEntry>)> done)
{
    if (!m_session) {
        done({});
        return;
    }
    m_session->Post([period, verified_since_ms, done = std::move(done)](IdentitySession& s) {
        done(s.storage_projection.Settlement(period, verified_since_ms));
    });
}

void CybouCoreApplicationAdapter::identityKeysChanged()
{
    if (!m_session || m_reopening) return;
    m_reopening = true;
    // Drafts exist only on this device: keep the latest ones across the reopen.
    auto drafts = m_known_drafts;
    for (const auto& pending : std::as_const(m_pending_drafts)) drafts.insert(pending.id, pending);
    m_session.reset();
    setReady(false);
    openIdentity();
    for (const auto& draft : std::as_const(drafts)) {
        if (!m_deleted_drafts.contains(draft.id)) saveMailDraft(draft);
    }
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
    m_client_ids.clear();
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
    bool ready, CybouRestoreStepState restore)
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
    Q_EMIT mailSnapshot(items);
    for (const auto& draft : std::as_const(lost)) saveMailDraft(draft);
    m_last_files = std::move(files);
    emitFiles();
    Q_EMIT restoreProgressChanged(restore, restore);
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
