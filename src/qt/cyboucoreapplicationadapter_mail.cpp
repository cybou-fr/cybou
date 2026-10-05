// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <qt/cyboucoreapplicationadapter_internal.h>

#include <cybou/support_mail.h>
#include <QFile>

using namespace cybou::qt_detail;

namespace {
cybou::MailDraft StoredDraft(const CybouMailItem& draft, const QString& id)
{
    cybou::MailDraft stored{.draft_id = id.toStdString(), .to = draft.to_name.toStdString(),
        .subject = draft.subject.toStdString(), .body = draft.body.toStdString(),
        .updated_ms = static_cast<std::uint64_t>(std::max<qint64>(0, draft.time.toMSecsSinceEpoch()))};
    for (const auto& attachment : draft.attachments) {
        stored.attachments.push_back({.name = attachment.name.toStdString(), .logical_size = attachment.logical_size,
            .source_path = attachment.source_path.toStdString(),
            .reference_id = attachment.source_path.isEmpty() ? attachment.id.toStdString() : std::string{}});
    }
    return stored;
}
}

QVector<CybouMailItem> CybouCoreApplicationAdapter::IdentitySession::MailProjection::Snapshot()
{
    const auto loaded = session.runtime.GetStore().GetStateSnapshot();
    const cybou::CybouState* state = loaded && loaded.state ? &*loaded.state : nullptr;
    QVector<CybouMailItem> items;
    std::map<cybou::ChunkId, std::string> saved_roots;
    for (const auto& [hex, file] : session.files.Catalog()) {
        if (file.root_chunk_id) saved_roots.emplace(*file.root_chunk_id, hex);
    }
    for (const auto& record : session.application->ListMail()) {
        const auto id = ToHex(record.message.message_id);
        outbox.erase(id);
        CybouMailItem item;
        item.id = QString::fromStdString(id);
        item.folder = FolderOf(record.folder);
        item.from_name = DisplayName(state, record.sender);
        item.to_name = DisplayName(state, record.message.recipient_account_id);
        item.from_address = QString::fromStdString(record.sender.Value().GetHex());
        item.to_address = QString::fromStdString(record.message.recipient_account_id.Value().GetHex());
        item.subject = QString::fromStdString(record.message.subject);
        item.body = QString::fromStdString(record.message.body);
        item.preview = item.body.simplified().left(90);
        item.time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(record.message.client_timestamp_ms));
        item.unread = !record.read;
        item.starred = record.starred;
        item.operation_id = QString::fromStdString(record.operation_id.GetHex());
        item.finalized_height = record.finalized_height;
        item.outgoing = record.outgoing;
        if (const auto publication_record = session.runtime.FindFinalizedRootPublication(record.operation_id)) {
            item.root_chunk_id = ChunkHex(publication_record->root_chunk_id);
            // The support Identity checks each incoming message paid the support rate.
            if (!record.outgoing && state && cybou::SupportAccount(*state) == session.keystore.GetAccountId()) {
                const auto& params = session.runtime.GetNetworkGenesis().GetProtocolParameters();
                const auto fee = cybou::RootPublicationOperationFee(params, *publication_record);
                item.below_support_rate = !fee || *fee < cybou::SupportMailMinimumFee(params);
            }
        }
        if (record.outgoing) {
            // Sent means remotely durable, never merely finalized.
            const auto job = session.storage_projection.jobs.find(id);
            const auto durability = session.storage->GetDurability(record.operation_id);
            if (durability) {
                item.min_remote_replicas = static_cast<int>(durability->min_replicas);
                item.remote_replica_target = session.storage->RemoteReplicaTarget();
            }
            item.state = job != session.storage_projection.jobs.end() ? StateOf(job->second)
                : durability && durability->state == cybou::DurabilityState::PROTECTED
                    ? CybouContentState::Protected : CybouContentState::Securing;
            item.operation_state = job != session.storage_projection.jobs.end() ? OperationOf(job->second) : CybouOperationState::Finalized;
        } else {
            // Incoming: finalized and decrypted here, but the sender's
            // remote durability is not something this Identity has proved.
            item.state = CybouContentState::Received;
        }
        for (const auto& attachment : record.message.attachments) {
            CybouAttachmentItem a;
            a.id = QString::fromStdString(ToHex(attachment.attachment_id));
            a.name = QString::fromStdString(attachment.filename);
            a.logical_size = attachment.logical_size;
            // Sent content shares its message's durability; received content is the sender's.
            a.state = record.outgoing ? item.state : CybouContentState::Received;
            // "Saved to Files" is a Files entry referencing the same protected content.
            if (const auto saved = saved_roots.find(attachment.root_chunk_id); saved != saved_roots.end()) {
                a.saved_file_id = QString::fromStdString(saved->second);
            }
            item.attachments.append(a);
        }
        items.append(item);
    }
    for (const auto& draft : session.application->ListDrafts()) {
        CybouMailItem item;
        item.id = QString::fromStdString(draft.draft_id);
        item.folder = CybouMailFolder::Drafts;
        item.draft = true;
        item.state = CybouContentState::Local;
        item.to_name = QString::fromStdString(draft.to);
        item.subject = QString::fromStdString(draft.subject);
        item.body = QString::fromStdString(draft.body);
        item.preview = item.body.simplified().left(90);
        item.time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(draft.updated_ms));
        for (std::size_t i{0}; i < draft.attachments.size(); ++i) {
            const auto& stored = draft.attachments[i];
            CybouAttachmentItem attachment;
            attachment.name = QString::fromStdString(stored.name);
            attachment.logical_size = stored.logical_size;
            attachment.source_path = QString::fromStdString(stored.source_path);
            attachment.id = stored.reference_id.empty()
                ? QStringLiteral("att-draft-%1").arg(i) : QString::fromStdString(stored.reference_id);
            attachment.state = stored.reference_id.empty() ? CybouContentState::Local : CybouContentState::Protected;
            item.attachments.append(attachment);
        }
        items.append(item);
    }
    for (auto& [id, item] : outbox) {
        if (const auto job = session.storage_projection.jobs.find(id); job != session.storage_projection.jobs.end()) {
            item.state = StateOf(job->second);
            item.operation_state = OperationOf(job->second);
            if (!job->second.operation_id.IsNull()) {
                item.operation_id = QString::fromStdString(job->second.operation_id.GetHex());
            }
            item.finalized_height = job->second.finalized_height;
        }
        items.append(item);
    }
    return items;
}

