// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_BFT_ENGINE_H
#define CYBOU_BFT_ENGINE_H

#include <cybou/bft.h>
#include <cybou/block.h>
#include <cybou/validator.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

enum class BftStep : uint8_t {
    PROPOSE,
    PREVOTE,
    PRECOMMIT,
    FINALIZED,
};

struct BftProposalMsg {
    uint256 network_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 proposer_id;
    CybouBlockV1 block;
    ValidatorSignature signature{};

    friend bool operator==(const BftProposalMsg&, const BftProposalMsg&) = default;
};

struct BftPrevoteMsg {
    uint256 network_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 validator_id;
    std::optional<uint256> block_id; // nullopt = nil
    ValidatorSignature signature{};

    friend bool operator==(const BftPrevoteMsg&, const BftPrevoteMsg&) = default;
};

struct BftPrecommitMsg {
    uint256 network_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 validator_id;
    std::optional<uint256> block_id; // nullopt = nil
    ValidatorSignature signature{};

    friend bool operator==(const BftPrecommitMsg&, const BftPrecommitMsg&) = default;
};

/** Compute domain-separated digest for proposal message */
uint256 ComputeProposalDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& proposer_id,
    const uint256& block_id);

/** Compute domain-separated digest for prevote message */
uint256 ComputePrevoteDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_id,
    const std::optional<uint256>& block_id);

/** Compute digest for nil precommit (non-nil uses ComputeBftCommitDigest) */
uint256 ComputePrecommitNilDigest(
    const uint256& network_id,
    uint64_t height,
    uint32_t round,
    const uint256& validator_id);

/**
 * Deterministic leader index for (height, round) with N=4 validators: (height + round) % 4
 */
inline size_t BftLeaderIndex(uint64_t height, uint32_t round)
{
    return static_cast<size_t>((height + static_cast<uint64_t>(round)) % BFT_STAGE1_VALIDATOR_COUNT);
}

/**
 * Individual BFT validator state machine for N=4, f=1 consensus.
 */
class BftValidatorNode
{
public:
    using MessageBroadcaster = std::function<void(const BftProposalMsg&, const BftPrevoteMsg*, const BftPrecommitMsg*)>;

    BftValidatorNode(
        size_t node_index,
        std::array<unsigned char, 32> private_key_seed,
        uint256 network_id,
        ValidatorSetV1 validator_set);

    size_t GetNodeIndex() const { return m_node_index; }
    const uint256& GetValidatorId() const { return m_validator_id; }
    uint64_t GetHeight() const { return m_height; }
    uint32_t GetRound() const { return m_round; }
    BftStep GetStep() const { return m_step; }
    const std::optional<FinalizedBlockV1>& GetLatestFinalizedBlock() const { return m_finalized_block; }

    void SetHeight(uint64_t height, const uint256& last_block_id);

    /** Start a new round. If this node is the leader, produces a proposal. */
    std::optional<BftProposalMsg> StartRound(
        uint32_t round,
        const std::vector<ProtocolOperationV1>& pending_ops,
        const uint256& resulting_state_root);

    /** Handle an incoming proposal message. Returns prevote message if produced. */
    std::optional<BftPrevoteMsg> ReceiveProposal(const BftProposalMsg& proposal);

    /** Handle an incoming prevote message. Returns precommit message if 2/3+ prevote quorum reached. */
    std::optional<BftPrecommitMsg> ReceivePrevote(const BftPrevoteMsg& prevote);

    /** Handle an incoming precommit message. Finalizes block if 2/3+ precommit quorum reached. */
    bool ReceivePrecommit(const BftPrecommitMsg& precommit);

    /** Trigger a round timeout: advances step/round when stalled. */
    std::optional<BftPrecommitMsg> OnPrevoteTimeout();
    void OnRoundTimeout();

private:
    size_t m_node_index;
    std::array<unsigned char, 32> m_private_key_seed;
    uint256 m_validator_id;
    uint256 m_network_id;
    ValidatorSetV1 m_validator_set;
    uint256 m_validator_set_commitment;

    uint64_t m_height{1};
    uint32_t m_round{0};
    BftStep m_step{BftStep::PROPOSE};
    uint256 m_last_block_id;

    // Locking state
    std::optional<CybouBlockV1> m_locked_block;
    int32_t m_locked_round{-1};

    // Current round tracking
    std::optional<BftProposalMsg> m_current_proposal;
    std::map<uint256, BftPrevoteMsg> m_prevotes;
    std::map<uint256, BftPrecommitMsg> m_precommits;
    bool m_prevoted{false};
    bool m_precommitted{false};

    std::optional<FinalizedBlockV1> m_finalized_block;
};

/**
 * Simulator harness for testing 4-node BFT network under normal, crash, and partitioned conditions.
 */
class BftSimulator
{
public:
    explicit BftSimulator(const uint256& network_id);

    /** Get validator set */
    const ValidatorSetV1& GetValidatorSet() const { return m_validator_set; }

    /** Access node i (0 <= i < 4) */
    BftValidatorNode& Node(size_t index) { return *m_nodes.at(index); }

    /** Set online/offline status for node (simulates crash / recovery) */
    void SetNodeOnline(size_t index, bool online);

    /** Set partition between two groups of nodes (e.g. {0,1} vs {2,3}) */
    void SetPartition(const std::vector<size_t>& partition_a, const std::vector<size_t>& partition_b);
    void ClearPartition();

    /** Run one step of consensus for all online nodes */
    bool StepRound(
        uint64_t height,
        uint32_t round,
        const std::vector<ProtocolOperationV1>& ops,
        const uint256& resulting_state_root);

private:
    uint256 m_network_id;
    ValidatorSetV1 m_validator_set;
    std::vector<std::unique_ptr<BftValidatorNode>> m_nodes;
    std::array<bool, BFT_STAGE1_VALIDATOR_COUNT> m_online{true, true, true, true};
    std::array<std::array<bool, 4>, 4> m_can_communicate;
};

} // namespace cybou

#endif // CYBOU_BFT_ENGINE_H
