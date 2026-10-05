// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Целочисленная арифметика storage rent (DEC-279) для shadow accounting.
///
/// Здесь нет consensus state: M4 только оценивает rent и вознаграждение
/// providers, не перемещая CYBOU. Та же арифметика станет канонической в M5.

#ifndef CYBOU_STORAGE_ECONOMY_H
#define CYBOU_STORAGE_ECONOMY_H

#include <cstdint>
#include <optional>

namespace cybou {

/// \brief Billing unit: один authorized chunk, 512 KiB.
inline constexpr std::uint64_t STORAGE_BILLING_UNIT_BYTES{512ULL * 1024};
/// \brief Billing units в одном GiB.
inline constexpr std::uint64_t STORAGE_BILLING_UNITS_PER_GIB{2048};
/// \brief Секунд в сутках rent.
inline constexpr std::uint64_t STORAGE_SECONDS_PER_DAY{86400};
/// \brief Beta-ставка: CYBOU за GiB за сутки за одну remote replica (provisional до M4).
inline constexpr std::uint64_t STORAGE_RATE_CYBOU_PER_GIB_DAY_REPLICA{5};
/// \brief Знаменатель rent: `2048 × 86400`.
inline constexpr std::uint64_t STORAGE_RENT_DENOMINATOR{STORAGE_BILLING_UNITS_PER_GIB * STORAGE_SECONDS_PER_DAY};

/// \brief Число billing units для \p bytes (округление вверх).
constexpr std::uint64_t StorageBillingUnits(const std::uint64_t bytes)
{
    return bytes / STORAGE_BILLING_UNIT_BYTES + (bytes % STORAGE_BILLING_UNIT_BYTES != 0);
}

/// \brief Начисление rent с переносом целочисленного остатка между периодами.
struct StorageRentAccumulator {
    /// \brief Уже начисленные целые CYBOU.
    std::uint64_t cybou{0};
    /// \brief Остаток числителя `< STORAGE_RENT_DENOMINATOR`, переносимый в следующий период.
    std::uint64_t remainder{0};
};

/// \brief Добавляет `units × seconds × replicas × rate` к accumulator.
/// \details Округление — floor с переносом остатка: сумма нескольких периодов
/// равна rent за их общую длительность, и ничего не теряется и не создаётся.
/// \return \c false при переполнении; accumulator тогда не меняется.
bool AccrueStorageRent(StorageRentAccumulator& accumulator, std::uint64_t units, std::uint64_t seconds,
    std::uint64_t replicas);

/// \brief Rent за одни сутки для \p units на \p replicas репликах, floor в CYBOU.
/// \return std::nullopt при переполнении.
std::optional<std::uint64_t> StorageRentPerDay(std::uint64_t units, std::uint64_t replicas);

} // namespace cybou

#endif // CYBOU_STORAGE_ECONOMY_H