std::optional<cybou::AccountId> CybouCoreApplicationAdapter::IdentitySession::MailProjection::ResolveRecipient(const QString& name) const
{
    QString label = name.trimmed().toLower();
    if (label.endsWith(QStringLiteral(".cybou"))) label.chop(6);
    const auto loaded = session.runtime.GetStore().GetStateSnapshot();
    if (!loaded || !loaded.state) return std::nullopt;
    if (const auto* account = loaded.state->names.Resolve(label.toStdString())) return *account;
    // An Identity without a name can be addressed by its full AccountID.
    const auto value = cybou::Hash256::FromHex(label.toStdString());
    if (!value) return std::nullopt;
    const cybou::AccountId account{*value};
    if (loaded.state->identities.Find(account)) return account;
    return std::nullopt;
}

void CybouCoreApplicationAdapter::saveMailDraft(const CybouMailItem& draft, CommandProgress progress)
{
    if (!m_session) {
        if (progress) progress(CybouCommandState::Failed, tr("Mail is unavailable."));
        return;
    }
    m_pending_drafts.insert(draft.id, draft);
    m_deleted_drafts.remove(draft.id);
    Q_EMIT mailItemChanged(draft);
    m_session->Post([stored = StoredDraft(draft, draft.id), progress](IdentitySession& s) {
        s.StateToGui([progress] { if (progress) progress(CybouCommandState::Running, {}); });
        bool ok{false};
        try { ok = s.application->SaveDraft(stored); } catch (const std::exception&) { }
        s.StateToGui([owner = s.owner, progress, ok] {
            const auto error = ok ? QString{} : tr("The draft could not be saved. Your text is kept open; try again.");
            if (progress) progress(ok ? CybouCommandState::Committed : CybouCommandState::Failed, error);
            else if (!ok) Q_EMIT owner->commandFailed(error);
        });
    });
}

