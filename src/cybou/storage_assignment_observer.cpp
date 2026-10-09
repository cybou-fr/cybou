// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_observer.h>
#include <cybou/encrypted_chunk.h>
#include <openssl/rand.h>

namespace cybou {
std::optional<StorageAssignmentObservation> ObserveAssignedStorageReplica(
    StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    const std::uint8_t slot, const std::span<const unsigned char> receipt,
    const std::uint32_t stored_size, const std::span<const unsigned char> expected_bytes,
    const bool force_full)
{
    if (!stored_size || stored_size > ENCRYPTED_CHUNK_MAX_STORED_BYTES ||
        slot >= assignment.plan.selected.size() ||
        provider.storage_id != assignment.plan.selected[slot].storage_id ||
        !VerifyAssignedStorageReceipt(genesis, assignment, slot, receipt, stored_size)) return std::nullopt;
    const auto& chunk = assignment.plan.context.chunk;
    if (!expected_bytes.empty() &&
        (expected_bytes.size() != stored_size || ComputeChunkId(expected_bytes) != chunk)) return std::nullopt;
    StorageAssignmentObservation result{
        .kind = StorageAssignmentObservationKind::FULL_GET,
        .assignment_commitment = assignment.plan.commitment,
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
}
