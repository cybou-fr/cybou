// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_SYNC_RESULT_H
#define CYBOU_SYNC_RESULT_H

/// \file
/// \brief Результат одного прохода проверенной P2P-синхронизации Full Node.

#include <cstdint>

namespace cybou {

/// \brief Исход одной попытки verified sync с отдельным пиром.
enum class SyncPeerStatus : uint8_t {
    UP_TO_DATE,
    BLOCKS_APPLIED,
    CONNECTION_FAILED,
    NETWORK_MISMATCH,
    PROTOCOL_ERROR,
};

/// \brief Итог одного verified sync-pass по известным пирам.
struct SyncPeerResult {
    SyncPeerStatus status{SyncPeerStatus::CONNECTION_FAILED};
    uint64_t blocks_applied{0};
    /// \brief Проход по known peers завершён; это только liveness/UX hint, не доказательство свежести сети.
    bool caught_up_with_known_peers{false};

    operator uint64_t() const { return blocks_applied; }
    bool IsConnected() const {
        return status == SyncPeerStatus::UP_TO_DATE || status == SyncPeerStatus::BLOCKS_APPLIED;
    }
};

} // namespace cybou

#endif // CYBOU_SYNC_RESULT_H
