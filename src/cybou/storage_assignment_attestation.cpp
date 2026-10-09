// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment_attestation.h>
#include <cybou/binary_codec.h>
#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>
#include <algorithm>

namespace cybou {
namespace {
Hash256 Hash(const StorageAssignmentId& id) { return Hash256{std::span<const unsigned char, 32>{id}}; }
std::optional<StorageAssignmentId> Digest(const VerifiedNetworkGenesis& genesis, const StorageAssignmentPlan& plan)
{
    if (ComputeNetworkBinding(genesis.GetNetworkPublicKey()) != Hash(plan.context.network_binding) ||
        PrepareStorageAssignment(plan.context, plan.eligible) != plan) return std::nullopt;
    StorageAssignmentId result{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes("CYBOU/STORAGE-ASSIGNMENT-ATTESTATION"),
            plan.commitment}, result.data())) return std::nullopt;
    return result;
}
std::string Key(const StorageAssignmentPlan& plan)
{ return "storage/assignment-attestation/" + Hash(plan.commitment).GetHex(); }
std::string BindingKey(const StorageAssignmentPlan& plan, const StorageAssignmentId& storage)
{ return Key(plan) + "/binding/" + Hash(storage).GetHex(); }
std::optional<AttestedStorageAssignment> Read(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentPlan& plan)
{
    const auto bytes = db.Get(Key(plan));
    if (!bytes) return std::nullopt;
    try {
        BinaryReader in{*bytes};
        AttestedStorageAssignment result{plan};
        result.signature.ed25519 = in.Fixed<std::array<unsigned char, 64>>();
        const auto ml = in.Fixed(3309);
        result.signature.ml_dsa.assign(ml.begin(), ml.end());
        in.Finish();
        return VerifyStorageAssignmentAttestation(genesis, result) ?
            std::optional<AttestedStorageAssignment>{result} : std::nullopt;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}
}

std::optional<std::vector<unsigned char>> EncodeStorageAssignmentAttestation(
    const AttestedStorageAssignment& assignment)
{
    const auto& plan = assignment.plan;
    if (PrepareStorageAssignment(plan.context, plan.eligible) != plan ||
        assignment.signature.ml_dsa.size() != 3309) return std::nullopt;
    try {
        BinaryWriter out{MAX_OPERATION_PAYLOAD_BYTES};
        const auto& c = plan.context;
        out.Fixed(c.network_binding); out.Fixed(c.publication); out.Fixed(c.chunk);
        out.Fixed(c.finalized_seed); out.Fixed(c.payer);
        out.U64(c.epoch); out.U64(c.term_start); out.U64(c.term_end); out.U8(c.replicas);
        out.U32(static_cast<std::uint32_t>(plan.eligible.size()));
        for (const auto& provider : plan.eligible) {
            out.Fixed(provider.storage_id); out.Fixed(provider.payout_account);
        }
        out.Fixed(assignment.signature.ed25519); out.Fixed(assignment.signature.ml_dsa);
        return out.Take();
    } catch (const std::length_error&) { return std::nullopt; }
}

