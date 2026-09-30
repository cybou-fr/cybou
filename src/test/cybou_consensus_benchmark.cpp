// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block.h>
#include <cybou/block_executor.h>
#include <cybou/chunk_possession.h>
#include <cybou/identity_service.h>
#include <cybou/protocol_limits.h>
#include <cybou/service_evidence.h>
#include <test/cybou_service_test_fixture.h>

#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
using namespace cybou;

FinalizedValidationSnapshot Snapshot(CybouNodeRuntime& runtime)
{
    std::optional<FinalizedValidationSnapshot> snapshot;
    if (!runtime.ReadFinalizedValidationSnapshot([&](const auto& value) { snapshot = value; })) {
        throw std::runtime_error{"cannot read finalized snapshot"};
    }
    return std::move(*snapshot);
}

AuthorizedNodeBinding Bind(CybouIdentityService& owner, const FinalizedValidationSnapshot& snapshot,
    std::span<const unsigned char, 32> service_seed, std::span<const unsigned char, 32> provider_seed)
{
    const auto key = DeriveIdentityPublicKey(service_seed, IdentityKeyPurpose::VALIDATION_NODE);
    const auto provider = DeriveIdentityPublicKey(provider_seed, IdentityKeyPurpose::STORAGE_PROVIDER);
    const auto account = *owner.GetAccountId();
    const auto* identity = snapshot.state.identities.Find(account);
    if (!key || !provider || !identity) throw std::runtime_error{"cannot create bound storage node"};
    AuthorizedNodeBinding binding;
    binding.binding.key = *key;
    binding.binding.provider_key = *provider;
    binding.authorization = {account, identity->nonce, identity->key_epoch, IdentityOperationKind::NODE_BINDING,
        *ComputeNodeBindingCommitment(binding.binding), {}};
    const auto digest = ComputeIdentityOperationDigest(snapshot.base.network_id, binding.authorization);
    if (!digest) throw std::runtime_error{"cannot hash binding authorization"};
    const auto owner_signature = owner.GetKeyStore().SignAuthorization(*digest);
    const auto node_signature = SignIdentityMessage(service_seed, IdentityKeyPurpose::VALIDATION_NODE, *digest);
    const auto provider_signature = SignIdentityMessage(provider_seed, IdentityKeyPurpose::STORAGE_PROVIDER, *digest);
    if (!owner_signature || !node_signature || !provider_signature) throw std::runtime_error{"cannot sign node binding"};
    binding.authorization.signature = *owner_signature;
    binding.node_proof = *node_signature;
    binding.provider_proof = *provider_signature;
    return binding;
}

ChunkId SyntheticChunkId(uint32_t index)
{
    ChunkId id{};
    id.begin()[0] = 0xff;
    for (unsigned byte = 0; byte < 4; ++byte) id.begin()[1 + byte] = index >> (byte * 8);
    id.begin()[5] = 0x7e;
    return id;
}
}

