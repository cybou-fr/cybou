// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Identity storage orchestration, recovery and settlement preparation.

#include <cybou/storage_service_internal.h>
#include <cybou/node_runtime.h>
#include <cybou/storage_io_scheduler.h>
#include <cybou/root_publication.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace cybou {

StorageService::StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
    PrivateApplicationStore& application_db, const std::uint8_t remote_replica_target)
    : m_runtime{runtime}, m_transport{transport}, m_db{application_db},
      m_target{std::clamp<std::uint8_t>(remote_replica_target, 1, MAX_REPLICAS_PER_CHUNK)},
      m_placements{std::make_unique<PlacementRepository>(application_db)},
      m_evidence{std::make_unique<EvidenceLedger>(application_db)},
      m_verifier{std::make_unique<ReplicaVerifier>(transport, *m_evidence)}
{
}

StorageService::~StorageService() = default;

namespace {
std::string SettlementJournalKey(const CybouNodeRuntime& runtime, std::uint64_t period)
{ return "storage/settlement/" + runtime.GetNetworkBinding().GetHex() + '/' + std::to_string(period); }
std::optional<std::vector<unsigned char>> PreparedBytes(StorageSettlement settlement)
{
    settlement.poa_signature = {};
    settlement.poa_signature.ml_dsa.resize(3309);
    return SerializeProtocolOperation(ProtocolOperation{std::move(settlement)});
}
std::optional<StorageSettlement> DecodeSettlement(const std::vector<unsigned char>& bytes)
{
    const auto op = DeserializeProtocolOperation(bytes);
    if (!op || SerializeProtocolOperation(*op) != bytes) return std::nullopt;
    const auto* settlement = std::get_if<StorageSettlement>(&*op);
    return settlement ? std::optional{*settlement} : std::nullopt;
}
}

std::optional<StorageSettlement> StorageService::PreparedSettlement(const std::uint64_t period)
{
    PrivateApplicationStore::Batch snapshot{m_db};
    if (!snapshot.IsOutermost() || !m_db.IsUnlocked()) throw std::runtime_error{"settlement journal unavailable"};
    const auto key = SettlementJournalKey(m_runtime, period);
    const auto prepared = m_db.Get(key + "/prepared");
    const auto signed_bytes = m_db.Get(key + "/signed");
    if (!prepared) {
        if (m_db.Has(key + "/prepared") || m_db.Has(key + "/signed"))
            throw std::runtime_error{"missing settlement preparation"};
        return std::nullopt;
    }
    auto result = DecodeSettlement(*prepared);
    if (!result || result->period != period || PreparedBytes(*result) != prepared)
        throw std::runtime_error{"corrupt settlement preparation"};
    if (signed_bytes) {
        result = DecodeSettlement(*signed_bytes);
        const auto digest = result ? ComputeStorageSettlementDigest(m_runtime.GetNetworkBinding(), *result) : std::nullopt;
        if (!result || PreparedBytes(*result) != prepared || !digest ||
            !VerifyIdentityMessage(m_runtime.GetNetworkGenesis().GetPoaPublicKey(), result->poa_signature, *digest))
            throw std::runtime_error{"corrupt signed settlement"};
    } else if (m_db.Has(key + "/signed")) throw std::runtime_error{"unreadable signed settlement"};
    return result;
}

