// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/publication_service.h>
#include <cybou/support_mail.h>

#include <cybou/chunk_retention.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/identity_kem.h>
#include <cybou/node_runtime.h>
#include <cybou/root_publication.h>
#include <cybou/storage_service.h>

#include <openssl/rand.h>

#include <algorithm>
#include <chrono>
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <variant>

namespace cybou {
namespace {

/** Local pin of a job's staged chunks: opaque Identity holder + job reference. */
RetentionKey JobRetention(const AccountId& account, const std::string_view job_id)
{
    const auto& value = account.Value();
    return {.holder = RetentionTag("CYBOU/RETENTION/IDENTITY/v1", std::span{value.begin(), 32}),
        .reference = RetentionTag("CYBOU/RETENTION/PUBLICATION-JOB/v1",
            std::span{reinterpret_cast<const unsigned char*>(job_id.data()), job_id.size()})};
}

std::uint64_t NowMs()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

constexpr std::array<unsigned char, 5> MAGIC{'C', 'Y', 'P', 'J', 1};
constexpr std::size_t FIXED_SIZE{5 + 32 + 32 + 1 + 8 + 8 + 32 + 4};
constexpr std::string_view JOB_INDEX_KEY{"publication/jobs"};

std::string LeavesKey(const std::string_view id)
{
    return "publication/leaves/" + std::string{id};
}

/** Publication intent including the content key; erased once finalized. */
std::string IntentKey(const std::string_view id)
{
    return "publication/intent/" + std::string{id};
}

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

std::string CancelKey(const std::string_view id)
{
    return "publication/cancel/" + std::string{id};
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
    uint256 network_binding;
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
    if (const auto attempt = m_application_db.Get("publication/staging-attempt")) {
        const std::string job{attempt->begin(), attempt->end()};
        if (!ValidJobId(job)) throw std::runtime_error{"invalid local publication staging marker"};
        if (!m_application_db.Has(JobKey(job)) && !m_application_db.Has(IntentKey(job))) {
            if (!m_runtime.GetChunkRetention().Release(JobRetention(m_application_db.Account(), job), NowMs()) ||
                !m_application_db.Erase(LeavesKey(job))) throw std::runtime_error{"cannot clean interrupted publication pins"};
        }
        if (!m_application_db.Erase("publication/staging-attempt")) throw std::runtime_error{"cannot clear publication staging marker"};
    }

}

std::optional<PublicationService::Job> PublicationService::Load(const std::string_view local_job_id) const
{
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return std::nullopt;
    const auto encoded = m_application_db.Get(JobKey(local_job_id));
    if (!encoded || encoded->size() < FIXED_SIZE ||
        !std::equal(MAGIC.begin(), MAGIC.end(), encoded->begin())) return std::nullopt;
    Job job;
    std::size_t offset{MAGIC.size()};
    std::copy_n(encoded->begin() + offset, 32, job.network_binding.begin()); offset += 32;
    const auto account = AccountId::FromBytes(std::span<const unsigned char>{*encoded}.subspan(offset, 32));
    offset += 32;
    if (!account || job.network_binding != m_runtime.GetNetworkBinding() ||
        *account != m_application_db.Account() || m_identity.GetAccountId() != account) return std::nullopt;
    job.account_id = *account;
    const auto phase = (*encoded)[offset++];
    if (phase < static_cast<std::uint8_t>(PublicationJobPhase::WAITING_FINALITY) ||
        phase > static_cast<std::uint8_t>(PublicationJobPhase::QUEUED)) return std::nullopt;
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
        job.network_binding != m_runtime.GetNetworkBinding()) return false;
    const auto publication = SerializeRootPublication(job.publication);
    if (!publication || publication->empty() || publication->size() > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    std::vector<unsigned char> encoded(MAGIC.begin(), MAGIC.end());
    encoded.insert(encoded.end(), job.network_binding.begin(), job.network_binding.end());
    encoded.insert(encoded.end(), job.account_id.Value().begin(), job.account_id.Value().end());
    encoded.push_back(static_cast<unsigned char>(job.phase));
    Append64(encoded, job.nonce);
    Append64(encoded, job.key_epoch);
    encoded.insert(encoded.end(), job.operation_id.begin(), job.operation_id.end());
    Append32(encoded, static_cast<std::uint32_t>(publication->size()));
    encoded.insert(encoded.end(), publication->begin(), publication->end());
    if (!m_application_db.Put(JobKey(local_job_id), encoded)) return false;
    // Once finalized the exact publication is fixed; the intent (with its key) is no longer needed.
    if (job.phase == PublicationJobPhase::SECURING || job.phase == PublicationJobPhase::PROTECTED) {
        m_application_db.Erase(IntentKey(local_job_id));
    }
    // Job index for resumption and status listing; IDs are short ASCII.
    auto index = m_application_db.Get(JOB_INDEX_KEY).value_or(std::vector<unsigned char>{});
    std::string_view listed{reinterpret_cast<const char*>(index.data()), index.size()};
    for (std::size_t start{0}; start < listed.size();) {
        const auto end = std::min(listed.find('\n', start), listed.size());
        if (listed.substr(start, end - start) == local_job_id) return true;
        start = end + 1;
    }
    index.insert(index.end(), local_job_id.begin(), local_job_id.end());
    index.push_back('\n');
    return m_application_db.Put(JOB_INDEX_KEY, index);
}

PublicationJobResult PublicationService::SubmitPrepared(const std::string_view local_job_id,
    const PreparedPublicationBundle& bundle, const std::optional<AccountId> recipient)
{
    std::lock_guard lock{m_mutex};
    return SubmitPreparedLocked(local_job_id, bundle, recipient, std::nullopt);
}

PublicationJobResult PublicationService::SubmitPreparedLocked(const std::string_view local_job_id,
    const PreparedPublicationBundle& bundle, const std::optional<AccountId> recipient,
    const std::optional<std::pair<XWingPublicKey, std::uint64_t>> future_self)
{
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return Failure("Application DB is locked or job ID invalid");
    if (IsCancellationPending(local_job_id)) return Failure("Publication cancellation is pending cleanup");
    if (auto existing = Load(local_job_id)) return ResumeLocked(local_job_id, *existing);
    if (m_application_db.Has(JobKey(local_job_id))) return Failure("Existing publication job is corrupt");
    const auto account = m_identity.GetAccountId();
    if (!account || *account != m_application_db.Account() ||
        bundle.root_chunk_id == ChunkId{} || bundle.chunk_authorization_root == ChunkId{} ||
        bundle.chunk_count == 0 || bundle.content_key == ContentKey{} ||
        !m_runtime.GetChunkBlobStore().Has(bundle.root_chunk_id)) {
        return Failure("Prepared publication or unlocked Identity is invalid");
    }
    const Intent intent{.bundle = bundle, .recipient = recipient, .future_self = future_self};
    if (!SaveIntent(local_job_id, intent)) return Failure("Cannot save private publication intent");
    return BuildAndSubmit(local_job_id, intent);
}

PublicationJobResult PublicationService::BuildAndSubmit(const std::string_view local_job_id, const Intent& intent)
{
    const auto& bundle = intent.bundle;
    const auto& recipient = intent.recipient;
    const auto& future_self = intent.future_self;
    const auto account = m_identity.GetAccountId();
    if (!account) return Failure("Identity is locked");
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* sender = loaded && loaded.state ? loaded.state->identities.Find(*account) : nullptr;
    if (!sender || sender->nonce == std::numeric_limits<std::uint64_t>::max()) {
        return Failure("Publisher Identity is not finalized or nonce is exhausted");
    }
    const auto self_public = m_identity.GetIdentityXWingPublicKey();
    const auto self_package = self_public ? EncodeIdentityKemPackage(*self_public) : std::nullopt;
    const auto network_bytes = std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32};
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
    if (future_self) {
        // RecoveryBridge: also readable by the next, not yet published, KEM key.
        if (future_self->second != sender->key_epoch + 1) return Failure("Invalid future key epoch");
        const auto future_capsule = CreateRootRecipientCapsule(network_bytes, account_bytes,
            sender->nonce, sender->key_epoch, bundle.root_chunk_id, future_self->first,
            future_self->second, bundle.content_key);
        if (!future_capsule) return Failure("Cannot create future recovery capsule");
        publication.recipient_capsules.push_back(*future_capsule);
    }
    if (const auto support = SupportAccount(*loaded.state); recipient && support && *recipient == *support &&
        *recipient != *account) {
        // Support mail pays the support rate; the network still sees no recipient.
        const auto& params = m_runtime.GetNetworkGenesis().GetProtocolParameters();
        if (!PadPublicationToFee(params, publication, SupportMailMinimumFee(params))) {
            return Failure("Cannot pay the support rate for this message");
        }
    }
    if (!ComputeRootPublicationPayloadCommitment(publication)) {
        return Failure("Cannot commit RootPublication payload");
    }

