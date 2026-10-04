// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

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

} // namespace cybou
