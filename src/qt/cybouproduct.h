// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef BITCOIN_QT_CYBOUPRODUCT_H
#define BITCOIN_QT_CYBOUPRODUCT_H

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QVector>

/**
 * UI-level product types for the Identity-centric desktop.
 *
 * These describe what the user sees (Identity, Mail, Files, Wallet), not the
 * protocol. Pages consume only these types; core adapters and UI fixtures
 * fill them. Identifiers are opaque strings that pages never parse.
 */

/** Identity lifecycle as surfaced to the UI (one Identity per user). */
enum class CybouIdentityState {
    None,
    Creating,
    Restoring,
    Syncing,
    Active,
    Locked,
    NeedsAttention,
};

/** Sub-step shown while an Identity is being created. */
enum class CybouIdentityStep {
    PreparingKeys,
    CreatingIdentity,
    WaitingForConfirmation,
};

/**
 * Finality-first content lifecycle shared by Mail and Files
 * (docs/cybou/82 §0, 83 §0). Finalized is not Protected: content is
 * Securing between PoA finality and the durability threshold.
 */
enum class CybouContentState {
    Local,
    Preparing,
    WaitingForConfirmation,
    Securing,
    /** This Identity's own content reached its remote durability target. */
    Protected,
    /**
     * Incoming content: finalized, end-to-end encrypted and opened here. The
     * sender's remote durability is not known to the recipient, so this is
     * never shown as Protected.
     */
    Received,
    TemporarilyUnavailable,
    NeedsAttention,
};

/** Download/retrieval progress for protected content. */
enum class CybouRetrievalState {
    Idle,
    Downloading,
    Verifying,
    Decrypting,
    Ready,
};

/** User-facing result of an Identity operation such as IdentityRotate. */
enum class CybouOperationOutcome {
    Finalized,
    Pending,
    Failed,
};

/** Restore progress rows shown after a mnemonic restore. */
enum class CybouRestoreStepState {
    Pending,
    Running,
    Done,
};

struct CybouRestoreProgress {
    CybouRestoreStepState identity{CybouRestoreStepState::Pending};
    CybouRestoreStepState wallet{CybouRestoreStepState::Pending};
    CybouRestoreStepState names{CybouRestoreStepState::Pending};
    CybouRestoreStepState mail{CybouRestoreStepState::Pending};
    CybouRestoreStepState files{CybouRestoreStepState::Pending};
};

struct CybouAttachmentItem {
    QString id; ///< opaque content reference
    QString name;
    quint64 logical_size{0};
    CybouContentState state{CybouContentState::Local};
    int progress_percent{-1}; ///< -1 when no meaningful percentage exists
    CybouRetrievalState retrieval{CybouRetrievalState::Idle};
    /** Files item created by "Save to Files" for this attachment, if any. */
    QString saved_file_id;
    /** Local file chosen in Compose; device-local, never shown or published. */
    QString source_path;
};

enum class CybouMailFolder {
    Inbox,
    Sent,
    Drafts,
    Archive,
    Trash,
};

struct CybouMailItem {
    QString id; ///< opaque
    CybouMailFolder folder{CybouMailFolder::Inbox};
    QString from_name;
    QString to_name;
    QString subject;
    QString preview;
    QString body;
    QDateTime time;
    bool unread{false};
    bool starred{false};
    bool draft{false};
    CybouContentState state{CybouContentState::Protected};
    QVector<CybouAttachmentItem> attachments;
    /** Evidence for Security Details → Advanced; empty until reported. */
    QString operation_id;
    quint64 finalized_height{0};
    QString root_chunk_id;
};

struct CybouFileItem {
    QString id; ///< opaque UI/backend id; never parsed as a hash
    QString name;
    QString parent_id; ///< empty for My files root
    quint64 logical_size{0};
    QDateTime modified;
    bool folder{false};
    bool starred{false};
    bool trashed{false};
    CybouContentState state{CybouContentState::Local};
    int progress_percent{-1};
    CybouRetrievalState retrieval{CybouRetrievalState::Idle};
    /** Decrypted content can be opened on this device without fetching.
        Independent of Protected, which is about network durability. */
    bool available_offline{false};
    /** Advanced details only; empty until the backend reports them. */
    QString content_root_id;
    quint64 finalized_height{0};
};

enum class CybouActivityKind {
    MailReceived,
    MailSent,
    FileUploaded,
    PaymentSent,
    PaymentReceived,
    ServiceFee,
    OnboardingCredit,
    IdentitySynced,
};

struct CybouActivityItem {
    CybouActivityKind kind{CybouActivityKind::IdentitySynced};
    QString title;
    QString subtitle;
    QDateTime time;
};

enum class CybouWalletEntryKind {
    Received,
    Sent,
    NetworkServiceFee,
    OnboardingCredit,
    MovedToSystemBalance,
};

struct CybouWalletEntry {
    QString id;
    CybouWalletEntryKind kind{CybouWalletEntryKind::Received};
    qint64 amount{0}; ///< signed whole CYBOU
    bool system_side{false};
    QString counterparty_name;
    QDateTime time;
    bool pending{false};
};

struct CybouNameItem {
    QString name; ///< full label, e.g. stan.cybou
    bool primary{false};
};

