// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_observation_store.h>
#include <cybou/binary_codec.h>
#include <cybou/node_runtime.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
#include <limits>

namespace cybou {
namespace {
constexpr std::size_t MAX_RECEIPT_BYTES{8192};
template<typename Assignment>
std::string Prefix(const Assignment& assignment, std::uint8_t slot)
{
    return "storage/assignment-observations/" +
        Hash256{std::span<const unsigned char, 32>{assignment.plan.commitment}}.GetHex() +
        '/' + std::to_string(slot);
}
bool AssignmentValid(PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const AttestedStorageAssignment& assignment, std::uint8_t slot)
{
    return db.IsUnlocked() && slot < assignment.plan.selected.size() &&
        LoadStorageAssignmentAttestation(db, genesis, assignment.plan.context) == assignment;
}
struct CanonicalObservation {
    StorageAssignmentPlan plan;
    CanonicalStorageAssignment resolved;
    CybouNodeRuntime* runtime;
    Hash256 funding, activation;
};
bool AssignmentValid(PrivateApplicationStore& db, const VerifiedNetworkGenesis&,
    const CanonicalObservation& assignment, uint8_t slot)
{
    const auto current = ResolveCanonicalStorageAssignment(*assignment.runtime,
        assignment.funding, assignment.activation, assignment.plan.context.chunk);
    return current && *current == assignment.resolved && slot < assignment.plan.selected.size() &&
        db.IsUnlocked() && LoadStorageAssignment(db, assignment.plan.context) == assignment.plan;
}
bool ReceiptValid(const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    uint8_t slot, std::span<const unsigned char> receipt, uint32_t size)
{ return VerifyAssignedStorageReceipt(genesis, assignment, slot, receipt, size); }
bool ReceiptValid(const VerifiedNetworkGenesis&, const CanonicalObservation& assignment,
    uint8_t slot, std::span<const unsigned char> receipt, uint32_t size)
{
    if (!size || slot >= assignment.plan.selected.size()) return false;
    const auto& context = assignment.plan.context;
    const auto storage = VerifyStorageReceipt(receipt,
        Hash256{std::span<const unsigned char,32>{context.network_binding}},
        Hash256{std::span<const unsigned char,32>{context.publication}}, context.chunk, size);
    return storage && *storage == assignment.plan.selected[slot].storage_id;
}
std::optional<StorageAssignmentObservation> Observe(StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    uint8_t slot, std::span<const unsigned char> receipt, uint32_t size,
    std::span<const unsigned char> reference, bool full)
{ return ObserveAssignedStorageReplica(transport, provider, genesis, assignment, slot, receipt, size, reference, full); }
std::optional<StorageAssignmentObservation> Observe(StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis&, const CanonicalObservation& assignment,
    uint8_t slot, std::span<const unsigned char> receipt, uint32_t size,
    std::span<const unsigned char> reference, bool full)
{ return ObserveCanonicalStorageReplica(*assignment.runtime, transport, provider,
    assignment.funding, assignment.activation, assignment.plan.context.chunk, slot, receipt, size, reference, full); }
std::optional<std::vector<std::uint64_t>> Index(PrivateApplicationStore& db, const std::string& prefix)
{
    const auto bytes = db.Get(prefix + "/index");
    if (!bytes) return db.Has(prefix + "/index") || !db.IsUnlocked() ? std::nullopt :
        std::optional<std::vector<std::uint64_t>>{std::vector<std::uint64_t>{}};
    try {
        BinaryReader in{*bytes};
        const auto count = in.U32();
        if (count > MAX_STORAGE_ASSIGNMENT_OBSERVATIONS) return std::nullopt;
        std::vector<std::uint64_t> result;
        for (std::uint32_t i{0}; i < count; ++i) {
            const auto time = in.U64();
            if (!time || (!result.empty() && time <= result.back()) ||
                !db.Has(prefix + '/' + std::to_string(time))) return std::nullopt;
            result.push_back(time);
        }
        in.Finish(); return result;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}
std::vector<unsigned char> Encode(const StoredStorageAssignmentObservation& record)
{
    const auto& o = record.observation;
    BinaryWriter out;
    out.U64(record.observed_at_utc); out.U32(record.stored_size);
    out.Fixed(o.assignment_commitment); out.Fixed(o.storage_id); out.U8(o.replica_slot);
    out.U8(o.kind == StorageAssignmentObservationKind::OFFSET_AUDIT ? 0 : 1);
    out.Bytes(o.receipt, MAX_RECEIPT_BYTES);
    if (o.kind == StorageAssignmentObservationKind::OFFSET_AUDIT) {
        out.Fixed(o.challenge->chunk_id); out.U64(o.challenge->byte_offset); out.Fixed(o.challenge->nonce);
        out.U8(o.answer->held ? 1 : 0); out.Fixed(o.answer->response_hash);
    }
    return out.Take();
}
std::optional<StoredStorageAssignmentObservation> Decode(std::span<const unsigned char> bytes)
{
    try {
        BinaryReader in{bytes};
        StoredStorageAssignmentObservation r;
        r.observed_at_utc = in.U64(); r.stored_size = in.U32();
        auto& o = r.observation;
        o.assignment_commitment = in.Fixed<StorageAssignmentId>();
        o.storage_id = in.Fixed<StorageAssignmentId>(); o.replica_slot = in.U8();
        const auto kind = in.U8(); if (kind > 1) return std::nullopt;
        o.kind = kind == 0 ? StorageAssignmentObservationKind::OFFSET_AUDIT : StorageAssignmentObservationKind::FULL_GET;
        const auto receipt = in.Bytes(MAX_RECEIPT_BYTES); o.receipt.assign(receipt.begin(), receipt.end());
        if (kind == 0) {
            o.challenge = StorageAuditChallenge{in.Fixed<ChunkId>(), in.U64(), in.Fixed<StorageAssignmentId>()};
            o.answer = StorageAuditAnswer{in.Flag(), in.Fixed<Hash256>()};
        }
        in.Finish(); return r;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}
template<typename Assignment>
bool Verify(const VerifiedNetworkGenesis& genesis, const Assignment& assignment,
    std::uint8_t slot, const StoredStorageAssignmentObservation& record, std::span<const unsigned char> reference)
{
    const auto& o = record.observation;
    if (!record.observed_at_utc || !record.stored_size || record.stored_size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
        reference.size() != record.stored_size || ComputeChunkId(reference) != assignment.plan.context.chunk ||
        o.assignment_commitment != assignment.plan.commitment || o.replica_slot != slot ||
        slot >= assignment.plan.selected.size() || o.storage_id != assignment.plan.selected[slot].storage_id ||
        !ReceiptValid(genesis, assignment, slot, o.receipt, record.stored_size)) return false;
    if (o.kind == StorageAssignmentObservationKind::FULL_GET) return !o.challenge && !o.answer;
    if (o.kind != StorageAssignmentObservationKind::OFFSET_AUDIT || !o.challenge || !o.answer ||
        !o.answer->held || o.challenge->chunk_id != assignment.plan.context.chunk) return false;
    const auto hash = ComputeStorageAuditResponse(reference, o.challenge->byte_offset, o.challenge->nonce);
    return hash && *hash == o.answer->response_hash;
}

template<typename Assignment>
std::optional<StoredStorageAssignmentObservation> LoadObservation(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const Assignment& assignment, uint8_t slot, uint64_t observed_at_utc,
    std::span<const unsigned char> reference_bytes)
{
    PrivateApplicationStore::Batch snapshot{db};
    if (!AssignmentValid(db, genesis, assignment, slot)) return std::nullopt;
    const auto prefix = Prefix(assignment, slot);
    const auto index = Index(db, prefix);
    if (!index || !std::binary_search(index->begin(), index->end(), observed_at_utc)) return std::nullopt;
    const auto bytes = db.Get(prefix + '/' + std::to_string(observed_at_utc));
    const auto record = bytes ? Decode(*bytes) : std::nullopt;
    return record && record->observed_at_utc == observed_at_utc &&
        Verify(genesis, assignment, slot, *record, reference_bytes) ? record : std::nullopt;
}

struct ServiceCheckpoint {
    std::uint64_t anchor{0}, seconds{0}, last_attempt{0}, last_success{0}, successes{0};
    bool pending{false};
};
std::vector<unsigned char> EncodeCheckpoint(const ServiceCheckpoint& c)
{
    BinaryWriter out;
    out.U64(c.anchor); out.U64(c.seconds); out.U64(c.last_attempt);
    out.U64(c.last_success); out.U64(c.successes); out.U8(c.pending ? 1 : 0);
    return out.Take();
}
std::optional<ServiceCheckpoint> DecodeCheckpoint(std::span<const unsigned char> bytes)
{
    try {
        BinaryReader in{bytes};
        ServiceCheckpoint c{in.U64(), in.U64(), in.U64(), in.U64(), in.U64(), in.Flag()};
        in.Finish();
        if (!c.anchor || !c.seconds || !c.last_attempt || c.last_success > c.last_attempt ||
            (c.pending && c.last_success)) return std::nullopt;
        return c;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}

template<typename Assignment>
std::optional<StoredStorageAssignmentObservation> ObserveService(
    PrivateApplicationStore& db, StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const Assignment& assignment,
    std::uint8_t slot, std::span<const unsigned char> receipt, std::uint32_t size,
    std::uint64_t time, std::uint64_t through, std::span<const unsigned char> reference,
    bool force_full, const StorageAssignmentEvidenceScope& scope)
{
    const auto& c = scope.assignment.context;
    if (!time || time > through || !size || size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
        scope.assignment != assignment.plan || scope.replica_slot != slot ||
        !scope.term_start_utc || !scope.period_seconds || c.term_end <= c.term_start ||
        c.term_end - c.term_start > (std::numeric_limits<std::uint64_t>::max() - scope.term_start_utc) / scope.period_seconds ||
        time < scope.term_start_utc || time > scope.term_start_utc + (c.term_end - c.term_start) * scope.period_seconds ||
        slot >= assignment.plan.selected.size() || provider.storage_id != assignment.plan.selected[slot].storage_id ||
        !ReceiptValid(genesis, assignment, slot, receipt, size) ||
        (!reference.empty() && (reference.size() != size || ComputeChunkId(reference) != c.chunk))) return std::nullopt;
    const auto prefix = Prefix(assignment, slot);
    const auto checkpoint_key = prefix + "/service-checkpoint";
    ServiceCheckpoint prior{scope.term_start_utc, scope.period_seconds};
    std::optional<std::vector<unsigned char>> previous_record;
    std::vector<unsigned char> intent;
    {
        PrivateApplicationStore::Batch prepare{db};
        if (!prepare.IsOutermost() || !AssignmentValid(db, genesis, assignment, slot)) return std::nullopt;
        const auto index = Index(db, prefix); if (!index) return std::nullopt;
        if (const auto bytes = db.Get(checkpoint_key)) {
            const auto decoded = DecodeCheckpoint(*bytes);
            if (!decoded) return std::nullopt;
            prior = *decoded;
        } else if (db.Has(checkpoint_key) || !index->empty()) return std::nullopt;
        if (prior.anchor != scope.term_start_utc || prior.seconds != scope.period_seconds ||
            prior.successes != index->size() ||
            (!index->empty() && (index->back() > prior.last_attempt ||
                (prior.last_success && index->back() != prior.last_success)))) return std::nullopt;
        // Exact committed retry is read-only; do not issue a fresh random audit.
        if (std::binary_search(index->begin(), index->end(), time)) {
            if (!prior.pending && prior.last_success == time)
                return LoadObservation(db, genesis, assignment, slot, time, reference);
            return std::nullopt;
        }
        if (time <= prior.last_attempt || index->size() >= MAX_STORAGE_ASSIGNMENT_OBSERVATIONS) return std::nullopt;
        if (prior.last_success) {
            previous_record = db.Get(prefix + '/' + std::to_string(prior.last_success));
            const auto previous = previous_record ? Decode(*previous_record) : std::nullopt;
            if (!previous || previous->observed_at_utc != prior.last_success ||
                (!reference.empty() && !Verify(genesis, assignment, slot, *previous, reference))) return std::nullopt;
        }
        // The durable intent has no successful predecessor. An interrupted or
        // failed completion therefore cannot later bridge this unchecked gap.
        auto pending = prior;
        pending.last_attempt = time; pending.last_success = 0; pending.pending = true;
        intent = EncodeCheckpoint(pending);
        if (!db.Put(checkpoint_key, intent) || !prepare.Commit()) return std::nullopt;
    }
    const bool required_get = prior.successes == 0 ||
        prior.successes % STORAGE_ASSIGNMENT_FULL_GET_EVERY_SUCCESSES == STORAGE_ASSIGNMENT_FULL_GET_EVERY_SUCCESSES - 1;
    const auto observation = Observe(transport, provider, genesis, assignment,
        slot, receipt, size, reference, force_full || required_get);
    PrivateApplicationStore::Batch complete{db};
    if (!complete.IsOutermost() || !AssignmentValid(db, genesis, assignment, slot) ||
        db.Get(checkpoint_key) != intent) return std::nullopt;
    auto next = prior;
    next.last_attempt = time; next.last_success = 0; next.pending = false;
    if (!observation) {
        // Failed checks preserve the success ordinal (and hence the GET deadline).
        if (!db.Put(checkpoint_key, EncodeCheckpoint(next)) || !complete.Commit()) return std::nullopt;
        return std::nullopt;
    }
    StoredStorageAssignmentObservation record{time, size, *observation};
    const auto verified_bytes = observation->kind == StorageAssignmentObservationKind::FULL_GET ?
        std::span<const unsigned char>{observation->retrieved_bytes} : reference;
    if (!Verify(genesis, assignment, slot, record, verified_bytes)) return std::nullopt;
    auto index = Index(db, prefix);
    if (!index || index->size() != prior.successes || (!index->empty() && index->back() >= time)) return std::nullopt;
    const auto encoded = Encode(record);
    const auto key = prefix + '/' + std::to_string(time);
    if (db.Has(key)) return std::nullopt;
    if (prior.last_success && time - prior.last_success <= STORAGE_ASSIGNMENT_MAX_SERVICE_GAP_SECONDS) {
        const auto previous = Decode(*previous_record);
        if (!previous || !Verify(genesis, assignment, slot, *previous, verified_bytes)) return std::nullopt;
        auto start = prior.last_success;
        while (start < time) {
            const auto period_offset = (start - scope.term_start_utc) / scope.period_seconds;
            const auto end = std::min(time, scope.term_start_utc + (period_offset + 1) * scope.period_seconds);
            StorageAssignmentInterval interval{c.term_start + period_offset, start, end};
            // Local content fingerprint of the two retained observations and
            // this exact period slice; no new signature/KDF domain or wire proof.
            BinaryWriter proof;
            proof.Bytes(*previous_record, 16384); proof.Bytes(encoded, 16384);
            proof.U64(scope.term_start_utc); proof.U64(scope.period_seconds);
            proof.U64(interval.period); proof.U64(start); proof.U64(end);
            const auto proof_bytes = proof.Take();
            BinaryWriter pair;
            pair.U64(previous->observed_at_utc); pair.U64(time);
            pair.U64(scope.term_start_utc); pair.U64(scope.period_seconds);
            pair.U64(interval.period); pair.U64(start); pair.U64(end);
            const auto pair_bytes = pair.Take();
            if (!crypto::ComputeSha256({std::span<const unsigned char>{proof_bytes}}, interval.proof_commitment.data()) ||
                !db.Put(prefix + "/interval-proof/" + Hash256{std::span<const unsigned char,32>{interval.proof_commitment}}.GetHex(), pair_bytes) ||
                AppendStorageAssignmentEvidence(db, scope, interval, through) != StorageEvidenceAppendResult::ADDED) return std::nullopt;
            start = end;
        }
    }
    index->push_back(time);
    BinaryWriter index_bytes; index_bytes.U32(static_cast<std::uint32_t>(index->size()));
    for (const auto observed_time : *index) index_bytes.U64(observed_time);
    next.last_success = time; ++next.successes;
    if (!db.Put(key, encoded) || !db.Put(prefix + "/index", index_bytes.Take()) ||
        !db.Put(checkpoint_key, EncodeCheckpoint(next)) || !complete.Commit()) return std::nullopt;
    return record;
}
}

std::optional<StoredStorageAssignmentObservation> ObserveAndStoreAssignedStorageReplica(
    PrivateApplicationStore& db, StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    const std::uint8_t slot, const std::span<const unsigned char> receipt, const std::uint32_t stored_size,
    const std::uint64_t observed_at_utc, const std::uint64_t verified_through_utc,
    const std::span<const unsigned char> expected_bytes, const bool force_full,
    const StorageAssignmentEvidenceScope* service_scope)
{
    if (service_scope) return ObserveService(db, transport, provider, genesis, assignment, slot,
        receipt, stored_size, observed_at_utc, verified_through_utc, expected_bytes, force_full, *service_scope);
    if (!observed_at_utc || observed_at_utc > verified_through_utc) return std::nullopt;
    {
        PrivateApplicationStore::Batch check{db};
        if (!check.IsOutermost() || !AssignmentValid(db, genesis, assignment, slot) ||
            !Index(db, Prefix(assignment, slot))) return std::nullopt;
    } // Never hold the database lock during network I/O.
    const auto observed = ObserveAssignedStorageReplica(transport, provider, genesis, assignment,
        slot, receipt, stored_size, expected_bytes, force_full);
    if (!observed) return std::nullopt;
    StoredStorageAssignmentObservation record{observed_at_utc, stored_size, *observed};
    const auto reference = observed->kind == StorageAssignmentObservationKind::FULL_GET ?
        std::span<const unsigned char>{observed->retrieved_bytes} : expected_bytes;
    if (!Verify(genesis, assignment, slot, record, reference)) return std::nullopt;
    PrivateApplicationStore::Batch batch{db};
    if (!batch.IsOutermost() || !AssignmentValid(db, genesis, assignment, slot)) return std::nullopt;
    const auto prefix = Prefix(assignment, slot);
    auto index = Index(db, prefix); if (!index) return std::nullopt;
    const auto key = prefix + '/' + std::to_string(observed_at_utc);
    const auto encoded = Encode(record);
    const auto position = std::lower_bound(index->begin(), index->end(), observed_at_utc);
    if (position != index->end() && *position == observed_at_utc) {
        return db.Get(key) == encoded ? std::optional{record} : std::nullopt;
    }
    if (db.Has(key) || index->size() >= MAX_STORAGE_ASSIGNMENT_OBSERVATIONS) return std::nullopt;
    index->insert(position, observed_at_utc);
    BinaryWriter index_bytes; index_bytes.U32(static_cast<std::uint32_t>(index->size()));
    for (const auto time : *index) index_bytes.U64(time);
    if (!db.Put(key, encoded) || !db.Put(prefix + "/index", index_bytes.Take()) || !batch.Commit()) return std::nullopt;
    return record;
}

std::optional<StoredStorageAssignmentObservation> LoadAssignedStorageObservation(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const AttestedStorageAssignment& assignment, const std::uint8_t slot,
    const std::uint64_t observed_at_utc, const std::span<const unsigned char> reference_bytes)
{
    return LoadObservation(db, genesis, assignment, slot, observed_at_utc, reference_bytes);
}

std::optional<StoredStorageAssignmentObservation> ObserveAndStoreCanonicalStorageReplica(
    PrivateApplicationStore& db, CybouNodeRuntime& runtime, StorageTransport& transport,
    const StorageEndpoint& provider, const Hash256& funding, const Hash256& activation,
    const ChunkId& chunk, uint8_t slot, std::span<const unsigned char> receipt, uint32_t size,
    uint64_t time, uint64_t through, std::span<const unsigned char> expected, bool force_full)
{
    const auto resolved = ResolveCanonicalStorageAssignment(runtime, funding, activation, chunk);
    if (!resolved || time < resolved->effective_start_utc || time > resolved->end_utc || time > through ||
        slot >= resolved->plan.selected.size()) return std::nullopt;
    {
        PrivateApplicationStore::Batch check{db};
        if (!check.IsOutermost() || !db.IsUnlocked()) return std::nullopt;
    }
    CanonicalObservation assignment{resolved->plan, *resolved, &runtime, funding, activation};
    if (!ReceiptValid(runtime.GetNetworkGenesis(), assignment, slot, receipt, size) ||
        !FreezeStorageAssignment(db, resolved->plan)) return std::nullopt;
    const StorageAssignmentEvidenceScope scope{resolved->plan, slot, resolved->term_start_utc, resolved->period_seconds};
    return ObserveService(db, transport, provider, runtime.GetNetworkGenesis(), assignment,
        slot, receipt, size, time, through, expected, force_full, scope);
}

bool VerifyCanonicalStorageInterval(PrivateApplicationStore& db, CybouNodeRuntime& runtime,
    const Hash256& funding, const Hash256& activation, const ChunkId& chunk, uint8_t slot,
    const StorageAssignmentInterval& interval, std::span<const unsigned char> reference)
{
    const auto resolved = ResolveCanonicalStorageHistory(runtime, funding, activation, chunk);
    if (!resolved || slot >= resolved->plan.selected.size()) return false;
    PrivateApplicationStore::Batch snapshot{db};
    if (!snapshot.IsOutermost() || !db.IsUnlocked() ||
        LoadStorageAssignment(db, resolved->plan.context) != resolved->plan) return false;
    CanonicalObservation assignment{resolved->plan, *resolved, &runtime, funding, activation};
    const auto prefix = Prefix(assignment, slot);
    const auto bytes = db.Get(prefix + "/interval-proof/" + Hash256{std::span<const unsigned char,32>{interval.proof_commitment}}.GetHex());
    if (!bytes) return false;
    try {
        BinaryReader in{*bytes, 56};
        const auto previous_time = in.U64(), current_time = in.U64();
        if (in.U64() != resolved->term_start_utc || in.U64() != resolved->period_seconds ||
            in.U64() != interval.period || in.U64() != interval.start_utc || in.U64() != interval.end_utc) return false;
        in.Finish();
        const auto previous_bytes = db.Get(prefix + '/' + std::to_string(previous_time));
        const auto current_bytes = db.Get(prefix + '/' + std::to_string(current_time));
        if (!previous_bytes || !current_bytes) return false;
        BinaryWriter proof;
        proof.Bytes(*previous_bytes, 16384); proof.Bytes(*current_bytes, 16384);
        proof.U64(resolved->term_start_utc); proof.U64(resolved->period_seconds);
        proof.U64(interval.period); proof.U64(interval.start_utc); proof.U64(interval.end_utc);
        const auto proof_bytes = proof.Take();
        StorageAssignmentId fingerprint{};
        if (!crypto::ComputeSha256({std::span<const unsigned char>{proof_bytes}}, fingerprint.data()) ||
            fingerprint != interval.proof_commitment) return false;
        const auto previous = Decode(*previous_bytes), current = Decode(*current_bytes);
        if (!previous || !current || previous->observed_at_utc != previous_time || current->observed_at_utc != current_time) return false;
        if (!previous || !current || !Verify(runtime.GetNetworkGenesis(), assignment, slot, *previous, reference) ||
            !Verify(runtime.GetNetworkGenesis(), assignment, slot, *current, reference) ||
            previous->observed_at_utc < resolved->effective_start_utc || current->observed_at_utc > resolved->end_utc ||
            current->observed_at_utc <= previous->observed_at_utc ||
            current->observed_at_utc - previous->observed_at_utc > STORAGE_ASSIGNMENT_MAX_SERVICE_GAP_SECONDS ||
            interval.period < resolved->plan.context.term_start || interval.period >= resolved->plan.context.term_end) return false;
        if (interval.period - resolved->plan.context.term_start >=
            (std::numeric_limits<uint64_t>::max() - resolved->term_start_utc) / resolved->period_seconds) return false;
        const StorageAssignmentEvidenceScope scope{resolved->plan, slot, resolved->term_start_utc, resolved->period_seconds};
        const auto claims = LoadStorageFundedSlotClaims(db, scope);
        if (!claims || std::none_of(claims->begin(), claims->end(), [&](const auto& claim) {
            return claim.assignment == resolved->plan.commitment && claim.interval == interval;
        })) return false;
        const auto period_start = resolved->term_start_utc +
            (interval.period - resolved->plan.context.term_start) * resolved->period_seconds;
        if (interval.start_utc != std::max(previous->observed_at_utc, period_start) ||
            interval.end_utc != std::min(current->observed_at_utc, period_start + resolved->period_seconds) ||
            interval.end_utc <= interval.start_utc) return false;
        const auto index = Index(db, prefix);
        if (!index) return false;
        const auto pos = std::lower_bound(index->begin(), index->end(), previous->observed_at_utc);
        if (pos == index->end() || *pos != previous->observed_at_utc || std::next(pos) == index->end() ||
            *std::next(pos) != current->observed_at_utc) return false;
        return true;
    } catch (const std::invalid_argument&) { return false;
    } catch (const std::length_error&) { return false; }
}
}
