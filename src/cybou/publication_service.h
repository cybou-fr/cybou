// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PUBLICATION_SERVICE_H
#define CYBOU_PUBLICATION_SERVICE_H

#include <cybou/identity_operation_coordinator.h>
#include <cybou/private_application_schema.h>
#include <cybou/private_application_store.h>
#include <cybou/encrypted_chunk_tree.h>

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cybou {

class CybouNodeRuntime;
class StorageService;

struct PreparedPublicationBundle {
    ChunkId root_chunk_id{};
    ContentKey content_key{};
    ChunkId chunk_authorization_root{};
    std::uint32_t chunk_count{0};
};

enum class PublicationJobPhase : std::uint8_t {
    WAITING_FINALITY = 1,
    SECURING = 2,
    NEEDS_ATTENTION = 3,
    /** Finalized and every chunk has reached the remote replica target. */
    PROTECTED = 4,
    /** Waiting for an earlier Identity operation; capsules are built at submission. */
    QUEUED = 5,
};

struct PublicationJobResult {
    PublicationJobPhase phase{PublicationJobPhase::NEEDS_ATTENTION};
    cybou::Hash256 operation_id;
    std::uint64_t finalized_height{0};
    std::string error;
    /** 0..100 toward the remote replica target while SECURING; -1 when unknown. */
    int durability_percent{-1};
};

/** New local content to encrypt into its own tree within the publication. */
struct NewContent {
    EncryptedTreeSource source;
};

/** Random 32-byte private identifier for messages, attachments and Files items. */
std::optional<PrivateItemId> NewPrivateItemId();

/** Submits a prepared local bundle through the durable Identity operation journal.
 *
 * One recipient plus a mandatory owner capsule is supported. The persisted
 * private job is the exact RootPublication intent; after finality it awaits
 * StorageService to establish remote durability.
 */
class PublicationService final {
public:
    PublicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
        PrivateApplicationStore& application_db, IdentityOperationCoordinator& coordinator);
    PublicationJobResult SubmitPrepared(std::string_view local_job_id,
        const PreparedPublicationBundle& bundle,
        std::optional<AccountId> recipient = std::nullopt);
    PublicationJobResult Resume(std::string_view local_job_id);
    /** Cancels a never-submitted queued job, or a job whose operation is
     * explicitly known rejected. Pending/uncertain/finalized work is retained. */
    bool CancelPublication(std::string_view local_job_id);
    std::optional<PublicationJobResult> GetJob(std::string_view local_job_id);
    /** Records StorageService's report that remote durability is met. Only a
     * finalized (SECURING) job can become PROTECTED. */
    bool MarkProtected(std::string_view local_job_id);

    /**
     * Mail: encrypts new attachments into child trees, fills their root/key
     * references into the message, and publishes the MAIL_MESSAGE root with a
     * recipient capsule and a self capsule. Attachments that already carry a
     * protected root/key are referenced as-is (no re-upload).
     * new_attachments[i].first is an index into message.attachments.
     * Repeating a job ID resumes the recorded publication.
     */
    PublicationJobResult PublishMail(std::string_view local_job_id, MailMessage message,
        std::vector<std::pair<std::size_t, NewContent>> new_attachments = {});
    /**
     * Files: publishes one FILES_MUTATION_BATCH with a self capsule. For each
     * new_content entry the referenced UPSERT item receives the new tree's
     * root/key and logical size.
     */
    PublicationJobResult PublishFiles(std::string_view local_job_id, FilesMutationBatch batch,
        std::vector<std::pair<std::size_t, NewContent>> new_content = {});

    /**
     * Step one of a safe IdentityRotate: publishes an IDENTITY_RECOVERY_BRIDGE
     * with every known KEM seed, readable by the current key and by the KEM
     * key the new recovery entropy will publish at the next epoch.
     */
    PublicationJobResult PublishRecoveryBridge(std::string_view local_job_id,
        std::span<const unsigned char, 32> new_recovery_entropy);
    /**
     * True only when the bridge job is PROTECTED and its finalized root opens
     * with the new entropy and decodes to this Identity's complete bridge.
     * IdentityRotate must not be submitted before this returns true.
     */
    bool VerifyRecoveryBridge(std::string_view local_job_id, std::span<const unsigned char, 32> new_recovery_entropy,
        StorageService& storage);

    /** Local job IDs known to this Identity, oldest first. */
    std::vector<std::string> Jobs();
    /** Advances every unfinished job: finality, then StorageService placement. */
    std::vector<std::pair<std::string, PublicationJobResult>> ProcessDurability(StorageService& storage);

private:
    struct Job;
    /** What to publish, independent of the nonce it is eventually signed with. */
    struct Intent {
        PreparedPublicationBundle bundle;
        std::optional<AccountId> recipient;
        std::optional<std::pair<XWingPublicKey, std::uint64_t>> future_self;
    };
    std::optional<Intent> LoadIntent(std::string_view local_job_id) const;
    bool SaveIntent(std::string_view local_job_id, const Intent& intent);
    PublicationJobResult BuildAndSubmit(std::string_view local_job_id, const Intent& intent);
    struct Staged {
        PreparedPublicationBundle bundle;
        std::vector<ChunkId> leaves;
    };
    using BuildMetadata = std::function<std::optional<std::vector<unsigned char>>(
        std::span<const EncryptedTreeSummary> children)>;
    std::optional<Job> Load(std::string_view local_job_id) const;
    bool IsCancellationPending(std::string_view local_job_id) const;
    bool FinishCancellation(std::string_view local_job_id);
    bool Save(std::string_view local_job_id, const Job& job);
    PublicationJobResult ResumeLocked(std::string_view local_job_id, Job& job);
    PublicationJobResult SubmitPreparedLocked(std::string_view local_job_id,
        const PreparedPublicationBundle& bundle, std::optional<AccountId> recipient,
        std::optional<std::pair<XWingPublicKey, std::uint64_t>> future_self);
    std::optional<Staged> Stage(std::string_view local_job_id, std::vector<NewContent>& children,
        const BuildMetadata& build_metadata, std::string& error);
    std::optional<std::vector<ChunkId>> LoadLeaves(std::string_view local_job_id) const;

    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_identity;
    PrivateApplicationStore& m_application_db;
    IdentityOperationCoordinator& m_coordinator;
    std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_PUBLICATION_SERVICE_H
