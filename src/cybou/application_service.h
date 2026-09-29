// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_APPLICATION_SERVICE_H
#define CYBOU_APPLICATION_SERVICE_H

#include <cybou/private_application_schema.h>
#include <cybou/private_application_store.h>
#include <cybou/root_publication.h>

#include <uint256.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

namespace cybou {

class CybouKeyStore;
class CybouNodeRuntime;
class StorageService;

/** Local mailbox placement. Archive and Trash are local, not published. */
enum class MailFolder : std::uint8_t { INBOX = 1, SENT = 2, ARCHIVE = 3, TRASH = 4 };

struct MailRecord {
    uint256 operation_id;
    std::uint64_t finalized_height{0};
    std::uint32_t operation_index{0};
    /** From the outer authorized publication, never from decrypted content. */
    AccountId sender;
    bool outgoing{false};
    MailMessage message;
    MailFolder folder{MailFolder::INBOX};
    bool read{false};
    bool starred{false};
};

/** Canonical publication order of the mutation that produced an item's state. */
struct PrivateOrder {
    std::uint64_t height{0};
    std::uint32_t operation_index{0};
    std::uint32_t mutation_index{0};
    auto operator<=>(const PrivateOrder&) const = default;
};

struct FileRecord {
    FileItem item;
    uint256 operation_id;
    PrivateOrder order;
    bool deleted{false};
};

enum class AccessibleRootState : std::uint8_t {
    DISCOVERED = 1,
    INDEXED = 2,
    /** Root content could not be fetched yet; retried without blocking the scan. */
    TEMPORARILY_UNAVAILABLE = 3,
    /** Opened but not a valid private document for this Identity; never retried. */
    INVALID = 4,
};

/** Device-local attachment of an unsent draft: a local file or a Files reference. */
struct DraftAttachment {
    std::string name;
    std::uint64_t logical_size{0};
    std::string source_path;   ///< local file chosen on this device
    std::string reference_id;  ///< Files reference, e.g. "ref-<item>"
    bool operator==(const DraftAttachment&) const = default;
};

/**
 * Unsent Mail compose state. Drafts live only in the encrypted Application DB
 * of this device; they are never published and are not rebuilt from history.
 */
struct MailDraft {
    std::string draft_id; ///< [a-z0-9-], at most 64 characters
    std::string to;
    std::string subject;
    std::string body;
    std::uint64_t updated_ms{0};
    std::vector<DraftAttachment> attachments;
    bool operator==(const MailDraft&) const = default;
};

struct ApplicationScanProgress {
    std::uint64_t scanned_height{0};
    std::uint64_t finalized_height{0};
    std::uint32_t unavailable_roots{0};
    bool Complete() const { return scanned_height >= finalized_height && unavailable_roots == 0; }
};

/** Trash is represented privately as this reserved Files parent. */
PrivateItemId FilesTrashParent();

/**
 * Inbound discovery and private indexing for one unlocked Identity.
 *
 * Scans canonical finalized blocks for AuthorizedRootPublications, tries
 * this Identity's recoverable KEM epochs on each capsule, and records only
 * publications that open. Roots are fetched through StorageService and
 * decoded as private schemas into the encrypted Application DB. Processing is
 * idempotent per OperationID and resumes from a persisted checkpoint. The
 * whole projection is rebuildable from finalized history and storage.
 */
class ApplicationService final {
public:
    ApplicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
        PrivateApplicationStore& application_db, StorageService& storage);

    /** Scans up to max_blocks new finalized blocks and retries unavailable roots. */
    ApplicationScanProgress Scan(std::uint64_t max_blocks = 256);
    ApplicationScanProgress Progress();

    std::vector<MailRecord> ListMail();
    std::optional<MailRecord> GetMail(const PrivateItemId& message_id);
    /** Local mailbox state; never published. */
    bool SetMailRead(const PrivateItemId& message_id, bool read);
    bool SetMailStarred(const PrivateItemId& message_id, bool starred);
    bool MoveMail(const PrivateItemId& message_id, MailFolder folder);

    /** Current Files catalog (deleted items excluded). */
    std::vector<FileRecord> ListFiles();
    std::optional<FileRecord> GetFile(const PrivateItemId& item_id);

    /** Device-local drafts, newest first. */
    bool SaveDraft(const MailDraft& draft);
    std::vector<MailDraft> ListDrafts();
    bool DeleteDraft(std::string_view draft_id);

    /** Own RecoveryBridges in canonical order (for historical KEM recovery). */
    std::vector<IdentityRecoveryBridge> RecoveryBridges();

    std::optional<AccessibleRootState> PublicationState(const uint256& operation_id);

private:
    struct Accessible;
    bool ProcessBlock(std::uint64_t height, std::uint64_t my_key_epoch);
    bool ProcessPublication(std::uint64_t height, std::uint32_t index, const uint256& operation_id,
        const AuthorizedRootPublication& publication, std::uint64_t my_key_epoch);
    AccessibleRootState Index(const uint256& operation_id, Accessible& accessible);
    bool ApplyMail(const uint256& operation_id, const Accessible& accessible, const MailMessage& message);
    bool ApplyFiles(const uint256& operation_id, const Accessible& accessible, const FilesMutationBatch& batch);
    bool ApplyBridge(const uint256& operation_id, const Accessible& accessible, const IdentityRecoveryBridge& bridge);
    std::optional<Accessible> LoadAccessible(const uint256& operation_id) const;
    bool SaveAccessible(const uint256& operation_id, const Accessible& accessible);
    std::optional<MailRecord> LoadMail(const PrivateItemId& id) const;
    bool SaveMail(const MailRecord& record);
    std::optional<FileRecord> LoadFile(const PrivateItemId& id) const;
    bool SaveFile(const FileRecord& record);
    std::uint64_t Checkpoint() const;
    /** Imports verified historical KEM seeds; true when an epoch is recovered for the first time. */
    bool ImportBridgeSeeds(const AccountId& me, std::uint64_t my_key_epoch);

    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_identity;
    PrivateApplicationStore& m_application_db;
    StorageService& m_storage;
    std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_APPLICATION_SERVICE_H
