// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/publication_service.h>

#include <cybou/node_runtime.h>
#include <cybou/root_publication.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string>

namespace cybou {
namespace {

constexpr std::array<unsigned char, 5> MAGIC{'C', 'Y', 'P', 'J', 1};
constexpr std::size_t FIXED_SIZE{5 + 32 + 32 + 1 + 8 + 8 + 32 + 4};

bool ValidJobId(const std::string_view value)
{
    return !value.empty() && value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](const char ch) {
            return (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-';
        });
}

std::string JobKey(const std::string_view id)
{
    return "publication/job/" + std::string{id};
}

void Append64(std::vector<unsigned char>& out, const std::uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

void Append32(std::vector<unsigned char>& out, const std::uint32_t value)
{
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

std::uint64_t Read64(const std::span<const unsigned char> bytes)
{
    std::uint64_t value{0};
    for (unsigned i{0}; i < 8; ++i) value |= std::uint64_t{bytes[i]} << (8 * i);
    return value;
}

std::uint32_t Read32(const std::span<const unsigned char> bytes)
{
    std::uint32_t value{0};
    for (unsigned i{0}; i < 4; ++i) value |= std::uint32_t{bytes[i]} << (8 * i);
    return value;
}

PublicationJobResult Failure(std::string message)
{
    return {.phase = PublicationJobPhase::NEEDS_ATTENTION, .error = std::move(message)};
}

} // namespace

struct PublicationService::Job {
    uint256 network_id;
    AccountId account_id;
    PublicationJobPhase phase{PublicationJobPhase::WAITING_FINALITY};
    std::uint64_t nonce{0};
    std::uint64_t key_epoch{0};
    uint256 operation_id;
    RootPublication publication;
};

PublicationService::PublicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
    PrivateApplicationStore& application_db, IdentityOperationCoordinator& coordinator)
    : m_runtime{runtime}, m_identity{identity}, m_application_db{application_db},
      m_coordinator{coordinator}
{
}

std::optional<PublicationService::Job> PublicationService::Load(const std::string_view local_job_id) const
{
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return std::nullopt;
    const auto encoded = m_application_db.Get(JobKey(local_job_id));
    if (!encoded || encoded->size() < FIXED_SIZE ||
        !std::equal(MAGIC.begin(), MAGIC.end(), encoded->begin())) return std::nullopt;
    Job job;
    std::size_t offset{MAGIC.size()};
    std::copy_n(encoded->begin() + offset, 32, job.network_id.begin()); offset += 32;
    const auto account = AccountId::FromBytes(std::span<const unsigned char>{*encoded}.subspan(offset, 32));
    offset += 32;
    if (!account || job.network_id != m_runtime.GetNetworkId() ||
        *account != m_application_db.Account() || m_identity.GetAccountId() != account) return std::nullopt;
    job.account_id = *account;
    const auto phase = (*encoded)[offset++];
    if (phase < static_cast<std::uint8_t>(PublicationJobPhase::WAITING_FINALITY) ||
        phase > static_cast<std::uint8_t>(PublicationJobPhase::NEEDS_ATTENTION)) return std::nullopt;
    job.phase = static_cast<PublicationJobPhase>(phase);
    job.nonce = Read64(std::span<const unsigned char>{*encoded}.subspan(offset, 8)); offset += 8;
    job.key_epoch = Read64(std::span<const unsigned char>{*encoded}.subspan(offset, 8)); offset += 8;
    std::copy_n(encoded->begin() + offset, 32, job.operation_id.begin()); offset += 32;
    const auto publication_size = Read32(std::span<const unsigned char>{*encoded}.subspan(offset, 4)); offset += 4;
    if (publication_size == 0 || publication_size > ROOT_PUBLICATION_MAX_BYTES ||
        encoded->size() != offset + publication_size) return std::nullopt;
    const auto publication = DeserializeRootPublication(
        std::span<const unsigned char>{*encoded}.subspan(offset, publication_size));
    if (!publication) return std::nullopt;
    job.publication = *publication;
    return job;
}

bool PublicationService::Save(const std::string_view local_job_id, const Job& job)
{
    if (!ValidJobId(local_job_id) || job.account_id != m_application_db.Account() ||
        job.network_id != m_runtime.GetNetworkId()) return false;
    const auto publication = SerializeRootPublication(job.publication);
    if (!publication || publication->empty() || publication->size() > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    std::vector<unsigned char> encoded(MAGIC.begin(), MAGIC.end());
    encoded.insert(encoded.end(), job.network_id.begin(), job.network_id.end());
    encoded.insert(encoded.end(), job.account_id.Value().begin(), job.account_id.Value().end());
    encoded.push_back(static_cast<unsigned char>(job.phase));
    Append64(encoded, job.nonce);
    Append64(encoded, job.key_epoch);
    encoded.insert(encoded.end(), job.operation_id.begin(), job.operation_id.end());
    Append32(encoded, static_cast<std::uint32_t>(publication->size()));
    encoded.insert(encoded.end(), publication->begin(), publication->end());
    return m_application_db.Put(JobKey(local_job_id), encoded);
}

PublicationJobResult PublicationService::SubmitPrepared(const std::string_view local_job_id,
    const PreparedPublicationBundle& bundle, const std::optional<AccountId> recipient)
{
    std::lock_guard lock{m_mutex};
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return Failure("Application DB is locked or job ID invalid");
    if (auto existing = Load(local_job_id)) return ResumeLocked(local_job_id, *existing);
    if (m_application_db.Has(JobKey(local_job_id))) return Failure("Existing publication job is corrupt");
    const auto account = m_identity.GetAccountId();
    if (!account || *account != m_application_db.Account() ||
        bundle.root_chunk_id == ChunkId{} || bundle.chunk_authorization_root == ChunkId{} ||
        bundle.chunk_count == 0 || bundle.content_key == ContentKey{} ||
        !m_runtime.GetChunkBlobStore().Has(bundle.root_chunk_id)) {
        return Failure("Prepared publication or unlocked Identity is invalid");
    }
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* sender = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    if (!sender || sender->nonce == std::numeric_limits<std::uint64_t>::max()) {
        return Failure("Publisher Identity is not finalized or nonce is exhausted");
    }
    const auto self_public = m_identity.GetIdentityXWingPublicKey();
    const auto self_package = self_public ? EncodeIdentityKemPackage(*self_public) : std::nullopt;
    const auto network_bytes = std::span<const unsigned char, 32>{m_runtime.GetNetworkId().begin(), 32};
    const auto account_bytes = std::span<const unsigned char, 32>{account->Value().begin(), 32};
    const auto self_commitment = self_package ? ComputeIdentityKemPackageCommitment(
        network_bytes, account_bytes, sender->key_epoch, *self_package) : std::nullopt;
    if (!self_public || !self_commitment || *self_commitment != sender->kem_package_id) {
        return Failure("Publisher KEM capability does not match finalized Identity");
    }

    RootPublication publication;
    publication.root_chunk_id = bundle.root_chunk_id;
    publication.chunk_authorization_root = bundle.chunk_authorization_root;
    publication.chunk_count = bundle.chunk_count;
    if (recipient && *recipient != *account) {
        const auto* recipient_record = loaded.state->identities.Find(*recipient);
        if (!recipient_record) return Failure("Recipient Identity is not finalized");
        const auto found = m_runtime.FindIdentityKemPackage(*recipient, recipient_record->key_epoch);
        const auto recipient_public = found.status == IdentityKemPackageLookupStatus::FOUND &&
            found.package_id == recipient_record->kem_package_id ?
            DecodeIdentityKemPackage(found.package) : std::nullopt;
        if (!recipient_public) return Failure("Recipient KEM capability is unavailable");
        const auto capsule = CreateRootRecipientCapsule(network_bytes, account_bytes,
            sender->nonce, sender->key_epoch, bundle.root_chunk_id, *recipient_public,
            recipient_record->key_epoch, bundle.content_key);
        if (!capsule) return Failure("Cannot create recipient capsule");
        publication.recipient_capsules.push_back(*capsule);
    }
    const auto self_capsule = CreateRootRecipientCapsule(network_bytes, account_bytes,
        sender->nonce, sender->key_epoch, bundle.root_chunk_id, *self_public,
        sender->key_epoch, bundle.content_key);
    if (!self_capsule) return Failure("Cannot create owner recovery capsule");
    publication.recipient_capsules.push_back(*self_capsule);
    if (!ComputeRootPublicationPayloadCommitment(publication)) {
        return Failure("Cannot commit RootPublication payload");
    }

    Job job{.network_id = m_runtime.GetNetworkId(), .account_id = *account,
        .phase = PublicationJobPhase::WAITING_FINALITY, .nonce = sender->nonce,
        .key_epoch = sender->key_epoch, .publication = std::move(publication)};
    if (!Save(local_job_id, job)) return Failure("Cannot save private publication job");
    return ResumeLocked(local_job_id, job);
}

PublicationJobResult PublicationService::ResumeLocked(const std::string_view local_job_id, Job& job)
{
    if (job.phase == PublicationJobPhase::SECURING) {
        return {.phase = job.phase, .operation_id = job.operation_id};
    }
    if (job.phase == PublicationJobPhase::NEEDS_ATTENTION) {
        return {.phase = job.phase, .operation_id = job.operation_id,
            .error = "Publication needs attention before retry"};
    }
    if (!job.operation_id.IsNull()) {
        const auto finalized = m_runtime.FindFinalizedRootPublication(job.operation_id);
        if (finalized) {
            if (*finalized != job.publication) return Failure("Finalized publication differs from private job");
            job.phase = PublicationJobPhase::SECURING;
            if (!Save(local_job_id, job)) return Failure("Cannot save finalized publication state");
            return {.phase = job.phase, .operation_id = job.operation_id,
                .finalized_height = m_runtime.FindFinalizedOperation(job.operation_id).height};
        }
        if (m_runtime.GetOperationStatus(job.operation_id).kind == OperationStatusKind::REJECTED_KNOWN) {
            job.phase = PublicationJobPhase::NEEDS_ATTENTION;
            Save(local_job_id, job);
            return Failure("RootPublication was rejected");
        }
    }
    const auto commitment = ComputeRootPublicationPayloadCommitment(job.publication);
    if (!commitment) return Failure("Saved RootPublication is invalid");
    const auto result = m_coordinator.Execute(IdentityOperationKind::ROOT_PUBLICATION, *commitment,
        [&](const IdentityOperationAuthorization& authorization) -> std::optional<ProtocolOperation> {
            if (authorization.account_id != job.account_id || authorization.nonce != job.nonce ||
                authorization.key_epoch != job.key_epoch) return std::nullopt;
            ProtocolOperation operation{AuthorizedRootPublication{authorization, job.publication}};
            const auto op_id = ComputeOperationId(operation);
            if (!op_id) return std::nullopt;
            job.operation_id = *op_id;
            if (!Save(local_job_id, job)) return std::nullopt;
            return operation;
        });
    if (result.phase == IdentityOperationPhase::FINALIZED) {
        const auto finalized = m_runtime.FindFinalizedRootPublication(result.op_id);
        if (!finalized || *finalized != job.publication) return Failure("Cannot verify finalized RootPublication");
        job.operation_id = result.op_id;
        job.phase = PublicationJobPhase::SECURING;
        if (!Save(local_job_id, job)) return Failure("Cannot save finalized publication state");
        return {.phase = job.phase, .operation_id = job.operation_id,
            .finalized_height = result.finalized_height};
    }
    if (result.phase == IdentityOperationPhase::ACCEPTED ||
        result.phase == IdentityOperationPhase::UNCERTAIN) {
        job.operation_id = result.op_id;
        if (!Save(local_job_id, job)) return Failure("Cannot save pending publication state");
        return {.phase = PublicationJobPhase::WAITING_FINALITY, .operation_id = result.op_id};
    }
    job.phase = PublicationJobPhase::NEEDS_ATTENTION;
    Save(local_job_id, job);
    return {.phase = job.phase, .operation_id = result.op_id, .error = result.error};
}

PublicationJobResult PublicationService::Resume(const std::string_view local_job_id)
{
    std::lock_guard lock{m_mutex};
    auto job = Load(local_job_id);
    if (!job) return Failure("Publication job is absent, corrupt, or Identity is locked");
    return ResumeLocked(local_job_id, *job);
}

std::optional<PublicationJobResult> PublicationService::GetJob(const std::string_view local_job_id)
{
    std::lock_guard lock{m_mutex};
    auto job = Load(local_job_id);
    if (!job) return std::nullopt;
    if (job->phase == PublicationJobPhase::WAITING_FINALITY && !job->operation_id.IsNull()) {
        const auto finalized = m_runtime.FindFinalizedRootPublication(job->operation_id);
        if (finalized && *finalized == job->publication) {
            job->phase = PublicationJobPhase::SECURING;
            if (!Save(local_job_id, *job)) return std::nullopt;
        }
    }
    return PublicationJobResult{.phase = job->phase, .operation_id = job->operation_id,
        .finalized_height = job->phase == PublicationJobPhase::SECURING ?
            m_runtime.FindFinalizedOperation(job->operation_id).height : 0};
}

} // namespace cybou
