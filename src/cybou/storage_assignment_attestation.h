// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_STORAGE_ASSIGNMENT_ATTESTATION_H
#define CYBOU_STORAGE_ASSIGNMENT_ATTESTATION_H
#include <cybou/storage_assignment.h>
#include <cybou/storage_audit.h>
#include <cybou/network_genesis.h>
#include <cybou/identity_registry.h>
#include <cybou/poa_signer.h>

namespace cybou {
struct StorageAssignmentBindingProof {
    StorageAssignmentId storage_id{};
    StoragePayoutBinding binding;
};
struct AttestedStorageAssignment {
    StorageAssignmentPlan plan;
    IdentityHybridSignature signature;
    friend bool operator==(const AttestedStorageAssignment&, const AttestedStorageAssignment&) = default;
};

/// Exact portable attestation bytes: context (185), eligible count (4),
/// canonical eligible pairs (64 each), Ed25519 (64), ML-DSA-65 (3309).
/// Selected providers and commitment are deterministically reconstructed.
/// Decoding is structural only: callers MUST verify signature, bindings and
/// independently finalized seed provenance before accepting any obligation.
std::optional<std::vector<unsigned char>> EncodeStorageAssignmentAttestation(
    const AttestedStorageAssignment& assignment);
std::optional<AttestedStorageAssignment> DecodeStorageAssignmentAttestation(
    std::span<const unsigned char> bytes);

/// Caller supplies the independently finalized registry at snapshot_id. Binding
/// verification is cryptographic; availability, budget, seed provenance, active
/// publication/lease and host independence remain caller validation requirements.
bool VerifyStorageAssignmentBindings(const StorageAssignmentPlan& plan,
    const IdentityRegistry& registry, const Hash256& snapshot_id,
    std::span<const StorageAssignmentBindingProof> proofs);

/// Freezes the plan durably BEFORE signing. Returns only a durably saved,
/// self-verified signature. Uses the existing signer; no key export/new signer.
/// Rejects calls inside an enclosing store batch, which cannot commit durably here.
std::optional<AttestedStorageAssignment> AttestStorageAssignment(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentPlan& plan,
    const IdentityRegistry& registry, const Hash256& snapshot_id,
    std::span<const StorageAssignmentBindingProof> proofs, const PoaSigner& signer);
bool VerifyStorageAssignmentAttestation(const VerifiedNetworkGenesis& genesis,
    const AttestedStorageAssignment& assignment);
std::optional<AttestedStorageAssignment> LoadStorageAssignmentAttestation(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentContext& context);
/// Restores the retained signed bindings and checks them against the historical
/// finalized registry at this assignment's seed, not today's rotated keys.
std::optional<std::vector<StorageAssignmentBindingProof>> LoadStorageAssignmentBindings(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentContext& context,
    const IdentityRegistry& registry, const Hash256& snapshot_id);

/// Signed admission for the exact assigned provider/publication/chunk/size.
/// Does not infer continuous service or append an economic interval.
bool VerifyAssignedStorageReceipt(const VerifiedNetworkGenesis& genesis,
    const AttestedStorageAssignment& assignment, std::uint8_t slot,
    std::span<const unsigned char> receipt, std::uint32_t stored_size);
}
#endif