    Job job{.network_binding = m_runtime.GetNetworkBinding(), .account_id = *account,
        .phase = PublicationJobPhase::WAITING_FINALITY, .nonce = sender->nonce,
        .key_epoch = sender->key_epoch, .publication = std::move(publication)};
    if (!Save(local_job_id, job)) return Failure("Cannot save private publication job");
    return ResumeLocked(local_job_id, job);
}

PublicationJobResult PublicationService::ResumeLocked(const std::string_view local_job_id, Job& job)
{
    if (IsCancellationPending(local_job_id)) return Failure("Publication cancellation is pending cleanup");
    if (job.phase == PublicationJobPhase::QUEUED) {
        // Never signed: rebuild capsules against the current nonce.
        const auto intent = LoadIntent(local_job_id);
        if (!intent) return Failure("Queued publication intent is missing");
        return BuildAndSubmit(local_job_id, *intent);
    }
    if (job.phase == PublicationJobPhase::SECURING || job.phase == PublicationJobPhase::PROTECTED) {
        return {.phase = job.phase, .operation_id = job.operation_id,
            .finalized_height = m_runtime.FindFinalizedOperation(job.operation_id).height};
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
    if (job.operation_id.IsNull() && (result.phase == IdentityOperationPhase::CONFLICT ||
            result.phase == IdentityOperationPhase::REJECTED) && LoadIntent(local_job_id)) {
        // Never signed (another operation holds the nonce, or the nonce moved):
        // wait and rebuild later instead of failing the user's action.
        job.phase = PublicationJobPhase::QUEUED;
        if (!Save(local_job_id, job)) return Failure("Cannot save queued publication");
        return {.phase = job.phase, .error = result.error};
    }
    job.phase = PublicationJobPhase::NEEDS_ATTENTION;
    Save(local_job_id, job);
    return {.phase = job.phase, .operation_id = job.operation_id, .error = result.error};
}

std::optional<PublicationService::Intent> PublicationService::LoadIntent(const std::string_view local_job_id) const
{
    const auto encoded = m_application_db.Get(IntentKey(local_job_id));
    constexpr std::size_t FIXED{32 + 32 + 32 + 4 + 1 + 1};
    if (!encoded || encoded->size() < FIXED) return std::nullopt;
    Intent intent;
    const std::span<const unsigned char> bytes{*encoded};
    std::size_t offset{0};
    std::copy_n(bytes.begin() + offset, 32, intent.bundle.root_chunk_id.begin()); offset += 32;
    std::copy_n(bytes.begin() + offset, 32, intent.bundle.content_key.begin()); offset += 32;
    std::copy_n(bytes.begin() + offset, 32, intent.bundle.chunk_authorization_root.begin()); offset += 32;
    intent.bundle.chunk_count = Read32(bytes.subspan(offset, 4)); offset += 4;
    const bool has_recipient = bytes[offset++] != 0;
    if (has_recipient) {
        if (bytes.size() < offset + 32) return std::nullopt;
        intent.recipient = AccountId::FromBytes(bytes.subspan(offset, 32));
        if (!intent.recipient) return std::nullopt;
        offset += 32;
    }
    if (bytes.size() < offset + 1) return std::nullopt;
    const bool has_future = bytes[offset++] != 0;
    if (has_future) {
        if (bytes.size() < offset + XWING_PUBLIC_KEY_SIZE + 8) return std::nullopt;
        XWingPublicKey key{};
        std::copy_n(bytes.begin() + offset, key.size(), key.begin());
        offset += key.size();
        intent.future_self = std::pair{key, Read64(bytes.subspan(offset, 8))};
        offset += 8;
    }
    if (offset != bytes.size()) return std::nullopt;
    return intent;
}

bool PublicationService::SaveIntent(const std::string_view local_job_id, const Intent& intent)
{
    std::vector<unsigned char> out;
    out.insert(out.end(), intent.bundle.root_chunk_id.begin(), intent.bundle.root_chunk_id.end());
    out.insert(out.end(), intent.bundle.content_key.begin(), intent.bundle.content_key.end());
    out.insert(out.end(), intent.bundle.chunk_authorization_root.begin(), intent.bundle.chunk_authorization_root.end());
    Append32(out, intent.bundle.chunk_count);
    out.push_back(intent.recipient ? 1 : 0);
    if (intent.recipient) out.insert(out.end(), intent.recipient->Value().begin(), intent.recipient->Value().end());
    out.push_back(intent.future_self ? 1 : 0);
    if (intent.future_self) {
        out.insert(out.end(), intent.future_self->first.begin(), intent.future_self->first.end());
        Append64(out, intent.future_self->second);
    }
    const bool saved = m_application_db.Put(IntentKey(local_job_id), out);
    crypto::CleanseMemory(out.data(), out.size());
    if (saved) {
        const auto attempt = m_application_db.Get("publication/staging-attempt");
        if (attempt && std::string_view{reinterpret_cast<const char*>(attempt->data()), attempt->size()} == local_job_id)
            (void)m_application_db.Erase("publication/staging-attempt");
    }
    return saved;
}

PublicationJobResult PublicationService::Resume(const std::string_view local_job_id)
{
    std::lock_guard lock{m_mutex};
    auto job = Load(local_job_id);
    if (!job) return Failure("Publication job is absent, corrupt, or Identity is locked");
    return ResumeLocked(local_job_id, *job);
}

bool PublicationService::IsCancellationPending(const std::string_view local_job_id) const
{
    return ValidJobId(local_job_id) && m_application_db.Has(CancelKey(local_job_id));
}

bool PublicationService::FinishCancellation(const std::string_view local_job_id)
{
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked() ||
        !IsCancellationPending(local_job_id)) return false;
    if (!m_runtime.GetChunkRetention().Release(JobRetention(m_application_db.Account(), local_job_id), NowMs())) {
        return false;
    }
    auto index = m_application_db.Get(JOB_INDEX_KEY).value_or(std::vector<unsigned char>{});
    std::vector<unsigned char> kept;
    const std::string_view listed{reinterpret_cast<const char*>(index.data()), index.size()};
    for (std::size_t start{0}; start < listed.size();) {
        const auto end = std::min(listed.find('\n', start), listed.size());
        if (end > start && listed.substr(start, end - start) != local_job_id) {
            kept.insert(kept.end(), listed.begin() + start, listed.begin() + end);
            kept.push_back('\n');
        }
        start = end + 1;
    }
    PrivateApplicationStore::Batch batch{m_application_db};
    const bool staged = m_application_db.Erase(JobKey(local_job_id)) &&
        m_application_db.Erase(IntentKey(local_job_id)) &&
        m_application_db.Erase(LeavesKey(local_job_id)) &&
        m_application_db.Put(JOB_INDEX_KEY, kept) &&
        m_application_db.Erase(CancelKey(local_job_id));
    return staged && batch.Commit();
}

bool PublicationService::CancelPublication(const std::string_view local_job_id)
{
    std::lock_guard lock{m_mutex};
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return false;
    if (IsCancellationPending(local_job_id)) return FinishCancellation(local_job_id);
    auto job = Load(local_job_id);
    if (!job) return false;
    bool safe_to_cancel = job->phase == PublicationJobPhase::QUEUED && job->operation_id.IsNull();
    if (!safe_to_cancel && job->phase == PublicationJobPhase::NEEDS_ATTENTION &&
        !job->operation_id.IsNull()) {
        safe_to_cancel = m_coordinator.GetStatus(job->operation_id).phase == IdentityOperationPhase::REJECTED;
    }
    if (!safe_to_cancel) return false;
    const std::array<unsigned char, 1> marker{1};
    if (!m_application_db.Put(CancelKey(local_job_id), marker)) return false;
    return FinishCancellation(local_job_id);
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
    const bool finalized = job->phase == PublicationJobPhase::SECURING ||
        job->phase == PublicationJobPhase::PROTECTED;
    return PublicationJobResult{.phase = job->phase, .operation_id = job->operation_id,
        .finalized_height = finalized ? m_runtime.FindFinalizedOperation(job->operation_id).height : 0};
}

bool PublicationService::MarkProtected(const std::string_view local_job_id)
{
    std::lock_guard lock{m_mutex};
    auto job = Load(local_job_id);
    if (!job) return false;
    if (job->phase == PublicationJobPhase::PROTECTED) return true;
    if (job->phase != PublicationJobPhase::SECURING) return false;
    job->phase = PublicationJobPhase::PROTECTED;
    return Save(local_job_id, *job);
}

std::optional<PrivateItemId> NewPrivateItemId()
{
    PrivateItemId id{};
    if (RAND_bytes(id.data(), static_cast<int>(id.size())) != 1) return std::nullopt;
    if (std::all_of(id.begin(), id.end(), [](unsigned char b) { return b == 0; }) ||
        std::all_of(id.begin(), id.end(), [](unsigned char b) { return b == 0xff; })) return std::nullopt;
    return id;
}

std::optional<PublicationService::Staged> PublicationService::Stage(const std::string_view local_job_id,
    std::vector<NewContent>& children, const BuildMetadata& build_metadata, std::string& error)
{
    if (m_application_db.Has(IntentKey(local_job_id))) {
        error = "Publication already has a saved intent; resume it before preparing content";
        return std::nullopt;
    }
    const auto network = std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32};
    const auto retention = JobRetention(m_application_db.Account(), local_job_id);
    auto& pins = m_runtime.GetChunkRetention();
    // Retrying an interrupted local attempt replaces its pins; no operation was submitted.
    if (!pins.Release(retention, NowMs())) { error = "Cannot reset local staging pins"; return std::nullopt; }
    if (!m_application_db.Put("publication/staging-attempt", std::span{
            reinterpret_cast<const unsigned char*>(local_job_id.data()), local_job_id.size()})) {
        error = "Cannot record local staging attempt"; return std::nullopt;
    }
    std::vector<EncryptedTreeSummary> summaries;
    auto cleanse = [&] {
        for (auto& summary : summaries) crypto::CleanseMemory(summary.content_key.data(), summary.content_key.size());
    };
    auto fail = [&]() -> std::optional<Staged> {
        cleanse();
        if (pins.Release(retention, NowMs())) {
            (void)m_application_db.Erase(LeavesKey(local_job_id));
            (void)m_application_db.Erase("publication/staging-attempt");
        }
        return std::nullopt;
    };
    try {
        Staged staged;
        std::set<ChunkId> unique;
        ChunkAuthorizationAccumulator accumulator;
        const auto stage = [&](std::uint32_t, const EncryptedChunk& chunk) {
            if (staged.leaves.size() >= MAX_PUBLICATION_CHUNKS || !unique.insert(chunk.id).second) return false;
            const std::array<ChunkId, 1> id{chunk.id};
            if (!pins.Pin(retention, id)) return false;
            const auto status = m_runtime.GetChunkBlobStore().Put(chunk.id, chunk.stored_bytes);
            if (status != ChunkBlobPutStatus::STORED && status != ChunkBlobPutStatus::ALREADY_STORED) return false;
            if (!accumulator.Add({chunk.id})) return false;
            staged.leaves.push_back(chunk.id);
            return true;
        };
        for (auto& child : children) {
            const auto tree = BuildEncryptedChunkTree(network, child.source, stage);
            if (!tree) { error = "Cannot encrypt content"; return fail(); }
            summaries.push_back(*tree);
        }
        auto metadata = build_metadata(summaries);
        cleanse();
        if (!metadata) { error = "Cannot encode private document"; return fail(); }
        const auto main = BuildEncryptedChunkTree(network,
            [](std::span<unsigned char>) -> std::optional<std::size_t> { return 0; }, stage, *metadata);
        crypto::CleanseMemory(metadata->data(), metadata->size());
        const auto commitment = accumulator.Finish();
        if (!main || !commitment || commitment->chunk_count != staged.leaves.size()) {
            error = "Cannot prepare publication"; return fail();
        }
        staged.bundle = {main->root_chunk_id, main->content_key, commitment->root, commitment->chunk_count};
        std::vector<unsigned char> encoded;
        encoded.reserve(staged.leaves.size() * 32);
        for (const auto& leaf : staged.leaves) encoded.insert(encoded.end(), leaf.begin(), leaf.end());
        if (!m_application_db.Put(LeavesKey(local_job_id), encoded)) {
            error = "Cannot save staged chunk order"; return fail();
        }
        return staged;
    } catch (const std::exception&) {
        error = "Local staging failed";
        return fail();
    }

}

