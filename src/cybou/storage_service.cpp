// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Identity storage orchestration, recovery and settlement preparation.

#include <cybou/storage_service_internal.h>
#include <cybou/node_runtime.h>
#include <cybou/storage_io_scheduler.h>
#include <cybou/root_publication.h>
#include <cybou/crypto/sha256.h>
#include <cybou/binary_codec.h>
#include <cybou/storage_assignment_observation_store.h>
#include <cybou/storage_economy.h>
#include <set>
#include <tuple>
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
std::string SettlementJournalKey(const CybouNodeRuntime& runtime, const StorageSettlement& request)
{
    auto key = "storage/settlement/" + runtime.GetNetworkBinding().GetHex() + "/action/" +
        std::to_string(static_cast<unsigned>(request.action)) + '/' + std::to_string(request.period);
    if (request.action == StorageSettlementAction::PREPARE)
        key += '/' + request.funded_term_id.GetHex() + '/' + std::to_string(request.assignment_epoch);
    else if (request.action == StorageSettlementAction::ACTIVATE)
        key += '/' + request.preparation_id.GetHex();
    return key;
}
// Existing app.db value bound, not a retention/compaction policy. Oversize lists
// fail before preparing anything; obligations are never truncated to fit.
constexpr std::size_t EVIDENCE_BYTES_LIMIT{4 * 1024 * 1024};
std::optional<std::vector<unsigned char>> EvidenceBytes(std::span<const Hash256> references)
{
    if (references.size() > (EVIDENCE_BYTES_LIMIT - 4) / 32) return std::nullopt;
    BinaryWriter out{EVIDENCE_BYTES_LIMIT}; out.U32(static_cast<uint32_t>(references.size()));
    const Hash256 zero;
    for (std::size_t i = 0; i < references.size(); ++i) {
        if (references[i] == zero || (i && !(references[i - 1] < references[i]))) return std::nullopt;
        out.Fixed({references[i].begin(), 32});
    }
    return out.Take();
}
bool EvidenceValid(std::span<const unsigned char> bytes, const StorageSettlement& request)
{
    if (bytes.size() < 4 || bytes.size() > EVIDENCE_BYTES_LIMIT) return false;
    const auto count = ReadLittleEndian<uint32_t>(bytes.first(4));
    if (count != (bytes.size() - 4) / 32 || bytes.size() != 4 + std::size_t{count} * 32 ||
        (!request.entries.empty() && !count)) return false;
    Hash256 prior, digest;
    for (uint32_t i = 0; i < count; ++i) {
        const Hash256 ref{std::span<const unsigned char,32>{bytes.data() + 4 + std::size_t{i} * 32, 32}};
        if (ref == Hash256{} || (i && !(prior < ref))) return false;
        prior = ref;
    }
    return crypto::ComputeSha256({bytes}, digest.begin()) && digest == request.evidence_root;
}
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
    return PreparedSettlement(StorageSettlement{.period = period});
}

