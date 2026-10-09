// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <cybou/storage_economy.h>

#include <limits>

namespace cybou {
namespace {

bool CheckedMul(const std::uint64_t a, const std::uint64_t b, std::uint64_t& out)
{
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) return false;
    out = a * b;
    return true;
}

} // namespace

bool AccrueStorageRent(StorageRentAccumulator& accumulator, const std::uint64_t units, const std::uint64_t seconds,
    const std::uint64_t replicas)
{
    if (accumulator.remainder >= STORAGE_RENT_DENOMINATOR) return false;
    std::uint64_t numerator{0};
    if (!CheckedMul(units, seconds, numerator) || !CheckedMul(numerator, replicas, numerator) ||
        !CheckedMul(numerator, STORAGE_RATE_CYBOU_PER_GIB_DAY_REPLICA, numerator)) return false;
    // Делим до сложения с остатком, чтобы не переполнить сумму на больших периодах.
    const std::uint64_t whole = numerator / STORAGE_RENT_DENOMINATOR;
    std::uint64_t remainder = numerator % STORAGE_RENT_DENOMINATOR + accumulator.remainder;
    const std::uint64_t carry = remainder / STORAGE_RENT_DENOMINATOR;
    remainder %= STORAGE_RENT_DENOMINATOR;
    if (whole > std::numeric_limits<std::uint64_t>::max() - carry ||
        accumulator.cybou > std::numeric_limits<std::uint64_t>::max() - whole - carry) return false;
    accumulator.cybou += whole + carry;
    accumulator.remainder = remainder;
    return true;
}

std::optional<std::uint64_t> StorageRentPerDay(const std::uint64_t units, const std::uint64_t replicas)
{
    StorageRentAccumulator day;
    if (!AccrueStorageRent(day, units, STORAGE_SECONDS_PER_DAY, replicas)) return std::nullopt;
    return day.cybou;
}

std::optional<AssignedStorageBudget> ComputeAssignedStorageBudget(const std::uint64_t units,
    const std::uint64_t replicas, const std::uint64_t periods, const std::uint64_t period_seconds,
    const std::uint64_t rate)
{
    if (!units || !replicas || !periods || !period_seconds || !rate) return std::nullopt;
    std::uint64_t seconds{0}, unit_seconds{0};
    if (!CheckedMul(periods, period_seconds, seconds) || !CheckedMul(units, seconds, unit_seconds)) {
        return std::nullopt;
    }
    const auto numerator = static_cast<unsigned __int128>(unit_seconds) * rate;
    const auto share = numerator / STORAGE_RENT_DENOMINATOR + (numerator % STORAGE_RENT_DENOMINATOR != 0);
    if (share > std::numeric_limits<std::uint64_t>::max()) return std::nullopt;
    AssignedStorageBudget budget{.per_replica = static_cast<std::uint64_t>(share),
        .contracted_unit_seconds = unit_seconds};
    if (!CheckedMul(budget.per_replica, replicas, budget.total)) return std::nullopt;
    return budget;
}

std::uint64_t StorageVerifiedIntervalSeconds(const std::int64_t previous_ms,
    const std::int64_t now_ms, const std::int64_t maximum_gap_ms)
{
    if (previous_ms <= 0 || now_ms <= previous_ms || maximum_gap_ms <= 0) return 0;
    // Both timestamps are positive, so subtraction cannot overflow int64.
    const auto gap = now_ms - previous_ms;
    return gap <= maximum_gap_ms ? static_cast<std::uint64_t>(gap / 1000) : 0;
}

std::optional<std::uint64_t> ComputeAssignedStoragePayout(const AssignedStorageBudget& budget,
    const std::uint64_t verified_unit_seconds, const std::uint64_t finalized_paid)
{
    if (!budget.per_replica || !budget.contracted_unit_seconds || budget.total < budget.per_replica ||
        budget.total % budget.per_replica != 0 ||
        verified_unit_seconds > budget.contracted_unit_seconds) return std::nullopt;
    const auto entitlement = static_cast<std::uint64_t>(
        static_cast<unsigned __int128>(budget.per_replica) * verified_unit_seconds /
        budget.contracted_unit_seconds);
    if (finalized_paid > entitlement) return std::nullopt;
    return entitlement - finalized_paid;
}

} // namespace cybou
