// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboucoreapplicationadapter_internal.h>

#include <cybou/encrypted_chunk_tree.h>
#include <QFile>
#include <QFileInfo>
#include <set>

using namespace cybou::qt_detail;

bool CybouCoreApplicationAdapter::IdentitySession::FilesProjection::AvailableOffline(const cybou::FileItem& item)
{
    if (!item.root_chunk_id || !item.content_key) return false;
    const auto cached = locally_complete.find(*item.root_chunk_id);
    if (cached != locally_complete.end()) return cached->second;
    const auto& blobs = session.runtime.GetChunkBlobStore();
    bool all_present{true};
    const bool walked = cybou::EnumerateEncryptedTreeChunks(
        std::span<const unsigned char, 32>{session.runtime.GetNetworkBinding().begin(), 32}, *item.content_key,
        *item.root_chunk_id, [&](const cybou::ChunkId& id) { return blobs.Get(id); },
        [&](const cybou::ChunkId& id) {
            all_present = all_present && blobs.Has(id);
            return all_present;
        });
    const bool complete = walked && all_present;
    locally_complete[*item.root_chunk_id] = complete;
    return complete;
}

std::optional<cybou::FileItem> CybouCoreApplicationAdapter::IdentitySession::FilesProjection::CurrentFile(const std::string& hex)
{
    if (const auto pending = file_overlay.find(hex); pending != file_overlay.end()) {
        if (pending->second.deleted) return std::nullopt;
        return pending->second.item;
    }
    const auto id = FromHex(QString::fromStdString(hex));
    if (!id) return std::nullopt;
    const auto record = session.application->GetFile(*id);
    return record ? std::optional{record->item} : std::nullopt;
}

std::optional<std::pair<cybou::ChunkId, std::pair<cybou::ContentKey, std::uint64_t>>> CybouCoreApplicationAdapter::IdentitySession::FilesProjection::ContentOfAttachmentSource(
    const QString& attachment_id)
{
    if (!attachment_id.startsWith(QStringLiteral("ref-"))) return std::nullopt;
    const auto item = CurrentFile(attachment_id.mid(4).toStdString());
    if (!item || !item->root_chunk_id || !item->content_key) return std::nullopt;
    return std::pair{*item->root_chunk_id, std::pair{*item->content_key, item->logical_size}};
}

std::map<std::string, cybou::FileItem> CybouCoreApplicationAdapter::IdentitySession::FilesProjection::Catalog()
{
    std::map<std::string, cybou::FileItem> catalog;
    for (const auto& record : session.application->ListFiles()) catalog[ToHex(record.item.item_id)] = record.item;
    for (const auto& [hex, pending] : file_overlay) {
        if (pending.deleted) catalog.erase(hex);
        else catalog[hex] = pending.item;
    }
    return catalog;
}

bool CybouCoreApplicationAdapter::IdentitySession::FilesProjection::PublishFileChange(cybou::FilesMutationBatch batch, std::optional<std::pair<std::size_t, cybou::NewContent>> content)
{
    const auto job_id = RandomJobId("files-");
    if (job_id.empty()) return false;
    std::vector<std::pair<std::size_t, cybou::NewContent>> contents;
    if (content) contents.push_back(std::move(*content));
    const auto touched = batch.mutations;
    const auto result = session.publication->PublishFiles(job_id, std::move(batch), std::move(contents));
    session.storage_projection.jobs[job_id] = result;
    if (result.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) return false;
    const auto now = QDateTime::currentDateTime();
    for (const auto& mutation : touched) {
        const auto hex = ToHex(mutation.item_id);
        PendingFile pending{.job_id = job_id, .deleted = mutation.kind == cybou::FileMutationKind::DELETE_ITEM,
            .modified = now};
        if (mutation.item) pending.item = *mutation.item;
        file_overlay[hex] = pending;
        item_jobs[hex] = job_id;
        file_modified[hex] = now;
    }
    return true;
}