std::optional<AttestedStorageAssignment> DecodeStorageAssignmentAttestation(
    const std::span<const unsigned char> bytes)
{
    constexpr std::size_t fixed_size{185 + 4 + 64 + 3309};
    if (bytes.size() < fixed_size || bytes.size() > MAX_OPERATION_PAYLOAD_BYTES) return std::nullopt;
    try {
        BinaryReader in{bytes, MAX_OPERATION_PAYLOAD_BYTES};
        StorageAssignmentContext c;
        c.network_binding = in.Fixed<StorageAssignmentId>(); c.publication = in.Fixed<StorageAssignmentId>();
        c.chunk = in.Fixed<StorageAssignmentId>(); c.finalized_seed = in.Fixed<StorageAssignmentId>();
        c.payer = in.Fixed<StorageAssignmentId>(); c.epoch = in.U64(); c.term_start = in.U64();
        c.term_end = in.U64(); c.replicas = in.U8();
        const auto count = in.U32();
        // Check the exact size BEFORE allocation or any deterministic draw.
        if (!count || count > MAX_STORAGE_ASSIGNMENT_CANDIDATES ||
            bytes.size() != fixed_size + std::size_t{count} * 64) return std::nullopt;
        std::vector<StorageAssignmentProvider> eligible;
        eligible.reserve(count);
        for (std::uint32_t i{0}; i < count; ++i) {
            StorageAssignmentProvider provider{in.Fixed<StorageAssignmentId>(), in.Fixed<StorageAssignmentId>()};
            if (!eligible.empty() && !(eligible.back() < provider)) return std::nullopt;
            eligible.push_back(provider);
        }
        const auto plan = PrepareStorageAssignment(c, std::move(eligible));
        if (!plan) return std::nullopt;
        AttestedStorageAssignment result{*plan};
        result.signature.ed25519 = in.Fixed<std::array<unsigned char, 64>>();
        const auto ml = in.Fixed(3309);
        result.signature.ml_dsa.assign(ml.begin(), ml.end());
        in.Finish();
        return result;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}

bool VerifyStorageAssignmentBindings(const StorageAssignmentPlan& plan, const IdentityRegistry& registry,
    const Hash256& snapshot_id, const std::span<const StorageAssignmentBindingProof> proofs)
{
    if (Hash(plan.context.finalized_seed) != snapshot_id || proofs.empty() || proofs.size() != plan.eligible.size() ||
        proofs.size() > MAX_STORAGE_ASSIGNMENT_CANDIDATES ||
        PrepareStorageAssignment(plan.context, plan.eligible) != plan) return false;
    std::vector<StorageAssignmentProvider> providers;
    for (const auto& proof : proofs) {
        const auto* identity = registry.Find(proof.binding.payout_account);
        if (!identity || identity->authorization_key.purpose != IdentityKeyPurpose::AUTHORIZATION ||
            !VerifyStoragePayoutBindingStorageKey(proof.binding, Hash(plan.context.network_binding), proof.storage_id) ||
            !VerifyIdentityMessage(identity->authorization_key, proof.binding.authorization,
                StoragePayoutBindingDigest(Hash(plan.context.network_binding), proof.storage_id, proof.binding.payout_account))) return false;
        StorageAssignmentProvider provider{.storage_id = proof.storage_id};
        std::copy_n(proof.binding.payout_account.Value().begin(), 32, provider.payout_account.begin());
        providers.push_back(provider);
    }
    std::sort(providers.begin(), providers.end());
    return providers == plan.eligible;
}

bool VerifyStorageAssignmentAttestation(const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment)
{
    const auto digest = Digest(genesis, assignment.plan);
    return digest && VerifyIdentityMessage(genesis.GetPoaPublicKey(), assignment.signature, *digest);
}

std::optional<AttestedStorageAssignment> AttestStorageAssignment(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentPlan& plan, const IdentityRegistry& registry,
    const Hash256& snapshot_id, const std::span<const StorageAssignmentBindingProof> proofs, const PoaSigner& signer)
{
    const auto digest = Digest(genesis, plan);
    if (!digest || signer.PublicKey() != genesis.GetPoaPublicKey() ||
        !VerifyStorageAssignmentBindings(plan, registry, snapshot_id, proofs)) return std::nullopt;
    {
        PrivateApplicationStore::Batch freeze{db};
        if (!freeze.IsOutermost() || !FreezeStorageAssignment(db, plan) || !freeze.Commit()) return std::nullopt;
    }
    PrivateApplicationStore::Batch batch{db};
    if (const auto cached = Read(db, genesis, plan)) {
        if (!LoadStorageAssignmentBindings(db, genesis, plan.context, registry, snapshot_id)) return std::nullopt;
        return cached;
    }
    if (db.Has(Key(plan))) return std::nullopt; // Never replace an unreadable signature.
    const auto signature = signer.Sign(*digest);
    if (!signature) return std::nullopt;
    AttestedStorageAssignment result{plan, *signature};
    if (!VerifyStorageAssignmentAttestation(genesis, result)) return std::nullopt;
    BinaryWriter out; out.Fixed(signature->ed25519); out.Fixed(signature->ml_dsa);
    for (const auto& proof : proofs) {
        if (!db.Put(BindingKey(plan, proof.storage_id), EncodeStoragePayoutBinding(proof.binding))) return std::nullopt;
    }
    if (!db.Put(Key(plan), out.Take()) || !batch.Commit()) return std::nullopt;
    return result;
}

std::optional<AttestedStorageAssignment> LoadStorageAssignmentAttestation(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentContext& context)
{
    const auto plan = LoadStorageAssignment(db, context);
    return plan ? Read(db, genesis, *plan) : std::nullopt;
}

std::optional<std::vector<StorageAssignmentBindingProof>> LoadStorageAssignmentBindings(PrivateApplicationStore& db,
    const VerifiedNetworkGenesis& genesis, const StorageAssignmentContext& context,
    const IdentityRegistry& registry, const Hash256& snapshot_id)
{
    PrivateApplicationStore::Batch snapshot{db};
    const auto attested = LoadStorageAssignmentAttestation(db, genesis, context);
    if (!attested) return std::nullopt;
    std::vector<StorageAssignmentBindingProof> proofs;
    for (const auto& provider : attested->plan.eligible) {
        const auto bytes = db.Get(BindingKey(attested->plan, provider.storage_id));
        const auto binding = bytes ? DecodeStoragePayoutBinding(*bytes) : std::nullopt;
        if (!binding) return std::nullopt;
        proofs.push_back({provider.storage_id, *binding});
    }
    return VerifyStorageAssignmentBindings(attested->plan, registry, snapshot_id, proofs) ?
        std::optional<std::vector<StorageAssignmentBindingProof>>{proofs} : std::nullopt;
}

bool VerifyAssignedStorageReceipt(const VerifiedNetworkGenesis& genesis, const AttestedStorageAssignment& assignment,
    const std::uint8_t slot, const std::span<const unsigned char> receipt, const std::uint32_t stored_size)
{
    if (!stored_size || slot >= assignment.plan.selected.size() ||
        !VerifyStorageAssignmentAttestation(genesis, assignment)) return false;
    const auto& c = assignment.plan.context;
    const auto storage = VerifyStorageReceipt(receipt, Hash(c.network_binding), Hash(c.publication), c.chunk, stored_size);
    return storage && *storage == assignment.plan.selected[slot].storage_id;
}
}
