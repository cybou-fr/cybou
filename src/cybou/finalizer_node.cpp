// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/finalizer_node.h>

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

CybouFinalizerNode::CybouFinalizerNode(
    CybouStateStore& store, const RecoveryEntropy& poa_recovery_entropy)
    : m_store{store},
      m_finalizer{std::make_unique<PoaFinalizer>(store.GetDatabase(), store.GetNetworkId(),
          store.GetNetworkDefinition().genesis_block_id, poa_recovery_entropy,
          store.GetNetworkDefinition().poa_finalizer_public_key)},
      m_pool{store}
{
}

CybouFinalizerNode::~CybouFinalizerNode() = default;

OperationSubmitStatus CybouFinalizerNode::SubmitOperationWithStatus(
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

bool CybouFinalizerNode::SubmitOperation(const ProtocolOperation& operation)
{
    return SubmitOperationWithStatus(operation) == OperationSubmitStatus::ACCEPTED;
}

BlockProductionResult CybouFinalizerNode::ProduceNextBlock(const bool sync)
{
    if (SafetyHalted()) return Failure(BlockProductionError::POA_SAFETY_HALTED);
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
    m_pool.Revalidate();
    return BlockProductionResult{.finalized_block = std::move(finalized)};
}

bool CybouFinalizerNode::SafetyHalted() const
{
    return m_store.PoaSafetyHalted() || m_finalizer->SafetyHalted();
}

} // namespace cybou
