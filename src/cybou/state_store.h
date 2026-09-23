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

enum class BlockTransitionError : uint8_t {
    NONE,
    INVALID_BLOCK_ID,
    STATE_NOT_INITIALIZED,
    STATE_MISMATCH,
    PARENT_MISMATCH,
    BLOCK_ALREADY_APPLIED,
    INVALID_OPERATION,
    NOT_CURRENT_TIP,
    MISSING_OR_CORRUPT_UNDO,
};

struct BlockTransitionResult {
    BlockTransitionError error{BlockTransitionError::NONE};
    AccountCreateResult op_result{};

    explicit operator bool() const { return error == BlockTransitionError::NONE; }
};

/** Atomic LevelDB snapshot persistence for the canonical CYBOU state. */
class CybouStateStore
{
public:
    explicit CybouStateStore(CDBWrapper& db) : m_db{db} {}

    void Write(const CybouState& state, bool sync = true);
    StateLoadResult Load() const;

    AccountCreateResult CreateAccountAndWrite(
        const AccountCreateOpV1& op,
        const uint256& network_id,
        uint64_t block_height,
        uint64_t epoch,
        unsigned int required_work_bits,
        uint64_t onboarding_bonus,
        CybouState& state,
        bool sync = true);

    BlockTransitionResult ApplyBlock(
        const uint256& block_id,
        const uint256& previous_block_id,
        const std::vector<AccountCreateOpV1>& ops,
        const uint256& network_id,
        uint64_t block_height,
        uint64_t epoch,
        unsigned int required_work_bits,
        uint64_t onboarding_bonus,
        CybouState& state,
        bool sync = true);

    BlockTransitionResult RollbackBlock(
        const uint256& block_id,
        CybouState& state,
        bool sync = true);

private:
    CDBWrapper& m_db;
};

} // namespace cybou

#endif // CYBOU_STATE_STORE_H
