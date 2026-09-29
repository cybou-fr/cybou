// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PUBLICATION_SERVICE_H
#define CYBOU_PUBLICATION_SERVICE_H

#include <cybou/identity_operation_coordinator.h>
#include <cybou/private_application_store.h>
#include <cybou/publication_bundle_stager.h>

#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace cybou {

class CybouNodeRuntime;

enum class PublicationJobPhase : std::uint8_t {
    WAITING_FINALITY = 1,
    SECURING = 2,
    NEEDS_ATTENTION = 3,
};

struct PublicationJobResult {
    PublicationJobPhase phase{PublicationJobPhase::NEEDS_ATTENTION};
    uint256 operation_id;
    std::uint64_t finalized_height{0};
    std::string error;
};

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
    std::optional<PublicationJobResult> GetJob(std::string_view local_job_id);

private:
    struct Job;
    std::optional<Job> Load(std::string_view local_job_id) const;
    bool Save(std::string_view local_job_id, const Job& job);
    PublicationJobResult ResumeLocked(std::string_view local_job_id, Job& job);

    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_identity;
    PrivateApplicationStore& m_application_db;
    IdentityOperationCoordinator& m_coordinator;
    std::mutex m_mutex;
};

} // namespace cybou

#endif // CYBOU_PUBLICATION_SERVICE_H