OperationSubmitResult StorageService::SubmitSettlement(const std::uint64_t period, const std::uint64_t start,
    std::vector<StorageSettlementEntry> entries)
{
    StorageSettlement requested{.period = period, .period_start_utc = start, .entries = std::move(entries)};
    const auto seconds = m_runtime.GetNetworkGenesis().GetProtocolParameters().storage_settlement_period_seconds;
    if (start > std::numeric_limits<uint64_t>::max() - seconds) return {};
    requested.period_end_utc = start + seconds;
    // Entry-only UI callers do not supply cumulative evidence/witnesses. Their
    // malformed entries fail serialization; an actual empty period uses SHA256(u32 zero).
    const std::array<unsigned char,4> empty_count{};
    if (!crypto::ComputeSha256({std::span<const unsigned char>{empty_count}}, requested.evidence_root.begin())) return {};
    const auto prepared_bytes = PreparedBytes(requested);
    if (!prepared_bytes) return {};
    const auto key = SettlementJournalKey(m_runtime, period);
    const auto retained = PreparedSettlement(period);
    if (retained && PreparedBytes(*retained) != prepared_bytes)
        throw std::runtime_error{"different settlement already prepared for this period"};
    // Invalid fresh input must not freeze the period. Retained exact operations
    // still follow replay/reconciliation, even if canonical state has advanced.
    if (!retained && m_runtime.CheckStorageSettlementInputs(requested) != StorageSettlementError::NONE) return {};
    {
        PrivateApplicationStore::Batch prepare{m_db};
        if (!prepare.IsOutermost() || !m_db.IsUnlocked()) throw std::runtime_error{"settlement journal unavailable"};
        const auto prior = m_db.Get(key + "/prepared");
        if (prior && prior != prepared_bytes) throw std::runtime_error{"conflicting settlement preparation"};
        if ((!prior && (m_db.Has(key + "/prepared") || m_db.Has(key + "/signed"))) ||
            (!prior && !m_db.Put(key + "/prepared", *prepared_bytes)) || !prepare.Commit())
            throw std::runtime_error{"cannot persist settlement preparation"};
    }
    auto settlement = PreparedSettlement(period);
    if (!settlement) throw std::runtime_error{"lost settlement preparation"};
    auto digest = ComputeStorageSettlementDigest(m_runtime.GetNetworkBinding(), *settlement);
    if (!digest) return {};
    if (!VerifyIdentityMessage(m_runtime.GetNetworkGenesis().GetPoaPublicKey(), settlement->poa_signature, *digest)) {
        const auto signed_settlement = m_runtime.SignStorageSettlement(*settlement);
        if (!signed_settlement) return {};
        const auto bytes = SerializeProtocolOperation(ProtocolOperation{*signed_settlement});
        if (!bytes) return {};
        {
            PrivateApplicationStore::Batch save{m_db};
            if (!save.IsOutermost() || m_db.Get(key + "/prepared") != prepared_bytes)
                throw std::runtime_error{"settlement journal changed during signing"};
            // A concurrent exact signer may have saved first; never overwrite it.
            if (!m_db.Has(key + "/signed") && !m_db.Put(key + "/signed", *bytes))
                throw std::runtime_error{"cannot persist signed settlement"};
            if (!save.Commit()) throw std::runtime_error{"cannot commit signed settlement"};
        }
        settlement = PreparedSettlement(period);
        if (!settlement) throw std::runtime_error{"lost signed settlement"};
    }
    const ProtocolOperation op{*settlement};
    const auto id = ComputeOperationId(op);
    if (!id) return {};
    if (m_runtime.GetOperationStatus(*id).kind == OperationStatusKind::FINALIZED)
        return {.status = OperationSubmitStatus::ALREADY_FINALIZED, .op_id = *id};
    // No app.db transaction spans admission/relay. All retained bytes survive rejection.
    return m_runtime.SubmitOperation(op);
}

std::map<std::array<unsigned char, 32>, StorageProviderEvidence> StorageService::ProviderEvidence()
{
    return m_evidence->ProviderEvidence();
}

std::optional<StorageAuditAnswer> RuntimeStorageTransport::Audit(const StorageEndpoint& provider,
    const StorageAuditChallenge& challenge)
{
    return m_runtime.AuditChunkAtStorageEndpoint(provider.address, provider.port, provider.storage_id, challenge);
}

int PublicationDurability::ProgressPercent(const std::uint8_t target) const
{
    if (state == DurabilityState::PROTECTED) return 100;
    if (chunk_count == 0 || target == 0) return 0;
    return static_cast<int>(std::uint64_t{chunks_at_target} * 100 / chunk_count);
}

std::vector<StorageEndpoint> RuntimeStorageTransport::Providers()
{
    std::vector<StorageEndpoint> providers;
    for (auto& peer : m_runtime.StorageEndpoints()) {
        StorageEndpoint endpoint{peer.storage_id, peer.address, peer.port};
        if (peer.payout_account) {
            std::array<unsigned char, 32> account{};
            std::copy(peer.payout_account->Value().begin(), peer.payout_account->Value().end(), account.begin());
            endpoint.payout_account = account;
        }
        if (!HasProvider(providers, endpoint)) providers.push_back(std::move(endpoint));
    }
    return providers;
}

std::optional<ChunkAdmissionResult> RuntimeStorageTransport::Put(const StorageEndpoint& provider,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    return m_runtime.PutChunkToStorageEndpoint(provider.address, provider.port, provider.storage_id, publication_operation_id,
        chunk_id, stored_bytes, proof);
}

