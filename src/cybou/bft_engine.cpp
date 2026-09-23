// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/bft_engine.h>

#include <crypto/sha256.h>

#include <algorithm>
#include <string>
#include <string_view>

namespace cybou {

namespace {

inline void AppendUint64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

} // namespace

uint256 ComputeProposalDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& proposer_id,
    const uint256& block_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_PROPOSAL/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());

    unsigned char h_bytes[8];
    for (int i = 0; i < 8; ++i) h_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    hasher.Write(h_bytes, sizeof(h_bytes));

    unsigned char r_bytes[4];
    for (int i = 0; i < 4; ++i) r_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    hasher.Write(r_bytes, sizeof(r_bytes));

    hasher.Write(proposer_id.begin(), proposer_id.size());
    hasher.Write(block_id.begin(), block_id.size());

    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

uint256 ComputePrevoteDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_id,
    const std::optional<uint256>& block_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_PREVOTE/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());

    unsigned char h_bytes[8];
    for (int i = 0; i < 8; ++i) h_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    hasher.Write(h_bytes, sizeof(h_bytes));

    unsigned char r_bytes[4];
    for (int i = 0; i < 4; ++i) r_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    hasher.Write(r_bytes, sizeof(r_bytes));

    hasher.Write(validator_id.begin(), validator_id.size());

    const uint256 blk = block_id.value_or(uint256{});
    hasher.Write(blk.begin(), blk.size());

    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

