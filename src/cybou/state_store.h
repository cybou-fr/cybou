// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_STORE_H
#define CYBOU_STATE_STORE_H

#include <cybou/block.h>
#include <cybou/block_executor.h>
#include <cybou/kv_store.h>
#include <cybou/network_definition.h>
#include <cybou/protocol_operation.h>
#include <cybou/state.h>
#include <serialize.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace cybou {

struct FinalizedHead {
    uint256 block_id;
    uint64_t height{0};

    SERIALIZE_METHODS(FinalizedHead, obj)
    {
        READWRITE(obj.block_id, obj.height);
    }

    friend bool operator==(const FinalizedHead&, const FinalizedHead&) = default;
};

enum class StateLoadError : uint8_t {
    NONE,
    NOT_FOUND,
    CORRUPT,
    INVALID_NETWORK_DEFINITION,
    NETWORK_MISMATCH,
};

struct StateLoadResult {
    StateLoadError error{StateLoadError::NONE};
    std::optional<CybouState> state;

    explicit operator bool() const { return error == StateLoadError::NONE && state.has_value(); }
};

enum class GenesisInitError : uint8_t {
    NONE,
    ALREADY_INITIALIZED,
    INVALID_NETWORK_DEFINITION,
    GENESIS_STATE_MISMATCH,
};

struct GenesisInitResult {
    GenesisInitError error{GenesisInitError::NONE};

    explicit operator bool() const { return error == GenesisInitError::NONE; }
};

enum class BlockTransitionError : uint8_t {
    NONE,
    INVALID_BLOCK_ID,
    STATE_NOT_INITIALIZED,
    INVALID_NETWORK_DEFINITION,
    NETWORK_MISMATCH,
    CORRUPT_STATE,
    PARENT_MISMATCH,
    BLOCK_ALREADY_APPLIED,
    INVALID_HEIGHT,
    CORRUPT_HEAD,
    INVALID_OPERATION,
    TOO_MANY_ACCOUNT_CREATES,
    INVALID_CERTIFICATE,
    STATE_ROOT_MISMATCH,
    FEE_ROUTING_FAILED,
};

struct BlockTransitionResult {
    BlockTransitionError error{BlockTransitionError::NONE};
    BlockExecutionResult op_result{};
    explicit operator bool() const { return error == BlockTransitionError::NONE; }
};

/**
 * Sole owner of the canonical CYBOU state.
 *
 * CYBOU accepts one genesis-bound PoA finalizer: a finalized block is never reorged, so
 * there is intentionally no production rollback/undo path. State transition
 * is strictly candidate-validate-commit on top of the store's own canonical
 * state; callers never hold or supply a copy of consensus state and there is
 * no arbitrary Write() entry point.
 */
class CybouStateStore
{
public:
    CybouStateStore(
        KVStore& db,
        CybouNetworkDefinition network_definition,
        std::shared_ptr<OperatorAuthoritySignatureVerifier> operator_verifier = nullptr);

    /** Persist genesis state at height 0. Fails if already initialized. */
    GenesisInitResult InitializeGenesis(const CybouState& genesis_state, bool sync = true);

    /** Load the canonical state with hash integrity verification. */
    StateLoadResult LoadState() const;

    /** Hash of the canonical state, if initialized. */
    std::optional<uint256> GetStateRoot() const;

    /** Canonical finalized head (block id and height). */
    std::optional<FinalizedHead> GetFinalizedHead() const;

    /** Last finalized block id (genesis block id before any committed child). */
    std::optional<uint256> GetFinalizedTip() const;

    /** Canonical finalized height (0 for genesis, monotonically increasing with each finalized block). */
    std::optional<uint64_t> GetFinalizedHeight() const;

    /** Compute a proposal root from the canonical parent state without committing it. */
    std::optional<uint256> ComputeCandidateStateRoot(
        const std::vector<ProtocolOperation>& operations,
        uint64_t height) const;

    /** Network identity derived from the immutable canonical definition. */
    const uint256& GetNetworkId() const { return m_network_id; }
    const CybouNetworkDefinition& GetNetworkDefinition() const { return m_network_definition; }
    KVStore& GetDatabase() const { return m_db; }

    /** Network identity persisted with genesis, if initialized. */
    std::optional<uint256> GetStoredNetworkId() const;

    /**
     * Atomically commit a PoA-finalized block:
     * - verifies parent equals current head
     * - verifies height equals head.height + 1
     * - verifies block ID and genesis-bound PoA finality certificate
     * - verifies operations and fee routing against a throwaway candidate
     * - verifies candidate state root matches block.resulting_state_root
     * - atomically writes new state, hash, head, and finalized block in one batch.
     */
    BlockTransitionResult CommitFinalizedBlock(
        const FinalizedBlock& finalized_block,
        bool sync = true);

    /** Retrieve a persisted finalized block by its block ID. */
    std::optional<FinalizedBlock> GetBlock(const uint256& block_id) const;

    /** Retrieve a finalized non-genesis block by canonical height. */
    std::optional<FinalizedBlock> GetBlockAtHeight(uint64_t height) const;

    /** Check the local index of operations committed with finalized blocks. */
    bool HasIndexedFinalizedOperation(const uint256& op_id) const;
    /** Return the indexed finalized height for an operation, if present and valid. */
    std::optional<uint64_t> GetFinalizedOperationHeight(const uint256& op_id) const;


private:
    KVStore& m_db;
    const CybouNetworkDefinition m_network_definition;
    const NetworkDefinitionError m_network_definition_error;
    const uint256 m_network_id;
    std::shared_ptr<OperatorAuthoritySignatureVerifier> m_operator_verifier;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
