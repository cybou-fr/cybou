// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboucoreapplicationadapter_internal.h>

#include <cybou/encrypted_chunk_tree.h>
#include <QSaveFile>
#include <set>

using namespace cybou::qt_detail;

QString CybouCoreApplicationAdapter::IdentitySession::StorageProjection::DownloadContent(const cybou::ChunkId& root, const cybou::ContentKey& key, std::uint64_t size,
    const QString& destination)
{
    QSaveFile out{destination};
    out.setDirectWriteFallback(false);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) return tr("The destination cannot be written.");
    std::set<cybou::ChunkId> seen;
    bool missing{false};
    const auto written = cybou::FetchEncryptedChunkTree(
        std::span<const unsigned char, 32>{session.runtime.GetNetworkBinding().begin(), 32}, key, root,
        [&](const cybou::ChunkId& chunk) {
            auto bytes = session.storage->Fetch(chunk);
            if (!bytes) missing = true;
            return bytes;
        },
        [](std::span<const unsigned char>) { return true; },
        [&](const cybou::ChunkId& chunk) { return seen.insert(chunk).second; },
        [&](std::span<const unsigned char> data) {
            return out.write(reinterpret_cast<const char*>(data.data()), static_cast<qint64>(data.size())) ==
                static_cast<qint64>(data.size());
        },
        std::max<std::uint64_t>(size, 1));
    if (!written || *written != size) {
        out.cancelWriting();
        return missing ? tr("This content is temporarily unavailable. Try again later.")
                       : tr("This content could not be verified.");
    }
    // Atomic replacement: failed verification/write/commit preserves the old destination.
    if (!out.commit()) return tr("The destination cannot be written.");
    return {};
}

void CybouCoreApplicationAdapter::IdentitySession::StorageProjection::RevokeUnreferenced()
{
    std::set<cybou::Hash256> catalog_sources, live_messages;
    std::set<cybou::ChunkId> live_roots;
    for (const auto& record : session.local->ListFiles()) {
        catalog_sources.insert(record.operation_id);
        if (!record.deleted && record.item.root_chunk_id) live_roots.insert(*record.item.root_chunk_id);
    }
    for (const auto& record : session.local->ListMail()) {
        if (record.folder == cybou::MailFolder::DELETED) continue;
        live_messages.insert(record.operation_id);
        for (const auto& attachment : record.message.attachments) live_roots.insert(attachment.root_chunk_id);
    }
    (void)session.publication->RevokeUnreferenced([&](const cybou::Hash256& operation_id,
                                              std::span<const cybou::ChunkId> leaves) {
        return catalog_sources.contains(operation_id) || live_messages.contains(operation_id) ||
            std::any_of(leaves.begin(), leaves.end(), [&](const cybou::ChunkId& leaf) { return live_roots.contains(leaf); });
    });
}

void CybouCoreApplicationAdapter::IdentitySession::StorageProjection::Refresh(bool index_complete)
{
    // Bounded durability audit every few ticks: Protected can fall back to Securing.
    if (++ticks % AUDIT_EVERY_TICKS == 0) session.storage->AuditNextPlacement(AUDIT_CHUNKS_PER_PASS);
    if (ticks % GC_EVERY_TICKS == 0) {
        (void)session.runtime.CollectChunkGarbage(LOCAL_CACHE_BUDGET_BYTES, static_cast<std::uint64_t>(
            QDateTime::currentMSecsSinceEpoch()));
        session.files.locally_complete.clear(); // eviction or outside changes
    }
    for (const auto& [id, status] : session.publication->ProcessDurability(*session.storage, 1)) {
        jobs[id] = status;
        for (const auto& pending : session.local->Outbox()) {
            if (pending.job_id != id) continue;
            if (!session.local->SetPublicationStatus(id, status)) throw std::runtime_error{"cannot save Outbox durability"};
            if (status.phase == cybou::PublicationJobPhase::PROTECTED) (void)session.stager->Release(id);
            break;
        }
    }
    // Only a complete index knows every reference to an own publication.
    if (index_complete && ticks % REVOKE_EVERY_TICKS == 0) RevokeUnreferenced();
}

std::vector<cybou::StorageSettlementEntry> CybouCoreApplicationAdapter::IdentitySession::StorageProjection::Settlement(std::uint64_t period, std::int64_t verified_since_ms)
{
    return session.storage ? session.storage->SettlementEntries(period, verified_since_ms)
                           : std::vector<cybou::StorageSettlementEntry>{};
}
