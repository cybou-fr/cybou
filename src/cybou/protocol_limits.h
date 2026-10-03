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
/// \details 1U << 20 = 1 048 576 отдельных ChunkId. Лимит разделяется wire-форматом,
///          локальным исполнением кандидат-операции и storage admission, чтобы одна и та же
///          публикация не расходилась по допустимости между Full Node.
inline constexpr std::uint32_t MAX_PUBLICATION_CHUNKS{1U << 20};

/// \brief Максимальный serialized ProtocolOperation, принимаемый в pool, P2P и CLI.
/// \details 128 * 1024 байт = 128 KiB полезной нагрузки exact signed operation.
inline constexpr std::uint32_t MAX_OPERATION_PAYLOAD_BYTES{128U * 1024U};
/// \brief Максимальный serialized finalized block, передаваемый по CYBOU P2P.
/// \details 32 * 1024 * 1024 байт = 32 MiB на весь блок вместе с операциями и PoA certificate.
inline constexpr std::uint32_t MAX_FINALIZED_BLOCK_BYTES{32U * 1024U * 1024U};

/// \brief Базовый кредит доверия для удалённого сетевого хранения при 0 AUTH (5 GiB, DEC-269).
inline constexpr std::uint64_t ONBOARDING_STORAGE_CREDIT_BYTES{5ULL * 1024ULL * 1024ULL * 1024ULL};

/// \brief Коэффициент взаимного обязательства физического хранения (1:3, DEC-269).
/// \details Предоставление 15 GiB локального дискового пространства обеспечивает 5 GiB в сети.
inline constexpr std::uint32_t RECIPROCAL_STORAGE_RATIO{3};
inline constexpr std::uint64_t LOCAL_ONBOARDING_STORAGE_BASELINE_BYTES{ONBOARDING_STORAGE_CREDIT_BYTES * RECIPROCAL_STORAGE_RATIO};

/// \brief Единица учёта квоты: один авторизованный chunk = 512 KiB полезной ёмкости.
/// \details Консенсус знает только `chunk_count` публикации, поэтому квота и максимальный
///          размер файла выражены в chunk-ах; байтовые значения — их пересчёт.
inline constexpr std::uint64_t QUOTA_CHUNK_BYTES{512ULL * 1024ULL};

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

/// \brief Лимиты одного уровня (DEC-272).
struct AuthorityTierLimits {
    /// \brief Метрируемых операций одной Identity в одном блоке.
    std::uint32_t operations_per_block;
    /// \brief Метрируемых операций одной Identity за эпоху (`epoch_blocks` блоков, ~17 мин).
    std::uint32_t operations_per_epoch;
    /// \brief Суммарная квота удалённого хранения, chunk-и.
    std::uint64_t storage_quota_chunks;
    /// \brief Максимум chunk-ов одной RootPublication (максимальный размер файла).
    std::uint32_t max_publication_chunks;
    /// \brief Сложность relay-PoW обычной операции, бит.
    std::uint32_t operation_work_bits;
};

/// \brief Таблица уровней. Хранилище 5/25/100/500 GiB/2 TiB, файл ~20% квоты
///        (1/4/16/64/256 GiB), PoW ощутим и снижается вместе с доверием.
constexpr AuthorityTierLimits ComputeAuthorityTierLimits(std::uint64_t authority) noexcept
{
    constexpr std::uint32_t GIB{static_cast<std::uint32_t>(1024ULL * 1024ULL * 1024ULL / QUOTA_CHUNK_BYTES)};
    switch (ComputeAuthorityTier(authority)) {
    case AuthorityTier::T0: return {1, 30, 5ULL * GIB, 1 * GIB, 22};
    case AuthorityTier::T1: return {5, 150, 25ULL * GIB, 4 * GIB, 21};
    case AuthorityTier::T2: return {25, 750, 100ULL * GIB, 16 * GIB, 20};
    case AuthorityTier::T3: return {100, 3'000, 500ULL * GIB, 64 * GIB, 19};
    case AuthorityTier::VALIDATOR: return {1'000, 30'000, 2048ULL * GIB, 256 * GIB, 18};
    }
    return {1, 30, 5ULL * GIB, 1 * GIB, 22};
}

/// \brief Дополнительная сложность relay-PoW аренды имени (NameCommit/NameReveal), бит.
/// \details +4 бита = в 16 раз дороже обычной операции: имя — дефицитный ресурс.
inline constexpr std::uint32_t NAME_OPERATION_EXTRA_WORK_BITS{4};

/// \brief Квота удалённого хранения по финализированному AUTH, байты.
constexpr std::uint64_t ComputeStorageQuotaBytes(std::uint64_t authority) noexcept
{
    return ComputeAuthorityTierLimits(authority).storage_quota_chunks * QUOTA_CHUNK_BYTES;
}

/// \brief Метрируемых операций на блок по финализированному AUTH.
constexpr std::uint32_t ComputeMaxOperationsPerBlock(std::uint64_t authority) noexcept
{
    return ComputeAuthorityTierLimits(authority).operations_per_block;
}

static_assert(ComputeAuthorityTierLimits(UINT64_MAX).max_publication_chunks <= MAX_PUBLICATION_CHUNKS);
static_assert(ComputeStorageQuotaBytes(0) == ONBOARDING_STORAGE_CREDIT_BYTES);

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
