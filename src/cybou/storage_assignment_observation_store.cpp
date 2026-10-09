// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_observation_store.h>
#include <cybou/binary_codec.h>
#include <cybou/encrypted_chunk.h>
#include <algorithm>

namespace cybou {
namespace {
constexpr std::size_t MAX_RECEIPT_BYTES{8192};
std::string Prefix(const AttestedStorageAssignment& assignment, std::uint8_t slot)
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
bool Verify(const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    std::uint8_t slot, const StoredStorageAssignmentObservation& record, std::span<const unsigned char> reference)
{
    const auto& o = record.observation;
    if (!record.observed_at_utc || !record.stored_size || record.stored_size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
        reference.size() != record.stored_size || ComputeChunkId(reference) != assignment.plan.context.chunk ||
        o.assignment_commitment != assignment.plan.commitment || o.replica_slot != slot ||
        slot >= assignment.plan.selected.size() || o.storage_id != assignment.plan.selected[slot].storage_id ||
        !VerifyAssignedStorageReceipt(genesis, assignment, slot, o.receipt, record.stored_size)) return false;
    if (o.kind == StorageAssignmentObservationKind::FULL_GET) return !o.challenge && !o.answer;
    if (o.kind != StorageAssignmentObservationKind::OFFSET_AUDIT || !o.challenge || !o.answer ||
        !o.answer->held || o.challenge->chunk_id != assignment.plan.context.chunk) return false;
    const auto hash = ComputeStorageAuditResponse(reference, o.challenge->byte_offset, o.challenge->nonce);
    return hash && *hash == o.answer->response_hash;
}
}

std::optional<StoredStorageAssignmentObservation> ObserveAndStoreAssignedStorageReplica(
    PrivateApplicationStore& db, StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    const std::uint8_t slot, const std::span<const unsigned char> receipt, const std::uint32_t stored_size,
    const std::uint64_t observed_at_utc, const std::uint64_t verified_through_utc,
    const std::span<const unsigned char> expected_bytes, const bool force_full)
{
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
}
