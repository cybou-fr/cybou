// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_OBSERVATION_STORE_H
#define CYBOU_STORAGE_ASSIGNMENT_OBSERVATION_STORE_H
#include <cybou/storage_assignment_observer.h>
#include <cybou/storage_assignment_evidence.h>

namespace cybou {
inline constexpr std::size_t MAX_STORAGE_ASSIGNMENT_OBSERVATIONS{4096};
inline constexpr std::uint64_t STORAGE_ASSIGNMENT_CHECK_INTERVAL_SECONDS{12 * 60 * 60};
inline constexpr std::uint64_t STORAGE_ASSIGNMENT_MAX_SERVICE_GAP_SECONDS{24 * 60 * 60};
inline constexpr std::uint64_t STORAGE_ASSIGNMENT_FULL_GET_EVERY_SUCCESSES{8};
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
    std::span<const unsigned char> expected_bytes = {}, bool force_full = false,
    const StorageAssignmentEvidenceScope* service_scope = nullptr);

/// With service_scope: enforce first/eighth-success GET, persist an attempt
/// BEFORE I/O (crash/failure breaks continuity), then atomically retain the
/// successful observation, completed service intervals and success counter.
/// The supplied funded scope/seed/bindings still require independent provenance
/// validation. This off-chain path never modifies canonical paid or balances.

/// Reload checks assignment, receipt and audit hash against exact local bytes.
/// GET bodies remain in the content store, not this metadata journal. A stored
/// GET result is the trusted collector's assertion, not a remotely signed proof.
/// Reload never generates service intervals or issues payments.
std::optional<StoredStorageAssignmentObservation> LoadAssignedStorageObservation(
    PrivateApplicationStore& db, const VerifiedNetworkGenesis& genesis,
    const AttestedStorageAssignment& assignment, std::uint8_t slot,
    std::uint64_t observed_at_utc, std::span<const unsigned char> reference_bytes);
/// Durable canonical collector: derives scope/time from finalized state, journals
/// intent before I/O and re-resolves the active epoch before atomic completion.
/// Uses the same first/eighth-success GET and two-success/max-24-hour gap policy.
/// Caller supplies observation UTC; this does not schedule checks or issue PAY.
std::optional<StoredStorageAssignmentObservation> ObserveAndStoreCanonicalStorageReplica(
    PrivateApplicationStore& db, CybouNodeRuntime& runtime, StorageTransport& transport,
    const StorageEndpoint& provider, const Hash256& funding, const Hash256& activation,
    const ChunkId& chunk, std::uint8_t slot, std::span<const unsigned char> receipt,
    std::uint32_t stored_size, std::uint64_t observed_at_utc, std::uint64_t verified_through_utc,
    std::span<const unsigned char> expected_bytes = {}, bool force_full = false);
struct VerifiedCanonicalStorageService {
    Hash256 activation;
    StorageAssignmentProvider provider;
    std::uint64_t verified_unit_seconds{0};
    std::vector<Hash256> evidence_references;
};
/// Read-only completed-period service for one authorized chunk/slot across ALL
/// canonical historical epochs. Every shared claim must resolve and verify;
/// missing plans/proofs fail, never become zero. Caller aggregates chunks and
/// applies canonical paid/budget arithmetic before submitting PAY.
/// Rejects enclosing transactions and a canonical state change during the read.
std::optional<std::vector<VerifiedCanonicalStorageService>> LoadCanonicalStorageService(
    PrivateApplicationStore& db, CybouNodeRuntime& runtime, const Hash256& funding,
    const ChunkId& chunk, std::uint8_t slot, std::uint64_t through_period,
    std::span<const unsigned char> reference_bytes);

/// Reconstructs the exact retained observation pair and period-slice fingerprint,
/// checks both records/receipts/audit hashes and canonical historical time bounds.
/// GET remains the trusted collector assertion over the supplied exact chunk bytes.
/// Does not infer service from time, accept missing proof material or issue PAY.
bool VerifyCanonicalStorageInterval(PrivateApplicationStore& db, CybouNodeRuntime& runtime,
    const Hash256& funding, const Hash256& activation, const ChunkId& chunk, std::uint8_t slot,
    const StorageAssignmentInterval& interval, std::span<const unsigned char> reference_bytes);
}
#endif
