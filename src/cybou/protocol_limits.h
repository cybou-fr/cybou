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

/// \brief Уровень ресурсов Identity по её финализированному AUTH (DEC-272).
enum class AuthorityTier : std::uint8_t { T0, T1, T2, T3, VALIDATOR };

/// \brief Границы уровней: T1 >= 10k, T2 >= 100k, T3 >= 1M, VALIDATOR > 10M AUTH.
constexpr AuthorityTier ComputeAuthorityTier(std::uint64_t authority) noexcept
{
    if (authority > 10'000'000ULL) return AuthorityTier::VALIDATOR;
    if (authority >= 1'000'000ULL) return AuthorityTier::T3;
    if (authority >= 100'000ULL) return AuthorityTier::T2;
    if (authority >= 10'000ULL) return AuthorityTier::T1;
    return AuthorityTier::T0;
}

/// \brief Лимиты одного уровня (DEC-272). Хранение AUTH не квотирует (DEC-274).
struct AuthorityTierLimits {
    /// \brief Метрируемых операций одной Identity в одном блоке.
    std::uint32_t operations_per_block;
    /// \brief Метрируемых операций одной Identity за эпоху (`epoch_blocks` блоков, ~17 мин).
    std::uint32_t operations_per_epoch;
    /// \brief Сложность relay-PoW обычной операции, бит.
    std::uint32_t operation_work_bits;
};

/// \brief Таблица уровней: PoW ощутим и снижается вместе с доверием.
constexpr AuthorityTierLimits ComputeAuthorityTierLimits(std::uint64_t authority) noexcept
{
    switch (ComputeAuthorityTier(authority)) {
    case AuthorityTier::T0: return {1, 30, 22};
    case AuthorityTier::T1: return {5, 150, 21};
    case AuthorityTier::T2: return {25, 750, 20};
    case AuthorityTier::T3: return {100, 3'000, 19};
    case AuthorityTier::VALIDATOR: return {1'000, 30'000, 18};
    }
    return {1, 30, 22};
}

/// \brief Дополнительная сложность relay-PoW аренды имени (NameCommit/NameReveal), бит.
/// \details +4 бита = в 16 раз дороже обычной операции: имя — дефицитный ресурс.
inline constexpr std::uint32_t NAME_OPERATION_EXTRA_WORK_BITS{4};

/// \brief Метрируемых операций на блок по финализированному AUTH.
constexpr std::uint32_t ComputeMaxOperationsPerBlock(std::uint64_t authority) noexcept
{
    return ComputeAuthorityTierLimits(authority).operations_per_block;
}

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