QVector<CybouFileItem> CybouCoreApplicationAdapter::IdentitySession::FilesProjection::FilesSnapshot()
{
    // A pending change is dropped only once history reflects that exact publication.
    for (auto it = file_overlay.begin(); it != file_overlay.end();) {
        const auto job = session.storage_projection.jobs.find(it->second.job_id);
        const bool finalized = job != session.storage_projection.jobs.end() && !job->second.operation_id.IsNull() &&
            (job->second.phase == cybou::PublicationJobPhase::SECURING ||
                job->second.phase == cybou::PublicationJobPhase::PROTECTED);
        bool indexed{false};
        if (finalized) {
            const auto id = FromHex(QString::fromStdString(it->first));
            const auto current = id ? session.application->GetFile(*id) : std::nullopt;
            indexed = it->second.deleted ? !current
                : current && current->operation_id == job->second.operation_id;
        }
        it = indexed ? file_overlay.erase(it) : std::next(it);
    }
    std::map<std::string, cybou::Hash256> operations;
    std::set<std::string> starred;
    for (const auto& record : session.application->ListFiles()) {
        operations[ToHex(record.item.item_id)] = record.operation_id;
        if (record.starred) starred.insert(ToHex(record.item.item_id));
    }
    const auto catalog = Catalog();
    const auto trash = ToHex(cybou::FilesTrashParent());
    const auto parent_of = [&](const cybou::FileItem& item) {
        return item.parent_id ? ToHex(*item.parent_id) : std::string{};
    };
    const auto in_trash = [&](std::string hex) {
        for (int depth{0}; depth < 64 && !hex.empty(); ++depth) {
            const auto it = catalog.find(hex);
            if (it == catalog.end()) return false;
            const auto parent = parent_of(it->second);
            if (parent == trash) return true;
            hex = parent;
        }
        return false;
    };
    QVector<CybouFileItem> files;
    for (const auto& [hex, item] : catalog) {
        CybouFileItem out;
        out.id = QString::fromStdString(hex);
        out.name = QString::fromStdString(item.name);
        const auto parent = parent_of(item);
        out.parent_id = parent == trash ? QString{} : QString::fromStdString(parent);
        out.folder = item.kind == cybou::FileItemKind::FOLDER;
        out.logical_size = item.logical_size;
        out.trashed = in_trash(hex);
        if (item.modified_ms != 0) out.modified = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(item.modified_ms));
        if (item.root_chunk_id) out.content_root_id = ChunkHex(*item.root_chunk_id);
        out.starred = starred.contains(hex);
        const auto job = item_jobs.find(hex);
        const auto status = job != item_jobs.end() ? session.storage_projection.jobs.find(job->second) : session.storage_projection.jobs.end();
        out.available_offline = AvailableOffline(item);
        if (status != session.storage_projection.jobs.end()) {
            out.state = StateOf(status->second);
            out.operation_state = OperationOf(status->second);
            if (!status->second.operation_id.IsNull()) out.operation_id = QString::fromStdString(status->second.operation_id.GetHex());
            out.progress_percent = status->second.phase == cybou::PublicationJobPhase::SECURING
                ? status->second.durability_percent : -1;
            out.finalized_height = status->second.finalized_height;
        } else if (const auto op = operations.find(hex); op != operations.end()) {
            // Protected only when remote durability is known, never merely finalized.
            const auto durability = session.storage->GetDurability(op->second);
            if (durability) {
                out.min_remote_replicas = static_cast<int>(durability->min_replicas);
                out.remote_replica_target = session.storage->RemoteReplicaTarget();
            }
            out.state = durability && durability->state == cybou::DurabilityState::PROTECTED
                ? CybouContentState::Protected : CybouContentState::Securing;
            out.finalized_height = session.runtime.FindFinalizedOperation(op->second).height;
            out.operation_id = QString::fromStdString(op->second.GetHex());
            out.operation_state = CybouOperationState::Finalized;
        }
        files.append(out);
    }
    return files;
}

namespace {
std::optional<cybou::PrivateItemId> ParentId(const QString& hex)
{
    if (hex.isEmpty()) return std::nullopt;
    return FromHex(hex);
}
} // namespace