uint256 ComputePrecommitNilDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_id)
{
    static constexpr std::string_view DOMAIN{"CYBOU/BFT_PRECOMMIT_NIL/V1"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(network_id.begin(), network_id.size());

    unsigned char h_bytes[8];
    for (int i = 0; i < 8; ++i) h_bytes[i] = static_cast<unsigned char>(height >> (8 * i));
    hasher.Write(h_bytes, sizeof(h_bytes));

    unsigned char r_bytes[4];
    for (int i = 0; i < 4; ++i) r_bytes[i] = static_cast<unsigned char>(round >> (8 * i));
    hasher.Write(r_bytes, sizeof(r_bytes));

    hasher.Write(validator_id.begin(), validator_id.size());

    uint256 digest;
    hasher.Finalize(digest.begin());
    return digest;
}

BftValidatorNode::BftValidatorNode(
    size_t node_index,
    std::array<unsigned char, 32> private_key_seed,
    uint256 network_id,
    ValidatorSetV1 validator_set)
    : m_node_index{node_index},
      m_private_key_seed{private_key_seed},
      m_network_id{network_id},
      m_validator_set{std::move(validator_set)},
      m_validator_set_commitment{ComputeValidatorSetCommitment(m_validator_set)}
{
    const auto pub = DeriveEd25519PublicKey(m_private_key_seed);
    if (pub) {
        m_validator_id = *pub;
    }
}

void BftValidatorNode::SetHeight(uint64_t height, const uint256& last_block_id)
{
    m_height = height;
    m_last_block_id = last_block_id;
    m_round = 0;
    m_step = BftStep::PROPOSE;
    m_locked_block.reset();
    m_locked_round = -1;
    m_current_proposal.reset();
    m_prevotes.clear();
    m_precommits.clear();
    m_prevoted = false;
    m_precommitted = false;
    m_finalized_block.reset();
}

std::optional<BftProposalMsg> BftValidatorNode::StartRound(
    uint32_t round,
    const std::vector<ProtocolOperationV1>& pending_ops,
    const uint256& resulting_state_root)
{
    m_round = round;
    m_step = BftStep::PROPOSE;
    m_current_proposal.reset();
    m_prevotes.clear();
    m_precommits.clear();
    m_prevoted = false;
    m_precommitted = false;
    m_finalized_block.reset();

    if (BftLeaderIndex(m_height, m_round) != m_node_index) {
        return std::nullopt;
    }

    CybouBlockV1 block;
    if (m_locked_block.has_value()) {
        block = *m_locked_block;
    } else {
        block = CybouBlockV1{
            .version = CYBOU_BLOCK_VERSION,
            .parent_block_id = m_last_block_id,
            .height = m_height,
            .operations = pending_ops,
            .resulting_state_root = resulting_state_root,
        };
    }

    const uint256 block_id = ComputeBlockId(block);
    const uint256 digest = ComputeProposalDigest(m_network_id, m_height, m_round, m_validator_id, block_id);
    const auto sig = SignValidatorVote(m_private_key_seed, digest);
    if (!sig) return std::nullopt;

    BftProposalMsg proposal{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .proposer_id = m_validator_id,
        .block = std::move(block),
        .signature = *sig,
    };

    m_current_proposal = proposal;
    return proposal;
}

std::optional<BftPrevoteMsg> BftValidatorNode::ReceiveProposal(const BftProposalMsg& proposal)
{
    if (m_prevoted) return std::nullopt;
    if (proposal.network_id != m_network_id || proposal.height != m_height || proposal.round != m_round) {
        return std::nullopt;
    }

    const size_t leader_idx = BftLeaderIndex(m_height, m_round);
    if (leader_idx >= m_validator_set.validators.size()) return std::nullopt;
    if (proposal.proposer_id != m_validator_set.validators[leader_idx].validator_id) {
        return std::nullopt;
    }

    const uint256 block_id = ComputeBlockId(proposal.block);
    const uint256 digest = ComputeProposalDigest(m_network_id, m_height, m_round, proposal.proposer_id, block_id);
    if (!VerifyValidatorSignature(proposal.proposer_id, proposal.signature, digest)) {
        return std::nullopt;
    }

    bool valid_block = (proposal.block.height == m_height && proposal.block.parent_block_id == m_last_block_id);
    if (m_locked_block.has_value()) {
        if (ComputeBlockId(*m_locked_block) != block_id) {
            valid_block = false;
        }
    }

    m_current_proposal = proposal;
    m_step = BftStep::PREVOTE;
    m_prevoted = true;

    std::optional<uint256> vote_block = valid_block ? std::optional<uint256>(block_id) : std::nullopt;
    const uint256 prevote_digest = ComputePrevoteDigest(m_network_id, m_height, m_round, m_validator_id, vote_block);
    const auto sig = SignValidatorVote(m_private_key_seed, prevote_digest);
    if (!sig) return std::nullopt;

    BftPrevoteMsg msg{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .validator_id = m_validator_id,
        .block_id = vote_block,
        .signature = *sig,
    };

    m_prevotes[m_validator_id] = msg;
    return msg;
}

std::optional<BftPrecommitMsg> BftValidatorNode::ReceivePrevote(const BftPrevoteMsg& prevote)
{
    if (prevote.network_id != m_network_id || prevote.height != m_height || prevote.round != m_round) {
        return std::nullopt;
    }

    const auto* val = m_validator_set.FindValidator(prevote.validator_id);
    if (!val) return std::nullopt;

    const uint256 digest = ComputePrevoteDigest(m_network_id, m_height, m_round, prevote.validator_id, prevote.block_id);
    if (!VerifyValidatorSignature(val->consensus_public_key, prevote.signature, digest)) {
        return std::nullopt;
    }

    m_prevotes[prevote.validator_id] = prevote;

    if (m_precommitted) return std::nullopt;

    std::map<uint256, size_t> block_counts;
    size_t nil_counts{0};
    for (const auto& [vid, pv] : m_prevotes) {
        if (pv.block_id.has_value()) {
            block_counts[*pv.block_id]++;
        } else {
            nil_counts++;
        }
    }

    const size_t quorum = m_validator_set.QuorumThreshold();

    for (const auto& [blk_id, count] : block_counts) {
        if (count >= quorum && m_current_proposal.has_value()) {
            m_locked_block = m_current_proposal->block;
            m_locked_round = static_cast<int32_t>(m_round);
            m_step = BftStep::PRECOMMIT;
            m_precommitted = true;

            const uint256 commit_digest = ComputeBftCommitDigest(
                m_network_id, blk_id, m_height, m_validator_set_commitment);
            const auto sig = SignValidatorVote(m_private_key_seed, commit_digest);
            if (!sig) return std::nullopt;

            BftPrecommitMsg msg{
                .network_id = m_network_id,
                .height = m_height,
                .round = m_round,
                .validator_id = m_validator_id,
                .block_id = blk_id,
                .signature = *sig,
            };
            m_precommits[m_validator_id] = msg;
            return msg;
        }
    }

    if (m_prevotes.size() >= quorum) {
        m_step = BftStep::PRECOMMIT;
        m_precommitted = true;

        const uint256 nil_digest = ComputePrecommitNilDigest(m_network_id, m_height, m_round, m_validator_id);
        const auto sig = SignValidatorVote(m_private_key_seed, nil_digest);
        if (!sig) return std::nullopt;

        BftPrecommitMsg msg{
            .network_id = m_network_id,
            .height = m_height,
            .round = m_round,
            .validator_id = m_validator_id,
            .block_id = std::nullopt,
            .signature = *sig,
        };
        m_precommits[m_validator_id] = msg;
        return msg;
    }

    return std::nullopt;
}

bool BftValidatorNode::ReceivePrecommit(const BftPrecommitMsg& precommit)
{
    if (precommit.network_id != m_network_id || precommit.height != m_height || precommit.round != m_round) {
        return false;
    }

    const auto* val = m_validator_set.FindValidator(precommit.validator_id);
    if (!val) return false;

    if (precommit.block_id.has_value()) {
        const uint256 commit_digest = ComputeBftCommitDigest(
            m_network_id, *precommit.block_id, m_height, m_validator_set_commitment);
        if (!VerifyValidatorSignature(val->consensus_public_key, precommit.signature, commit_digest)) {
            return false;
        }
    } else {
        const uint256 nil_digest = ComputePrecommitNilDigest(
            m_network_id, m_height, m_round, precommit.validator_id);
        if (!VerifyValidatorSignature(val->consensus_public_key, precommit.signature, nil_digest)) {
            return false;
        }
    }

    m_precommits[precommit.validator_id] = precommit;

    if (m_step == BftStep::FINALIZED) return true;

    std::map<uint256, std::vector<BftCommitVoteV1>> commit_votes_by_block;
    for (const auto& [vid, pc] : m_precommits) {
        if (pc.block_id.has_value()) {
            commit_votes_by_block[*pc.block_id].push_back(BftCommitVoteV1{
                .validator_id = vid,
                .signature = pc.signature,
            });
        }
    }

    const size_t quorum = m_validator_set.QuorumThreshold();
    for (auto& [blk_id, votes] : commit_votes_by_block) {
        if (votes.size() >= quorum && m_current_proposal.has_value()) {
            BftFinalityCertificateV1 cert{
                .version = BFT_FINALITY_CERTIFICATE_VERSION,
                .network_id = m_network_id,
                .block_id = blk_id,
                .height = m_height,
                .validator_set_commitment = m_validator_set_commitment,
                .commit_votes = std::move(votes),
            };

            if (VerifyFinalityCertificate(cert, m_validator_set, m_network_id) == FinalityVerificationError::NONE) {
                m_finalized_block = FinalizedBlockV1{
                    .block = m_current_proposal->block,
                    .certificate = std::move(cert),
                };
                m_step = BftStep::FINALIZED;
                m_locked_block.reset();
                m_locked_round = -1;
                return true;
            }
        }
    }

    return false;
}

std::optional<BftPrecommitMsg> BftValidatorNode::OnPrevoteTimeout()
{
    if (m_precommitted) return std::nullopt;
    m_step = BftStep::PRECOMMIT;
    m_precommitted = true;

    const uint256 nil_digest = ComputePrecommitNilDigest(m_network_id, m_height, m_round, m_validator_id);
    const auto sig = SignValidatorVote(m_private_key_seed, nil_digest);
    if (!sig) return std::nullopt;

    BftPrecommitMsg msg{
        .network_id = m_network_id,
        .height = m_height,
        .round = m_round,
        .validator_id = m_validator_id,
        .block_id = std::nullopt,
        .signature = *sig,
    };
    m_precommits[m_validator_id] = msg;
    return msg;
}

void BftValidatorNode::OnRoundTimeout()
{
    if (m_step != BftStep::FINALIZED) {
        m_round += 1;
        m_step = BftStep::PROPOSE;
        m_current_proposal.reset();
        m_prevotes.clear();
        m_precommits.clear();
        m_prevoted = false;
        m_precommitted = false;
    }
}

// ------------------------------------------------------------------------------------------------
// BftSimulator
// ------------------------------------------------------------------------------------------------

BftSimulator::BftSimulator(const uint256& network_id)
    : m_network_id{network_id}
{
    m_validator_set.version = 1;
    m_validator_set.validators.resize(BFT_STAGE1_VALIDATOR_COUNT);

    std::vector<std::array<unsigned char, 32>> seeds;
    for (size_t i = 0; i < BFT_STAGE1_VALIDATOR_COUNT; ++i) {
        std::string seed_str = "CYBOU_SIM_VALIDATOR_SEED_" + std::to_string(i);
        uint256 seed_hash;
        CSHA256().Write(reinterpret_cast<const unsigned char*>(seed_str.data()), seed_str.size()).Finalize(seed_hash.begin());

        std::array<unsigned char, 32> seed{};
        std::copy_n(seed_hash.begin(), 32, seed.begin());
        seeds.push_back(seed);

        const auto pub = DeriveEd25519PublicKey(seed);
        m_validator_set.validators[i] = ValidatorV1{
            .validator_id = *pub,
            .consensus_public_key = *pub,
            .weight = 1,
        };
    }

    for (size_t i = 0; i < BFT_STAGE1_VALIDATOR_COUNT; ++i) {
        m_nodes.push_back(std::make_unique<BftValidatorNode>(
            i, seeds[i], m_network_id, m_validator_set));
    }

    ClearPartition();
}

void BftSimulator::SetNodeOnline(size_t index, bool online)
{
    if (index < BFT_STAGE1_VALIDATOR_COUNT) {
        m_online[index] = online;
    }
}

void BftSimulator::SetPartition(const std::vector<size_t>& partition_a, const std::vector<size_t>& partition_b)
{
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            m_can_communicate[i][j] = false;
        }
    }
    for (size_t i : partition_a) {
        for (size_t j : partition_a) {
            if (i < 4 && j < 4) m_can_communicate[i][j] = true;
        }
    }
    for (size_t i : partition_b) {
        for (size_t j : partition_b) {
            if (i < 4 && j < 4) m_can_communicate[i][j] = true;
        }
    }
}

