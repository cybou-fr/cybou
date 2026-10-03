// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Исполнение блоков без побочных эффектов на хранилище.

#include <cybou/block_executor.h>

#include <cybou/protocol_limits.h>

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace cybou {
namespace {

struct ByteArray32Hasher {
    size_t operator()(const std::array<unsigned char, 32>& value) const noexcept
    {
        size_t hash{1469598103934665603ull};
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    size_t operator()(const cybou::Hash256& value) const noexcept
    {
        size_t hash{1469598103934665603ull};
        for (const unsigned char byte : value) {
            hash ^= byte;
            hash *= 1099511628211ull;
        }
        return hash;
    }
};

struct AccountIdHasher {
    size_t operator()(const AccountId& account) const noexcept
    {
        return ByteArray32Hasher{}(account.Value());
    }
};

} // namespace

BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key)
{
    const auto fail = [](BlockExecutionError error) {
        BlockExecutionResult result{};
        result.error = error;
        return result;
    };
    if (ValidateCybouState(parent) != StateValidationError::NONE) return fail(BlockExecutionError::INVALID_STATE);
    const uint64_t initial_supply = TotalSupply(parent);
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOp>(operation);
    });
    if (creates > params.max_account_creates_per_block) return fail(BlockExecutionError::TOO_MANY_ACCOUNT_CREATES);
    auto candidate = parent;
    std::unordered_set<std::array<unsigned char, 32>, ByteArray32Hasher> adjustment_digests;
    std::unordered_set<AccountId, AccountIdHasher> auth_credited_accounts;
    adjustment_digests.reserve(operations.size());
    auth_credited_accounts.reserve(operations.size());
    if (params.name_commit_max_lifetime > 0) {
        // Истечение pending commit-ов является частью детерминированного block execution:
        // одинаковая высота должна давать одинаковый реестр имён даже при пустом блоке.
        std::erase_if(candidate.names.pending_commits, [&](const auto& item) {
            return block_height > item.second.commit_height &&
                block_height - item.second.commit_height > params.name_commit_max_lifetime;
        });
    }
    // Окна лимитов (DEC-272): счётчики другого блока или другой эпохи обнуляются до
    // исполнения, пустые записи удаляются. Идемпотентно для повторного исполнения той же высоты.
    const uint64_t epoch = EpochForHeight(block_height, params);
    for (auto it = candidate.usage.begin(); it != candidate.usage.end();) {
        auto& usage = it->second;
        if (usage.epoch != epoch) usage.epoch_operations = 0;
        if (usage.epoch_operations == 0) usage.epoch = 0;
        if (usage.block_height != block_height) usage.block_operations = 0;
        if (usage.block_operations == 0) usage.block_height = 0;
        it = usage.Empty() ? candidate.usage.erase(it) : std::next(it);
    }
    for (size_t i{0}; i < operations.size(); ++i) {
        // Лимиты уровня считаются по AUTH финализированного родителя: одно и то же
        // решение для пула, Validation и PoA. AccountCreate и PoaAuthAdjustment не метрируются.
        const auto metered = AuthorizingAccount(operations[i]);
        const auto* publication_op = std::get_if<AuthorizedRootPublication>(&operations[i]);
        if (metered) {
            const auto parent_account = parent.accounts.find(*metered);
            const auto limits = ComputeAuthorityTierLimits(
                parent_account == parent.accounts.end() ? 0 : parent_account->second.authority);
            const auto found = candidate.usage.find(*metered);
            const AccountUsage usage = found == candidate.usage.end() ? AccountUsage{} : found->second;
            if (usage.block_operations >= limits.operations_per_block ||
                usage.epoch_operations >= limits.operations_per_epoch) {
                auto failure = fail(BlockExecutionError::OPERATION_LIMIT_EXCEEDED);
                failure.failed_operation_index = i;
                return failure;
            }
            if (publication_op) {
                const uint32_t chunks = publication_op->publication.chunk_count;
                if (chunks > limits.max_publication_chunks) {
                    auto failure = fail(BlockExecutionError::PUBLICATION_TOO_LARGE);
                    failure.failed_operation_index = i;
                    return failure;
                }
                if (usage.stored_chunks > limits.storage_quota_chunks ||
                    chunks > limits.storage_quota_chunks - usage.stored_chunks) {
                    auto failure = fail(BlockExecutionError::STORAGE_QUOTA_EXCEEDED);
                    failure.failed_operation_index = i;
                    return failure;
                }
            }
        }
        if (const auto* create = std::get_if<AccountCreateOp>(&operations[i])) {
            const auto result = ApplyAccountCreate(*create, network_binding, block_height, params, candidate);
            if (result != AccountCreateStateError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_ACCOUNT_CREATE);
                failure.failed_operation_index = i;
                failure.create_error = result;
                return failure;
            }
        } else if (const auto* payment = std::get_if<AuthorizedPayment>(&operations[i])) {
            const auto result = ApplyPayment(*payment, network_binding, params, candidate);
            if (result != PaymentError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_PAYMENT);
                failure.failed_operation_index = i;
                failure.payment_error = result;
                return failure;
            }
        } else if (const auto* rotate = std::get_if<IdentityRotate>(&operations[i])) {
            const auto result = candidate.identities.RotateIdentity(*rotate, network_binding);
            if (result != IdentityRegistryError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_IDENTITY_ROTATE);
                failure.failed_operation_index = i;
                failure.identity_error = result;
                return failure;
            }
        } else if (const auto* lock = std::get_if<AuthorizedSystemLock>(&operations[i])) {
            const auto result = ApplySystemLock(*lock, network_binding, candidate);
            if (result != SystemLockError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_SYSTEM_LOCK);
                failure.failed_operation_index = i;
                failure.lock_error = result;
                return failure;
            }
        } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operations[i])) {
            const auto result = ApplyNameCommit(*commit, network_binding, block_height, params, candidate);
            if (result != NameCommitError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_NAME_COMMIT);
                failure.failed_operation_index = i;
                failure.name_commit_error = result;
                return failure;
            }
        } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operations[i])) {
            const auto result = ApplyNameReveal(*reveal, network_binding, block_height, params, candidate);
            if (result != NameRevealError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_NAME_REVEAL);
                failure.failed_operation_index = i;
                failure.name_reveal_error = result;
                return failure;
            }
        } else if (const auto* publication = std::get_if<AuthorizedRootPublication>(&operations[i])) {
            const auto result = ApplyRootPublication(*publication, network_binding, params, candidate);
            if (result != RootPublicationError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_ROOT_PUBLICATION);
                failure.failed_operation_index = i;
                failure.root_publication_error = result;
                return failure;
            }
        } else if (const auto* revoke = std::get_if<AuthorizedRevokePublication>(&operations[i])) {
            const auto result = ApplyRevokePublication(*revoke, network_binding, params, candidate);
            if (result != RevokePublicationError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_REVOKE_PUBLICATION);
                failure.failed_operation_index = i;
                failure.revoke_error = result;
                return failure;
            }
        } else if (const auto* adjustment = std::get_if<PoaAuthAdjustment>(&operations[i])) {
            const auto digest = ComputePoaAuthAdjustmentDigest(network_binding, *adjustment);
            auto result = !poa_key ? PoaAuthAdjustmentError::INVALID_SIGNATURE
                : !digest || !adjustment_digests.insert(*digest).second ? PoaAuthAdjustmentError::INVALID_PAYLOAD
                : ApplyPoaAuthAdjustment(*adjustment, network_binding, block_height, *poa_key, candidate);
            if (result != PoaAuthAdjustmentError::NONE) {
                auto failure = fail(BlockExecutionError::INVALID_POA_AUTH_ADJUSTMENT);
                failure.failed_operation_index = i;
                failure.poa_auth_error = result;
                return failure;
            }
        }
        if (publication_op) {
            const auto id = ComputeOperationId(operations[i]);
            if (!id || !RecordPublication(candidate, *id, publication_op->authorization.account_id,
                    publication_op->publication.chunk_authorization_root, publication_op->publication.chunk_count,
                    block_height)) {
                return fail(BlockExecutionError::INVALID_STATE);
            }
        }
        if (metered) {
            auto& usage = candidate.usage[*metered];
            usage.epoch = epoch;
            ++usage.epoch_operations;
            usage.block_height = block_height;
            ++usage.block_operations;
        }
        // Finalized execution only: only operations that confer network utility
        // earn AUTH (RootPublication, SystemLock). Identity maintenance (IdentityRotate,
        // NameCommit/Reveal) and payments earn no AUTH to prevent Sybil/ping-pong farming.
        // Furthermore, an account may earn at most AUTH_PER_FINALIZED_OPERATION per block
        // to enforce a strict velocity limit.
        if (const auto actor = AuthorityEarningAccount(operations[i])) {
            if (auth_credited_accounts.insert(*actor).second) {
                const auto account = candidate.accounts.find(*actor);
                if (account == candidate.accounts.end()) return fail(BlockExecutionError::INVALID_STATE);
                auto& authority = account->second.authority;
                authority = authority > std::numeric_limits<uint64_t>::max() - AUTH_PER_FINALIZED_OPERATION
                    ? std::numeric_limits<uint64_t>::max() : authority + AUTH_PER_FINALIZED_OPERATION;
            }
        }
    }
    const uint64_t final_supply = TotalSupply(candidate);
    // Любой change total supply означает консенсусную ошибку: комиссии лишь
    // перераспределяют CYBOU между canonical account values, а AUTH живёт отдельно.
    if (final_supply != initial_supply) return fail(BlockExecutionError::SUPPLY_CHANGED);
    if (ValidateCybouState(candidate) != StateValidationError::NONE) return fail(BlockExecutionError::INVALID_STATE);
    const auto root = CybouStateHash(candidate);
    if (!root) return fail(BlockExecutionError::INVALID_STATE);
    BlockExecutionResult success{};
    success.state = std::move(candidate);
    success.state_root = *root;
    return success;
}

} // namespace cybou
