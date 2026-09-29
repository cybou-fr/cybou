// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>

#include <limits>
#include <utility>

namespace cybou {
namespace {

AuthorityProductionResult Failure(const AuthorityProductionError error)
{
    AuthorityProductionResult result;
    result.error = error;
    return result;
}

} // namespace

CybouAuthorityNode::CybouAuthorityNode(
    CybouStateStore& store, const RecoveryEntropy& poa_recovery_entropy)
    : m_store{store},
      m_finalizer{std::make_unique<PoaFinalizer>(store.GetDatabase(), store.GetNetworkId(),
          store.GetNetworkDefinition().genesis_block_id, poa_recovery_entropy,
          store.GetNetworkDefinition().poa_finalizer_public_key)},
      m_pool{store}
{
}

CybouAuthorityNode::~CybouAuthorityNode() = default;

OperationSubmitStatus CybouAuthorityNode::SubmitOperationWithStatus(
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

bool CybouAuthorityNode::SubmitOperation(const ProtocolOperation& operation)
{
    return SubmitOperationWithStatus(operation) == OperationSubmitStatus::ACCEPTED;
}

AuthorityProductionResult CybouAuthorityNode::ProduceNextBlock(const bool sync)
{
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max()) {
        return Failure(AuthorityProductionError::STATE_UNAVAILABLE);
    }

    const uint64_t height = head->height + 1;
    const auto operations = m_pool.Snapshot();
    const auto state_root = m_store.ComputeCandidateStateRoot(operations, height);
    if (!state_root) return Failure(AuthorityProductionError::INVALID_PENDING_OPERATIONS);

    CybouBlock block{
        .parent_block_id = head->block_id,
        .height = height,
        .operations = operations,
        .resulting_state_root = *state_root,
    };
    const auto signing = m_finalizer->SignFinality(head->height, head->block_id, block);
    if (!signing.certificate) return Failure(AuthorityProductionError::POA_SIGNING_FAILED);

    FinalizedBlock finalized{
        .block = std::move(block),
        .certificate = *signing.certificate,
    };
    const auto serialized = SerializeFinalizedBlock(finalized);
    if (!serialized || serialized->size() > MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES) {
        return Failure(AuthorityProductionError::BLOCK_TOO_LARGE);
    }

    const auto committed = m_store.CommitFinalizedBlock(finalized, sync);
    if (!committed) {
        auto failure = Failure(AuthorityProductionError::COMMIT_FAILED);
        failure.commit_result = committed;
        return failure;
    }
    m_pool.Revalidate();
    return AuthorityProductionResult{.finalized_block = std::move(finalized)};
}

} // namespace cybou
