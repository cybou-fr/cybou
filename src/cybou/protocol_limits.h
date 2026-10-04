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
/// \details 1U << 20 = 1 048 576 отдельных ChunkId. Это parser/memory/CPU safety bound,
///          а не право хранения: хранение оплачивается арендой (DEC-274).
inline constexpr std::uint32_t MAX_PUBLICATION_CHUNKS{1U << 20};

/// \brief Максимальный serialized ProtocolOperation, принимаемый в pool, P2P и CLI.
/// \details 128 * 1024 байт = 128 KiB полезной нагрузки exact signed operation.
inline constexpr std::uint32_t MAX_OPERATION_PAYLOAD_BYTES{128U * 1024U};
/// \brief Максимальный serialized finalized block, передаваемый по CYBOU P2P.
/// \details 32 * 1024 * 1024 байт = 32 MiB на весь блок вместе с операциями и PoA certificate.
inline constexpr std::uint32_t MAX_FINALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

/// \brief Сложность relay-PoW обычной операции, бит; одна для всех Identity (DEC-273).
inline constexpr std::uint32_t OPERATION_WORK_BITS{22};
/// \brief Дополнительная сложность relay-PoW аренды имени (NameCommit/NameReveal), бит.
/// \details +4 бита = в 16 раз дороже обычной операции: имя — дефицитный ресурс.
inline constexpr std::uint32_t NAME_OPERATION_EXTRA_WORK_BITS{4};

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
