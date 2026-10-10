// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
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
    const IdentityHybridPublicKey* poa_key,
    const cybou::Hash256& verified_parent_id)
    : m_parent{&parent},
      m_candidate{parent},
      m_network_binding{network_binding},
      m_block_height{block_height},
      m_verified_parent_id{verified_parent_id},
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

    const auto* publication_op = std::get_if<AuthorizedRootPublication>(&operation);

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
    } else if (const auto* lease = std::get_if<AuthorizedStorageLease>(&operation)) {
        const auto result = ApplyStorageLease(*lease, m_network_binding, m_params, m_candidate);
        if (result != StorageLeaseError::NONE) {
            auto failure = fail(BlockExecutionError::INVALID_STORAGE_LEASE);
            failure.lease_error = result;
            return failure;
        }
    } else if (const auto* settlement = std::get_if<StorageSettlement>(&operation)) {
        const auto result = !m_poa_key ? StorageSettlementError::INVALID_SIGNATURE
            : ApplyStorageSettlement(*settlement, m_network_binding, m_params, *m_poa_key, m_candidate, m_block_height, m_verified_parent_id);
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

BlockExecutionResult BlockExecutor::Finalize() const &
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

BlockExecutionResult BlockExecutor::Finalize() &&
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
    success.state = std::move(m_candidate);
    success.state_root = *root;
    return success;
}

std::optional<cybou::Hash256> BlockExecutor::StorageAssignmentSeed(const uint64_t preparation_height) const noexcept
{
    if (!m_valid || m_verified_parent_id.IsNull() ||
        preparation_height > std::numeric_limits<uint64_t>::max() - 2 ||
        m_block_height != preparation_height + 2) return std::nullopt;
    return m_verified_parent_id;
}

BlockExecutionResult ExecuteBlockOperations(const CybouState& parent,
    const std::vector<ProtocolOperation>& operations,
    const cybou::Hash256& network_binding, uint64_t block_height,
    const CybouProtocolParameters& params,
    const IdentityHybridPublicKey* poa_key,
    const cybou::Hash256& verified_parent_id)
{
    const auto creates = std::count_if(operations.begin(), operations.end(), [](const auto& operation) {
        return std::holds_alternative<AccountCreateOp>(operation);
    });
    if (creates > params.max_account_creates_per_block) {
        BlockExecutionResult result{};
        result.error = BlockExecutionError::TOO_MANY_ACCOUNT_CREATES;
        return result;
    }

    BlockExecutor executor(parent, network_binding, block_height, params, poa_key, verified_parent_id);
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
    return std::move(executor).Finalize();
}

} // namespace cybou
