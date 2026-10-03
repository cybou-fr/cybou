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
    /// \brief Пир доступен и не дал новых finalized blocks.
    UP_TO_DATE,
    /// \brief Из этого прохода были коммичены один или более finalized blocks.
    BLOCKS_APPLIED,
    /// \brief До пира не удалось установить/удержать рабочее соединение.
    CONNECTION_FAILED,
    /// \brief Пир объявил другую official network и не должен использоваться дальше.
    NETWORK_MISMATCH,
    /// \brief Сеанс завершился нарушением протокола, декодирования или локальной safety-проверки.
    PROTOCOL_ERROR,
};

/// \brief Итог одного verified sync-pass по известным пирам.
struct SyncPeerResult {
    /// \brief Наиболее значимый итог прохода.
    SyncPeerStatus status{SyncPeerStatus::CONNECTION_FAILED};
    /// \brief Сколько finalized blocks было применено в этом проходе.
    uint64_t blocks_applied{0};
    /// \brief Проход по known peers завершён; это только liveness/UX hint, не доказательство свежести сети.
    bool caught_up_with_known_peers{false};

    /// \brief Удобное приведение к числу применённых блоков.
    operator uint64_t() const { return blocks_applied; }
    /// \brief true, если с пиром/маршрутом удалось провести корректный протокольный обмен.
    bool IsConnected() const {
        return status == SyncPeerStatus::UP_TO_DATE || status == SyncPeerStatus::BLOCKS_APPLIED;
    }
};

} // namespace cybou

#endif // CYBOU_SYNC_RESULT_H
