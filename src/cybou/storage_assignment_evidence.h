// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_EVIDENCE_H
#define CYBOU_STORAGE_ASSIGNMENT_EVIDENCE_H
#include <cybou/storage_assignment.h>

namespace cybou {
inline constexpr std::size_t MAX_STORAGE_ASSIGNMENT_INTERVALS{4096};

struct StorageAssignmentInterval {
    std::uint64_t period{0}, start_utc{0}, end_utc{0};
    StorageAssignmentId proof_commitment{};
    friend bool operator==(const StorageAssignmentInterval&, const StorageAssignmentInterval&) = default;
};
struct StorageAssignmentEvidenceScope {
    StorageAssignmentPlan assignment;
    std::uint8_t replica_slot{0};
    std::uint64_t term_start_utc{0}, period_seconds{0};
};
enum class StorageEvidenceAppendResult { ADDED, DUPLICATE, REJECTED };

/// Internal accounting of previously verified evidence, not a raw-proof verifier.
/// Caller must verify assignment attestation, binding, proof and UTC policy first.
/// No clock, elapsed-time inference, payout or new database is introduced.
/// A shared funded-slot journal excludes overlapping service across replacement
/// assignment epochs. Local and shared claims are committed together and queried
/// only when they agree; lost/corrupt shared claims never become zero service.
StorageEvidenceAppendResult AppendStorageAssignmentEvidence(PrivateApplicationStore& db,
    const StorageAssignmentEvidenceScope& scope, const StorageAssignmentInterval& interval,
    std::uint64_t verified_through_utc);

/// Cumulative unit-seconds for ONE assigned authorized chunk/replica through the
/// requested completed period. Empty readable journal returns zero; missing or
/// mismatched assignment, locked/corrupt journal returns nullopt. Later periods
/// never credit earlier settlement. Caller aggregates distinct authorized chunks.
std::optional<std::uint64_t> StorageAssignmentVerifiedSeconds(PrivateApplicationStore& db,
    const StorageAssignmentEvidenceScope& scope, std::uint64_t through_period);
}
#endif
