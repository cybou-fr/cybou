// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_OBSERVATION_STORE_H
#define CYBOU_STORAGE_ASSIGNMENT_OBSERVATION_STORE_H
#include <cybou/storage_assignment_observer.h>

namespace cybou {
inline constexpr std::size_t MAX_STORAGE_ASSIGNMENT_OBSERVATIONS{4096};
struct StoredStorageAssignmentObservation {
    std::uint64_t observed_at_utc{0};
    std::uint32_t stored_size{0};
    StorageAssignmentObservation observation;
};

/// Trusted local collector boundary: verifies over authenticated transport and
/// durably retains a successful observation in existing encrypted app.db.
/// Caller supplies UTC and independently validates funded lease/seed provenance.
/// One immutable record per assignment/slot/UTC second; conflicts/full/corrupt
/// journals fail without eviction. Nested store transactions are rejected.
std::optional<StoredStorageAssignmentObservation> ObserveAndStoreAssignedStorageReplica(
    PrivateApplicationStore& db, StorageTransport& transport, const StorageEndpoint& provider,
    const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    std::uint8_t slot, std::span<const unsigned char> receipt, std::uint32_t stored_size,
    std::uint64_t observed_at_utc, std::uint64_t verified_through_utc,
    std::span<const unsigned char> expected_bytes = {}, bool force_full = false);

/// Reload checks assignment, receipt and audit hash against exact local bytes.
/// GET bodies remain in the content store, not this metadata journal. A stored
/// GET result is the trusted collector's assertion, not a remotely signed proof.
/// No elapsed-time credit, interval generation or payment occurs on either path.
std::optional<StoredStorageAssignmentObservation> LoadAssignedStorageObservation(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const AttestedStorageAssignment& assignment, std::uint8_t slot,
    std::uint64_t observed_at_utc, std::span<const unsigned char> reference_bytes);
}
#endif