std::optional<std::vector<unsigned char>> RuntimeStorageTransport::Get(const StorageEndpoint& provider,
    const ChunkId& chunk_id)
{
    return m_runtime.GetChunkFromStorageEndpoint(provider.address, provider.port, provider.storage_id, chunk_id);
}

std::optional<ChunkAuthorizationProof> RuntimeStorageTransport::GetProof(const StorageEndpoint& provider,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id)
{
    return m_runtime.GetChunkAuthorizationProofFromStorageEndpoint(provider.address, provider.port,
        provider.storage_id, publication_operation_id, chunk_id);
}

bool StorageService::Track(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto index = m_placements->PlacementIndex();
    if (std::find(index.begin(), index.end(), operation_id) != index.end()) return true;
    const auto placement = m_placements->Load(operation_id);
    return placement && m_placements->Save(*placement);
}

PublicationDurability StorageService::Summarize(const Placement& placement) const
{
    PublicationDurability result;
    result.chunk_count = static_cast<std::uint32_t>(placement.leaves.size());
    result.min_replicas = std::numeric_limits<std::uint32_t>::max();
    for (const auto& replicas : placement.replicas) {
        std::set<std::array<unsigned char, 32>> unique;
        for (const auto& r : replicas) unique.insert(r.storage_id);
        const auto count = static_cast<std::uint32_t>(unique.size());
        result.min_replicas = std::min(result.min_replicas, count);
        if (count >= m_target) ++result.chunks_at_target;
    }
    if (placement.leaves.empty()) result.min_replicas = 0;
    result.state = result.chunk_count > 0 && result.chunks_at_target == result.chunk_count
        ? DurabilityState::PROTECTED : DurabilityState::SECURING;
    if (const auto seen = m_observations.find(placement.operation_id); seen != m_observations.end()) {
        result.observed_at_ms = seen->second.observed_at_ms;
        result.observation = seen->second.observation;
        result.condition = seen->second.condition;
    }
    return result;
}

PublicationDurability StorageService::Observe(const cybou::Hash256& id, PublicationDurability result,
    StorageObservation observation, StorageCondition condition)
{
    result.observed_at_ms = StorageEvidenceNowMs();
    result.observation = observation;
    result.condition = condition;
    constexpr std::size_t limit{1024};
    if (!m_observations.contains(id) && m_observations.size() >= limit) {
        const auto oldest = std::min_element(m_observations.begin(), m_observations.end(),
            [](const auto& a, const auto& b) { return a.second.observed_at_ms < b.second.observed_at_ms; });
        m_observations.erase(oldest);
    }
    m_observations[id] = result;
    // Raw transport errors can contain endpoints; they are not retained for GUI projection.
    m_observations[id].error.clear();
    return result;
}

PublicationDurability StorageService::Secure(const cybou::Hash256& operation_id, const std::span<const ChunkId> leaves)
{
    std::unique_lock lock{m_mutex};
    if (!m_placements->IsUnlocked()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Application DB is locked"};
    }
    // Размещаем только finalized публикации и только с их точным набором leaves.
    // Это соответствует finality-first admission из docs/cybou/STORAGE_ADMISSION.md:
    // только finalized RootPublication даёт право на placement.
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return {.state = DurabilityState::SECURING, .error = "Publication is not finalized yet"};
    if (leaves.empty() || leaves.size() > MAX_PUBLICATION_CHUNKS || leaves.size() != publication->chunk_count) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Chunk list does not match the publication"};
    }
    ChunkAuthorizationAccumulator accumulator;
    for (const auto& leaf : leaves) {
        if (!accumulator.Add({leaf})) {
            return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Chunk list is invalid"};
        }
    }
    const auto summary = accumulator.Finish();
    if (!summary || summary->root != publication->chunk_authorization_root) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Chunk list does not match the publication"};
    }
    m_placement_cv.wait(lock, [&] { return !m_active_placements.contains(operation_id); });
    auto placement = m_placements->Load(operation_id);
    if (!placement || placement->leaves.size() != leaves.size() ||
        !std::equal(leaves.begin(), leaves.end(), placement->leaves.begin())) {
        placement = Placement{.operation_id = operation_id,
            .leaves = {leaves.begin(), leaves.end()},
            .replicas = std::vector<std::vector<StorageEndpoint>>(leaves.size())};
        if (!m_placements->Save(*placement)) {
            return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save placement state"};
        }
        if (auto log = m_runtime.EventLog()) log->Write(NodeEvent::placement_created,{{"operation_id",operation_id.GetHex()}});
    }
    return Place(lock, *placement);
}