void CybouCoreApplicationAdapter::uploadFile(const QString& file_id, const QString& source_path, const QString& parent_id)
{
    const auto item_id = cybou::NewPrivateItemId();
    if (!m_session || !item_id) return;
    const QString hex = QString::fromStdString(ToHex(*item_id));
    m_client_ids.insert(file_id, hex);
    const QFileInfo info{source_path};
    CybouFileItem shown;
    shown.id = hex;
    shown.name = info.fileName();
    shown.parent_id = resolveFileId(parent_id);
    shown.logical_size = static_cast<quint64>(std::max<qint64>(0, info.size()));
    shown.modified = QDateTime::currentDateTime();
    shown.state = CybouContentState::Local;
    shown.operation_state = CybouOperationState::Preparing;
    shown.available_offline = true;
    showPendingFile(shown);
    m_session->Post([item_id = *item_id, name = shown.name.toStdString(), parent = ParentId(shown.parent_id),
                        path = source_path, hex = hex.toStdString()](IdentitySession& s) {
        auto file = std::make_shared<QFile>(path);
        if (!file->open(QIODevice::ReadOnly)) {
            s.ToGui([owner = s.owner, id = QString::fromStdString(hex)] {
                if (auto it = owner->m_pending_files.find(id); it != owner->m_pending_files.end()) {
                    it->state = CybouContentState::NeedsAttention;
                }
                Q_EMIT owner->fileStateChanged(id, CybouContentState::NeedsAttention, -1);
                Q_EMIT owner->commandFailed(tr("The file could not be read."));
            });
            return;
        }
        cybou::FileItem item{.item_id = item_id, .parent_id = parent, .kind = cybou::FileItemKind::FILE, .name = name};
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id, item});
        // Content is streamed from disk into encrypted chunks; no plaintext copy is kept.
        cybou::NewContent content{[file](std::span<unsigned char> out) -> std::optional<std::size_t> {
            const auto n = file->read(reinterpret_cast<char*>(out.data()), static_cast<qint64>(out.size()));
            if (n < 0) return std::nullopt;
            return static_cast<std::size_t>(n);
        }};
        const bool ok = s.files.PublishFileChange(std::move(batch), std::pair{std::size_t{0}, std::move(content)});
        if (ok) {
            // The staged size is authoritative once indexed; show the source size meanwhile.
            s.files.file_overlay[hex].item.logical_size = static_cast<std::uint64_t>(file->size());
        } else {
            s.ToGui([owner = s.owner, id = QString::fromStdString(hex)] {
                if (auto it = owner->m_pending_files.find(id); it != owner->m_pending_files.end()) {
                    it->state = CybouContentState::NeedsAttention;
                }
                Q_EMIT owner->fileStateChanged(id, CybouContentState::NeedsAttention, -1);
                Q_EMIT owner->commandFailed(tr("The file could not be uploaded."));
            });
        }
    });
}

void CybouCoreApplicationAdapter::downloadFile(const QString& file_id, const QString& destination)
{
    const QString hex = resolveFileId(file_id);
    if (!m_session) return;
    m_session->Post([hex = hex.toStdString(), destination](IdentitySession& s) {
        const auto item = s.files.CurrentFile(hex);
        const auto id = QString::fromStdString(hex);
        const QString error = item && item->root_chunk_id && item->content_key
            ? s.storage_projection.DownloadContent(*item->root_chunk_id, *item->content_key, item->logical_size, destination)
            : tr("This file has no content yet.");
        if (error.isEmpty()) s.files.locally_complete.clear();
        s.ToGui([owner = s.owner, id, error] {
            if (!error.isEmpty()) {
                Q_EMIT owner->fileRetrievalChanged(id, CybouRetrievalState::Idle);
                Q_EMIT owner->commandFailed(error);
                return;
            }
            Q_EMIT owner->fileRetrievalChanged(id, CybouRetrievalState::Ready);
            QMetaObject::invokeMethod(owner, [owner, id] {
                Q_EMIT owner->fileRetrievalChanged(id, CybouRetrievalState::Idle);
            }, Qt::QueuedConnection);
        });
    });
}

