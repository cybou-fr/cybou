// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_PAYOUT_H
#define CYBOU_STORAGE_ASSIGNMENT_PAYOUT_H
#include <cybou/storage_assignment_attestation.h>
#include <cybou/storage_assignment_evidence.h>
#include <cybou/storage_economy.h>

namespace cybou {
struct StorageAssignmentPaid {
    StorageAssignmentProvider provider;
    std::uint64_t finalized_paid{0};
};
struct StorageAssignmentPayout {
    StorageAssignmentProvider provider;
    std::uint64_t verified_unit_seconds{0}, entitlement{0}, finalized_paid{0}, payout{0};
};
struct StorageAssignmentSlotQuote {
    AssignedStorageBudget budget;
    std::uint64_t verified_unit_seconds{0}, finalized_paid{0}, payout{0};
    std::vector<StorageAssignmentPayout> entries;
};

/// Read-only target quote for ONE funded replica slot across all chunks/epochs.
/// Caller supplies the finalized authorized chunk manifest, immutable funded
/// rate/term/UTC and canonical paid history. These inputs are not proven here.
/// Every shared claim must resolve to a supplied frozen, genesis-attested plan
/// and a consistent local mirror. Missing epochs/chunks/corruption fail closed.
/// Provider-specific cumulative floors share ONE slot budget; replacement does
/// not replenish it. Preparation never changes paid or emits a wire operation.
std::optional<StorageAssignmentSlotQuote> PrepareStorageAssignmentSlotPayouts(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    std::span<const StorageAssignmentEvidenceScope> assignments,
    std::span<const ChunkId> authorized_chunks, std::uint64_t rate,
    std::uint64_t through_period, std::span<const StorageAssignmentPaid> paid);
}
#endif
