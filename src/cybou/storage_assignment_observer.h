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
}
#endif