std::optional<std::vector<ChunkId>> PublicationService::LoadLeaves(const std::string_view local_job_id) const
{
    const auto encoded = m_application_db.Get(LeavesKey(local_job_id));
    if (!encoded || encoded->empty() || encoded->size() % 32 != 0) return std::nullopt;
    std::vector<ChunkId> leaves(encoded->size() / 32);
    for (std::size_t i{0}; i < leaves.size(); ++i) std::copy_n(encoded->begin() + i * 32, 32, leaves[i].begin());
    return leaves;
}

PublicationJobResult PublicationService::PublishMail(const std::string_view local_job_id, MailMessage message,
    std::vector<std::pair<std::size_t, NewContent>> new_attachments)
{
    std::lock_guard lock{m_mutex};
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return Failure("Application DB is locked or job ID invalid");
    if (IsCancellationPending(local_job_id)) return Failure("Publication cancellation is pending cleanup");
    if (auto existing = Load(local_job_id)) return ResumeLocked(local_job_id, *existing);
    const auto me = m_identity.GetAccountId();
    if (!me) return Failure("Identity is locked");
    std::set<std::size_t> targets;
    std::vector<NewContent> children;
    std::vector<std::size_t> order;
    for (auto& [index, content] : new_attachments) {
        if (index >= message.attachments.size() || !targets.insert(index).second || !content.source) {
            return Failure("Invalid attachment content");
        }
        order.push_back(index);
        children.push_back(std::move(content));
    }
    for (std::size_t i{0}; i < message.attachments.size(); ++i) {
        // Reused attachments must already reference protected content.
        if (!targets.contains(i) && (message.attachments[i].root_chunk_id == ChunkId{} ||
                message.attachments[i].content_key == ContentKey{})) return Failure("Attachment has no content");
    }
    std::string error;
    const auto staged = Stage(local_job_id, children,
        [&](std::span<const EncryptedTreeSummary> summaries) -> std::optional<std::vector<unsigned char>> {
            for (std::size_t i{0}; i < summaries.size(); ++i) {
                auto& attachment = message.attachments[order[i]];
                attachment.root_chunk_id = summaries[i].root_chunk_id;
                attachment.content_key = summaries[i].content_key;
                attachment.logical_size = summaries[i].plaintext_bytes;
            }
            return EncodePrivateApplicationDocument(message);
        }, error);
    for (auto& attachment : message.attachments) {
        crypto::CleanseMemory(attachment.content_key.data(), attachment.content_key.size());
    }
    if (!staged) return Failure(error);
    const std::optional<AccountId> recipient = message.recipient_account_id != *me
        ? std::optional<AccountId>{message.recipient_account_id} : std::nullopt;
    return SubmitPreparedLocked(local_job_id, staged->bundle, recipient, std::nullopt);
}

