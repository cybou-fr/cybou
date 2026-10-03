// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see COPYING.
#ifndef CYBOU_OPERATION_SUBMIT_H
#define CYBOU_OPERATION_SUBMIT_H
#include <cybou/hash256.h>
#include <cstdint>
namespace cybou {
enum class OperationSubmitStatus : uint8_t {
    REJECTED = 0x00,
    ACCEPTED = 0x01,
    ALREADY_PENDING = 0x02,
    ALREADY_FINALIZED = 0x03,
    INVALID_PAYLOAD = 0x04,
    NETWORK_MISMATCH = 0x05,
    RELAY_QUEUED = 0x06,
    POA_SIGNER_UNAVAILABLE = 0x07,
    RELAY_QUEUE_FULL = 0x08,
};

struct OperationSubmitResult {
    OperationSubmitStatus status{OperationSubmitStatus::REJECTED};
    cybou::Hash256 op_id;
    // Local transport metadata: no acknowledgment after sending does not prove
    // that the relay peer rejected the operation.
    bool delivery_uncertain{false};

    explicit operator bool() const {
        return status == OperationSubmitStatus::ACCEPTED ||
               status == OperationSubmitStatus::ALREADY_PENDING ||
               status == OperationSubmitStatus::ALREADY_FINALIZED ||
               status == OperationSubmitStatus::RELAY_QUEUED;
    }
};

} // namespace cybou
#endif