std::optional<ChunkAuthorizationProof> StorageService::GetAuthorizationProof(
    const cybou::Hash256& operation_id, const ChunkId& chunk_id)
{
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return std::nullopt;
    for (const auto& provider : m_transport.Providers()) {
        const auto proof = m_runtime.StorageIo().Submit(provider.storage_id, StorageIoScheduler::Kind::READ,
            [this, provider, operation_id, chunk_id] { return m_transport.GetProof(provider, operation_id, chunk_id); }).get();
        // Доверяем не provider-ответу самому по себе, а локальной повторной проверке proof против finalized state.
        if (proof && VerifyChunkAuthorizationProof(*publication, chunk_id, *proof)) return proof;
    }
    return std::nullopt;
}

PublicationDurability StorageService::Rebuild(const cybou::Hash256& operation_id,
    const std::span<const ChunkId> candidate_chunks)
{
    std::unique_lock lock{m_mutex};
    if (!m_placements->IsUnlocked()) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Application DB is locked"};
    }
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication) return {.state = DurabilityState::SECURING, .error = "Publication is not finalized yet"};
    if (publication->chunk_count == 0 || publication->chunk_count > MAX_PUBLICATION_CHUNKS ||
        candidate_chunks.empty() || candidate_chunks.size() > MAX_PUBLICATION_CHUNKS) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Candidate chunk set is invalid"};
    }

    m_placement_cv.wait(lock, [&] { return !m_active_placements.contains(operation_id); });
    ActivePlacementGuard guard{lock, m_active_placements, m_placement_cv, operation_id};
    Placement placement{.operation_id = operation_id,
        .leaves = std::vector<ChunkId>(publication->chunk_count),
        .replicas = std::vector<std::vector<StorageEndpoint>>(publication->chunk_count)};
    if (const auto progress=m_placements->Load(operation_id, true); progress && progress->leaves.size()==publication->chunk_count)
        placement=*progress;
    std::vector<bool> found(publication->chunk_count, false);
    std::set<ChunkId> verified;
    for (std::size_t i{0}; i<placement.leaves.size(); ++i) {
        found[i]=placement.leaves[i]!=ChunkId{} && !placement.replicas[i].empty();
        if (found[i]) verified.insert(placement.leaves[i]);
    }
    std::set<ChunkId> unique;
    lock.unlock();
    const auto providers = m_transport.Providers();
    lock.lock();
    for (const auto& chunk_id : candidate_chunks) {
        if (chunk_id == ChunkId{} || !unique.insert(chunk_id).second) continue;
        if (verified.contains(chunk_id)) continue;
        for (const auto& provider : providers) {
            lock.unlock();
            const auto proof = m_runtime.StorageIo().Submit(provider.storage_id, StorageIoScheduler::Kind::READ,
            [this, provider, operation_id, chunk_id] { return m_transport.GetProof(provider, operation_id, chunk_id); }).get();
            lock.lock();
            // Rebuild принимает только те кандидаты, которые достижимый provider может доказать против finalized publication.
            if (!proof || !VerifyChunkAuthorizationProof(*publication, chunk_id, *proof)) continue;
            if (proof->leaf_index >= placement.leaves.size()) {
                return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Provider returned an invalid leaf index"};
            }
            const auto index = proof->leaf_index;
            if (found[index] && placement.leaves[index] != chunk_id) {
                return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Providers disagree on publication leaves"};
            }
            placement.leaves[index] = chunk_id;
            found[index] = true;
            verified.insert(chunk_id);
            if (!HasProvider(placement.replicas[index], provider)) placement.replicas[index].push_back(provider);
        }
        // Preserve verified progress and yield when providers are unavailable or rate limited.
        if (!verified.contains(chunk_id)) break;
    }
    if (std::find(found.begin(), found.end(), false) != found.end()) {
        if (!m_placements->Save(placement, true)) return {.state=DurabilityState::NEEDS_ATTENTION, .error="Cannot save rebuild progress"};
        return {.state = DurabilityState::SECURING, .chunk_count = publication->chunk_count,
            .error = "Some finalized chunk proofs are not available from reachable providers"};
    }
    ChunkAuthorizationAccumulator accumulator;
    for (const auto& leaf : placement.leaves) {
        if (!accumulator.Add({leaf})) {
            return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Rebuilt chunk order is invalid"};
        }
    }
    const auto commitment = accumulator.Finish();
    if (!commitment || commitment->root != publication->chunk_authorization_root) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Rebuilt leaves do not match finalized authorization"};
    }
    if (!m_placements->Save(placement)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Cannot save rebuilt placement state"};
    }
    m_placements->EraseRebuild(operation_id);
    guard.Finish();
    return Place(lock, placement);
}