std::optional<StorageSettlement> StorageService::PreparedSettlement(const StorageSettlement& request)
{
    PrivateApplicationStore::Batch snapshot{m_db};
    if (!snapshot.IsOutermost() || !m_db.IsUnlocked()) throw std::runtime_error{"settlement journal unavailable"};
    const auto key = SettlementJournalKey(m_runtime, request);
    const auto prior_key = "storage/settlement/" + m_runtime.GetNetworkBinding().GetHex() + '/' + std::to_string(request.period);
    if (m_db.Has(prior_key + "/prepared") || m_db.Has(prior_key + "/signed"))
        throw std::runtime_error{"period-only settlement journal requires explicit reconciliation"};
    const auto prepared = m_db.Get(key + "/prepared");
    const auto signed_bytes = m_db.Get(key + "/signed");
    if (!prepared) {
        if (m_db.Has(key + "/prepared") || m_db.Has(key + "/signed") || m_db.Has(key + "/evidence"))
            throw std::runtime_error{"missing settlement preparation"};
        return std::nullopt;
    }
    auto result = DecodeSettlement(*prepared);
    if (!result || SettlementJournalKey(m_runtime, *result) != key || PreparedBytes(*result) != prepared)
        throw std::runtime_error{"corrupt settlement preparation"};
    const auto evidence = m_db.Get(key + "/evidence");
    if (result->action == StorageSettlementAction::PAY) {
        if (!evidence || !EvidenceValid(*evidence, *result))
            throw std::runtime_error{"missing or corrupt settlement evidence references"};
    } else if (m_db.Has(key + "/evidence")) throw std::runtime_error{"unexpected assignment evidence references"};
    if (signed_bytes) {
        result = DecodeSettlement(*signed_bytes);
        const auto digest = result ? ComputeStorageSettlementDigest(m_runtime.GetNetworkBinding(), *result) : std::nullopt;
        if (!result || PreparedBytes(*result) != prepared || !digest ||
            !VerifyIdentityMessage(m_runtime.GetNetworkGenesis().GetPoaPublicKey(), result->poa_signature, *digest))
            throw std::runtime_error{"corrupt signed settlement"};
    } else if (m_db.Has(key + "/signed")) throw std::runtime_error{"unreadable signed settlement"};
    return result;
}

