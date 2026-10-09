// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_evidence.h>
#include <cybou/binary_codec.h>
#include <cybou/hash256.h>
#include <algorithm>
#include <limits>
#include <set>

namespace cybou {
namespace {
bool ScopeValid(PrivateApplicationStore& db, const StorageAssignmentEvidenceScope& scope)
{
    const auto& c = scope.assignment.context;
    const auto periods = c.term_end - c.term_start;
    if (c.term_end <= c.term_start || !scope.term_start_utc || !scope.period_seconds ||
        scope.replica_slot >= scope.assignment.selected.size() ||
        periods > (std::numeric_limits<std::uint64_t>::max() - scope.term_start_utc) / scope.period_seconds) return false;
    return db.IsUnlocked() && LoadStorageAssignment(db, c) == scope.assignment;
}
std::string Key(const StorageAssignmentEvidenceScope& scope)
{
    return "storage/assignment-evidence/" +
        Hash256{std::span<const unsigned char, 32>{scope.assignment.commitment}}.GetHex() +
        '/' + std::to_string(scope.replica_slot);
}
bool IntervalValid(const StorageAssignmentEvidenceScope& scope, const StorageAssignmentInterval& interval)
{
    const auto& c = scope.assignment.context;
    if (interval.period < c.term_start || interval.period >= c.term_end ||
        interval.end_utc <= interval.start_utc ||
        std::all_of(interval.proof_commitment.begin(), interval.proof_commitment.end(), [](auto b) { return b == 0; })) return false;
    const auto start = scope.term_start_utc + (interval.period - c.term_start) * scope.period_seconds;
    return interval.start_utc >= start && interval.end_utc <= start + scope.period_seconds;
}
std::optional<std::vector<StorageAssignmentInterval>> Read(PrivateApplicationStore& db,
    const StorageAssignmentEvidenceScope& scope)
{
    const auto key = Key(scope);
    const auto bytes = db.Get(key);
    if (!bytes) return db.Has(key) || !db.IsUnlocked() ? std::nullopt :
        std::optional<std::vector<StorageAssignmentInterval>>{std::vector<StorageAssignmentInterval>{}};
    try {
        BinaryReader in{*bytes};
        if (in.Fixed<StorageAssignmentId>() != scope.assignment.commitment ||
            in.U8() != scope.replica_slot || in.U64() != scope.term_start_utc ||
            in.U64() != scope.period_seconds) return std::nullopt;
        const auto count = in.U32();
        if (count > MAX_STORAGE_ASSIGNMENT_INTERVALS) return std::nullopt;
        std::vector<StorageAssignmentInterval> result;
        std::set<StorageAssignmentId> proofs;
        for (std::uint32_t i{0}; i < count; ++i) {
            StorageAssignmentInterval interval{in.U64(), in.U64(), in.U64(), in.Fixed<StorageAssignmentId>()};
            if (!IntervalValid(scope, interval) || !proofs.insert(interval.proof_commitment).second ||
                (!result.empty() && result.back().end_utc > interval.start_utc)) return std::nullopt;
            result.push_back(interval);
        }
        in.Finish();
        return result;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}
std::vector<unsigned char> Encode(const StorageAssignmentEvidenceScope& scope,
    const std::vector<StorageAssignmentInterval>& intervals)
{
    BinaryWriter out;
    out.Fixed(scope.assignment.commitment); out.U8(scope.replica_slot);
    out.U64(scope.term_start_utc); out.U64(scope.period_seconds);
    out.U32(static_cast<std::uint32_t>(intervals.size()));
    for (const auto& interval : intervals) {
        out.U64(interval.period); out.U64(interval.start_utc); out.U64(interval.end_utc);
        out.Fixed(interval.proof_commitment);
    }
    return out.Take();
}
struct Claim {
    StorageAssignmentId assignment{};
    StorageAssignmentInterval interval;
};
std::string FundedKey(const StorageAssignmentEvidenceScope& scope)
{
    const auto hex = [](const auto& id) { return Hash256{std::span<const unsigned char, 32>{id}}.GetHex(); };
    const auto& c = scope.assignment.context;
    // Epoch, seed and provider deliberately do not identify a separately funded slot.
    return "storage/funded-slot-evidence/" + hex(c.network_binding) + '/' + hex(c.publication) +
        '/' + hex(c.chunk) + '/' + hex(c.payer) + '/' + std::to_string(c.term_start) +
        '/' + std::to_string(c.term_end) + '/' + std::to_string(scope.replica_slot);
}
constexpr std::size_t CLAIM_RECORD_LIMIT{20 + 88 * MAX_STORAGE_ASSIGNMENT_INTERVALS};
std::optional<std::vector<Claim>> Claims(PrivateApplicationStore& db, const StorageAssignmentEvidenceScope& scope)
{
    const auto key = FundedKey(scope);
    const auto bytes = db.Get(key);
    const auto initialized = db.Get(key + "/initialized");
    if (!bytes) return db.Has(key) || db.Has(key + "/initialized") || !db.IsUnlocked() ?
        std::nullopt : std::optional{std::vector<Claim>{}};
    if (initialized != std::vector<unsigned char>{1}) return std::nullopt;
    try {
        BinaryReader in{*bytes, CLAIM_RECORD_LIMIT};
        if (in.U64() != scope.term_start_utc || in.U64() != scope.period_seconds) return std::nullopt;
        const auto count = in.U32(); if (count > MAX_STORAGE_ASSIGNMENT_INTERVALS) return std::nullopt;
        std::vector<Claim> result;
        std::set<StorageAssignmentId> proofs;
        for (std::uint32_t i{0}; i < count; ++i) {
            Claim claim{in.Fixed<StorageAssignmentId>(),
                {in.U64(), in.U64(), in.U64(), in.Fixed<StorageAssignmentId>()}};
            if (std::all_of(claim.assignment.begin(), claim.assignment.end(), [](auto b) { return b == 0; }) ||
                !IntervalValid(scope, claim.interval) || !proofs.insert(claim.interval.proof_commitment).second ||
                (!result.empty() && result.back().interval.end_utc > claim.interval.start_utc)) return std::nullopt;
            result.push_back(claim);
        }
        in.Finish(); return result;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}
std::vector<unsigned char> EncodeClaims(const StorageAssignmentEvidenceScope& scope, const std::vector<Claim>& claims)
{
    BinaryWriter out{CLAIM_RECORD_LIMIT};
    out.U64(scope.term_start_utc); out.U64(scope.period_seconds); out.U32(static_cast<std::uint32_t>(claims.size()));
    for (const auto& claim : claims) {
        out.Fixed(claim.assignment); out.U64(claim.interval.period); out.U64(claim.interval.start_utc);
        out.U64(claim.interval.end_utc); out.Fixed(claim.interval.proof_commitment);
    }
    return out.Take();
}
bool Consistent(const StorageAssignmentEvidenceScope& scope,
    const std::vector<StorageAssignmentInterval>& intervals, const std::vector<Claim>& claims)
{
    std::vector<StorageAssignmentInterval> owned;
    for (const auto& claim : claims) {
        if (claim.assignment == scope.assignment.commitment) owned.push_back(claim.interval);
    }
    return owned == intervals;
}
}

StorageEvidenceAppendResult AppendStorageAssignmentEvidence(PrivateApplicationStore& db,
    const StorageAssignmentEvidenceScope& scope, const StorageAssignmentInterval& interval,
    const std::uint64_t verified_through_utc)
{
    PrivateApplicationStore::Batch batch{db};
    if (!ScopeValid(db, scope) || !IntervalValid(scope, interval) || interval.end_utc > verified_through_utc) {
        return StorageEvidenceAppendResult::REJECTED;
    }
    auto intervals = Read(db, scope);
    auto claims = Claims(db, scope);
    if (!intervals || !claims || !Consistent(scope, *intervals, *claims)) return StorageEvidenceAppendResult::REJECTED;
    for (const auto& prior : *intervals) {
        if (prior == interval) return StorageEvidenceAppendResult::DUPLICATE;
        if (prior.proof_commitment == interval.proof_commitment ||
            (prior.start_utc < interval.end_utc && interval.start_utc < prior.end_utc)) {
            return StorageEvidenceAppendResult::REJECTED;
        }
    }
    if (intervals->size() >= MAX_STORAGE_ASSIGNMENT_INTERVALS) return StorageEvidenceAppendResult::REJECTED;
    if (claims->size() >= MAX_STORAGE_ASSIGNMENT_INTERVALS) return StorageEvidenceAppendResult::REJECTED;
    for (const auto& claim : *claims) {
        const auto& prior = claim.interval;
        if (prior.proof_commitment == interval.proof_commitment ||
            (prior.start_utc < interval.end_utc && interval.start_utc < prior.end_utc)) return StorageEvidenceAppendResult::REJECTED;
    }
    claims->insert(std::lower_bound(claims->begin(), claims->end(), interval.start_utc,
        [](const auto& prior, auto start) { return prior.interval.start_utc < start; }),
        Claim{scope.assignment.commitment, interval});
    intervals->insert(std::lower_bound(intervals->begin(), intervals->end(), interval.start_utc,
        [](const auto& prior, auto start) { return prior.start_utc < start; }), interval);
    return db.Put(Key(scope), Encode(scope, *intervals)) &&
        db.Put(FundedKey(scope), EncodeClaims(scope, *claims)) &&
        db.Put(FundedKey(scope) + "/initialized", std::vector<unsigned char>{1}) && batch.Commit() ?
        StorageEvidenceAppendResult::ADDED : StorageEvidenceAppendResult::REJECTED;
}

std::optional<std::uint64_t> StorageAssignmentVerifiedSeconds(PrivateApplicationStore& db,
    const StorageAssignmentEvidenceScope& scope, const std::uint64_t through_period)
{
    PrivateApplicationStore::Batch snapshot{db};
    if (!ScopeValid(db, scope) || through_period < scope.assignment.context.term_start ||
        through_period >= scope.assignment.context.term_end) return std::nullopt;
    const auto intervals = Read(db, scope);
    const auto claims = Claims(db, scope);
    if (!intervals || !claims || !Consistent(scope, *intervals, *claims)) return std::nullopt;
    std::uint64_t seconds{0};
    for (const auto& interval : *intervals) {
        if (interval.period <= through_period) seconds += interval.end_utc - interval.start_utc;
    }
    // Non-overlap plus validated term bounds limits sum to the representable term duration.
    return seconds;
}
}
