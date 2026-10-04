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

BlockExecutor::BlockExecutor(const CybouState& parent,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key)
    : m_parent{&parent},
      m_candidate{parent},
      m_network_binding{network_binding},
      m_block_height{block_height},
      m_epoch{EpochForHeight(block_height, params)},
      m_params{params},
      m_poa_key{poa_key}
{
    if (ValidateCybouState(parent) != StateValidationError::NONE) {
        m_init_error = BlockExecutionError::INVALID_STATE;
        return;
    }
    m_initial_supply = TotalCybou(parent);
    if (m_params.name_commit_max_lifetime > 0) {
        // Истечение pending commit-ов является частью детерминированного block execution:
        // одинаковая высота должна давать одинаковый реестр имён даже при пустом блоке.
        std::erase_if(m_candidate.names.pending_commits, [&](const auto& item) {
            return m_block_height > item.second.commit_height &&
                m_block_height - item.second.commit_height > m_params.name_commit_max_lifetime;
        });
    }
    // Окна лимитов (DEC-272): счётчики другого блока или другой эпохи обнуляются до
    // исполнения, пустые записи удаляются. Идемпотентно для повторного исполнения той же высоты.
    for (auto it = m_candidate.usage.begin(); it != m_candidate.usage.end();) {
        auto& usage = it->second;
        if (usage.epoch != m_epoch) usage.epoch_operations = 0;
        if (usage.epoch_operations == 0) usage.epoch = 0;
        if (usage.block_height != m_block_height) usage.block_operations = 0;
        if (usage.block_operations == 0) usage.block_height = 0;
        it = usage.Empty() ? m_candidate.usage.erase(it) : std::next(it);
    }
    m_valid = true;
}

BlockExecutionResult BlockExecutor::ApplyOperation(const ProtocolOperation& operation)
{
    const auto fail = [this](BlockExecutionError error) {
        m_valid = false;
        BlockExecutionResult result{};
        result.error = error;
        return result;
    };
    if (!m_valid) return fail(m_init_error);

    // Лимиты уровня считаются по AUTH финализированного родителя: одно и то же
    // решение для пула, Validation и PoA. AccountCreate и PoaAuthAdjustment не метрируются.
    // Хранение не квотируется AUTH (DEC-274): оно оплачивается арендой.
    const auto metered = AuthorizingAccount(operation);
    const auto* publication_op = std::get_if<AuthorizedRootPublication>(&operation);
    if (metered) {
        const auto parent_account = m_parent->accounts.find(*metered);
        const auto limits = ComputeAuthorityTierLimits(
            parent_account == m_parent->accounts.end() ? 0 : parent_account->second.authority);
        const auto found = m_candidate.usage.find(*metered);
        const AccountUsage usage = found == m_candidate.usage.end() ? AccountUsage{} : found->second;
        if (usage.block_operations >= limits.operations_per_block ||
            usage.epoch_operations >= limits.operations_per_epoch) {
            return fail(BlockExecutionError::OPERATION_LIMIT_EXCEEDED);
        }
    }

    std::optional<std::array<unsigned char, 32>> adjustment_digest;

    if (const auto* create = std::get_if<AccountCreateOp>(&operation)) {
        if (m_account_creates >= m_params.max_account_creates_per_block) {
            return fail(BlockExecutionError::TOO_MANY_ACCOUNT_CREATES);
        }
        const auto result = ApplyAccountCreate(*create, m_network_binding, m_block_height, m_params, m_candidate);
        if (result != AccountCreateStateError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_ACCOUNT_CREATE);
            failure.create_error = result;
            return failure;
        }
    } else if (const auto* payment = std::get_if<AuthorizedPayment>(&operation)) {
        const auto result = ApplyPayment(*payment, m_network_binding, m_params, m_candidate);
        if (result != PaymentError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_PAYMENT);
            failure.payment_error = result;
            return failure;
        }
    } else if (const auto* rotate = std::get_if<IdentityRotate>(&operation)) {
        const auto result = m_candidate.identities.RotateIdentity(*rotate, m_network_binding);
        if (result != IdentityRegistryError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_IDENTITY_ROTATE);
            failure.identity_error = result;
            return failure;
        }
    } else if (const auto* lock = std::get_if<AuthorizedSystemLock>(&operation)) {
        const auto result = ApplySystemLock(*lock, m_network_binding, m_candidate);
        if (result != SystemLockError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_SYSTEM_LOCK);
            failure.lock_error = result;
            return failure;
        }
    } else if (const auto* commit = std::get_if<AuthorizedNameCommit>(&operation)) {
        const auto result = ApplyNameCommit(*commit, m_network_binding, m_block_height, m_params, m_candidate);
        if (result != NameCommitError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_NAME_COMMIT);
            failure.name_commit_error = result;
            return failure;
        }
    } else if (const auto* reveal = std::get_if<AuthorizedNameReveal>(&operation)) {
        const auto result = ApplyNameReveal(*reveal, m_network_binding, m_block_height, m_params, m_candidate);
        if (result != NameRevealError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_NAME_REVEAL);
            failure.name_reveal_error = result;
            return failure;
        }
    } else if (const auto* publication = std::get_if<AuthorizedRootPublication>(&operation)) {
        const auto result = ApplyRootPublication(*publication, m_network_binding, m_params, m_candidate);
        if (result != RootPublicationError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_ROOT_PUBLICATION);
            failure.root_publication_error = result;
            return failure;
        }
    } else if (const auto* revoke = std::get_if<AuthorizedRevokePublication>(&operation)) {
        const auto result = ApplyRevokePublication(*revoke, m_network_binding, m_params, m_candidate);
        if (result != RevokePublicationError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_REVOKE_PUBLICATION);
            failure.revoke_error = result;
            return failure;
        }
    } else if (const auto* adjustment = std::get_if<PoaAuthAdjustment>(&operation)) {
        const auto digest = ComputePoaAuthAdjustmentDigest(m_network_binding, *adjustment);
        auto result = !m_poa_key ? PoaAuthAdjustmentError::INVALID_SIGNATURE
            : !digest || m_adjustment_digests.contains(*digest) ? PoaAuthAdjustmentError::INVALID_PAYLOAD
            : ApplyPoaAuthAdjustment(*adjustment, m_network_binding, m_block_height, *m_poa_key, m_candidate);
        if (result != PoaAuthAdjustmentError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_POA_AUTH_ADJUSTMENT);
            failure.poa_auth_error = result;
            return failure;
        }
        adjustment_digest = digest;
    } else if (const auto* lease = std::get_if<AuthorizedStorageLease>(&operation)) {
        const auto result = ApplyStorageLease(*lease, m_network_binding, m_params, m_candidate);
        if (result != StorageLeaseError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_STORAGE_LEASE);
            failure.lease_error = result;
            return failure;
        }
    } else if (const auto* settlement = std::get_if<StorageSettlement>(&operation)) {
        const auto result = !m_poa_key ? StorageSettlementError::INVALID_SIGNATURE
            : ApplyStorageSettlement(*settlement, m_network_binding, m_params, *m_poa_key, m_candidate);
        if (result != StorageSettlementError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_STORAGE_SETTLEMENT);
            failure.settlement_error = result;
            return failure;
        }
    }

    if (publication_op) {
        const auto id = ComputeOperationId(operation);
        if (!id || !RecordPublication(m_candidate, *id, publication_op->authorization.account_id,
                publication_op->publication.chunk_authorization_root, publication_op->publication.chunk_count,
                m_block_height)) {
            return fail(BlockExecutionError::INVALID_STATE);
        }
    }
    if (metered) {
        auto& usage = m_candidate.usage[*metered];
        usage.epoch = m_epoch;
        ++usage.epoch_operations;
        usage.block_height = m_block_height;
        ++usage.block_operations;
    }
    bool auth_credited{false};
    std::optional<AccountId> credited_actor;
    if (const auto actor = AuthorityEarningAccount(operation)) {
        if (!m_auth_credited_accounts.contains(*actor)) {
            const auto account = m_candidate.accounts.find(*actor);
            if (account == m_candidate.accounts.end()) return fail(BlockExecutionError::INVALID_STATE);
            auto& authority = account->second.authority;
            authority = authority > std::numeric_limits<uint64_t>::max() - AUTH_PER_FINALIZED_OPERATION
                ? std::numeric_limits<uint64_t>::max() : authority + AUTH_PER_FINALIZED_OPERATION;
            auth_credited = true;
            credited_actor = actor;
        }
    }

    if (adjustment_digest) m_adjustment_digests.insert(*adjustment_digest);
    if (auth_credited && credited_actor) m_auth_credited_accounts.insert(*credited_actor);
    if (std::holds_alternative<AccountCreateOp>(operation)) {
        ++m_account_creates;
    }

    return BlockExecutionResult{};
}