StorageService::CanonicalSettlementPreparation StorageService::PrepareSettlement(const uint64_t period)
{
    // Never rebase an exact operation, including an already-finalized period.
    if (const auto retained = PreparedSettlement(period)) {
        PrivateApplicationStore::Batch snapshot{m_db};
        if (!snapshot.IsOutermost()) throw std::runtime_error{"settlement journal unavailable"};
        const auto bytes = m_db.Get(SettlementJournalKey(m_runtime, *retained) + "/evidence");
        if (!bytes || !EvidenceValid(*bytes, *retained)) throw std::runtime_error{"settlement evidence unavailable"};
        CanonicalSettlementPreparation result{*retained, {}};
        const auto count = ReadLittleEndian<uint32_t>(std::span<const unsigned char>{*bytes}.first(4));
        for (uint32_t i = 0; i < count; ++i)
            result.evidence_references.emplace_back(std::span<const unsigned char,32>{bytes->data() + 4 + size_t{i} * 32, 32});
        return result;
    }
    const auto root = m_runtime.GetStateRoot();
    const auto state = m_runtime.GetStore().GetStateSnapshot();
    if (!state || CybouStateHash(*state.state) != root || period != state.state->settlement.next_period ||
        period == std::numeric_limits<uint64_t>::max()) throw std::runtime_error{"settlement cursor unavailable"};
    const auto start = state.state->settlement.next_period_start_utc;
    const auto seconds = m_runtime.GetNetworkGenesis().GetProtocolParameters().storage_settlement_period_seconds;
    if (!start || !seconds || start > std::numeric_limits<uint64_t>::max() - seconds)
        throw std::runtime_error{"settlement time anchor unavailable"};
    PrivateApplicationStore::Batch snapshot{m_db};
    if (!snapshot.IsOutermost() || !m_db.IsUnlocked()) throw std::runtime_error{"settlement evidence unavailable"};
    CanonicalSettlementPreparation result;
    auto& pay = result.settlement;
    pay.period = period; pay.period_start_utc = start; pay.period_end_utc = start + seconds;
    using Key = std::tuple<uint8_t, std::array<unsigned char,32>, AccountId>;
    struct Service { uint64_t seconds{0}; std::vector<Hash256> references; };
    std::set<Hash256> witnesses;
    size_t reference_count{0};
    for (const auto& [publication, lease] : state.state->leases) {
        for (const auto& term : lease.funded_terms) {
            if (period < term.first_period || period >= std::min(term.end_period, lease.end_period)) continue;
            if (term.refunded_onboarding || term.refunded_locked || term.period_seconds != seconds || term.assignments.empty())
                throw std::runtime_error{"active funded assignment unavailable"};
            std::vector<ChunkId> manifest;
            for (size_t i = 0; i < term.assignments.size(); ++i) {
                const auto& epoch = term.assignments[i];
                const auto end = std::min({term.end_period, lease.end_period,
                    i + 1 < term.assignments.size() ? term.assignments[i + 1].effective_period : term.end_period});
                if (end <= epoch.effective_period) continue;
                const auto status = m_runtime.GetOperationStatus(epoch.operation_id);
                if (status.kind != OperationStatusKind::FINALIZED) throw std::runtime_error{"activation not finalized"};
                const auto block = m_runtime.GetBlockAtHeight(status.finalized_height);
                if (!block) throw std::runtime_error{"activation block unavailable"};
                const StorageSettlement* activation{nullptr};
                for (const auto& operation : block->block.operations)
                    if (ComputeOperationId(operation) == epoch.operation_id) activation = std::get_if<StorageSettlement>(&operation);
                if (!activation || activation->action != StorageSettlementAction::ACTIVATE ||
                    activation->preparation_id != epoch.preparation_id || activation->manifest.size() != lease.units)
                    throw std::runtime_error{"canonical manifest unavailable"};
                if (manifest.empty()) manifest = activation->manifest;
                else if (manifest != activation->manifest) throw std::runtime_error{"canonical manifests disagree"};
            }
            if (manifest.empty()) throw std::runtime_error{"canonical manifest unavailable"};
            std::map<Key, Service> services;
            for (const auto& chunk : manifest) {
                const auto bytes = m_runtime.GetChunkBlobStore().Get(chunk);
                if (!bytes) throw std::runtime_error{"settlement reference chunk unavailable"};
                for (uint8_t slot = 0; slot < lease.replicas; ++slot) {
                    const auto verified = detail::LoadCanonicalStorageServiceSnapshot(m_db, m_runtime,
                        term.funding_operation_id, chunk, slot, period, *bytes);
                    if (!verified) throw std::runtime_error{"canonical service evidence incomplete"};
                    for (const auto& item : *verified) {
                        const auto payout = AccountId::FromBytes(item.provider.payout_account);
                        if (!payout) throw std::runtime_error{"canonical payout account unavailable"};
                        auto& accumulated = services[{slot, item.provider.storage_id, *payout}];
                        if (item.verified_unit_seconds > std::numeric_limits<uint64_t>::max() - accumulated.seconds)
                            throw std::runtime_error{"canonical service overflow"};
                        accumulated.seconds += item.verified_unit_seconds;
                        if (item.evidence_references.size() > (EVIDENCE_BYTES_LIMIT - 4) / 32 - reference_count)
                            throw std::runtime_error{"settlement evidence exceeds atomic bound"};
                        reference_count += item.evidence_references.size();
                        accumulated.references.insert(accumulated.references.end(), item.evidence_references.begin(), item.evidence_references.end());
                    }
                }
            }
            for (const auto& ledger : term.service_payments) {
                const auto found = services.find({ledger.slot, ledger.storage_id, ledger.payout_account});
                if (found == services.end() || found->second.seconds < ledger.verified_unit_seconds)
                    throw std::runtime_error{"collector service regresses canonical ledger"};
            }
            const AssignedStorageBudget budget{term.replica_share, term.initial_onboarding + term.initial_locked,
                term.contracted_unit_seconds};
            for (const auto& [key, service] : services) {
                const auto& [slot, storage, payout] = key;
                const auto ledger = std::find_if(term.service_payments.begin(), term.service_payments.end(), [&](const auto& paid) {
                    return std::tie(paid.slot, paid.storage_id, paid.payout_account) == key;
                });
                const auto prior_service = ledger == term.service_payments.end() ? 0 : ledger->verified_unit_seconds;
                if (service.seconds == prior_service) continue;
                const auto due = ComputeAssignedStoragePayout(budget, service.seconds,
                    ledger == term.service_payments.end() ? 0 : ledger->paid);
                if (!due || pay.entries.size() == MAX_STORAGE_SETTLEMENT_ENTRIES)
                    throw std::runtime_error{"settlement payout exceeds atomic bound"};
                pay.entries.push_back({term.funding_operation_id, payout, *due, slot, storage, service.seconds});
                result.evidence_references.insert(result.evidence_references.end(), service.references.begin(), service.references.end());
                for (size_t i = 0; i < term.assignments.size(); ++i) {
                    const auto& epoch = term.assignments[i];
                    const auto end = std::min({period + 1, term.end_period, lease.end_period,
                        i + 1 < term.assignments.size() ? term.assignments[i + 1].effective_period : term.end_period});
                    if (end > epoch.effective_period && std::any_of(epoch.allocations.begin(), epoch.allocations.end(), [&](const auto& a) {
                        return std::tie(a.slot, a.storage_id, a.payout_account) == key;
                    })) witnesses.insert(epoch.operation_id);
                }
            }
        }
    }
    std::sort(pay.entries.begin(), pay.entries.end(), [](const auto& a, const auto& b) {
        return std::tie(a.funding_operation_id, a.slot, a.storage_id, a.payout_account) <
            std::tie(b.funding_operation_id, b.slot, b.storage_id, b.payout_account);
    });
    pay.activation_witnesses.assign(witnesses.begin(), witnesses.end());
    auto& refs = result.evidence_references;
    std::sort(refs.begin(), refs.end()); refs.erase(std::unique(refs.begin(), refs.end()), refs.end());
    const auto encoded = EvidenceBytes(refs);
    if (!encoded || !crypto::ComputeSha256({std::span<const unsigned char>{*encoded}}, pay.evidence_root.begin()) ||
        !PreparedBytes(pay) || m_runtime.GetStateRoot() != root ||
        m_runtime.CheckStorageSettlementInputs(pay) != StorageSettlementError::NONE || m_runtime.GetStateRoot() != root)
        throw std::runtime_error{"canonical settlement preparation rejected"};
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
    return SubmitSettlement(requested);
}