void CybouCoreApplicationAdapter::sendMail(const CybouMailItem& message, const QString& draft_id, CommandProgress progress)
{
    if (!m_session) {
        if (progress) progress(CybouCommandState::Failed, tr("Mail is unavailable."));
        return;
    }
    const QString client_id = message.id;
    CybouMailItem pending = message;
    // Re-editing/retrying one draft replaces its previous failed temporary row.
    for (auto it = m_send_drafts.begin(); it != m_send_drafts.end();) {
        if (!draft_id.isEmpty() && it.value() == draft_id && it.key() != client_id) {
            const auto previous = it.key();
            m_pending_sends.remove(previous);
            it = m_send_drafts.erase(it);
            Q_EMIT mailItemRemoved(previous);
        } else ++it;
    }
    m_pending_sends.insert(client_id, pending);
    if (!draft_id.isEmpty()) m_send_drafts.insert(client_id, draft_id);
    m_session->Post([client_id, message, draft_id, progress](IdentitySession& s) {
        s.StateToGui([progress] { if (progress) progress(CybouCommandState::Running, {}); });
        const auto fail = [&](const QString& error) {
            s.StateToGui([owner = s.owner, client_id, progress, error] {
                if (auto it = owner->m_pending_sends.find(client_id); it != owner->m_pending_sends.end()) it->state = CybouContentState::NeedsAttention;
                Q_EMIT owner->mailStateChanged(client_id, CybouContentState::NeedsAttention);
                if (progress) progress(CybouCommandState::Failed, error);
                else Q_EMIT owner->commandFailed(error);
            });
        };
        try {
            // No UI acknowledgement precedes durable backup and job ownership.
            if (!draft_id.isEmpty() && !s.application->SaveDraft(StoredDraft(message, draft_id))) {
                fail(tr("The draft could not be saved. The message has not been sent."));
                return;
            }
            if (!draft_id.isEmpty()) {
                CybouMailItem backup = message;
                backup.id = draft_id;
                backup.draft = true;
                backup.outgoing = false;
                backup.folder = CybouMailFolder::Drafts;
                s.StateToGui([owner = s.owner, backup] {
                    owner->m_deleted_drafts.remove(backup.id);
                    owner->m_known_drafts.insert(backup.id, backup);
                    Q_EMIT owner->mailItemChanged(backup);
                });
            }
            auto message_id = cybou::NewPrivateItemId();
            if (message_id && !draft_id.isEmpty()) message_id = s.application->BindDraftToMessage(draft_id.toStdString(), *message_id);
            if (!message_id) {
                fail(tr("Could not save the outgoing message. Your draft is kept."));
                return;
            }
            const auto job_id = ToHex(*message_id);
            const auto accept = [&](const cybou::PublicationJobResult& job, CybouMailItem outgoing) {
                if (job.phase == cybou::PublicationJobPhase::NEEDS_ATTENTION) {
                    fail(tr("The message needs attention. Your draft is kept; retry continues the same publication."));
                    return;
                }
                outgoing.id = QString::fromStdString(job_id);
                outgoing.state = StateOf(job);
                outgoing.operation_state = OperationOf(job);
                s.mail.outbox[job_id] = outgoing;
                const bool removed = !draft_id.isEmpty() && s.application->DeleteDraft(draft_id.toStdString(), true);
                s.StateToGui([owner = s.owner, client_id, draft_id, removed, outgoing, progress] {
                    owner->m_pending_sends.remove(client_id);
                    owner->m_send_drafts.remove(client_id);
                    if (removed) {
                        owner->m_pending_drafts.remove(draft_id);
                        owner->m_known_drafts.remove(draft_id);
                        owner->m_deleted_drafts.insert(draft_id);
                        Q_EMIT owner->mailItemRemoved(draft_id);
                    }
                    Q_EMIT owner->mailItemRemoved(client_id);
                    Q_EMIT owner->mailItemChanged(outgoing);
                    Q_EMIT owner->mailItemReplaced(client_id, outgoing.id);
                    if (progress) progress(CybouCommandState::Committed, {});
                });
            };
            if (s.publication->GetJob(job_id)) {
                const auto job = s.publication->Resume(job_id);
                s.storage_projection.jobs[job_id] = job;
                accept(job, message);
                return;
            }
            const auto recipient = s.mail.ResolveRecipient(message.to_name);
            if (!recipient) {
                fail(tr("No CYBOU Identity has this name. Your draft is kept."));
                return;
            }
            cybou::MailMessage mail;
            mail.message_id = *message_id;
            mail.recipient_account_id = *recipient;
            mail.client_timestamp_ms = static_cast<std::uint64_t>(message.time.toMSecsSinceEpoch());
            mail.subject = message.subject.toStdString();
            mail.body = message.body.toStdString();
            // New local files become encrypted child trees of this publication; Files
            // references reuse their protected content without re-upload.
            std::vector<std::pair<std::size_t, cybou::NewContent>> new_content;
            QString attachment_error;
            for (const auto& attachment : message.attachments) {
                const auto attachment_id = cybou::NewPrivateItemId();
                if (!attachment_id) {
                    attachment_error = tr("Could not prepare the message.");
                    break;
                }
                cybou::MailAttachment descriptor{.attachment_id = *attachment_id, .filename = attachment.name.toStdString(),
                    .logical_size = attachment.logical_size};
                if (!attachment.source_path.isEmpty()) {
                    auto file = std::make_shared<QFile>(attachment.source_path);
                    if (!file->open(QIODevice::ReadOnly)) {
                        attachment_error = tr("%1 could not be read.").arg(attachment.name);
                        break;
                    }
                    new_content.emplace_back(mail.attachments.size(), cybou::NewContent{
                        [file](std::span<unsigned char> out) -> std::optional<std::size_t> {
                            const auto n = file->read(reinterpret_cast<char*>(out.data()), static_cast<qint64>(out.size()));
                            if (n < 0) return std::nullopt;
                            return static_cast<std::size_t>(n);
                        }});
                } else if (const auto reused = s.files.ContentOfAttachmentSource(attachment.id)) {
                    descriptor.root_chunk_id = reused->first;
                    descriptor.content_key = reused->second.first;
                    descriptor.logical_size = reused->second.second;
                } else {
                    attachment_error = tr("%1 is not protected yet.").arg(attachment.name);
                    break;
                }
                mail.attachments.push_back(descriptor);
            }
            if (!attachment_error.isEmpty()) {
                fail(attachment_error);
                return;
            }
            CybouMailItem outgoing = message;
            outgoing.id = QString::fromStdString(job_id);
            outgoing.state = CybouContentState::Local;
            outgoing.operation_state = CybouOperationState::Preparing;
            for (int i = 0; i < outgoing.attachments.size(); ++i) {
                auto& shown = outgoing.attachments[i];
                shown.id = QString::fromStdString(ToHex(mail.attachments[static_cast<std::size_t>(i)].attachment_id));
                shown.source_path.clear();
                if (!CybouProduct::contentOnNetwork(shown.state)) shown.state = CybouContentState::Local;
            }
            const auto job = s.publication->PublishMail(job_id, std::move(mail), std::move(new_content));
            s.storage_projection.jobs[job_id] = job;
            // GetJob verifies durable job state; a transient return value is not ownership.
            if (!s.publication->GetJob(job_id)) {
                fail(tr("The outgoing message could not be saved. Your draft is kept."));
                return;
            }
            accept(job, outgoing);
        } catch (const std::exception&) {
            fail(tr("Could not prepare the message. Your draft is kept."));
        }
    });
}