void BftSimulator::ClearPartition()
{
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            m_can_communicate[i][j] = true;
        }
    }
}

bool BftSimulator::StepRound(
    uint64_t height,
    uint32_t round,
    const std::vector<ProtocolOperationV1>& ops,
    const uint256& resulting_state_root)
{
    const size_t leader_idx = BftLeaderIndex(height, round);
    std::optional<BftProposalMsg> proposal;

    if (m_online[leader_idx]) {
        proposal = m_nodes[leader_idx]->StartRound(round, ops, resulting_state_root);
    }

    std::vector<BftPrevoteMsg> prevotes;
    for (size_t i = 0; i < 4; ++i) {
        if (!m_online[i]) continue;
        if (i != leader_idx) {
            m_nodes[i]->StartRound(round, ops, resulting_state_root);
        }
        if (proposal.has_value() && m_can_communicate[leader_idx][i]) {
            auto pv = m_nodes[i]->ReceiveProposal(*proposal);
            if (pv) prevotes.push_back(*pv);
        }
    }

    std::vector<BftPrecommitMsg> precommits;
    for (size_t i = 0; i < 4; ++i) {
        if (!m_online[i]) continue;
        for (const auto& pv : prevotes) {
            size_t sender = 0;
            for (size_t k = 0; k < 4; ++k) {
                if (m_nodes[k]->GetValidatorId() == pv.validator_id) sender = k;
            }
            if (m_can_communicate[sender][i]) {
                auto pc = m_nodes[i]->ReceivePrevote(pv);
                if (pc) precommits.push_back(*pc);
            }
        }
    }

    size_t finalized_count = 0;
    for (size_t i = 0; i < 4; ++i) {
        if (!m_online[i]) continue;
        for (const auto& pc : precommits) {
            size_t sender = 0;
            for (size_t k = 0; k < 4; ++k) {
                if (m_nodes[k]->GetValidatorId() == pc.validator_id) sender = k;
            }
            if (m_can_communicate[sender][i]) {
                m_nodes[i]->ReceivePrecommit(pc);
            }
        }
        if (m_nodes[i]->GetStep() == BftStep::FINALIZED) {
            finalized_count++;
        }
    }

    return finalized_count >= m_validator_set.QuorumThreshold();
}

} // namespace cybou
