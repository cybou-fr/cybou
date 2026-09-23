// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/authority_node.h>

#include <cybou/bft_engine.h>
#include <cybou/signing.h>
#include <support/cleanse.h>

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
    CybouStateStore& store, std::array<unsigned char, 32> validator_private_key)
    : m_store{store}, m_validator_private_key{validator_private_key}
{
}

CybouAuthorityNode::~CybouAuthorityNode()
{
    memory_cleanse(m_validator_private_key.data(), m_validator_private_key.size());
}

bool CybouAuthorityNode::SubmitOperation(const ProtocolOperationV1& operation)
{
    if (m_pending.size() >= MAX_AUTHORITY_PENDING_OPERATIONS) return false;
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max()) return false;
    auto candidate = m_pending;
    candidate.push_back(operation);
    if (!m_store.ComputeCandidateStateRoot(candidate, head->height + 1)) return false;
    m_pending.push_back(operation);
    return true;
}

AuthorityProductionResult CybouAuthorityNode::ProduceNextBlock(const bool sync)
{
    const auto head = m_store.GetFinalizedHead();
    const auto set = m_store.GetValidatorSet();
    if (!head || !set || head->height == std::numeric_limits<uint64_t>::max()) {
        return Failure(AuthorityProductionError::STATE_UNAVAILABLE);
    }
    if (set->validators.size() != 1) {
        return Failure(AuthorityProductionError::NOT_AUTHORITY_MODE);
    }
    const auto public_key = DeriveEd25519PublicKey(m_validator_private_key);
    if (!public_key || set->validators[0].consensus_public_key != *public_key) {
        return Failure(AuthorityProductionError::VALIDATOR_KEY_MISMATCH);
    }
    const uint64_t height = head->height + 1;
    if (!m_store.ComputeCandidateStateRoot(m_pending, height)) {
        return Failure(AuthorityProductionError::INVALID_PENDING_OPERATIONS);
    }

    BftValidatorNode validator{
        0, m_validator_private_key, m_store.GetNetworkId(), *set,
        [this](const std::vector<ProtocolOperationV1>& operations, const uint64_t candidate_height) {
            return m_store.ComputeCandidateStateRoot(operations, candidate_height);
        },
    };
    validator.SetHeight(height, head->block_id, *set);
    const auto proposal = validator.StartRound(0, m_pending);
    if (!proposal) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    const auto prevote = validator.ReceiveProposal(*proposal);
    if (!prevote || !prevote->block_id) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    const auto precommit = validator.ReceivePrevote(*prevote);
    if (!precommit || !precommit->block_id || !validator.ReceivePrecommit(*precommit)) {
        return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    }
    const auto& finalized = validator.GetLatestFinalizedBlock();
    if (!finalized) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    if (SerializeFinalizedBlock(*finalized).size() > MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES) {
        return Failure(AuthorityProductionError::BLOCK_TOO_LARGE);
    }

    const auto committed = m_store.CommitFinalizedBlock(*finalized, *set, sync);
    if (!committed) {
        auto failure = Failure(AuthorityProductionError::COMMIT_FAILED);
        failure.commit_result = committed;
        return failure;
    }
    auto result = AuthorityProductionResult{.finalized_block = *finalized};
    m_pending.clear();
    return result;
}

} // namespace cybou