void CybouCoreApplicationAdapter::retryMail(const QString& id)
{
    if (!m_session) return;
    // Refused before publication: send the same message again.
    if (const auto pending = m_pending_sends.find(id); pending != m_pending_sends.end()) {
        CybouMailItem message = *pending;
        sendMail(message, m_send_drafts.value(id));
        return;
    }
    m_session->Post([job_id = id.toStdString()](IdentitySession& s) { s.storage_projection.jobs[job_id] = s.publication->Resume(job_id); });
}

void CybouCoreApplicationAdapter::setMailRead(const QString& id, bool read)
{
    const auto message_id = FromHex(id);
    if (!m_session || !message_id) return;
    m_session->Post([message_id = *message_id, read](IdentitySession& s) { s.application->SetMailRead(message_id, read); });
}

void CybouCoreApplicationAdapter::setMailStarred(const QString& id, bool starred)
{
    const auto message_id = FromHex(id);
    if (!m_session || !message_id) return;
    m_session->Post([message_id = *message_id, starred](IdentitySession& s) { s.application->SetMailStarred(message_id, starred); });
}

void CybouCoreApplicationAdapter::moveMail(const QString& id, CybouMailFolder folder, CommandProgress progress)
{
    const auto message_id = FromHex(id);
    const auto target = CoreFolder(folder);
    if (!m_session || !message_id || !target) {
        if (progress) progress(CybouCommandState::Failed, tr("This message cannot be moved there."));
        return;
    }
    m_session->Post([message_id = *message_id, target = *target, progress](IdentitySession& s) {
        s.StateToGui([progress] { if (progress) progress(CybouCommandState::Running, {}); });
        bool ok{false};
        try { ok = s.application->MoveMail(message_id, target); } catch (const std::exception&) { }
        // Report the durable local commit without waiting for a full history/storage refresh.
        s.StateToGui([owner = s.owner, progress, ok] {
            const auto error = ok ? QString{} : tr("This message could not be moved. Try again.");
            if (progress) progress(ok ? CybouCommandState::Committed : CybouCommandState::Failed, error);
            else if (!ok) Q_EMIT owner->commandFailed(error);
        });
    });
}