PublicationJobResult PublicationService::PublishFiles(const std::string_view local_job_id, FilesMutationBatch batch,
    std::vector<std::pair<std::size_t, NewContent>> new_content)
{
    std::lock_guard lock{m_mutex};
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return Failure("Application DB is locked or job ID invalid");
    if (IsCancellationPending(local_job_id)) return Failure("Publication cancellation is pending cleanup");
    if (auto existing = Load(local_job_id)) return ResumeLocked(local_job_id, *existing);
    if (batch.mutations.empty()) return Failure("Empty Files change");
    // Items without an explicit modification time get the publishing time.
    const auto now_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    for (auto& mutation : batch.mutations) {
        if (mutation.item && mutation.item->modified_ms == 0) mutation.item->modified_ms = now_ms;
    }
    std::set<std::size_t> targets;
    std::vector<NewContent> children;
    std::vector<std::size_t> order;
    for (auto& [index, content] : new_content) {
        if (index >= batch.mutations.size() || !targets.insert(index).second || !content.source ||
            batch.mutations[index].kind != FileMutationKind::UPSERT_ITEM || !batch.mutations[index].item ||
            batch.mutations[index].item->kind != FileItemKind::FILE) return Failure("Invalid file content");
        order.push_back(index);
        children.push_back(std::move(content));
    }
    std::string error;
    const auto staged = Stage(local_job_id, children,
        [&](std::span<const EncryptedTreeSummary> summaries) -> std::optional<std::vector<unsigned char>> {
            for (std::size_t i{0}; i < summaries.size(); ++i) {
                auto& item = *batch.mutations[order[i]].item;
                item.root_chunk_id = summaries[i].root_chunk_id;
                item.content_key = summaries[i].content_key;
                item.logical_size = summaries[i].plaintext_bytes;
            }
            return EncodePrivateApplicationDocument(batch);
        }, error);
    for (auto& mutation : batch.mutations) {
        if (mutation.item && mutation.item->content_key) {
            crypto::CleanseMemory(mutation.item->content_key->data(), mutation.item->content_key->size());
        }
    }
    if (!staged) return Failure(error);
    // Files are private: only the owner's self capsule.
    return SubmitPreparedLocked(local_job_id, staged->bundle, std::nullopt, std::nullopt);
}