/** Contact suggestion for .cybou autocomplete. */
struct CybouContact {
    QString display_name;
    QString name; ///< alice.cybou
    bool verified{true};
};

namespace CybouProduct {

inline QString contentStateText(CybouContentState state)
{
    switch (state) {
    case CybouContentState::Local: return QCoreApplication::translate("CybouProduct", "Local");
    case CybouContentState::Preparing: return QCoreApplication::translate("CybouProduct", "Preparing…");
    case CybouContentState::WaitingForConfirmation: return QCoreApplication::translate("CybouProduct", "Waiting for confirmation…");
    case CybouContentState::Securing: return QCoreApplication::translate("CybouProduct", "Securing…");
    case CybouContentState::Protected: return QCoreApplication::translate("CybouProduct", "Protected");
    case CybouContentState::Received: return QCoreApplication::translate("CybouProduct", "Received");
    case CybouContentState::TemporarilyUnavailable: return QCoreApplication::translate("CybouProduct", "Temporarily unavailable");
    case CybouContentState::NeedsAttention: return QCoreApplication::translate("CybouProduct", "Needs attention");
    }
    return {};
}

/**
 * State text that accounts for connectivity: pending content that cannot
 * progress while offline reads "Waiting for network".
 */
inline QString contentStateText(CybouContentState state, bool online)
{
    if (!online && (state == CybouContentState::Preparing || state == CybouContentState::WaitingForConfirmation))
        return QCoreApplication::translate("CybouProduct", "Waiting for network");
    return contentStateText(state);
}

/** Attachment/file progress text, e.g. "Securing 42%". */
inline QString progressText(CybouContentState state, int percent, bool online = true)
{
    if (state == CybouContentState::Securing && percent >= 0)
        return QCoreApplication::translate("CybouProduct", "Securing %1%").arg(percent);
    return contentStateText(state, online);
}

/** Mail send wording: a Protected outgoing message reads as Sent. */
inline QString mailStateText(const CybouMailItem& item)
{
    if (item.draft) return QCoreApplication::translate("CybouProduct", "Draft");
    if (item.folder == CybouMailFolder::Sent && item.state == CybouContentState::Protected)
        return QCoreApplication::translate("CybouProduct", "Sent");
    return contentStateText(item.state);
}

/** Content already on the network: this Identity's Protected content or Received content. */
inline bool contentOnNetwork(CybouContentState state)
{
    return state == CybouContentState::Protected || state == CybouContentState::Received;
}

inline bool contentPending(CybouContentState state)
{
    return state == CybouContentState::Preparing ||
        state == CybouContentState::WaitingForConfirmation ||
        state == CybouContentState::Securing;
}

inline QString retrievalText(CybouRetrievalState state)
{
    switch (state) {
    case CybouRetrievalState::Idle: return {};
    case CybouRetrievalState::Downloading: return QCoreApplication::translate("CybouProduct", "Downloading…");
    case CybouRetrievalState::Verifying: return QCoreApplication::translate("CybouProduct", "Verifying…");
    case CybouRetrievalState::Decrypting: return QCoreApplication::translate("CybouProduct", "Decrypting…");
    case CybouRetrievalState::Ready: return QCoreApplication::translate("CybouProduct", "Ready");
    }
    return {};
}

/**
 * Local availability, independent of protection: a Protected file may be
 * fetched from the network when opened, and an unprotected upload is still
 * available on the device that is uploading it.
 */
inline QString localAvailabilityText(const CybouFileItem& item)
{
    return item.available_offline ? QCoreApplication::translate("CybouProduct", "Available offline")
                                  : QCoreApplication::translate("CybouProduct", "Downloaded when opened");
}

/** List status: protection state, plus "Available offline" once Protected. */
inline QString fileStatusText(const CybouFileItem& item, bool online)
{
    if (item.folder) return {};
    if (item.retrieval != CybouRetrievalState::Idle && item.retrieval != CybouRetrievalState::Ready)
        return retrievalText(item.retrieval);
    const QString state = progressText(item.state, item.progress_percent, online);
    if (item.state == CybouContentState::Protected && item.available_offline)
        return state + QStringLiteral("  ·  ") + localAvailabilityText(item);
    return state;
}

inline QString sizeText(quint64 bytes)
{
    if (bytes < 1024) return QCoreApplication::translate("CybouProduct", "%1 B").arg(bytes);
    const double kb = bytes / 1024.0;
    if (kb < 1024) return QCoreApplication::translate("CybouProduct", "%1 KB").arg(kb, 0, 'f', 0);
    const double mb = kb / 1024.0;
    if (mb < 1024) return QCoreApplication::translate("CybouProduct", "%1 MB").arg(mb, 0, 'f', 1);
    return QCoreApplication::translate("CybouProduct", "%1 GB").arg(mb / 1024.0, 0, 'f', 1);
}

/** Short display form of an opaque account id: 2af3…91bc. */
inline QString shortId(const QString& id)
{
    if (id.size() <= 12) return id;
    return id.left(4) + QStringLiteral("…") + id.right(4);
}

} // namespace CybouProduct

#endif // BITCOIN_QT_CYBOUPRODUCT_H
