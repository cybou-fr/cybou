// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <qt/cyboufixturebackend.h>

#include <QDateTime>
#include <QFileInfo>
#include <QTimer>

#include <utility>

CybouFixtureApplicationBackend::CybouFixtureApplicationBackend(QObject* parent)
    : CybouApplicationBackend{parent}
{
}

void CybouFixtureApplicationBackend::seed(QVector<CybouMailItem> mail, QVector<CybouFileItem> files)
{
    m_mail = std::move(mail);
    m_files = std::move(files);
    if (!m_open) return;
    Q_EMIT mailSnapshot(m_mail);
    Q_EMIT filesSnapshot(m_files);
}

void CybouFixtureApplicationBackend::seedProgressively(QVector<CybouMailItem> mail, QVector<CybouFileItem> files)
{
    // Metadata first: Sent and Files come from own publications, then the
    // Inbox fills in. Large content is never fetched by a restore.
    m_mail.clear();
    m_files.clear();
    Q_EMIT restoreProgressChanged(CybouRestoreStepState::Running, CybouRestoreStepState::Running);
    later(1, [this, files = std::move(files)] {
        m_files = files;
        if (m_open) Q_EMIT filesSnapshot(m_files);
        Q_EMIT restoreProgressChanged(CybouRestoreStepState::Running, CybouRestoreStepState::Done);
    });
    later(2, [this, mail = std::move(mail)] {
        m_mail = mail;
        if (m_open) Q_EMIT mailSnapshot(m_mail);
        Q_EMIT restoreProgressChanged(CybouRestoreStepState::Done, CybouRestoreStepState::Done);
    });
}

void CybouFixtureApplicationBackend::setAutoAdvance(bool enabled, int step_ms)
{
    m_auto_advance = enabled;
    m_step_ms = step_ms;
}

void CybouFixtureApplicationBackend::openIdentity()
{
    if (m_open) return;
    m_open = true;
    Q_EMIT mailSnapshot(m_mail);
    Q_EMIT filesSnapshot(m_files);
    Q_EMIT applicationLoadChanged(CybouApplicationLoadState::Ready, 0, 0, {});
}

void CybouFixtureApplicationBackend::closeIdentity()
{
    // The fixture store survives so unlocking shows the same data again.
    m_open = false;
}

void CybouFixtureApplicationBackend::refreshProjection(CommandProgress progress)
{
    if (!m_open) { if (progress) progress(CybouCommandState::Failed, tr("Mail is unavailable.")); return; }
    Q_EMIT mailSnapshot(m_mail);
    Q_EMIT filesSnapshot(m_files);
    if (progress) progress(CybouCommandState::Committed, {});
}

