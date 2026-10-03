// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_PROTOCOL_LIMITS_H
#define CYBOU_PROTOCOL_LIMITS_H

/// \file
/// \brief Consensus-safe upper bounds shared между wire, execution и storage слоями.

#include <cstddef>
#include <cstdint>

namespace cybou {

/// \brief Верхняя граница числа чанков в одной RootPublication на всех слоях.
inline constexpr std::uint32_t MAX_PUBLICATION_CHUNKS{1U << 20};

/// \brief Максимальный serialized ProtocolOperation, принимаемый в pool, P2P и CLI.
inline constexpr std::uint32_t MAX_OPERATION_PAYLOAD_BYTES{128U * 1024U};
/// \brief Максимальный serialized finalized block, передаваемый по CYBOU P2P.
inline constexpr std::uint32_t MAX_FINALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