void CybouCoreApplicationAdapter::createFolder(const QString& folder_id, const QString& name, const QString& parent_id)
{
    const auto item_id = cybou::NewPrivateItemId();
    if (!m_session || !item_id) return;
    const QString hex = QString::fromStdString(ToHex(*item_id));
    m_client_ids.insert(folder_id, hex);
    CybouFileItem shown;
    shown.id = hex;
    shown.name = name;
    shown.parent_id = resolveFileId(parent_id);
    shown.folder = true;
    shown.modified = QDateTime::currentDateTime();
    shown.state = CybouContentState::Local;
    shown.operation_state = CybouOperationState::Preparing;
    showPendingFile(shown);
    m_session->Post([item_id = *item_id, name = name.toStdString(), parent = ParentId(shown.parent_id)](IdentitySession& s) {
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
            cybou::FileItem{.item_id = item_id, .parent_id = parent, .kind = cybou::FileItemKind::FOLDER, .name = name}});
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The folder could not be created.")); });
        }
    });
}

void CybouCoreApplicationAdapter::renameFile(const QString& id, const QString& name)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString(), name = name.toStdString()](IdentitySession& s) {
        auto item = s.files.CurrentFile(hex);
        if (!item) return;
        item->name = name;
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be renamed.")); });
        }
    });
}

void CybouCoreApplicationAdapter::moveFile(const QString& id, const QString& parent_id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString(), parent = ParentId(resolveFileId(parent_id))](IdentitySession& s) {
        auto item = s.files.CurrentFile(hex);
        if (!item) return;
        // Never move a folder into itself or its own subtree.
        for (auto cursor = parent; cursor;) {
            if (*cursor == item->item_id) return;
            const auto above = s.files.CurrentFile(ToHex(*cursor));
            cursor = above ? above->parent_id : std::nullopt;
        }
        item->parent_id = parent;
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be moved.")); });
        }
    });
}

void CybouCoreApplicationAdapter::copyFile(const QString& id, const QString& copy_id, const QString& parent_id)
{
    const auto new_id = cybou::NewPrivateItemId();
    if (!m_session || !new_id) return;
    m_client_ids.insert(copy_id, QString::fromStdString(ToHex(*new_id)));
    m_session->Post([hex = resolveFileId(id).toStdString(), new_id = *new_id,
                        parent = ParentId(resolveFileId(parent_id))](IdentitySession& s) {
        auto item = s.files.CurrentFile(hex);
        if (!item || item->kind != cybou::FileItemKind::FILE) return;
        // A copy is a new catalog entry referencing the same protected content.
        item->item_id = new_id;
        item->parent_id = parent;
        item->modified_ms = static_cast<std::uint64_t>(QDateTime::currentMSecsSinceEpoch());
        item->name = tr("Copy of %1").arg(QString::fromStdString(item->name)).toStdString();
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, new_id, *item});
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The copy could not be created.")); });
        }
    });
}

void CybouCoreApplicationAdapter::setFileStarred(const QString& id, bool starred)
{
    const QString hex = resolveFileId(id);
    if (!m_session) return;
    // Shown at once; persisted as encrypted Identity state once the item is indexed.
    m_pending_stars.insert(hex, starred);
    emitFiles();
    postFileStar(hex, starred);
}

void CybouCoreApplicationAdapter::postFileStar(const QString& hex, bool starred)
{
    const auto item_id = FromHex(hex);
    if (!m_session || !item_id) return;
    m_session->Post([item_id = *item_id, starred](IdentitySession& s) { (void)s.application->SetFileStarred(item_id, starred); });
}

void CybouCoreApplicationAdapter::trashFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](IdentitySession& s) {
        auto item = s.files.CurrentFile(hex);
        if (!item || item->parent_id == cybou::FilesTrashParent()) return;
        // Contents follow their folder into Trash without separate changes.
        item->parent_id = cybou::FilesTrashParent();
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be moved to Trash.")); });
        }
    });
}