PublicationJobResult PublicationService::PublishRecoveryBridge(const std::string_view local_job_id,
    const std::span<const unsigned char, 32> new_recovery_entropy)
{
    std::lock_guard lock{m_mutex};
    if (!ValidJobId(local_job_id) || !m_application_db.IsUnlocked()) return Failure("Application DB is locked or job ID invalid");
    if (IsCancellationPending(local_job_id)) return Failure("Publication cancellation is pending cleanup");
    if (auto existing = Load(local_job_id)) return ResumeLocked(local_job_id, *existing);
    const auto me = m_identity.GetAccountId();
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = me && loaded && loaded.state ? loaded.state->identities.Find(*me) : nullptr;
    if (!record) return Failure("Identity is not finalized");
    const std::uint64_t current_epoch = record->key_epoch;
    // A bridge that silently omits an epoch would make that content unrecoverable.
    for (std::uint64_t epoch{0}; epoch < current_epoch; ++epoch) {
        if (m_runtime.FindIdentityKemPackage(*me, epoch).status == IdentityKemPackageLookupStatus::FOUND &&
            !m_identity.HasKemSeedForEpoch(epoch, current_epoch)) {
            return Failure("Earlier encryption keys are not recovered on this device yet");
        }
    }
    auto future_seed = DeriveIdentityXWingSeed(new_recovery_entropy);
    const auto future_public = future_seed ? DeriveXWingPublicKey(*future_seed) : std::nullopt;
    if (future_seed) crypto::CleanseMemory(future_seed->data(), future_seed->size());
    if (!future_public) return Failure("Invalid new recovery phrase");

    IdentityRecoveryBridge bridge{.account_id = *me, .next_key_epoch = current_epoch + 1};
    for (auto& [epoch, seed] : m_identity.KemSeedsForRecoveryBridge(current_epoch)) {
        bridge.historical_seeds.push_back({epoch, seed});
        crypto::CleanseMemory(seed.data(), seed.size());
    }
    std::vector<NewContent> none;
    std::string error;
    const auto staged = Stage(local_job_id, none,
        [&](std::span<const EncryptedTreeSummary>) { return EncodePrivateApplicationDocument(bridge); }, error);
    for (auto& entry : bridge.historical_seeds) crypto::CleanseMemory(entry.seed.data(), entry.seed.size());
    if (!staged) return Failure(error);
    return SubmitPreparedLocked(local_job_id, staged->bundle, std::nullopt,
        std::pair<XWingPublicKey, std::uint64_t>{*future_public, current_epoch + 1});
}