void CybouCoreApplicationAdapter::deleteMailForever(const QStringList& ids)
{
    if (!m_session) return;
    std::vector<cybou::PrivateItemId> messages;
    for (const auto& id : ids) {
        const auto message_id = FromHex(id);
        if (!message_id) continue;
        messages.push_back(*message_id);
        m_deleted_mail.insert(id);
        Q_EMIT mailItemRemoved(id);
    }
    m_session->Post([owner = this, messages = std::move(messages)](IdentitySession& s) {
        bool all{true};
        for (const auto& message : messages) all = s.application->MoveMail(message, cybou::MailFolder::DELETED) && all;
        if (!all) s.ToGui([owner] { Q_EMIT owner->commandFailed(tr("Some messages could not be deleted.")); });
        s.Refresh();
    });
}

void CybouCoreApplicationAdapter::deleteMail(const QString& id)
{
    if (!m_session) return;
    if (m_pending_sends.remove(id) > 0) {
        Q_EMIT mailItemRemoved(id);
        return;
    }
    if (id.startsWith(QStringLiteral("draft-"))) {
        m_pending_drafts.remove(id);
        m_known_drafts.remove(id);
        m_deleted_drafts.insert(id);
        Q_EMIT mailItemRemoved(id);
        m_session->Post([draft_id = id.toStdString()](IdentitySession& s) { s.application->DeleteDraft(draft_id); });
        return;
    }
    // Delivered mail is part of finalized history and rebuilds from it; it stays in Trash.
    moveMail(id, CybouMailFolder::Trash);
}

void CybouCoreApplicationAdapter::downloadAttachment(const QString& message_id, const QString& attachment_id,
    const QString& destination)
{
    const auto message = FromHex(message_id);
    if (!m_session || !message) return;
    m_session->Post([message = *message, message_id, attachment_id, destination](IdentitySession& s) {
        QString error = tr("This attachment is not available.");
        if (const auto record = s.application->GetMail(message)) {
            for (const auto& attachment : record->message.attachments) {
                if (QString::fromStdString(ToHex(attachment.attachment_id)) != attachment_id) continue;
                error = s.storage_projection.DownloadContent(attachment.root_chunk_id, attachment.content_key, attachment.logical_size,
                    destination);
            }
        }
        s.ToGui([owner = s.owner, message_id, attachment_id, error] {
            if (!error.isEmpty()) {
                Q_EMIT owner->attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Idle);
                Q_EMIT owner->commandFailed(error);
                return;
            }
            Q_EMIT owner->attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Ready);
            QMetaObject::invokeMethod(owner, [owner, message_id, attachment_id] {
                Q_EMIT owner->attachmentRetrievalChanged(message_id, attachment_id, CybouRetrievalState::Idle);
            }, Qt::QueuedConnection);
        });
    });
}

void CybouCoreApplicationAdapter::saveAttachmentToFiles(const QString& message_id, const QString& attachment_id,
    const QString& file_id)
{
    const auto message = FromHex(message_id);
    const auto item_id = cybou::NewPrivateItemId();
    if (!m_session || !message || !item_id) return;
    const QString hex = QString::fromStdString(ToHex(*item_id));
    m_client_ids.insert(file_id, hex);
    m_session->Post([message = *message, attachment_id, item_id = *item_id](IdentitySession& s) {
        const auto record = s.application->GetMail(message);
        if (!record) return;
        for (const auto& attachment : record->message.attachments) {
            if (QString::fromStdString(ToHex(attachment.attachment_id)) != attachment_id) continue;
            // A new Files entry referencing the same protected content: no download, no upload.
            cybou::FilesMutationBatch batch;
            batch.mutations.push_back({cybou::FileMutationKind::UPSERT_ITEM, item_id,
                cybou::FileItem{.item_id = item_id, .kind = cybou::FileItemKind::FILE, .name = attachment.filename,
                    .logical_size = attachment.logical_size, .root_chunk_id = attachment.root_chunk_id,
                    .content_key = attachment.content_key}});
            if (!s.files.PublishFileChange(std::move(batch), std::nullopt)) {
                s.ToGui([owner = s.owner] { Q_EMIT owner->commandFailed(tr("The attachment could not be saved to Files.")); });
            }
            return;
        }
    });
}

