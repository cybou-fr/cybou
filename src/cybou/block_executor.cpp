// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/block_executor.h>
#include <cybou/authority.h>
#include <cybou/validation_service.h>
#include <cybou/chunk_authorization.h>

#include <algorithm>
#include <limits>
#include <bit>
#include <set>
#include <tuple>

namespace cybou {

BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const uint256& network_id, uint64_t block_height,
    const CybouProtocolParameters& params, const uint256& parent_block_id,
    const std::function<std::optional<RootPublication>(const uint256&)>& finalized_publication)
{
    const auto fail = [](BlockExecutionError error) {
        BlockExecutionResult result{};
        result.error = error;
        return result;
    };
    if (ValidateCybouState(parent) != StateValidationError::NONE) return fail(BlockExecutionError::INVALID_STATE);
    if (!ValidAuthorityPolicy(params.authority) || params.epoch_blocks == 0 || params.epoch_blocks > 65536) return fail(BlockExecutionError::INVALID_STATE);
    const uint64_t initial_supply = TotalSupply(parent);
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOp>(operation);
    });
    if (creates > params.max_account_creates_per_block) return fail(BlockExecutionError::TOO_MANY_ACCOUNT_CREATES);
    auto candidate = parent;
    const auto epoch = EpochForHeight(block_height, params);
    const auto slots = ContributionSlots(params.epoch_blocks);
    const auto full_mask = (uint32_t{1} << slots) - 1;
    for (auto& [account, owner] : candidate.accounts) {
        auto& a = owner.authority;
        if (a.epoch > epoch) return fail(BlockExecutionError::INVALID_STATE);
        if (a.epoch < epoch) {
            a.epoch = epoch; a.activity_this_epoch = 0; a.protocol_used = 0; a.validation_this_epoch = 0; owner.bandwidth_reserved = 0;
        }
        const auto evidence_epoch = EpochForHeight(block_height == 0 ? 0 : block_height - 1, params);
        if (a.liveness_epoch > evidence_epoch) return fail(BlockExecutionError::INVALID_STATE);
        if (a.liveness_epoch < evidence_epoch) {
            if (a.liveness_observations >= params.epoch_blocks / 2 + params.epoch_blocks % 2) a.liveness = SaturatingAdd(a.liveness, 1);
            a.liveness_epoch = evidence_epoch; a.liveness_observations = 0;
        }
    }
    for (auto& [key, pledge] : candidate.storage_pledges) {
        if (pledge.epoch > epoch) return fail(BlockExecutionError::INVALID_STATE);
        if (pledge.epoch < epoch) {
            if (!pledge.false_claim && pledge.response_mask == full_mask) {
                auto& a = candidate.accounts.at(key.first).authority;
                const auto work = SaturatingAdd(a.storage_remainder, pledge.stored_bytes);
                a.storage = SaturatingAdd(a.storage, work / AUTHORITY_STORAGE_BYTE_EPOCH_UNIT);
                a.storage_remainder = work % AUTHORITY_STORAGE_BYTE_EPOCH_UNIT;
            }
            pledge.epoch = epoch; pledge.response_mask = 0; pledge.false_claim = false;
        }
    }
    std::erase_if(candidate.resources,[&](const auto& entry) {
        return entry.second.grant.domain == ResourceDomain::BANDWIDTH && entry.second.epoch < epoch;
    });
    std::set<std::tuple<AccountId, unsigned, ChunkId, uint256>> service_keys;
    if (params.name_commit_max_lifetime > 0) {
        std::erase_if(candidate.names.pending_commits, [&](const auto& item) {
            return block_height > item.second.commit_height &&
                block_height - item.second.commit_height > params.name_commit_max_lifetime;
        });
    }
    for (size_t i{0}; i < operations.size(); ++i) {
        const auto account = AuthorizingAccount(operations[i]);
        if (account) {
            const auto owner = candidate.accounts.find(*account);
            if (owner == candidate.accounts.end()) return fail(BlockExecutionError::INVALID_STATE);
            auto& accumulator = owner->second.authority;
            const auto epoch = EpochForHeight(block_height, params);
            if (accumulator.epoch > epoch) return fail(BlockExecutionError::INVALID_STATE);
            if (accumulator.epoch != epoch) {
                accumulator.epoch = epoch; accumulator.activity_this_epoch = 0; accumulator.protocol_used = 0;
            }
            // Freeze eligibility to the parent: spending in this block cannot raise its own allowance.
            const auto original = parent.accounts.find(*account);
            auto fresh = owner->second; fresh.authority = {};
            const auto limits = ComputeAuthorityRecord(*account, original == parent.accounts.end() ? fresh : original->second,
                epoch, params.authority).budgets;
            if (accumulator.protocol_used >= limits.protocol_operations_per_epoch) {
                auto failure = fail(BlockExecutionError::AUTHORITY_BUDGET_EXHAUSTED); failure.failed_operation_index = i; return failure;
            }
            ++accumulator.protocol_used;
        }
        if (const auto* create = std::get_if<AccountCreateOp>(&operations[i])) {
            const auto result = ApplyAccountCreate(*create, network_id, block_height, params, candidate);
            if (result != AccountCreateStateError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_ACCOUNT_CREATE);
                failure.failed_operation_index = i;
                failure.create_error = result;
                return failure;
            }
        } else if (const auto* payment = std::get_if<AuthorizedPayment>(&operations[i])) {
            const auto result = ApplyPayment(*payment, network_id, params, candidate);
            if (result != PaymentError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_PAYMENT);
                failure.failed_operation_index = i;
                failure.payment_error = result;
                return failure;
            }
        } else if (const auto* rotate = std::get_if<IdentityRotate>(&operations[i])) {
            const auto result = candidate.identities.RotateIdentity(*rotate, network_id);
            if (result != IdentityRegistryError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_IDENTITY_ROTATE);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
            // Recovery rotation invalidates every service binding for this Identity.
            std::erase_if(candidate.bound_nodes, [&](const auto& node) { return node.second.account == rotate->account_id; });
            std::erase_if(candidate.storage_pledges, [&](const auto& pledge) { return pledge.first.first == rotate->account_id; });
        } else if (const auto* lock = std::get_if<AuthorizedSystemLock>(&operations[i])) {
            const auto result = ApplySystemLock(*lock, network_id, candidate);
            if (result != SystemLockError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_SYSTEM_LOCK);
                failure.failed_operation_index = i;
                failure.lock_error = result;
                return failure;
            }
        } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operations[i])) {
            const auto result = ApplyNameCommit(*commit, network_id, block_height, params, candidate);
            if (result != NameCommitError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_NAME_COMMIT);
                failure.failed_operation_index = i;
                failure.name_commit_error = result;
                return failure;
            }
        } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operations[i])) {
            const auto result = ApplyNameReveal(*reveal, network_id, block_height, params, candidate);
            if (result != NameRevealError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_NAME_REVEAL);
                failure.failed_operation_index = i;
                failure.name_reveal_error = result;
                return failure;
            }
        } else if (const auto* publication = std::get_if<AuthorizedRootPublication>(&operations[i])) {
            const auto result = ApplyRootPublication(*publication, network_id, params, candidate);
            if (result != RootPublicationError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_ROOT_PUBLICATION);
                failure.failed_operation_index = i;
                failure.root_publication_error = result;
                return failure;
            }
        } else if (const auto* reservation = std::get_if<AuthorizedResourceReservation>(&operations[i])) {
            const auto commitment = ResourceReservationCommitment(reservation->reservation);
            const auto operation_id = ComputeOperationId(operations[i]);
            if (!commitment || !operation_id || reservation->authorization.kind != IdentityOperationKind::RESOURCE_RESERVATION ||
                *commitment != reservation->authorization.payload_commitment ||
                candidate.identities.AuthorizeOperation(reservation->authorization,network_id) != IdentityRegistryError::NONE)
                return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
            const auto original = parent.accounts.find(*account);
            if (original == parent.accounts.end()) return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
            const auto limits = ComputeAuthorityRecord(*account,original->second,epoch,params.authority).budgets;
            uint64_t storage{0}; size_t count{0}; std::set<uint256> uses;
            for (const auto& [id,r] : candidate.resources) if (r.account == *account) {
                ++count; uses.insert(r.grant.use_commitment);
                if (r.grant.domain == ResourceDomain::STORAGE) storage = SaturatingAdd(storage,r.grant.bytes);
            }
            const auto added = reservation->reservation.grants.size();
            if (count+added > MAX_RESOURCE_GRANTS_PER_ACCOUNT || candidate.resources.size()+added > MAX_IDENTITY_REGISTRY_ACCOUNTS)
                return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
            auto& owner = candidate.accounts.at(*account);
            for (size_t index=0;index<added;++index) {
                const auto& grant = reservation->reservation.grants[index];
                if (!uses.insert(grant.use_commitment).second) return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
                auto& used = grant.domain == ResourceDomain::STORAGE ? storage : owner.bandwidth_reserved;
                const auto limit = grant.domain == ResourceDomain::STORAGE ? limits.storage_bytes : limits.bandwidth_bytes_per_epoch;
                if (used>limit || grant.bytes>limit-used) return fail(BlockExecutionError::AUTHORITY_BUDGET_EXHAUSTED);
                used+=grant.bytes;
                if (!candidate.resources.emplace(ResourceGrantId(*operation_id,index),ReservedResource{*account,grant,epoch}).second)
                    return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
            }
            const auto fee = uint64_t{added}*4;
            if (owner.system_balance<fee || candidate.pending_fee_pool>UINT64_MAX-fee) return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
            owner.system_balance-=fee; candidate.pending_fee_pool+=fee;
        } else if (const auto* release = std::get_if<AuthorizedResourceRelease>(&operations[i])) {
            const auto commitment = ResourceReleaseCommitment(release->release);
            if (!commitment || release->authorization.kind != IdentityOperationKind::RESOURCE_RELEASE ||
                *commitment != release->authorization.payload_commitment ||
                candidate.identities.AuthorizeOperation(release->authorization,network_id) != IdentityRegistryError::NONE)
                return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
            for (const auto& id : release->release.grants) {
                const auto grant=candidate.resources.find(id);
                if (grant==candidate.resources.end() || grant->second.account!=*account) return fail(BlockExecutionError::INVALID_RESOURCE_RESERVATION);
                // A spent or cancelled bandwidth reservation is never refunded within its epoch.
                candidate.resources.erase(grant);
            }
        } else if (const auto* binding = std::get_if<AuthorizedNodeBinding>(&operations[i])) {
            const auto id = binding->binding.revoke
                ? (binding->binding.revoke_node_id.IsNull() ? std::optional<uint256>{} : std::optional{binding->binding.revoke_node_id})
                : ValidationNodeId(binding->binding.key);
            const auto commitment = ComputeNodeBindingCommitment(binding->binding);
            const auto digest = ComputeIdentityOperationDigest(network_id, binding->authorization);
            if (!id || !commitment || binding->authorization.kind != IdentityOperationKind::NODE_BINDING ||
                *commitment != binding->authorization.payload_commitment || !digest) return fail(BlockExecutionError::INVALID_NODE_BINDING);
            if (!binding->binding.revoke && !VerifyIdentityMessage(binding->binding.key, binding->node_proof, *digest))
                return fail(BlockExecutionError::INVALID_NODE_BINDING);
            if (!binding->binding.revoke && binding->binding.provider_key &&
                (!binding->provider_proof || !VerifyIdentityMessage(*binding->binding.provider_key, *binding->provider_proof, *digest)))
                return fail(BlockExecutionError::INVALID_NODE_BINDING);
            const auto existing = candidate.bound_nodes.find(*id);
            if (binding->binding.revoke) {
                if (existing == candidate.bound_nodes.end() || existing->second.account != *account) return fail(BlockExecutionError::INVALID_NODE_BINDING);
            } else {
                if (existing != candidate.bound_nodes.end() || candidate.bound_nodes.size() >= MAX_IDENTITY_REGISTRY_ACCOUNTS ||
                    std::count_if(candidate.bound_nodes.begin(), candidate.bound_nodes.end(), [&](const auto& entry) { return entry.second.account == *account; }) >= 8)
                    return fail(BlockExecutionError::INVALID_NODE_BINDING);
            }
            if (candidate.identities.AuthorizeOperation(binding->authorization, network_id) != IdentityRegistryError::NONE) return fail(BlockExecutionError::INVALID_NODE_BINDING);
            if (binding->binding.revoke) {
                candidate.bound_nodes.erase(*id);
                std::erase_if(candidate.storage_pledges, [&](const auto& pledge) { return pledge.second.node_id == *id; });
            }
            else {
                if (binding->binding.provider_key && std::any_of(candidate.bound_nodes.begin(),candidate.bound_nodes.end(),[&](const auto& entry){
                    return entry.second.provider_key && StorageProviderId(*entry.second.provider_key) == StorageProviderId(*binding->binding.provider_key); })) return fail(BlockExecutionError::INVALID_NODE_BINDING);
                candidate.bound_nodes.emplace(*id, BoundNode{*account, binding->binding.key, binding->authorization.key_epoch, binding->binding.provider_key});
            }
        } else if (const auto* evidence = std::get_if<ServiceEvidence>(&operations[i])) {
            const auto node = parent.bound_nodes.find(evidence->node_id);
            const auto digest = ServiceEvidenceDigest(network_id, *evidence);
            const auto key = std::pair{evidence->account_id, evidence->chunk_id};
            if (block_height == 0 || parent_block_id.IsNull() || evidence->base_height != block_height - 1 ||
                evidence->base_block_id != parent_block_id || node == parent.bound_nodes.end() || node->second.account != *account ||
                !candidate.bound_nodes.contains(evidence->node_id) || candidate.bound_nodes.at(evidence->node_id) != node->second || !digest || !VerifyIdentityMessage(node->second.key, evidence->signature, *digest) ||
                !service_keys.emplace(*account, evidence->kind == ServiceEvidenceKind::VALIDATION_RECEIPT ? 1U : 0U,
                    evidence->chunk_id, evidence->kind == ServiceEvidenceKind::VALIDATION_RECEIPT ? evidence->publication_id : uint256{}).second)
                return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
            auto& a = candidate.accounts.at(*account).authority;
            if (evidence->kind != ServiceEvidenceKind::HEARTBEAT && evidence->kind != ServiceEvidenceKind::VALIDATION_RECEIPT && !node->second.provider_key)
                return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
            const auto slot = ContributionSlot(evidence->base_height, params.epoch_blocks);
            const auto observe = [&] {
                if (a.last_liveness_height != evidence->base_height) {
                    ++a.liveness_observations; a.last_liveness_height = evidence->base_height;
                }
            };
            if (evidence->kind == ServiceEvidenceKind::VALIDATION_RECEIPT) {
                if (CybouStateHash(parent) != evidence->base_state_root || evidence->validated_operation.size() < 2 ||
                    evidence->validated_operation[1] == static_cast<uint8_t>(ProtocolOperationKind::SERVICE_EVIDENCE) ||
                    evidence->validated_operation[1] == static_cast<uint8_t>(ProtocolOperationKind::ACCOUNT_CREATE)) return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                const auto target = DeserializeProtocolOperation(evidence->validated_operation);
                if (!target || ComputeOperationId(*target) != evidence->publication_id) return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                const auto checked = ExecuteBlockOperations(parent, {*target}, network_id, block_height, params, parent_block_id, finalized_publication);
                if (!checked) a.penalty_debt = SaturatingAdd(a.penalty_debt, FALSE_VALIDATION_CLAIM_PENALTY);
                else if (AuthorizingAccount(*target) != account && std::any_of(operations.begin(), operations.end(),
                    [&](const auto& included) { return !std::holds_alternative<ServiceEvidence>(included) && ComputeOperationId(included) == evidence->publication_id; }) && a.validation_this_epoch < 16) {
                    ++a.validation_this_epoch; a.validation = SaturatingAdd(a.validation, 1);
                    observe();
                }
            } else if (evidence->kind == ServiceEvidenceKind::HEARTBEAT) {
                observe();
            } else if (evidence->kind == ServiceEvidenceKind::STORAGE_COMMIT) {
                if (!finalized_publication || candidate.storage_pledges.contains(key) || candidate.storage_pledges.size() >= MAX_IDENTITY_REGISTRY_ACCOUNTS)
                    return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                const auto publication = finalized_publication(evidence->publication_id);
                if (!publication || !evidence->possession || !VerifyChunkAuthorizationPath(publication->chunk_authorization_root,
                    evidence->chunk_id, evidence->authorization_index, publication->chunk_count, evidence->authorization_siblings)) return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                const auto& proof = *evidence->possession;
                if (proof.leaf_index != (proof.stored_bytes + 1023) / 1024 - 1) return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                if (!VerifyChunkPossessionProof(evidence->chunk_id, proof)) a.penalty_debt = SaturatingAdd(a.penalty_debt, FALSE_STORAGE_CLAIM_PENALTY);
                else {
                    uint64_t pledged_bytes{0}; uint32_t count{0};
                    for (const auto& [prior, pledge] : candidate.storage_pledges) if (prior.first == *account) { pledged_bytes = SaturatingAdd(pledged_bytes, pledge.stored_bytes); ++count; }
                    const auto limit = ComputeAuthorityRecord(*account, parent.accounts.at(*account), epoch, params.authority).budgets.storage_bytes;
                    if (count >= MAX_STORAGE_PLEDGES_PER_IDENTITY || pledged_bytes > limit || proof.stored_bytes > limit - pledged_bytes || epoch == UINT64_MAX)
                        return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                    candidate.storage_pledges.emplace(key, StoragePledge{evidence->publication_id, evidence->node_id, proof.stored_bytes, epoch + 1, epoch, 0, false});
                }
            } else {
                const auto pledge = candidate.storage_pledges.find(key);
                if (pledge == candidate.storage_pledges.end() || pledge->second.node_id != evidence->node_id || pledge->second.publication_id != evidence->publication_id)
                    return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                if (evidence->kind == ServiceEvidenceKind::STORAGE_RELEASE) candidate.storage_pledges.erase(pledge);
                else {
                    auto& p = pledge->second;
                    if (!slot || EpochForHeight(evidence->base_height, params) != epoch || epoch < p.start_epoch || !evidence->possession || p.response_mask & (uint32_t{1} << *slot)) return fail(BlockExecutionError::INVALID_SERVICE_EVIDENCE);
                    const auto& proof = *evidence->possession;
                    const bool valid = proof.stored_bytes == p.stored_bytes &&
                        proof.leaf_index == StorageChallengeLeaf(network_id, parent_block_id, *account, evidence->chunk_id, p.stored_bytes) &&
                        VerifyChunkPossessionProof(evidence->chunk_id, proof);
                    p.response_mask |= uint32_t{1} << *slot;
                    if (!valid) {
                        if (!p.false_claim) a.penalty_debt = SaturatingAdd(a.penalty_debt, FALSE_STORAGE_CLAIM_PENALTY);
                        p.false_claim = true;
                    } else observe();
                }
            }
        }
        if (account && !std::holds_alternative<ServiceEvidence>(operations[i])) {
            auto& accumulator = candidate.accounts.at(*account).authority;
            if (accumulator.activity_this_epoch < params.authority.activity_cap_per_epoch) {
                ++accumulator.activity_this_epoch; accumulator.activity = SaturatingAdd(accumulator.activity, 1);
            }
            if (const auto* lock = std::get_if<AuthorizedSystemLock>(&operations[i]))
                accumulator.system_contribution = SaturatingAdd(accumulator.system_contribution, lock->lock.amount);
        }
    }
    const uint64_t chunks = candidate.pending_fee_pool / 4;
    const uint64_t security_addition = chunks * 3;
    if (candidate.security_reward_pool > std::numeric_limits<uint64_t>::max() - security_addition ||
        candidate.onboarding_pool > std::numeric_limits<uint64_t>::max() - chunks) return fail(BlockExecutionError::FEE_ROUTING_OVERFLOW);
    candidate.security_reward_pool += security_addition;
    candidate.onboarding_pool += chunks;
    candidate.pending_fee_pool %= 4;
    const uint64_t final_supply = TotalSupply(candidate);
    if (final_supply != initial_supply) return fail(BlockExecutionError::FEE_ROUTING_OVERFLOW);
    if (ValidateCybouState(candidate) != StateValidationError::NONE) return fail(BlockExecutionError::INVALID_STATE);
    const auto root = CybouStateHash(candidate);
    if (!root) return fail(BlockExecutionError::INVALID_STATE);
    BlockExecutionResult success{};
    success.state = std::move(candidate);
    success.state_root = *root;
    return success;
}

} // namespace cybou
