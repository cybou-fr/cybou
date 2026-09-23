// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STATE_STORE_H
#define CYBOU_STATE_STORE_H

#include <cybou/state.h>

#include <cstdint>
#include <optional>
#include <vector>

class CDBWrapper;

namespace cybou {

enum class StateLoadError : uint8_t {
    NONE,
    NOT_FOUND,
    CORRUPT,
};

struct StateLoadResult {
    StateLoadError error{StateLoadError::NONE};
    std::optional<CybouState> state;

    explicit operator bool() const { return error == StateLoadError::NONE && state.has_value(); }
};

enum class GenesisInitError : uint8_t {
    NONE,
    ALREADY_INITIALIZED,
};

struct GenesisInitResult {
    GenesisInitError error{GenesisInitError::NONE};

    explicit operator bool() const { return error == GenesisInitError::NONE; }
};

enum class BlockTransitionError : uint8_t {
    NONE,
    INVALID_BLOCK_ID,
    STATE_NOT_INITIALIZED,
    PARENT_MISMATCH,
    BLOCK_ALREADY_APPLIED,
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
    explicit CybouStateStore(CDBWrapper& db) : m_db{db} {}

    /** Persist the genesis state. Fails if a state already exists. */
    GenesisInitResult InitializeGenesis(const CybouState& genesis_state, bool sync = true);

    /** Load the canonical state with hash integrity verification. */
    StateLoadResult LoadState() const;

    /** Hash of the canonical state, if initialized. */
    std::optional<uint256> GetStateRoot() const;

    /** Last finalized block id, if any block has been committed. */
    std::optional<uint256> GetFinalizedTip() const;

    /**
     * Atomically commit a BFT-finalized block: all operations are executed
     * against a throwaway candidate derived from the canonical state and the
     * new state, its hash, and the tip are written in one batch. On any
     * failure the store is left untouched.
     */
    BlockTransitionResult CommitFinalizedBlock(
        const uint256& block_id,
        const uint256& previous_block_id,
        const std::vector<AccountCreateOpV1>& ops,
        const uint256& network_id,
        uint64_t block_height,
        const CybouProtocolParameters& params,
        bool sync = true);

private:
    void Write(const CybouState& state, bool sync);

    CDBWrapper& m_db;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
