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
    CybouStateStore& store, std::array<unsigned char, 32> validator_private_key,
    std::optional<std::filesystem::path> signing_journal)
    : m_store{store}, m_validator_private_key{validator_private_key},
      m_signing_journal{std::move(signing_journal)}
{
}

CybouAuthorityNode::~CybouAuthorityNode()
{
    memory_cleanse(m_validator_private_key.data(), m_validator_private_key.size());
}

OperationSubmitStatus CybouAuthorityNode::SubmitOperationWithStatus(const ProtocolOperation& operation)
{
    if (std::find(m_pending.begin(), m_pending.end(), operation) != m_pending.end()) {
        return OperationSubmitStatus::ALREADY_PENDING;
    }
    if (std::holds_alternative<AccountCreateOp>(operation)) {
        const auto& create = std::get<AccountCreateOp>(operation);
        const auto loaded = m_store.LoadState();
        if (loaded && loaded.state && loaded.state->accounts.contains(create.account_id)) {
            const auto head = m_store.GetFinalizedHead();
            if (!head) return OperationSubmitStatus::REJECTED;
            // AccountCreate is unique per AccountID. Only the exact finalized
            // operation may be acknowledged as a successful retry.
            for (uint64_t height = head->height; height > 0; --height) {
                const auto finalized = m_store.GetBlockAtHeight(height);
                if (!finalized) return OperationSubmitStatus::REJECTED;
                for (const auto& prior : finalized->block.operations) {
                    if (const auto* prior_create = std::get_if<AccountCreateOp>(&prior);
                        prior_create && prior_create->account_id == create.account_id) {
                        return prior == operation ? OperationSubmitStatus::ALREADY_FINALIZED
                                                  : OperationSubmitStatus::REJECTED;
                    }
                }
            }
            return OperationSubmitStatus::REJECTED;
        }
    }
    if (m_pending.size() >= MAX_AUTHORITY_PENDING_OPERATIONS) return OperationSubmitStatus::REJECTED;
    const auto head = m_store.GetFinalizedHead();
    if (!head || head->height == std::numeric_limits<uint64_t>::max()) return OperationSubmitStatus::REJECTED;
    auto candidate = m_pending;
    candidate.push_back(operation);
    if (!m_store.ComputeCandidateStateRoot(candidate, head->height + 1)) return OperationSubmitStatus::REJECTED;
    m_pending.push_back(operation);
    return OperationSubmitStatus::ACCEPTED;
}

bool CybouAuthorityNode::SubmitOperation(const ProtocolOperation& operation)
{
    return SubmitOperationWithStatus(operation) == OperationSubmitStatus::ACCEPTED;
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
    const auto keypair = GenerateValidatorKeyPair(m_validator_private_key);
    if (!keypair || set->validators[0].consensus_public_key != keypair->public_key) {
        return Failure(AuthorityProductionError::VALIDATOR_KEY_MISMATCH);
    }
    const uint64_t height = head->height + 1;
    if (!m_store.ComputeCandidateStateRoot(m_pending, height)) {
        return Failure(AuthorityProductionError::INVALID_PENDING_OPERATIONS);
    }

    BftValidatorNode validator{
        0, m_validator_private_key, m_store.GetNetworkId(), *set,
        [this](const std::vector<ProtocolOperation>& operations, const uint64_t candidate_height) {
            return m_store.ComputeCandidateStateRoot(operations, candidate_height);
        }, m_signing_journal,
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
    const auto serialized = SerializeFinalizedBlock(*finalized);
    if (!serialized || serialized->size() > MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES) {
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
