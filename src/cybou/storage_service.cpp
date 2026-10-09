// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Identity storage orchestration, recovery and settlement preparation.

#include <cybou/storage_service_internal.h>
#include <cybou/node_runtime.h>
#include <cybou/storage_io_scheduler.h>
#include <cybou/root_publication.h>
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace cybou {

StorageService::StorageService(CybouNodeRuntime& runtime, StorageTransport& transport,
    PrivateApplicationStore& application_db, const std::uint8_t remote_replica_target)
    : m_runtime{runtime}, m_transport{transport},
      m_target{std::clamp<std::uint8_t>(remote_replica_target, 1, MAX_REPLICAS_PER_CHUNK)},
      m_placements{std::make_unique<PlacementRepository>(application_db)},
      m_evidence{std::make_unique<EvidenceLedger>(application_db)},
      m_verifier{std::make_unique<ReplicaVerifier>(transport, *m_evidence)}
{
}

StorageService::~StorageService() = default;

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
    // Payout-аккаунты известны только из живых проверенных bindings (в placement они не хранятся).
    std::map<std::array<unsigned char, 32>, AccountId> payout_by_storage;
    for (const auto& provider : m_transport.Providers()) {
        if (!provider.payout_account) continue;
        if (const auto account = AccountId::FromBytes(*provider.payout_account)) {
            payout_by_storage.emplace(provider.storage_id, *account);
        }
    }
    std::vector<Placement> placements;
    {
        std::lock_guard lock{m_mutex};
        for (const auto& operation_id : m_placements->PlacementIndex()) {
            if (auto placement = m_placements->Load(operation_id)) placements.push_back(std::move(*placement));
        }
    }
    const auto& params = m_runtime.GetNetworkGenesis().GetProtocolParameters();
    std::vector<StorageSettlementEntry> entries;
    for (const auto& placement : placements) {
        const auto lease = m_runtime.GetStorageLease(placement.operation_id);
        if (!lease || lease->replicas == 0 || lease->units == 0 ||
            period < lease->first_period || period >= lease->end_period) continue;
        const auto term = std::find_if(lease->funded_terms.begin(), lease->funded_terms.end(), [&](const auto& funded) {
            return funded.first_period <= period && period < funded.end_period;
        });
        if (term == lease->funded_terms.end() || term->paid_onboarding > term->initial_onboarding ||
            term->paid_locked > term->initial_locked) throw std::runtime_error{"invalid active funded storage term"};
        std::uint64_t remaining = (term->initial_onboarding - term->paid_onboarding) +
            (term->initial_locked - term->paid_locked);
        if (remaining == 0) continue;
        auto funded_params = params;
        funded_params.storage_rate_per_gib_day_replica = term->rate;
        funded_params.storage_settlement_period_seconds = term->period_seconds;
        const auto cap = ComputeStorageLeasePeriodCap(funded_params, lease->units, lease->replicas);
        if (!cap) continue;
        // Число проверенных в этом периоде чанков на каждый payout-аккаунт; одна реплика чанка на аккаунт.
        const auto verified_chunks = m_evidence->VerifiedChunks(placement, payout_by_storage,
            lease->payer, verified_since_ms);
        std::vector<std::pair<std::uint64_t, AccountId>> ranked;
        for (const auto& [account, chunks] : verified_chunks) ranked.emplace_back(chunks, account);
        std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
            return a.first != b.first ? a.first > b.first : a.second < b.second;
        });
        if (ranked.size() > lease->replicas) ranked.resize(lease->replicas);
        if (ranked.empty()) continue;
        // Cap периода делится по слотам `units × replicas`: аккаунт получает долю своих проверенных слотов.
        // Целые CYBOU: floor на аккаунт, остаток до floor(cap × verified / slots) раздаётся по одному,
        // начиная со смещения `period`, чтобы остаток не доставался всегда одному аккаунту.
        const auto slots = static_cast<unsigned __int128>(lease->units) * lease->replicas;
        unsigned __int128 verified_slots{0};
        std::vector<std::uint64_t> amounts;
        std::uint64_t floor_sum{0};
        for (const auto& [chunks, account] : ranked) {
            const auto chunk_count = std::min<std::uint64_t>(chunks, lease->units);
            verified_slots += chunk_count;
            amounts.push_back(static_cast<std::uint64_t>(static_cast<unsigned __int128>(*cap) * chunk_count / slots));
            floor_sum += amounts.back();
        }
        auto leftover = static_cast<std::uint64_t>(static_cast<unsigned __int128>(*cap) * verified_slots / slots) - floor_sum;
        for (std::size_t i{0}; leftover > 0 && i < amounts.size(); ++i, --leftover) {
            ++amounts[(period + i) % amounts.size()];
        }
        for (std::size_t i{0}; i < ranked.size(); ++i) {
            const auto paid = std::min(amounts[i], remaining);
            if (paid == 0) continue;
            remaining -= paid;
            entries.push_back({.publication_id = placement.operation_id, .payout_account = ranked[i].second, .amount = paid});
            if (entries.size() > entry_limit) throw std::length_error{"settlement preparation exceeds entry limit; no partial settlement produced"};
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.publication_id != b.publication_id ? a.publication_id < b.publication_id : a.payout_account < b.payout_account;
    });
    return entries;
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
