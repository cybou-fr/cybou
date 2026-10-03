// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/poa_finalizer.h>

#include <limits>
#include <utility>

namespace cybou {
namespace {

BlockProductionResult Failure(const BlockProductionError error)
{
    BlockProductionResult result;
    result.error = error;
    return result;
}

} // namespace

PoaFinalizer::PoaFinalizer(
    CybouStateStore& store, OperationPool& pool)
    : m_store{store},
      m_finalizer{std::make_unique<PoaSigningService>(store.GetDatabase(), store.GetNetworkBinding(),
          store.GetNetworkDefinition().genesis_block_id, store.GetNetworkDefinition().poa_finalizer_public_key)},
      m_pool{pool}
{
}

PoaFinalizer::PoaFinalizer(
    CybouStateStore& store, OperationPool& pool, const RecoveryEntropy& poa_recovery_entropy)
    : m_store{store},
      m_finalizer{std::make_unique<PoaSigningService>(store.GetDatabase(), store.GetNetworkBinding(),
          store.GetNetworkDefinition().genesis_block_id, poa_recovery_entropy,
          store.GetNetworkDefinition().poa_finalizer_public_key)},
      m_pool{pool}
{
}

PoaFinalizer::~PoaFinalizer() = default;

OperationSubmitStatus PoaFinalizer::SubmitOperationWithStatus(
    const ProtocolOperation& operation, std::optional<std::string> source_peer)
{
    switch (m_pool.Admit(operation, std::move(source_peer))) {
    case PoolAdmission::ACCEPTED: return OperationSubmitStatus::ACCEPTED;
    case PoolAdmission::ALREADY_PENDING: return OperationSubmitStatus::ALREADY_PENDING;
    case PoolAdmission::ALREADY_FINALIZED: return OperationSubmitStatus::ALREADY_FINALIZED;
    case PoolAdmission::REJECTED: return OperationSubmitStatus::REJECTED;
    }
    return OperationSubmitStatus::REJECTED;
}

OperationSubmitResult PoaFinalizer::SubmitAuthAdjustment(
    const PoaAuthAction action, const AccountId& target, const uint64_t amount)
{
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max() || SafetyHalted()) return {};
    PoaAuthAdjustment adjustment{
        .action = action,
        .target_account_id = target,
        .amount = amount,
        .block_height = head->height + 1,
    };
    if (!m_finalizer->SignAuthAdjustment(adjustment)) return {};
    const ProtocolOperation operation{std::move(adjustment)};
    return {
        .status = SubmitOperationWithStatus(operation),
        .op_id = ComputeOperationId(operation).value_or(uint256{}),
    };
}

bool PoaFinalizer::SubmitOperation(const ProtocolOperation& operation)
{
    return SubmitOperationWithStatus(operation) == OperationSubmitStatus::ACCEPTED;
}

BlockProductionResult PoaFinalizer::ProduceNextBlock(const bool sync)
{
    if (SafetyHalted()) return Failure(BlockProductionError::POA_SAFETY_HALTED);
    if (!SignerEnabled()) return Failure(BlockProductionError::POA_SIGNING_FAILED);
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max()) {
        return Failure(BlockProductionError::STATE_UNAVAILABLE);
    }

    const uint64_t height = head->height + 1;
    const auto operations = m_pool.Snapshot();
    const auto state_root = m_store.ComputeCandidateStateRoot(operations, height);
    if (!state_root) return Failure(BlockProductionError::INVALID_PENDING_OPERATIONS);

    CybouBlock block{
        .parent_block_id = head->block_id,
        .height = height,
        .operations = operations,
        .resulting_state_root = *state_root,
    };
    const auto signing = m_finalizer->SignFinality(head->height, head->block_id, block);
    if (!signing.certificate) return Failure(BlockProductionError::POA_SIGNING_FAILED);

    FinalizedBlock finalized{
        .block = std::move(block),
        .certificate = *signing.certificate,
    };
    const auto serialized = SerializeFinalizedBlock(finalized);
    if (!serialized || serialized->size() > MAX_FINALIZER_SERIALIZED_BLOCK_BYTES) {
        return Failure(BlockProductionError::BLOCK_TOO_LARGE);
    }

    const auto committed = m_store.CommitFinalizedBlock(finalized, sync);
    if (!committed) {
        auto failure = Failure(BlockProductionError::COMMIT_FAILED);
        failure.commit_result = committed;
        return failure;
    }
    return BlockProductionResult{.finalized_block = std::move(finalized)};
}

bool PoaFinalizer::SafetyHalted() const
{
    return m_store.PoaSafetyHalted() || m_finalizer->SafetyHalted();
}

bool PoaFinalizer::EnableSigner(PoaSignerRef signer)
{
    return m_finalizer->EnableSigner(std::move(signer));
}

void PoaFinalizer::DisableSigner()
{
    m_finalizer->DisableSigner();
}

bool PoaFinalizer::SignerEnabled() const
{
    return m_finalizer->SignerEnabled();
}

} // namespace cybou