CybouMailItem* CybouFixtureApplicationBackend::mail(const QString& id)
{
    for (auto& item : m_mail) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

CybouFileItem* CybouFixtureApplicationBackend::file(const QString& id)
{
    for (auto& item : m_files) {
        if (item.id == id) return &item;
    }
    return nullptr;
}

QStringList CybouFixtureApplicationBackend::withDescendants(const QString& id) const
{
    QStringList ids{id};
    for (int i = 0; i < ids.size(); ++i) {
        for (const auto& item : m_files) {
            if (item.parent_id == ids.at(i)) ids << item.id;
        }
    }
    return ids;
}

void CybouFixtureApplicationBackend::changed(const CybouMailItem& item)
{
    if (m_open) Q_EMIT mailItemChanged(item);
}

void CybouFixtureApplicationBackend::changed(const CybouFileItem& item)
{
    if (m_open) Q_EMIT fileItemChanged(item);
}

void CybouFixtureApplicationBackend::later(int steps, std::function<void()> action)
{
    QTimer::singleShot(steps * m_step_ms, this, std::move(action));
}

/* ---- Mail ---- */

void CybouFixtureApplicationBackend::saveMailDraft(const CybouMailItem& draft, CommandProgress progress)
{
    if (!m_open) return;
    if (auto* existing = mail(draft.id)) *existing = draft;
    else m_mail.prepend(draft);
    changed(draft);
    if (progress) progress(CybouCommandState::Committed, {});
}

void CybouFixtureApplicationBackend::sendMail(const CybouMailItem& message, const QString& draft_id, CommandProgress progress)
{
    if (!m_open) return;
    if (auto* existing = mail(message.id)) *existing = message;
    else m_mail.prepend(message);
    changed(message);
    if (!draft_id.isEmpty()) deleteMail(draft_id);
    if (progress) progress(CybouCommandState::Committed, {});
    runSend(message.id);
}

void CybouFixtureApplicationBackend::retryMail(const QString& id)
{
    auto* item = mail(id);
    if (!m_open || !item) return;
    item->state = CybouContentState::Local;
    item->operation_state = CybouOperationState::Preparing;
    for (auto& attachment : item->attachments) {
        if (!CybouProduct::contentOnNetwork(attachment.state)) attachment.state = CybouContentState::Local;
    }
    changed(*item);
    runSend(id);
}

void CybouFixtureApplicationBackend::setMailRead(const QString& id, bool read)
{
    auto* item = mail(id);
    if (!m_open || !item || item->unread != read) return;
    item->unread = !read;
    changed(*item);
}

void CybouFixtureApplicationBackend::setMailStarred(const QString& id, bool starred)
{
    auto* item = mail(id);
    if (!m_open || !item || item->starred == starred) return;
    item->starred = starred;
    changed(*item);
}

void CybouFixtureApplicationBackend::moveMail(const QString& id, CybouMailFolder folder, CommandProgress progress)
{
    auto* item = mail(id);
    if (!m_open || !item) {
        if (progress) progress(CybouCommandState::Failed, tr("This message cannot be moved there."));
        return;
    }
    item->folder = folder;
    changed(*item);
    if (progress) progress(CybouCommandState::Committed, {});
}

void CybouFixtureApplicationBackend::deleteMail(const QString& id)
{
    if (!m_open) return;
    if (m_mail.removeIf([&id](const CybouMailItem& item) { return item.id == id; }) > 0) Q_EMIT mailItemRemoved(id);
}

void CybouFixtureApplicationBackend::downloadAttachment(const QString& message_id, const QString& attachment_id,
    const QString&)
{
    if (!m_open || !m_auto_advance) return;
    const auto step = [this, message_id, attachment_id](CybouRetrievalState retrieval) {
        auto* item = mail(message_id);
        if (!item) return;
        for (auto& attachment : item->attachments) {
            if (attachment.id == attachment_id) attachment.retrieval = retrieval;
        }
        if (m_open) Q_EMIT attachmentRetrievalChanged(message_id, attachment_id, retrieval);
    };
    later(1, [step] { step(CybouRetrievalState::Verifying); });
    later(2, [step] { step(CybouRetrievalState::Decrypting); });
    later(3, [step] { step(CybouRetrievalState::Ready); });
}

void CybouFixtureApplicationBackend::saveAttachmentToFiles(const QString& message_id, const QString& attachment_id,
    const QString& file_id)
{
    auto* message = mail(message_id);
    if (!m_open || !message) return;
    for (auto& attachment : message->attachments) {
        if (attachment.id != attachment_id) continue;
        if (!CybouProduct::contentOnNetwork(attachment.state)) {
            Q_EMIT commandFailed(tr("This attachment is not protected yet."));
            return;
        }
        // Same encrypted content, new independent catalog reference.
        CybouFileItem item;
        item.id = file_id;
        item.name = attachment.name;
        item.logical_size = attachment.logical_size;
        item.modified = QDateTime::currentDateTime();
        item.state = CybouContentState::Protected;
        m_files.append(item);
        attachment.saved_file_id = file_id;
        changed(item);
        changed(*message);
        return;
    }
}

void CybouFixtureApplicationBackend::runSend(const QString& id)
{
    // Finality-first lifecycle: local prepare -> PoA confirmation ->
    // storage admission of authorized chunks -> durability. Offline, the
    // message waits for the network and does not advance.
    if (!m_auto_advance || (m_online && !m_online())) return;
    const auto set = [this, id](CybouContentState state, CybouOperationState operation, int percent) {
        auto* item = mail(id);
        if (!item) return;
        item->state = state;
        item->operation_state = operation;
        for (auto& attachment : item->attachments) {
            if (CybouProduct::contentOnNetwork(attachment.state) && state != CybouContentState::Protected) continue;
            attachment.state = state;
            attachment.progress_percent = percent;
        }
        changed(*item);
    };
    later(1, [set] { set(CybouContentState::Local, CybouOperationState::Submitted, -1); });
    later(3, [set] { set(CybouContentState::Securing, CybouOperationState::Finalized, 35); });
    later(4, [set] { set(CybouContentState::Securing, CybouOperationState::Finalized, 80); });
    later(5, [set] { set(CybouContentState::Protected, CybouOperationState::Finalized, -1); });
}

/* ---- Files ---- */

void CybouFixtureApplicationBackend::uploadFile(const QString& file_id, const QString& source_path,
    const QString& parent_id)
{
    if (!m_open) return;
    const QFileInfo info{source_path};
    CybouFileItem item;
    item.id = file_id;
    item.name = info.fileName();
    item.parent_id = parent_id;
    item.logical_size = static_cast<quint64>(qMax<qint64>(0, info.size()));
    item.modified = QDateTime::currentDateTime();
    item.state = CybouContentState::Local;
    item.operation_state = CybouOperationState::Preparing;
    // The uploading device keeps its local copy.
    item.available_offline = true;
    m_files.append(item);
    changed(item);
    runUpload(file_id);
}

void CybouFixtureApplicationBackend::runUpload(const QString& id)
{
    // Same finality-first lifecycle as Mail; offline uploads wait.
    if (!m_auto_advance || (m_online && !m_online())) return;
    const auto set = [this, id](CybouContentState state, CybouOperationState operation, int percent) {
        auto* item = file(id);
        if (!item) return;
        item->state = state;
        item->operation_state = operation;
        item->progress_percent = percent;
        changed(*item);
    };
    later(1, [set] { set(CybouContentState::Local, CybouOperationState::Submitted, -1); });
    later(3, [set] { set(CybouContentState::Securing, CybouOperationState::Finalized, 20); });
    later(4, [set] { set(CybouContentState::Securing, CybouOperationState::Finalized, 65); });
    later(5, [set] { set(CybouContentState::Protected, CybouOperationState::Finalized, -1); });
}

void CybouFixtureApplicationBackend::downloadFile(const QString& file_id, const QString&)
{
    if (!m_open || !m_auto_advance) return;
    const auto step = [this, file_id](CybouRetrievalState retrieval, bool cached) {
        auto* item = file(file_id);
        if (!item) return;
        item->retrieval = retrieval;
        if (cached) item->available_offline = true;
        changed(*item);
    };
    later(1, [step] { step(CybouRetrievalState::Verifying, false); });
    later(2, [step] { step(CybouRetrievalState::Decrypting, false); });
    later(3, [step] { step(CybouRetrievalState::Ready, true); });
    later(6, [step] { step(CybouRetrievalState::Idle, false); });
}

void CybouFixtureApplicationBackend::createFolder(const QString& folder_id, const QString& name,
    const QString& parent_id)
{
    if (!m_open) return;
    CybouFileItem folder;
    folder.id = folder_id;
    folder.name = name;
    folder.parent_id = parent_id;
    folder.folder = true;
    folder.modified = QDateTime::currentDateTime();
    folder.state = CybouContentState::Protected;
    m_files.append(folder);
    changed(folder);
}

void CybouFixtureApplicationBackend::renameFile(const QString& id, const QString& name)
{
    auto* item = file(id);
    if (!m_open || !item) return;
    item->name = name;
    item->modified = QDateTime::currentDateTime();
    changed(*item);
}

void CybouFixtureApplicationBackend::moveFile(const QString& id, const QString& parent_id)
{
    auto* item = file(id);
    if (!m_open || !item || withDescendants(id).contains(parent_id)) return;
    item->parent_id = parent_id;
    changed(*item);
}

void CybouFixtureApplicationBackend::copyFile(const QString& id, const QString& copy_id, const QString& parent_id)
{
    const auto* source = file(id);
    if (!m_open || !source || source->folder) return;
    // A copy references the same protected content under a new catalog entry.
    CybouFileItem copy = *source;
    copy.id = copy_id;
    copy.parent_id = parent_id;
    copy.name = tr("Copy of %1").arg(source->name);
    copy.starred = false;
    copy.retrieval = CybouRetrievalState::Idle;
    copy.modified = QDateTime::currentDateTime();
    m_files.append(copy);
    changed(copy);
}

void CybouFixtureApplicationBackend::setFileStarred(const QString& id, bool starred)
{
    auto* item = file(id);
    if (!m_open || !item || item->starred == starred) return;
    item->starred = starred;
    changed(*item);
}

void CybouFixtureApplicationBackend::trashFile(const QString& id)
{
    if (!m_open) return;
    const QStringList ids = withDescendants(id);
    for (auto& item : m_files) {
        if (ids.contains(item.id) && !item.trashed) {
            item.trashed = true;
            changed(item);
        }
    }
}

void CybouFixtureApplicationBackend::restoreFile(const QString& id)
{
    if (!m_open) return;
    const QStringList ids = withDescendants(id);
    for (auto& item : m_files) {
        if (ids.contains(item.id) && item.trashed) {
            item.trashed = false;
            changed(item);
        }
    }
}

void CybouFixtureApplicationBackend::deleteFile(const QString& id)
{
    if (!m_open) return;
    const QStringList ids = withDescendants(id);
    m_files.removeIf([&ids](const CybouFileItem& item) { return ids.contains(item.id); });
    Q_EMIT fileItemsRemoved(ids);
}
