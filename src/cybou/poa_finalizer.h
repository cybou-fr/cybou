// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_POA_FINALIZER_H
#define CYBOU_POA_FINALIZER_H

#include <cybou/block.h>
#include <cybou/operation_pool.h>
#include <cybou/poa_signing_service.h>
#include <cybou/protocol_operation.h>
#include <cybou/state_store.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cybou {

enum class BlockProductionError : uint8_t {
    NONE,
    STATE_UNAVAILABLE,
    INVALID_PENDING_OPERATIONS,
    BLOCK_TOO_LARGE,
    POA_SIGNING_FAILED,
    POA_SAFETY_HALTED,
    COMMIT_FAILED,
};

struct BlockProductionResult {
    BlockProductionError error{BlockProductionError::NONE};
    std::optional<FinalizedBlock> finalized_block;
    BlockTransitionResult commit_result{};

    explicit operator bool() const { return error == BlockProductionError::NONE; }
};

enum class OperationSubmitStatus : uint8_t {
    REJECTED = 0x00,
    ACCEPTED = 0x01,
    ALREADY_PENDING = 0x02,
    ALREADY_FINALIZED = 0x03,
    INVALID_PAYLOAD = 0x04,
    NETWORK_MISMATCH = 0x05,
    RELAY_QUEUED = 0x06,
    FINALIZER_UNAVAILABLE = 0x07,
    RELAY_QUEUE_FULL = 0x08,
};

struct OperationSubmitResult {
    OperationSubmitStatus status{OperationSubmitStatus::REJECTED};
    uint256 op_id;
    // Local transport metadata: no acknowledgment after sending does not prove
    // that the remote finalizer rejected the operation.
    bool delivery_uncertain{false};

    explicit operator bool() const {
        return status == OperationSubmitStatus::ACCEPTED ||
               status == OperationSubmitStatus::ALREADY_PENDING ||
               status == OperationSubmitStatus::ALREADY_FINALIZED ||
               status == OperationSubmitStatus::RELAY_QUEUED;
    }
};

/**
 * Single genesis-bound PoA producer for canonical CYBOU blocks. It owns no
 * pending state: it seals the node's own independently executed candidate pool.
 */
class PoaFinalizer
{
public:
    /** Constructs an unarmed signer slot bound to the network's genesis key. */
    PoaFinalizer(CybouStateStore& store, OperationPool& pool);
    PoaFinalizer(CybouStateStore& store, OperationPool& pool, const RecoveryEntropy& poa_recovery_entropy);
    ~PoaFinalizer();

    /** Add an operation only if the complete pending batch executes on the current head. */
    bool SubmitOperation(const ProtocolOperation& operation);
    OperationSubmitStatus SubmitOperationWithStatus(const ProtocolOperation& operation,
        std::optional<std::string> source_peer = std::nullopt);
    /** Sign a GRANT/BURN for the next block and admit it like any other candidate. */
    OperationSubmitResult SubmitAuthAdjustment(PoaAuthAction action, const AccountId& target, uint64_t amount);

    /** Finalize the pending batch, including an empty block when the queue is empty. */
    BlockProductionResult ProduceNextBlock(bool sync = true);
    bool SafetyHalted() const;
    bool EnableSigner(PoaSignerRef signer);
    void DisableSigner();
    bool SignerEnabled() const;

private:
    CybouStateStore& m_store;
    std::unique_ptr<PoaSigningService> m_finalizer;
    OperationPool& m_pool;
};

} // namespace cybou

#endif // CYBOU_POA_FINALIZER_H
