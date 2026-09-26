// Copyright (c) 2026 Stanislav SAVELIEV
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
      m_signing_journal{std::move(signing_journal)}, m_pool{store}
{
}

CybouAuthorityNode::~CybouAuthorityNode()
{
    memory_cleanse(m_validator_private_key.data(), m_validator_private_key.size());
}

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

bool CybouAuthorityNode::EnsureValidator()
{
    const auto head = m_store.GetFinalizedHead();
    const auto set = m_store.GetValidatorSet();
    if (!head || !set || head->height == std::numeric_limits<uint64_t>::max()) return false;
    const uint64_t target_height = head->height + 1;
    const auto keypair = GenerateValidatorKeyPair(m_validator_private_key);
    if (!keypair) return false;
    std::optional<size_t> local_index;
    for (size_t i = 0; i < set->validators.size(); ++i) {
        if (set->validators[i].consensus_public_key == keypair->public_key) {
            local_index = i;
            break;
        }
    }
    if (!local_index) return false;

    if (!m_validator || m_validator->GetHeight() != target_height) {
        if (!m_validator) {
            m_validator = std::make_unique<BftValidatorNode>(
                *local_index, m_validator_private_key, m_store.GetNetworkId(), *set,
                [this](const std::vector<ProtocolOperation>& ops, uint64_t candidate_height) {
                    return m_store.ComputeCandidateStateRoot(ops, candidate_height);
                }, m_signing_journal);
        }
        m_validator->SetHeight(target_height, head->block_id, *set);
    }
    return true;
}

AuthorityProductionResult CybouAuthorityNode::ProduceNextBlock(const bool sync)
{
    const auto head = m_store.GetFinalizedHead();
    const auto set = m_store.GetValidatorSet();
    if (!head || !set || head->height == std::numeric_limits<uint64_t>::max()) {
        return Failure(AuthorityProductionError::STATE_UNAVAILABLE);
    }
    const auto keypair = GenerateValidatorKeyPair(m_validator_private_key);
    if (!keypair) {
        return Failure(AuthorityProductionError::VALIDATOR_KEY_MISMATCH);
    }
    std::optional<size_t> local_node_index;
    for (size_t i = 0; i < set->validators.size(); ++i) {
        if (set->validators[i].consensus_public_key == keypair->public_key) {
            local_node_index = i;
            break;
        }
    }
    if (!local_node_index) {
        return Failure(AuthorityProductionError::VALIDATOR_KEY_MISMATCH);
    }
    if (!EnsureValidator()) {
        return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    }

    const uint64_t height = head->height + 1;
    const auto pending = m_pool.Snapshot();
    if (!m_store.ComputeCandidateStateRoot(pending, height)) {
        return Failure(AuthorityProductionError::INVALID_PENDING_OPERATIONS);
    }

    if (set->validators.size() == 1) {
        const auto proposal = m_validator->StartRound(0, pending);
        if (!proposal) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
        const auto proposal_result = m_validator->ReceiveProposal(*proposal);
        if (!proposal_result || !proposal_result->block_id) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
        auto precommit = proposal_result.precommit;
        if (!precommit && !proposal_result.finalized) {
            precommit = m_validator->ReceivePrevote(*proposal_result);
        }
        if (precommit && precommit->block_id) {
            m_validator->ReceivePrecommit(*precommit);
        }
        const auto& finalized = m_validator->GetLatestFinalizedBlock();
        if (!finalized || !precommit || !precommit->block_id) {
            return Failure(AuthorityProductionError::CONSENSUS_FAILED);
        }
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
        m_pool.Revalidate();
        return result;
    }

    const size_t leader = BftLeaderIndex(height, 0, set->validators.size());
    if (*local_node_index != leader) {
        if (const auto& finalized = m_validator->GetLatestFinalizedBlock()) {
            return AuthorityProductionResult{.finalized_block = *finalized};
        }
        return Failure(AuthorityProductionError::NONE);
    }

    const auto proposal = m_validator->StartRound(0, pending);
    if (!proposal) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    const auto proposal_result = m_validator->ReceiveProposal(*proposal);
    if (!proposal_result || !proposal_result->block_id) return Failure(AuthorityProductionError::CONSENSUS_FAILED);
    auto precommit = proposal_result.precommit;
    if (!precommit && !proposal_result.finalized) {
        precommit = m_validator->ReceivePrevote(*proposal_result);
    }
    if (precommit && precommit->block_id) {
        m_validator->ReceivePrecommit(*precommit);
    }
    if (const auto& finalized = m_validator->GetLatestFinalizedBlock()) {
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
        m_pool.Revalidate();
        return AuthorityProductionResult{.finalized_block = *finalized};
    }
    return Failure(AuthorityProductionError::NONE);
}

std::optional<BftProposalMsg> CybouAuthorityNode::StartConsensusRound(uint32_t round)
{
    if (!EnsureValidator()) return std::nullopt;
    const auto pending = m_pool.Snapshot();
    const uint64_t height = m_validator->GetHeight();
    if (!m_store.ComputeCandidateStateRoot(pending, height)) return std::nullopt;
    return m_validator->StartRound(round, pending);
}

BftProposalResult CybouAuthorityNode::ReceiveProposal(const BftProposalMsg& proposal)
{
    if (!EnsureValidator()) return {};
    if (proposal.height != m_validator->GetHeight()) return {};
    return m_validator->ReceiveProposal(proposal);
}

std::optional<BftPrecommitMsg> CybouAuthorityNode::ReceivePrevote(const BftPrevoteMsg& prevote)
{
    if (!EnsureValidator()) return std::nullopt;
    if (prevote.height != m_validator->GetHeight()) return std::nullopt;
    return m_validator->ReceivePrevote(prevote);
}

std::optional<FinalizedBlock> CybouAuthorityNode::ReceivePrecommit(const BftPrecommitMsg& precommit)
{
    if (!EnsureValidator()) return std::nullopt;
    if (precommit.height != m_validator->GetHeight()) return std::nullopt;
    if (!m_validator->ReceivePrecommit(precommit)) return std::nullopt;
    return m_validator->GetLatestFinalizedBlock();
}

std::optional<BftPrevoteMsg> CybouAuthorityNode::OnProposalTimeout()
{
    if (!EnsureValidator()) return std::nullopt;
    return m_validator->OnProposalTimeout();
}

std::optional<BftPrecommitMsg> CybouAuthorityNode::OnPrevoteTimeout()
{
    if (!EnsureValidator()) return std::nullopt;
    return m_validator->OnPrevoteTimeout();
}

const std::optional<FinalizedBlock>& CybouAuthorityNode::GetLatestFinalizedBlock() const
{
    static const std::optional<FinalizedBlock> none{std::nullopt};
    return m_validator ? m_validator->GetLatestFinalizedBlock() : none;
}

std::optional<size_t> CybouAuthorityNode::GetValidatorIndex() const
{
    return m_validator ? std::optional<size_t>{m_validator->GetNodeIndex()} : std::nullopt;
}

std::optional<ConsensusProgress> CybouAuthorityNode::GetConsensusProgress()
{
    if (!EnsureValidator()) return std::nullopt;
    return ConsensusProgress{
        .height = m_validator->GetHeight(),
        .round = m_validator->GetRound(),
        .step = m_validator->GetStep(),
        .locked_round = m_validator->GetLockedRound(),
    };
}

} // namespace cybou