bool BlockExecutor::CanFinalize() const
{
    if (!m_valid) return false;
    uint64_t final_supply{0};
    if (ValidateCybouState(m_candidate, &final_supply) != StateValidationError::NONE) {
        return false;
    }
    return final_supply == m_initial_supply;
}

BlockExecutionResult BlockExecutor::Finalize() const
{
    const auto fail = [](BlockExecutionError error) {
        BlockExecutionResult result{};
        result.error = error;
        return result;
    };
    if (!m_valid) return fail(m_init_error);
    uint64_t final_supply{0};
    if (ValidateCybouState(m_candidate, &final_supply) != StateValidationError::NONE) {
        return fail(BlockExecutionError::INVALID_STATE);
    }
    if (final_supply != m_initial_supply) {
        return fail(BlockExecutionError::SUPPLY_CHANGED);
    }
    const auto root = CybouStateHash(m_candidate, /*validate=*/false);
    if (!root) return fail(BlockExecutionError::INVALID_STATE);
    BlockExecutionResult success{};
    success.state = m_candidate;
    success.state_root = *root;
    return success;
}

BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key)
{
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOp>(operation);
    });
    if (creates > params.max_account_creates_per_block) {
        BlockExecutionResult result{};
        result.error = BlockExecutionError::TOO_MANY_ACCOUNT_CREATES;
        return result;
    }

    BlockExecutor executor(parent, network_binding, block_height, params, poa_key);
    if (!executor.IsValid()) {
        BlockExecutionResult result{};
        result.error = executor.InitError();
        return result;
    }

    for (size_t i{0}; i < operations.size(); ++i) {
        auto result = executor.ApplyOperation(operations[i]);
        if (!result.IsOk()) {
            result.failed_operation_index = i;
            return result;
        }
    }
    return executor.Finalize();
}

} // namespace cybou
