// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_STORE_H
#define CYBOU_STATE_STORE_H

#include <cybou/network_definition.h>
#include <cybou/protocol_operation.h>
#include <cybou/state.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class CDBWrapper;

namespace cybou {

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
};

struct BlockTransitionResult {
    BlockTransitionError error{BlockTransitionError::NONE};
    AccountCreateResult op_result{};

    explicit operator bool() const { return error == BlockTransitionError::NONE; }
};

/**
 * Sole owner of the canonical CYBOU state.
 *
 * CYBOU has explicit BFT finality: a finalized block is never reorged, so
 * there is intentionally no production rollback/undo path. State transition
 * is strictly candidate-validate-commit on top of the store's own canonical
 * state; callers never hold or supply a copy of consensus state and there is
 * no arbitrary Write() entry point.
 */
class CybouStateStore
{
public:
    CybouStateStore(CDBWrapper& db, CybouNetworkDefinitionV1 network_definition)
        : m_db{db},
          m_network_definition{std::move(network_definition)},
          m_network_definition_error{ValidateNetworkDefinition(m_network_definition)},
          m_network_id{NetworkId(m_network_definition)}
    {
    }

    /** Persist genesis state and its canonical height. Fails if already initialized. */
    GenesisInitResult InitializeGenesis(const CybouState& genesis_state, bool sync = true, uint64_t genesis_height = 0);

    /** Load the canonical state with hash integrity verification. */
    StateLoadResult LoadState() const;

    /** Hash of the canonical state, if initialized. */
    std::optional<uint256> GetStateRoot() const;

    /** Last finalized block id, if any block has been committed. */
    std::optional<uint256> GetFinalizedTip() const;

    /** Canonical finalized height (genesis height before the first committed child). */
    std::optional<uint64_t> GetFinalizedHeight() const;

    /** Network identity derived from the immutable canonical definition. */
    const uint256& GetNetworkId() const { return m_network_id; }

    /** Network identity persisted with genesis, if initialized. */
    std::optional<uint256> GetStoredNetworkId() const;

    /**
     * Atomically commit a BFT-finalized block: all operations are executed
     * against a throwaway candidate derived from the canonical state and the
     * new state, its hash, and the tip are written in one batch. On any
     * failure the store is left untouched.
     */
    BlockTransitionResult CommitFinalizedBlock(
        const uint256& block_id,
        const uint256& previous_block_id,
        const std::vector<ProtocolOperationV1>& ops,
        uint64_t block_height,
        bool sync = true);

private:
    CDBWrapper& m_db;
    const CybouNetworkDefinitionV1 m_network_definition;
    const NetworkDefinitionError m_network_definition_error;
    const uint256 m_network_id;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
