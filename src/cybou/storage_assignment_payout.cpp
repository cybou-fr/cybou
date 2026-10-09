// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_payout.h>
#include <algorithm>
#include <map>
#include <set>
#include <limits>

namespace cybou {
namespace {
std::optional<StorageAssignmentSlotQuote> QuoteSlot(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const std::span<const StorageAssignmentEvidenceScope> assignments,
    const std::span<const ChunkId> authorized_chunks, const std::uint64_t rate,
    const std::uint64_t through_period, const std::span<const StorageAssignmentPaid> paid,
    const std::span<const StorageAssignmentRegistrySnapshot> registries)
{
    if (assignments.empty() || authorized_chunks.empty()) return std::nullopt;
    std::map<Hash256, const IdentityRegistry*> historical;
    for (const auto& registry : registries) {
        if (!registry.registry || !historical.emplace(registry.block_id, registry.registry).second) return std::nullopt;
    }
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
        const auto seed = Hash256{std::span<const unsigned char, 32>{s.finalized_seed}};
        const auto registry = historical.find(seed);
        if (!attested || attested->plan != scope.assignment ||
            registry == historical.end() ||
            !LoadStorageAssignmentBindings(db, genesis, s, *registry->second, seed) ||
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
std::optional<StorageAssignmentSlotQuote> PrepareStorageAssignmentSlotPayouts(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    std::span<const StorageAssignmentEvidenceScope> assignments,
    std::span<const ChunkId> chunks, std::uint64_t rate, std::uint64_t period,
    std::span<const StorageAssignmentPaid> paid,
    std::span<const StorageAssignmentRegistrySnapshot> registries)
{
    PrivateApplicationStore::Batch snapshot{db};
    if (!snapshot.IsOutermost()) return std::nullopt;
    return QuoteSlot(db, genesis, assignments, chunks, rate, period, paid, registries);
}
std::optional<StorageAssignmentTermQuote> PrepareStorageAssignmentTermPayouts(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    std::span<const StorageAssignmentEvidenceScope> assignments,
    std::span<const ChunkId> chunks, std::uint64_t rate, std::uint64_t period,
    std::span<const StorageAssignmentSlotPaid> paid,
    std::span<const StorageAssignmentRegistrySnapshot> registries)
{
    if (assignments.empty() || paid.size() > 1024) return std::nullopt;
    PrivateApplicationStore::Batch snapshot{db};
    if (!snapshot.IsOutermost()) return std::nullopt;
    const auto& base = assignments.front();
    const auto& c = base.assignment.context;
    if (!c.replicas || c.replicas > 2) return std::nullopt;
    std::vector<std::vector<StorageAssignmentEvidenceScope>> scopes(c.replicas);
    std::vector<std::vector<StorageAssignmentPaid>> payments(c.replicas);
    for (const auto& scope : assignments) {
        const auto& s = scope.assignment.context;
        if (scope.replica_slot >= c.replicas || s.network_binding != c.network_binding ||
            s.publication != c.publication || s.payer != c.payer || s.term_start != c.term_start ||
            s.term_end != c.term_end || s.replicas != c.replicas ||
            scope.term_start_utc != base.term_start_utc || scope.period_seconds != base.period_seconds) return std::nullopt;
        scopes[scope.replica_slot].push_back(scope);
    }
    for (const auto& payment : paid) {
        if (payment.replica_slot >= c.replicas) return std::nullopt;
        payments[payment.replica_slot].push_back(payment.payment);
    }
    StorageAssignmentTermQuote result;
    std::size_t entries{0};
    for (std::uint8_t slot{0}; slot < c.replicas; ++slot) {
        auto quote = QuoteSlot(db, genesis, scopes[slot], chunks, rate, period, payments[slot], registries);
        if (!quote || quote->entries.size() > 1024 - entries) return std::nullopt;
        entries += quote->entries.size();
        if (slot == 0) result.budget = quote->budget;
        if (quote->budget.per_replica != result.budget.per_replica || quote->budget.total != result.budget.total ||
            quote->budget.contracted_unit_seconds != result.budget.contracted_unit_seconds ||
            quote->finalized_paid > result.budget.total - result.finalized_paid - result.payout ||
            quote->payout > result.budget.total - result.finalized_paid - result.payout - quote->finalized_paid) return std::nullopt;
        result.finalized_paid += quote->finalized_paid; result.payout += quote->payout;
        result.slots.push_back(std::move(*quote));
    }
    if (c.replicas == 2) {
        std::map<StorageAssignmentId, const StorageAssignmentPlan*> plans;
        std::map<ChunkId, std::array<const StorageAssignmentEvidenceScope*, 2>> representatives;
        for (const auto& scope : assignments) {
            plans.emplace(scope.assignment.commitment, &scope.assignment);
            representatives[scope.assignment.context.chunk][scope.replica_slot] = &scope;
        }
        for (const auto& [chunk, slots] : representatives) {
            if (!slots[0] || !slots[1]) return std::nullopt;
            const auto left = LoadStorageFundedSlotClaims(db, *slots[0]);
            const auto right = LoadStorageFundedSlotClaims(db, *slots[1]);
            if (!left || !right) return std::nullopt;
            std::size_t a{0}, b{0};
            while (a < left->size() && b < right->size()) {
                const auto& x = (*left)[a]; const auto& y = (*right)[b];
                if (x.interval.start_utc < y.interval.end_utc && y.interval.start_utc < x.interval.end_utc) {
                    const auto xp = plans.find(x.assignment), yp = plans.find(y.assignment);
                    if (xp == plans.end() || yp == plans.end()) return std::nullopt;
                    const auto& first = xp->second->selected[0]; const auto& second = yp->second->selected[1];
                    if (first.storage_id == second.storage_id || first.payout_account == second.payout_account) return std::nullopt;
                }
                if (x.interval.end_utc <= y.interval.end_utc) ++a;
                else ++b;
            }
        }
    }
    return result;
}
}
