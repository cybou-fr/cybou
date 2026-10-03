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

/// \brief Вычисляет максимальную квоту удалённого хранения по финализированному AUTH (DEC-268, DEC-269).
constexpr std::uint64_t ComputeStorageQuotaBytes(std::uint64_t authority) noexcept
{
    if (authority > 10'000'000ULL) {
        return UINT64_MAX; // Неограниченно для уровня валидатора (AUTH > 10M)
    }
    if (authority >= 1'000'000ULL) {
        return 500ULL * 1024ULL * 1024ULL * 1024ULL; // 500 GiB
    }
    if (authority >= 100'000ULL) {
        return 100ULL * 1024ULL * 1024ULL * 1024ULL; // 100 GiB
    }
    if (authority >= 10'000ULL) {
        return 25ULL * 1024ULL * 1024ULL * 1024ULL; // 25 GiB
    }
    return ONBOARDING_STORAGE_CREDIT_BYTES; // 5 GiB baseline onboarding credit
}

/// \brief Вычисляет ограничение числа операций на блок по финализированному AUTH (DEC-268).
constexpr std::uint32_t ComputeMaxOperationsPerBlock(std::uint64_t authority) noexcept
{
    if (authority > 10'000'000ULL) {
        return UINT32_MAX; // Неограниченно для уровня валидатора
    }
    if (authority >= 1'000'000ULL) {
        return 100;
    }
    if (authority >= 100'000ULL) {
        return 25;
    }
    if (authority >= 10'000ULL) {
        return 5;
    }
    return 1; // 1 операция на блок для 0-AUTH для предотвращения флуда
}

} // namespace cybou

#endif // CYBOU_PROTOCOL_LIMITS_H