bool PublicationService::VerifyRecoveryBridge(const std::string_view local_job_id,
    const std::span<const unsigned char, 32> new_recovery_entropy, StorageService& storage)
{
    std::optional<Job> job;
    {
        std::lock_guard lock{m_mutex};
        job = Load(local_job_id);
    }
    const auto me = m_identity.GetAccountId();
    const auto current_public = m_identity.GetIdentityXWingPublicKey();
    if (!job || job->phase != PublicationJobPhase::PROTECTED || !me || !current_public) return false;
    const auto finalized = m_runtime.FindFinalizedRootPublication(job->operation_id);
    if (!finalized || *finalized != job->publication) return false;
    auto seed = DeriveIdentityXWingSeed(new_recovery_entropy);
    if (!seed) return false;
    const auto network = std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32};
    std::optional<ContentKey> key;
    for (const auto& capsule : finalized->recipient_capsules) {
        if (capsule.key_epoch != job->key_epoch + 1) continue;
        key = OpenRootRecipientCapsule(network, std::span<const unsigned char, 32>{me->Value().begin(), 32},
            job->nonce, job->key_epoch, finalized->root_chunk_id, capsule, *seed);
        if (key) break;
    }
    crypto::CleanseMemory(seed->data(), seed->size());
    if (!key) return false;
    std::vector<unsigned char> metadata;
    std::set<ChunkId> seen;
    const auto fetched = FetchEncryptedChunkTree(network, *key, finalized->root_chunk_id,
        [&](const ChunkId& id) { return storage.Fetch(id); },
        [&](std::span<const unsigned char> cbor) { metadata.assign(cbor.begin(), cbor.end()); return true; },
        [&](const ChunkId& id) { return seen.insert(id).second; },
        [](std::span<const unsigned char> data) { return data.empty(); }, 0);
    crypto::CleanseMemory(key->data(), key->size());
    auto document = fetched ? DecodePrivateApplicationDocument(metadata) : std::nullopt;
    crypto::CleanseMemory(metadata.data(), metadata.size());
    if (!document || !std::holds_alternative<IdentityRecoveryBridge>(*document)) return false;
    auto& bridge = std::get<IdentityRecoveryBridge>(*document);
    bool ok = bridge.account_id == *me && bridge.next_key_epoch == job->key_epoch + 1;
    // The bridge must carry the key that is being retired.
    bool has_current{false};
    for (auto& entry : bridge.historical_seeds) {
        if (entry.key_epoch == job->key_epoch) {
            const auto derived = DeriveXWingPublicKey(entry.seed);
            has_current = derived && *derived == *current_public;
        }
        crypto::CleanseMemory(entry.seed.data(), entry.seed.size());
    }
    return ok && has_current;
}

