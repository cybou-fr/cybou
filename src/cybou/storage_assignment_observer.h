// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_OBSERVER_H
#define CYBOU_STORAGE_ASSIGNMENT_OBSERVER_H
#include <cybou/storage_assignment_attestation.h>
#include <cybou/storage_service.h>

namespace cybou {
enum class StorageAssignmentObservationKind { OFFSET_AUDIT, FULL_GET };
struct StorageAssignmentObservation {
    StorageAssignmentObservationKind kind;
    StorageAssignmentId assignment_commitment{}, storage_id{};
    std::uint8_t replica_slot{0};
    std::vector<unsigned char> receipt;
    std::optional<StorageAuditChallenge> challenge;
    std::optional<StorageAuditAnswer> answer;
    std::vector<unsigned char> retrieved_bytes;
};

/// Queries only the genesis-attested slot with a valid signed admission receipt.
/// Transport must authenticate the requested StorageId, as RuntimeStorageTransport
/// does. Audit responses/GET bytes are local observations, not provider-signed
/// duration evidence. This call neither credits intervals nor issues payments.
/// Caller schedules periodic force_full checks and retains the returned evidence;
/// this synchronous transport call must run outside the GUI thread.
/// Missing audit support/response or RNG failure falls back to exact full GET;
/// an explicit negative/incorrect audit fails without hiding it behind a GET.
std::optional<StorageAssignmentObservation> ObserveAssignedStorageReplica(
    StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    std::uint8_t slot, std::span<const unsigned char> receipt, std::uint32_t stored_size,
    std::span<const unsigned char> expected_bytes = {}, bool force_full = false);
struct CanonicalStorageAssignment {
    StorageAssignmentPlan plan;
    std::uint64_t term_start_utc{0}, effective_start_utc{0}, end_utc{0}, period_seconds{0};
    friend bool operator==(const CanonicalStorageAssignment&, const CanonicalStorageAssignment&) = default;
};
/// Read-only material from this Full Node's finalized chain/state, not an authority
/// token supplied by callers. Consumers re-resolve it before committing evidence.
std::optional<CanonicalStorageAssignment> ResolveCanonicalStorageAssignment(
    CybouNodeRuntime& runtime, const Hash256& funding, const Hash256& activation, const ChunkId& chunk);

/// Historical read-only scope ends at replacement/closure. Never authorizes new
/// observations; raw/durable collection always uses the active-only resolver.
std::optional<CanonicalStorageAssignment> ResolveCanonicalStorageHistory(
    CybouNodeRuntime& runtime, const Hash256& funding, const Hash256& activation, const ChunkId& chunk);

/// Resolves the exact finalized ACTIVATE manifest and accepted PREPARE bindings
/// from this Full Node, never a caller-created historical registry or signature.
/// Only the active term/latest epoch can receive new observations. This raw I/O
/// boundary does not yet persist attempts, schedule checks or credit intervals.
std::optional<StorageAssignmentObservation> ObserveCanonicalStorageReplica(
    CybouNodeRuntime& runtime, StorageTransport& transport, const StorageEndpoint& provider,
    const Hash256& funding_operation_id, const Hash256& activation_operation_id,
    const ChunkId& chunk, std::uint8_t slot, std::span<const unsigned char> receipt,
    std::uint32_t stored_size, std::span<const unsigned char> expected_bytes = {},
    bool force_full = true);
}
#endif
