// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_AUTHORITY_NODE_H
#define CYBOU_AUTHORITY_NODE_H

#include <cybou/block.h>
#include <cybou/operation_pool.h>
#include <cybou/poa_finalizer.h>
#include <cybou/protocol_operation.h>
#include <cybou/state_store.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cybou {

enum class AuthorityProductionError : uint8_t {
    NONE,
    STATE_UNAVAILABLE,
    INVALID_PENDING_OPERATIONS,
    BLOCK_TOO_LARGE,
    POA_SIGNING_FAILED,
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
    // Local transport metadata: no acknowledgment after sending does not prove
    // that the remote authority rejected the operation.
    bool delivery_uncertain{false};

    explicit operator bool() const {
        return status == OperationSubmitStatus::ACCEPTED ||
               status == OperationSubmitStatus::ALREADY_PENDING ||
               status == OperationSubmitStatus::ALREADY_FINALIZED;
    }
};

/** Single genesis-bound PoA producer for canonical CYBOU blocks. */
class CybouAuthorityNode
{
public:
    CybouAuthorityNode(CybouStateStore& store, const RecoveryEntropy& poa_recovery_entropy);
    ~CybouAuthorityNode();

    /** Add an operation only if the complete pending batch executes on the current head. */
    bool SubmitOperation(const ProtocolOperation& operation);
    OperationSubmitStatus SubmitOperationWithStatus(const ProtocolOperation& operation,
        std::optional<std::string> source_peer = std::nullopt);
    size_t PendingCount() const { return m_pool.Size(); }
    bool HasPendingOperation(const uint256& id) const { return m_pool.Contains(id); }
    void ClearPending() { m_pool.Clear(); }
    void RevalidatePending() { m_pool.Revalidate(); }

    /** Finalize the pending batch, including an empty block when the queue is empty. */
    AuthorityProductionResult ProduceNextBlock(bool sync = true);

private:
    CybouStateStore& m_store;
    std::unique_ptr<PoaFinalizer> m_finalizer;
    OperationPool m_pool;
};

} // namespace cybou

#endif // CYBOU_AUTHORITY_NODE_H
