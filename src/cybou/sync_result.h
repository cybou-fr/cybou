// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_SYNC_RESULT_H
#define CYBOU_SYNC_RESULT_H

#include <cstdint>

namespace cybou {

enum class SyncPeerStatus : uint8_t {
    UP_TO_DATE,
    BLOCKS_APPLIED,
    CONNECTION_FAILED,
    NETWORK_MISMATCH,
    PROTOCOL_ERROR,
};

/** Outcome of one verified CYP2 sync pass. */
struct SyncPeerResult {
    SyncPeerStatus status{SyncPeerStatus::CONNECTION_FAILED};
    uint64_t blocks_applied{0};
    /** A bounded sync pass explicitly observed no next block; never inferred from batch size. */
    bool reached_peer_tip{false};

    operator uint64_t() const { return blocks_applied; }
    bool IsConnected() const {
        return status == SyncPeerStatus::UP_TO_DATE || status == SyncPeerStatus::BLOCKS_APPLIED;
    }
};

} // namespace cybou

#endif // CYBOU_SYNC_RESULT_H