void CybouCoreApplicationAdapter::restoreFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](IdentitySession& s) {
        auto item = s.files.CurrentFile(hex);
        if (!item) return;
        // Trash does not remember the old location; restored items return to My files.
        item->parent_id = std::nullopt;
        cybou::FilesMutationBatch batch;
        batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item->item_id, *item});
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be restored.")); });
        }
    });
}

void CybouCoreApplicationAdapter::deleteFiles(const QStringList& ids)
{
    if (!m_session || ids.isEmpty()) return;
    std::vector<std::string> roots;
    for (const auto& id : ids) roots.push_back(resolveFileId(id).toStdString());
    // One publication for the whole set: Empty Trash costs one network fee.
    m_session->Post([roots = std::move(roots)](IdentitySession& s) {
        const auto catalog = s.files.Catalog();
        std::vector<cybou::PrivateItemId> doomed;
        for (const auto& hex : roots) {
            const auto root = FromHex(QString::fromStdString(hex));
            if (root && s.files.CurrentFile(hex)) doomed.push_back(*root);
        }
        for (std::size_t i{0}; i < doomed.size(); ++i) {
            for (const auto& [child_hex, item] : catalog) {
                if (item.parent_id == doomed[i] &&
                    std::find(doomed.begin(), doomed.end(), item.item_id) == doomed.end()) doomed.push_back(item.item_id);
            }
        }
        if (doomed.empty()) return;
        cybou::FilesMutationBatch batch;
        for (const auto& item_id : doomed) {
            batch.mutations.push_back({cybou::FileMutationKind::DELETE_ITEM, item_id, std::nullopt});
        }
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The items could not be deleted.")); });
        }
    });
}

void CybouCoreApplicationAdapter::retryFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](IdentitySession& s) {
        const auto job = s.files.item_jobs.find(hex);
        if (job == s.files.item_jobs.end()) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("This change cannot be retried; upload the file again.")); });
            return;
        }
        s.storage_projection.jobs[job->second] = s.publication->Resume(job->second);
        if (s.storage_projection.jobs[job->second].phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The change still could not be sent. Try again later.")); });
        }
        s.Refresh();
    });
}

void CybouCoreApplicationAdapter::discardFile(const QString& id)
{
    if (!m_session) return;
    const QString hex = resolveFileId(id);
    // A file that never reached a publication (e.g. unreadable) exists only here.
    if (m_pending_files.remove(hex) > 0) {
        Q_EMIT fileItemsRemoved({hex});
        return;
    }
    m_session->Post([hex = hex.toStdString()](IdentitySession& s) {
        const auto job = s.files.item_jobs.find(hex);
        const std::string job_id = job == s.files.item_jobs.end() ? std::string{} : job->second;
        // Only never-submitted or rejected work can be cancelled; in-flight work stays.
        if (job_id.empty() || !s.publication->CancelPublication(job_id)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("This change is already on its way and cannot be discarded.")); });
            return;
        }
        for (auto it = s.files.file_overlay.begin(); it != s.files.file_overlay.end();) {
            if (it->second.job_id != job_id) { ++it; continue; }
            s.files.item_jobs.erase(it->first);
            it = s.files.file_overlay.erase(it);
        }
        s.storage_projection.jobs.erase(job_id);
        s.Refresh();
    });
}

void CybouCoreApplicationAdapter::deleteFile(const QString& id)
{
    if (!m_session) return;
    m_session->Post([hex = resolveFileId(id).toStdString()](IdentitySession& s) {
        const auto root = FromHex(QString::fromStdString(hex));
        if (!root || !s.files.CurrentFile(hex)) return;
        const auto catalog = s.files.Catalog();
        std::vector<cybou::PrivateItemId> doomed{*root};
        for (std::size_t i{0}; i < doomed.size(); ++i) {
            for (const auto& [child_hex, item] : catalog) {
                if (item.parent_id == doomed[i]) doomed.push_back(item.item_id);
            }
        }
        cybou::FilesMutationBatch batch;
        for (const auto& item_id : doomed) {
            batch.mutations.push_back({cybou::FileMutationKind::DELETE_ITEM, item_id, std::nullopt});
        }
        if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
            s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The item could not be deleted.")); });
        }
    });
}