std::vector<std::string> PublicationService::Jobs()
{
    std::vector<std::string> jobs;
    const auto index = m_application_db.Get(JOB_INDEX_KEY);
    if (!index) return jobs;
    std::string_view listed{reinterpret_cast<const char*>(index->data()), index->size()};
    for (std::size_t start{0}; start < listed.size();) {
        const auto end = std::min(listed.find('\n', start), listed.size());
        if (end > start) jobs.emplace_back(listed.substr(start, end - start));
        start = end + 1;
    }
    return jobs;
}

std::vector<std::pair<std::string, PublicationJobResult>> PublicationService::ProcessDurability(StorageService& storage)
{
    std::vector<std::pair<std::string, PublicationJobResult>> results;
    for (const auto& id : Jobs()) {
        auto status = GetJob(id);
        if (!status) continue;
        if (status->phase == PublicationJobPhase::SECURING) {
            if (const auto leaves = LoadLeaves(id)) {
                const auto durability = storage.Secure(status->operation_id, *leaves);
                status->durability_percent = durability.ProgressPercent(storage.RemoteReplicaTarget());
                if (durability.state == DurabilityState::PROTECTED && MarkProtected(id)) {
                    status->phase = PublicationJobPhase::PROTECTED;
                    // Remotely durable: the local copy becomes evictable cache.
                    (void)m_runtime.GetChunkRetention().Release(JobRetention(m_application_db.Account(), id), NowMs());
                } else if (!durability.error.empty()) {
                    status->error = durability.error;
                }
            }
        } else if (status->phase == PublicationJobPhase::QUEUED ||
                   (status->phase == PublicationJobPhase::WAITING_FINALITY && !status->operation_id.IsNull() &&
                       m_runtime.GetOperationStatus(status->operation_id).kind == OperationStatusKind::REJECTED_KNOWN)) {
            status = Resume(id);
        }
        if (status->phase == PublicationJobPhase::PROTECTED) {
            // StorageService owns durability; a job never stays PROTECTED
            // once its placement is not (audit loss, NEEDS_ATTENTION, ...).
            storage.Track(status->operation_id);
            const auto durability = storage.GetDurability(status->operation_id);
            if (durability && durability->state != DurabilityState::PROTECTED) {
                std::lock_guard lock{m_mutex};
                if (auto job = Load(id); job && job->phase == PublicationJobPhase::PROTECTED) {
                    job->phase = PublicationJobPhase::SECURING;
                    if (Save(id, *job)) {
                        status->phase = PublicationJobPhase::SECURING;
                        status->durability_percent = durability->ProgressPercent(storage.RemoteReplicaTarget());
                    }
                }
            } else {
                status->durability_percent = 100;
            }
        }
        results.emplace_back(id, *status);
    }
    return results;
}

} // namespace cybou
