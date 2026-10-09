// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_payout.h>
#include <algorithm>
#include <map>
#include <set>
#include <limits>

namespace cybou {
std::optional<StorageAssignmentSlotQuote> PrepareStorageAssignmentSlotPayouts(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const std::span<const StorageAssignmentEvidenceScope> assignments,
    const std::span<const ChunkId> authorized_chunks, const std::uint64_t rate,
    const std::uint64_t through_period, const std::span<const StorageAssignmentPaid> paid)
{
    if (assignments.empty() || authorized_chunks.empty()) return std::nullopt;
    PrivateApplicationStore::Batch snapshot{db};
    const auto& base = assignments.front();
    const auto& c = base.assignment.context;
    if (through_period < c.term_start || through_period >= c.term_end) return std::nullopt;
    const auto budget = ComputeAssignedStorageBudget(authorized_chunks.size(), c.replicas,
        c.term_end - c.term_start, base.period_seconds, rate);
    if (!budget) return std::nullopt;
    std::set<ChunkId> chunks;
    for (const auto& chunk : authorized_chunks) {
        if (std::all_of(chunk.begin(), chunk.end(), [](auto b) { return b == 0; }) ||
            !chunks.insert(chunk).second) return std::nullopt;
    }
    std::map<StorageAssignmentId, const StorageAssignmentEvidenceScope*> plans;
    std::map<ChunkId, const StorageAssignmentEvidenceScope*> representatives;
    for (const auto& scope : assignments) {
        const auto& s = scope.assignment.context;
        if (s.network_binding != c.network_binding || s.publication != c.publication || s.payer != c.payer ||
            s.term_start != c.term_start || s.term_end != c.term_end || s.replicas != c.replicas ||
            scope.replica_slot != base.replica_slot || scope.term_start_utc != base.term_start_utc ||
            scope.period_seconds != base.period_seconds || !chunks.contains(s.chunk) ||
            !plans.emplace(scope.assignment.commitment, &scope).second) return std::nullopt;
        const auto attested = LoadStorageAssignmentAttestation(db, genesis, s);
        if (!attested || attested->plan != scope.assignment ||
            !StorageAssignmentVerifiedSeconds(db, scope, through_period)) return std::nullopt;
        representatives.emplace(s.chunk, &scope);
    }
    if (representatives.size() != chunks.size()) return std::nullopt;
    std::map<StorageAssignmentProvider, std::uint64_t> service;
    std::uint64_t total_seconds{0};
    for (const auto& [chunk, scope] : representatives) {
        const auto claims = LoadStorageFundedSlotClaims(db, *scope);
        if (!claims) return std::nullopt;
        for (const auto& claim : *claims) {
            const auto plan = plans.find(claim.assignment);
            if (plan == plans.end() || plan->second->assignment.context.chunk != chunk) return std::nullopt;
            const auto& provider = plan->second->assignment.selected[base.replica_slot];
            auto& seconds = service[provider];
            if (claim.interval.period > through_period) continue;
            const auto amount = claim.interval.end_utc - claim.interval.start_utc;
            if (seconds > std::numeric_limits<std::uint64_t>::max() - amount ||
                total_seconds > budget->contracted_unit_seconds ||
                amount > budget->contracted_unit_seconds - total_seconds) return std::nullopt;
            seconds += amount; total_seconds += amount;
        }
    }
    if (service.size() > 1024 || paid.size() > 1024) return std::nullopt;
    std::map<StorageAssignmentProvider, std::uint64_t> finalized;
    for (const auto& payment : paid) {
        if (!service.contains(payment.provider) || !finalized.emplace(payment.provider, payment.finalized_paid).second) return std::nullopt;
    }
    StorageAssignmentSlotQuote result{.budget = *budget, .verified_unit_seconds = total_seconds};
    for (const auto& [provider, seconds] : service) {
        const auto already_paid = finalized.contains(provider) ? finalized.at(provider) : 0;
        const auto due = ComputeAssignedStoragePayout(*budget, seconds, already_paid);
        if (!due || already_paid + *due > budget->per_replica - result.finalized_paid - result.payout) return std::nullopt;
        result.entries.push_back({provider, seconds, already_paid + *due, already_paid, *due});
        result.finalized_paid += already_paid; result.payout += *due;
    }
    return result;
}
}