int main()
{
    constexpr size_t MAX_PENDING{256};
    CybouServiceTestFixture fixture{0x74, 16};
    std::array<unsigned char, 32> service_seed{};
    std::array<unsigned char, 32> provider_seed{};
    service_seed[0] = 0x45;
    provider_seed[0] = 0x46;
    auto owner = fixture.CreateIdentity("benchmark-owner.vault");
    if (!fixture.runtime->SubmitOperation(Bind(*owner, Snapshot(*fixture.runtime), service_seed, provider_seed)) ||
        !fixture.runtime->ProduceBlock()) throw std::runtime_error{"cannot finalize storage binding"};

    auto snapshot = Snapshot(*fixture.runtime);
    const auto account = *owner->GetAccountId();
    const auto node_id = ValidationNodeId(*DeriveIdentityPublicKey(service_seed, IdentityKeyPurpose::VALIDATION_NODE));
    if (!node_id) throw std::runtime_error{"cannot derive validation NodeID"};
    const auto publication_id = uint256::ONE;
    const auto size = ENCRYPTED_CHUNK_MAX_STORED_BYTES;
    std::vector<ProtocolOperation> operations;
    operations.reserve(MAX_PENDING);
    std::vector<unsigned char> bytes(size);
    uint32_t random{0x82ab4d31};
    const auto next = [&] { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    std::set<ChunkId> live_chunks;

    // Build 256 maximum-size signed storage proofs. The synthetic parent fills
    // this Identity's pledge map to its canonical per-Identity bound (4096),
    // exercising block transition against a near-limit state map.
    for (uint32_t index = 0; index < MAX_PENDING; ++index) {
        for (auto& byte : bytes) byte = static_cast<unsigned char>(next());
        const auto chunk = ComputeChunkId(bytes);
        const auto leaf = StorageChallengeLeaf(snapshot.base.network_id, snapshot.base.block_id, account, chunk, size);
        const auto proof = BuildChunkPossessionProof(bytes, leaf);
        if (!proof || !live_chunks.insert(chunk).second) throw std::runtime_error{"cannot build unique max-size proof"};
        snapshot.state.storage_pledges.emplace(std::pair{account, chunk},
            StoragePledge{publication_id, *node_id, size, 0, 0, 0, false});

        ServiceEvidence evidence;
        evidence.account_id = account;
        evidence.node_id = *node_id;
        evidence.base_height = snapshot.base.height;
        evidence.base_block_id = snapshot.base.block_id;
        evidence.kind = ServiceEvidenceKind::STORAGE_RESPONSE;
        evidence.publication_id = publication_id;
        evidence.chunk_id = chunk;
        evidence.possession = *proof;
        const auto digest = ServiceEvidenceDigest(snapshot.base.network_id, evidence);
        const auto signature = digest ? SignIdentityMessage(service_seed, IdentityKeyPurpose::VALIDATION_NODE, *digest) : std::nullopt;
        if (!signature) throw std::runtime_error{"cannot sign storage evidence"};
        evidence.signature = *signature;
        operations.emplace_back(std::move(evidence));
    }
    for (uint32_t index = 0; snapshot.state.storage_pledges.size() < MAX_STORAGE_PLEDGES_PER_IDENTITY; ++index) {
        auto chunk = SyntheticChunkId(index);
        while (live_chunks.contains(chunk)) ++chunk.begin()[5];
        snapshot.state.storage_pledges.emplace(std::pair{account, chunk},
            StoragePledge{publication_id, *node_id, size, 0, 0, 0, false});
    }
    if (ValidateCybouState(snapshot.state) != StateValidationError::NONE) {
        throw std::runtime_error{"synthetic parent state violates consensus invariants"};
    }

    const auto started = std::chrono::steady_clock::now();
    const auto result = ExecuteBlockOperations(snapshot.state, operations, snapshot.base.network_id,
        snapshot.base.height + 1, snapshot.parameters, snapshot.base.block_id);
    const auto execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    if (!result) throw std::runtime_error{"max-size storage evidence block was rejected: " +
        std::to_string(static_cast<unsigned>(result.error))};

    CybouBlock block;
    block.parent_block_id = snapshot.base.block_id;
    block.height = snapshot.base.height + 1;
    block.operations = operations;
    block.resulting_state_root = *result.state_root;
    const auto encoded = SerializeBlock(block);
    if (!encoded || encoded->size() > MAX_FINALIZER_SERIALIZED_BLOCK_BYTES) {
        throw std::runtime_error{"benchmark block exceeds the finalized block bound"};
    }
    std::cout << "scenario=max_storage_response_block"
              << " operations=" << operations.size()
              << " stored_bytes_per_proof=" << size
              << " active_pledges=" << snapshot.state.storage_pledges.size()
              << " block_bytes=" << encoded->size()
              << " execution_ms=" << execution_ms
              << " parent_state=synthetic_invariant_valid"
              << " valid=true\n";
    return 0;
}