OperationSubmitResult StorageService::SubmitSettlement(const StorageSettlement& requested,
    const std::span<const Hash256> evidence_references)
{
    const auto prepared_bytes = PreparedBytes(requested);
    if (!prepared_bytes) return {};
    const auto key = SettlementJournalKey(m_runtime, requested);
    const auto retained = PreparedSettlement(requested);
    if (retained && PreparedBytes(*retained) != prepared_bytes)
        throw std::runtime_error{"different settlement already prepared for this action scope"};
    std::optional<std::vector<unsigned char>> evidence;
    if (requested.action == StorageSettlementAction::PAY) {
        if (retained && evidence_references.empty()) evidence = m_db.Get(key + "/evidence");
        else evidence = EvidenceBytes(evidence_references);
        if (!evidence || !EvidenceValid(*evidence, requested)) return {};
    } else if (!evidence_references.empty()) return {};
    // Invalid fresh input must not freeze the period. Retained exact operations
    // still follow replay/reconciliation, even if canonical state has advanced.
    if (!retained && m_runtime.CheckStorageSettlementInputs(requested) != StorageSettlementError::NONE) return {};
    {
        PrivateApplicationStore::Batch prepare{m_db};
        if (!prepare.IsOutermost() || !m_db.IsUnlocked()) throw std::runtime_error{"settlement journal unavailable"};
        const auto prior = m_db.Get(key + "/prepared");
        if (prior && evidence && m_db.Get(key + "/evidence") != evidence)
            throw std::runtime_error{"conflicting settlement evidence references"};
        if (prior && prior != prepared_bytes) throw std::runtime_error{"conflicting settlement preparation"};
        if ((!prior && (m_db.Has(key + "/prepared") || m_db.Has(key + "/signed"))) ||
            (!prior && !m_db.Put(key + "/prepared", *prepared_bytes)) ||
            (!prior && evidence && !m_db.Put(key + "/evidence", *evidence)) || !prepare.Commit())
            throw std::runtime_error{"cannot persist settlement preparation"};
    }
    auto settlement = PreparedSettlement(requested);
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
        settlement = PreparedSettlement(requested);
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