PublicationDurability StorageService::Resume(const cybou::Hash256& operation_id)
{
    std::unique_lock lock{m_mutex};
    auto placement = m_placements->Load(operation_id);
    if (!placement) return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Unknown publication placement"};
    if (!m_runtime.FindFinalizedRootPublication(operation_id)) {
        return {.state = DurabilityState::NEEDS_ATTENTION, .error = "Publication is not finalized"};
    }
    return Place(lock, *placement);
}

std::optional<PublicationDurability> StorageService::GetDurability(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto placement = m_placements->Load(operation_id);
    if (!placement) return std::nullopt;
    return Summarize(*placement);
}

std::optional<StorageService::PlacementView> StorageService::DescribePlacement(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    auto placement = m_placements->Load(operation_id);
    if (!placement) return std::nullopt;
    return PlacementView{std::move(placement->leaves), std::move(placement->replicas)};
}

std::vector<StorageSettlementEntry> StorageService::SettlementEntries(const std::uint64_t period,
    const std::int64_t verified_since_ms, const std::size_t entry_limit)
{
    if (entry_limit > MAX_STORAGE_SETTLEMENT_ENTRIES) throw std::invalid_argument{"settlement entry limit exceeds protocol maximum"};
    (void)period; (void)verified_since_ms;
    // A live-provider snapshot is not cumulative assignment-bound service.
    // Do not fabricate PAY counters or advance a period with an empty fallback.
    throw std::runtime_error{"canonical cumulative evidence preparation is not connected"};
}

std::optional<std::uint64_t> StorageService::EstimatedDailyRent()
{
    std::uint64_t units{0};
    {
        std::lock_guard lock{m_mutex};
        for (const auto& operation_id : m_placements->PlacementIndex()) {
            const auto placement = m_placements->Load(operation_id);
            if (!placement) continue;
            // Один billing unit на authorized chunk (DEC-279).
            if (units > std::numeric_limits<std::uint64_t>::max() - placement->leaves.size()) return std::nullopt;
            units += placement->leaves.size();
        }
    }
    return StorageRentPerDay(units, m_target);
}

std::optional<std::vector<unsigned char>> StorageService::Fetch(const ChunkId& chunk_id)
{
    return FetchInternal(chunk_id, {});
}

std::optional<std::vector<unsigned char>> StorageService::FetchInternal(const ChunkId& chunk_id,
    const std::span<const StorageEndpoint> preferred)
{
    const auto now_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    if (auto local = m_runtime.GetChunkBlobStore().Get(chunk_id); local && ComputeChunkId(*local) == chunk_id) {
        (void)m_runtime.GetChunkRetention().NoteCacheUse(chunk_id, now_ms);
        return local;
    }
    std::vector<StorageEndpoint> candidates{preferred.begin(), preferred.end()};
    auto others = m_transport.Providers();
    if (!ProviderSelector::Shuffle(others)) return std::nullopt;
    for (auto& provider : others) {
        if (!HasProvider(candidates, provider)) {
            candidates.push_back(std::move(provider));
        }
    }
    for (const auto& provider : candidates) {
        auto bytes = m_runtime.StorageIo().Submit(provider.storage_id, StorageIoScheduler::Kind::READ,
            [this, provider, chunk_id] { return m_transport.Get(provider, chunk_id); }).get();
        if (!bytes || ComputeChunkId(*bytes) != chunk_id) continue;
        // Кешируем уже проверенный ciphertext; fetch не создаёт новых provider-обязательств и не меняет финализацию.
        (void)m_runtime.GetChunkBlobStore().Put(chunk_id, *bytes);
        (void)m_runtime.GetChunkRetention().NoteCacheUse(chunk_id, now_ms);
        return bytes;
    }
    return std::nullopt;
}

} // namespace cybou
