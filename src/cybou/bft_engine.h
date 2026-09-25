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
#include <filesystem>
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

/** Maximum number of future rounds a validator may advance when receiving a signed proposal from the elected leader.
 *  Rationale: Bounding future round advance prevents Byzantine or desynchronized leaders from forcing arbitrary
 *  round skips, while providing sufficient slack (2 rounds) for lagging nodes to resynchronize without waiting
 *  for multiple local round timeouts.
 */
inline constexpr uint32_t MAX_FUTURE_ROUND_ADVANCE = 2;

/**
 * Maximum serialized size of an authority/proposal block accepted by consensus.
 * One canonical bound shared by block production, the CYP2 proposal transport,
 * and the durable signing journal size limit below.
 */
inline constexpr size_t MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

/**
 * Largest durable CBS2 signing record the journal reader accepts: the largest
 * consensus-valid serialized block plus record header, checksum, and margin.
 * Keeping this bound tied to the consensus block bound prevents a validator
 * from locking (and durably recording) a block it would reject after restart.
 */
inline constexpr size_t MAX_SIGNING_RECORD_BYTES{MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES + 4096U};

struct BftProposalMsg {
    uint256 network_id;
    uint64_t height{0};
    uint32_t round{0};
    uint256 proposer_id;
    CybouBlock block;
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
 * Deterministic leader index for (height, round) with N validators: (height + round) % N
 */
inline size_t BftLeaderIndex(uint64_t height, uint32_t round, size_t validator_count = 4)
{
    return validator_count == 0 ? 0 : static_cast<size_t>((height + static_cast<uint64_t>(round)) % validator_count);
}

/** Wire serialization and deserialization for BFT consensus messages */
std::optional<std::vector<unsigned char>> SerializeBftProposalMsg(const BftProposalMsg& msg);
std::optional<BftProposalMsg> DeserializeBftProposalMsg(std::span<const unsigned char> bytes);

std::optional<std::vector<unsigned char>> SerializeBftPrevoteMsg(const BftPrevoteMsg& msg);
std::optional<BftPrevoteMsg> DeserializeBftPrevoteMsg(std::span<const unsigned char> bytes);

std::optional<std::vector<unsigned char>> SerializeBftPrecommitMsg(const BftPrecommitMsg& msg);
std::optional<BftPrecommitMsg> DeserializeBftPrecommitMsg(std::span<const unsigned char> bytes);

/**
 * Individual BFT validator state machine for N >= 1 consensus.
 */
class BftValidatorNode
{
public:
    using MessageBroadcaster = std::function<void(const BftProposalMsg&, const BftPrevoteMsg*, const BftPrecommitMsg*)>;
    using ExecuteOperations = std::function<std::optional<uint256>(const std::vector<ProtocolOperation>&, uint64_t)>;

    BftValidatorNode(
        size_t node_index,
        std::array<unsigned char, 32> private_key_seed,
        uint256 network_id,
        ValidatorSet validator_set,
        ExecuteOperations execute_operations,
        std::optional<std::filesystem::path> signing_journal = std::nullopt);
    ~BftValidatorNode();

    size_t GetNodeIndex() const { return m_node_index; }
    const uint256& GetValidatorId() const { return m_validator_id; }
    uint64_t GetHeight() const { return m_height; }
    uint32_t GetRound() const { return m_round; }
    BftStep GetStep() const { return m_step; }
    int32_t GetLockedRound() const { return m_locked_round; }
    const std::optional<CybouBlock>& GetLockedBlock() const { return m_locked_block; }
    const std::optional<FinalizedBlock>& GetLatestFinalizedBlock() const { return m_finalized_block; }

    void SetHeight(uint64_t height, const uint256& last_block_id, ValidatorSet validator_set);

    /** Start a new round. If this node is the leader, produces a proposal. */
    std::optional<BftProposalMsg> StartRound(
        uint32_t round,
        const std::vector<ProtocolOperation>& pending_ops);

    /** Handle an incoming proposal message. Returns prevote message if produced. */
    std::optional<BftPrevoteMsg> ReceiveProposal(const BftProposalMsg& proposal);

    /** Handle an incoming prevote message. Returns precommit message if quorum reached. */
    std::optional<BftPrecommitMsg> ReceivePrevote(const BftPrevoteMsg& prevote);

    /** Handle an incoming precommit message. Finalizes block if quorum reached. */
    bool ReceivePrecommit(const BftPrecommitMsg& precommit);

    /** Trigger a round timeout: advances step/round when stalled. */
    std::optional<BftPrevoteMsg> OnProposalTimeout();
    std::optional<BftPrecommitMsg> OnPrevoteTimeout();
    void OnRoundTimeout();

private:
    size_t m_node_index;
    std::array<unsigned char, 32> m_private_key_seed;
    uint256 m_validator_id;
    uint256 m_network_id;
    ValidatorSet m_validator_set;
    uint256 m_validator_set_commitment;
    ExecuteOperations m_execute_operations;

    uint64_t m_height{1};
    uint32_t m_round{0};
    BftStep m_step{BftStep::PROPOSE};
    uint256 m_last_block_id;

    // Locking state
    std::optional<CybouBlock> m_locked_block;
    int32_t m_locked_round{-1};

    // Current round tracking
    std::optional<BftProposalMsg> m_current_proposal;
    bool m_current_proposal_valid{false};
    std::map<uint256, BftPrevoteMsg> m_prevotes;
    std::map<uint256, BftPrecommitMsg> m_precommits;
    bool m_prevoted{false};
    bool m_precommitted{false};

    std::optional<FinalizedBlock> m_finalized_block;

    // Durable consensus journal state (CBS2 persists lock and round state).
    std::optional<std::filesystem::path> m_signing_journal;
    uint64_t m_last_signed_height{0};
    uint32_t m_last_signed_round{0};
    BftStep m_last_signed_step{BftStep::PROPOSE};
    bool m_has_signed{false};
    bool m_journal_valid{true};
    bool m_restarted{false};
    bool m_restarted_cbs1{false};
    std::optional<CybouBlock> m_recovered_locked_block;
    int32_t m_recovered_locked_round{-1};
    bool RecordSigningIntent(BftStep step, const uint256& digest);
};

/**
 * Simulator harness for testing N-node BFT network under normal, crash, and partitioned conditions.
 * Supports N=1 (Authority mode), N=2..3 (Integration mode), and N>=4 (BFT mode).
 */
class BftSimulator
{
public:
    explicit BftSimulator(const uint256& network_id, size_t validator_count = 4);

    /** Get validator set */
    const ValidatorSet& GetValidatorSet() const { return m_validator_set; }

    /** Number of validators */
    size_t NodeCount() const { return m_nodes.size(); }

    /** Quorum threshold */
    size_t Quorum() const { return m_validator_set.QuorumThreshold(); }

    /** Access node i */
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
        const std::vector<ProtocolOperation>& ops,
        const uint256& resulting_state_root);

private:
    uint256 m_network_id;
    ValidatorSet m_validator_set;
    std::vector<std::unique_ptr<BftValidatorNode>> m_nodes;
    std::vector<bool> m_online;
    std::vector<std::vector<bool>> m_can_communicate;
    uint256 m_expected_state_root; // Simulator-only execution fixture.
};

} // namespace cybou

#endif // CYBOU_BFT_ENGINE_H
