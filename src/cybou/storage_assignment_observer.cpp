// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_observer.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/node_runtime.h>
#include <algorithm>
#include <limits>
#include <openssl/rand.h>

namespace cybou {
namespace {
std::optional<StorageAssignmentObservation> ObservePlan(
    StorageTransport& transport, const StorageEndpoint& provider,
    const StorageAssignmentPlan& plan,
    const std::uint8_t slot, const std::span<const unsigned char> receipt,
    const std::uint32_t stored_size, const std::span<const unsigned char> expected_bytes,
    const bool force_full)
{
    if (!stored_size || stored_size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
        slot >= plan.selected.size() ||
        provider.storage_id != plan.selected[slot].storage_id) return std::nullopt;
    const auto storage = VerifyStorageReceipt(receipt,
        Hash256{std::span<const unsigned char,32>{plan.context.network_binding}},
        Hash256{std::span<const unsigned char,32>{plan.context.publication}}, plan.context.chunk, stored_size);
    if (!storage || *storage != plan.selected[slot].storage_id) return std::nullopt;
    const auto& chunk = plan.context.chunk;
    if (!expected_bytes.empty() &&
        (expected_bytes.size() != stored_size || ComputeChunkId(expected_bytes) != chunk)) return std::nullopt;
    StorageAssignmentObservation result{
        .kind = StorageAssignmentObservationKind::FULL_GET,
        .assignment_commitment = plan.commitment,
        .storage_id = provider.storage_id, .replica_slot = slot,
        .receipt = {receipt.begin(), receipt.end()}};
    std::uint64_t offset{0};
    StorageAuditChallenge challenge{.chunk_id = chunk};
    const bool full = force_full || expected_bytes.empty() ||
        RAND_bytes(reinterpret_cast<unsigned char*>(&offset), sizeof(offset)) != 1 ||
        RAND_bytes(challenge.nonce.data(), challenge.nonce.size()) != 1;
    if (!full) {
        challenge.byte_offset = offset % expected_bytes.size();
        if (const auto answer = transport.Audit(provider, challenge)) {
            const auto expected = ComputeStorageAuditResponse(expected_bytes, challenge.byte_offset, challenge.nonce);
            if (!answer->held || !expected || answer->response_hash != *expected) return std::nullopt;
            result.kind = StorageAssignmentObservationKind::OFFSET_AUDIT;
            result.challenge = challenge; result.answer = *answer;
            return result;
        }
    }
    auto bytes = transport.Get(provider, chunk);
    if (!bytes || bytes->size() != stored_size || ComputeChunkId(*bytes) != chunk) return std::nullopt;
    result.retrieved_bytes = std::move(*bytes);
    return result;
}
} // namespace

std::optional<StorageAssignmentObservation> ObserveAssignedStorageReplica(
    StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    uint8_t slot, std::span<const unsigned char> receipt, uint32_t size,
    std::span<const unsigned char> expected, bool force_full)
{
    if (!VerifyStorageAssignmentAttestation(genesis, assignment)) return std::nullopt;
    return ObservePlan(transport, provider, assignment.plan, slot, receipt, size, expected, force_full);
}

std::optional<CanonicalStorageAssignment> ResolveCanonicalStorageAssignment(
    CybouNodeRuntime& runtime, const Hash256& funding, const Hash256& activation, const ChunkId& chunk)
{
    const auto snapshot = runtime.GetStore().GetStateSnapshot();
    if (!snapshot) return std::nullopt;
    const auto& state = *snapshot.state;
    const auto status = runtime.GetOperationStatus(activation);
    if (status.kind != OperationStatusKind::FINALIZED) return std::nullopt;
    const auto block = runtime.GetBlockAtHeight(status.finalized_height);
    if (!block) return std::nullopt;
    const StorageSettlement* action = nullptr;
    for (const auto& op : block->block.operations) {
        if (ComputeOperationId(op) == activation) action = std::get_if<StorageSettlement>(&op);
    }
    if (!action || action->action != StorageSettlementAction::ACTIVATE ||
        std::find(action->manifest.begin(), action->manifest.end(), chunk) == action->manifest.end()) return std::nullopt;
    for (const auto& [publication, lease] : state.leases) {
        if (!state.publications.contains(publication) ||
            state.settlement.next_period >= lease.end_period) continue;
        for (const auto& term : lease.funded_terms) {
            if (term.funding_operation_id != funding || term.assignments.empty() ||
                state.settlement.next_period < term.first_period || state.settlement.next_period >= term.end_period) continue;
            const auto& accepted = term.assignments.back(); // Superseded epochs cannot receive new checks.
            if (accepted.operation_id != activation || accepted.preparation_id != action->preparation_id ||
                accepted.effective_period > state.settlement.next_period) return std::nullopt;
            const auto declaration = std::find_if(term.declarations.begin(), term.declarations.end(),
                [&](const auto& d) { return d.operation_id == accepted.preparation_id && d.epoch == accepted.epoch; });
            if (declaration == term.declarations.end()) return std::nullopt;
            StorageAssignmentContext context;
            std::copy_n(runtime.GetNetworkBinding().begin(), 32, context.network_binding.begin());
            std::copy_n(publication.begin(), 32, context.publication.begin());
            std::copy_n(accepted.seed.begin(), 32, context.finalized_seed.begin());
            std::copy_n(lease.payer.Value().begin(), 32, context.payer.begin());
            context.chunk = chunk; context.epoch = accepted.epoch;
            context.term_start = term.first_period; context.term_end = term.end_period; context.replicas = lease.replicas;
            std::vector<StorageAssignmentProvider> eligible;
            for (const auto& binding : declaration->eligible) {
                StorageAssignmentProvider candidate{.storage_id = binding.storage_id};
                std::copy_n(binding.payout_account.Value().begin(), 32, candidate.payout_account.begin());
                eligible.push_back(candidate);
            }
            const auto plan = PrepareStorageAssignment(context, std::move(eligible));
            if (!plan) return std::nullopt;
            const auto seconds = term.period_seconds;
            if (!seconds || state.settlement.next_period - term.first_period >
                std::numeric_limits<uint64_t>::max() / seconds) return std::nullopt;
            const auto elapsed = (state.settlement.next_period - term.first_period) * seconds;
            if (state.settlement.next_period_start_utc <= elapsed) return std::nullopt;
            const auto anchor = state.settlement.next_period_start_utc - elapsed;
            const auto closure = std::min(term.end_period, lease.end_period);
            if (closure - term.first_period > (std::numeric_limits<uint64_t>::max() - anchor) / seconds)
                return std::nullopt;
            return CanonicalStorageAssignment{*plan, anchor,
                anchor + (accepted.effective_period - term.first_period) * seconds,
                anchor + (closure - term.first_period) * seconds, seconds};
        }
    }
    return std::nullopt;
}

std::optional<StorageAssignmentObservation> ObserveCanonicalStorageReplica(
    CybouNodeRuntime& runtime, StorageTransport& transport, const StorageEndpoint& provider,
    const Hash256& funding, const Hash256& activation, const ChunkId& chunk, uint8_t slot,
    std::span<const unsigned char> receipt, uint32_t size,
    std::span<const unsigned char> expected, bool force_full)
{
    const auto resolved = ResolveCanonicalStorageAssignment(runtime, funding, activation, chunk);
    return resolved ? ObservePlan(transport, provider, resolved->plan, slot, receipt, size, expected, force_full) : std::nullopt;
}
}
