// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_AUTHORITY_NODE_H
#define CYBOU_AUTHORITY_NODE_H

#include <cybou/block.h>
#include <cybou/operation_pool.h>
#include <cybou/protocol_operation.h>
#include <cybou/state_store.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cybou {

inline constexpr size_t MAX_AUTHORITY_SERIALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

enum class AuthorityProductionError : uint8_t {
    NONE,
    STATE_UNAVAILABLE,
    NOT_AUTHORITY_MODE,
    VALIDATOR_KEY_MISMATCH,
    INVALID_PENDING_OPERATIONS,
    BLOCK_TOO_LARGE,
    CONSENSUS_FAILED,
    COMMIT_FAILED,
};

struct AuthorityProductionResult {
    AuthorityProductionError error{AuthorityProductionError::NONE};
    std::optional<FinalizedBlock> finalized_block;
    BlockTransitionResult commit_result{};

    explicit operator bool() const { return error == AuthorityProductionError::NONE; }
};

enum class OperationSubmitStatus : uint8_t {
    REJECTED = 0x00,
    ACCEPTED = 0x01,
    ALREADY_PENDING = 0x02,
    ALREADY_FINALIZED = 0x03,
    INVALID_PAYLOAD = 0x04,
    NETWORK_MISMATCH = 0x05,
};

struct OperationSubmitResult {
    OperationSubmitStatus status{OperationSubmitStatus::REJECTED};
    uint256 op_id;

    explicit operator bool() const {
        return status == OperationSubmitStatus::ACCEPTED ||
               status == OperationSubmitStatus::ALREADY_PENDING ||
               status == OperationSubmitStatus::ALREADY_FINALIZED;
    }
};

/** Single-validator producer for canonical CYBOU blocks and 1/1 finality. */
class CybouAuthorityNode
{
public:
    CybouAuthorityNode(CybouStateStore& store, std::array<unsigned char, 32> validator_private_key,
                       std::optional<std::filesystem::path> signing_journal = std::nullopt);
    ~CybouAuthorityNode();

    /** Add an operation only if the complete pending batch executes on the current head. */
    bool SubmitOperation(const ProtocolOperation& operation);
    OperationSubmitStatus SubmitOperationWithStatus(const ProtocolOperation& operation,
        std::optional<std::string> source_peer = std::nullopt);
    size_t PendingCount() const { return m_pool.Size(); }
    void ClearPending() { m_pool.Clear(); }
    void RevalidatePending() { m_pool.Revalidate(); }

    /** Finalize the pending batch, including an empty block when the queue is empty. */
    AuthorityProductionResult ProduceNextBlock(bool sync = true);

private:
    CybouStateStore& m_store;
    std::array<unsigned char, 32> m_validator_private_key;
    std::optional<std::filesystem::path> m_signing_journal;
    OperationPool m_pool;
};

} // namespace cybou

#endif // CYBOU_AUTHORITY_NODE_H
