// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

#include <cybou/storage_economy.h>
#include <cybou/storage_lease.h>

#include <boost/test/unit_test.hpp>

#include <limits>

BOOST_AUTO_TEST_SUITE(cybou_storage_economy_tests)

BOOST_AUTO_TEST_CASE(billing_units_round_up_to_512_kib_chunks)
{
    BOOST_CHECK_EQUAL(cybou::StorageBillingUnits(0), 0U);
    BOOST_CHECK_EQUAL(cybou::StorageBillingUnits(1), 1U);
    BOOST_CHECK_EQUAL(cybou::StorageBillingUnits(512 * 1024), 1U);
    BOOST_CHECK_EQUAL(cybou::StorageBillingUnits(512 * 1024 + 1), 2U);
    BOOST_CHECK_EQUAL(cybou::StorageBillingUnits(1ULL << 30), cybou::STORAGE_BILLING_UNITS_PER_GIB);
}

BOOST_AUTO_TEST_CASE(beta_rate_is_five_per_gib_day_replica)
{
    // DEC-279: 5 CYBOU / GiB / day / replica, so 10 per logical GiB per day at two replicas.
    BOOST_CHECK_EQUAL(*cybou::StorageRentPerDay(2048, 1), 5U);
    BOOST_CHECK_EQUAL(*cybou::StorageRentPerDay(2048, 2), 10U);
    BOOST_CHECK_EQUAL(*cybou::StorageRentPerDay(50 * 2048, 2), 500U);
    BOOST_CHECK_EQUAL(*cybou::StorageRentPerDay(1, 1), 0U);
}

BOOST_AUTO_TEST_CASE(remainder_carries_so_split_periods_equal_one_period)
{
    // Half a day of 1 GiB is 2.5 CYBOU: floor 2, then the carried half completes the 5.
    cybou::StorageRentAccumulator split;
    BOOST_REQUIRE(cybou::AccrueStorageRent(split, 2048, 43200, 1));
    BOOST_CHECK_EQUAL(split.cybou, 2U);
    BOOST_CHECK_GT(split.remainder, 0U);
    BOOST_REQUIRE(cybou::AccrueStorageRent(split, 2048, 43200, 1));
    BOOST_CHECK_EQUAL(split.cybou, 5U);
    BOOST_CHECK_EQUAL(split.remainder, 0U);

    // Many tiny periods add up exactly; nothing is lost or created by rounding.
    cybou::StorageRentAccumulator tiny;
    for (int i{0}; i < 86400; ++i) BOOST_REQUIRE(cybou::AccrueStorageRent(tiny, 2048, 1, 2));
    BOOST_CHECK_EQUAL(tiny.cybou, 10U);
    BOOST_CHECK_EQUAL(tiny.remainder, 0U);
}

BOOST_AUTO_TEST_CASE(overflow_is_rejected_without_changing_the_accumulator)
{
    cybou::StorageRentAccumulator accumulator{.cybou = 7, .remainder = 3};
    BOOST_CHECK(!cybou::AccrueStorageRent(accumulator, std::numeric_limits<std::uint64_t>::max(), 2, 1));
    BOOST_CHECK_EQUAL(accumulator.cybou, 7U);
    BOOST_CHECK_EQUAL(accumulator.remainder, 3U);
    BOOST_CHECK(!cybou::StorageRentPerDay(std::numeric_limits<std::uint64_t>::max(), 1));
}

BOOST_AUTO_TEST_CASE(assigned_terms_pay_only_cumulative_verified_service)
{
    for (const auto periods : {1ULL, 2ULL, 30ULL, 365ULL}) {
        for (const auto providers : {1ULL, 2ULL, 1000ULL}) {
            const auto budget = cybou::ComputeAssignedStorageBudget(1, providers, periods, 86400, 5);
            BOOST_REQUIRE(budget);
            BOOST_CHECK_EQUAL(budget->per_replica, 1U);
            BOOST_CHECK_EQUAL(budget->total, providers);
            BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, 0, 0), 0U);
            BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, budget->contracted_unit_seconds / 2, 0), 0U);
            BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, budget->contracted_unit_seconds, 0), 1U);
            BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, budget->contracted_unit_seconds, 1), 0U);
            if (periods > 1) BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, 86400, 0), 0U);
        }
    }
    const auto budget = cybou::ComputeAssignedStorageBudget(2048, 2, 30, 86400, 5);
    BOOST_REQUIRE(budget);
    BOOST_CHECK_EQUAL(budget->total, 300U);
    std::uint64_t paid{0};
    for (std::uint64_t day{1}; day <= 30; ++day) {
        const auto due = cybou::ComputeAssignedStoragePayout(*budget, 2048 * day * 86400, paid);
        BOOST_REQUIRE(due);
        BOOST_CHECK_EQUAL(*due, 5U);
        // A retry before finality has exactly the same quote.
        BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(*budget, 2048 * day * 86400, paid), *due);
        paid += *due;
    }
    BOOST_CHECK_EQUAL(paid, budget->per_replica);
    // Missing intervals do not count; a reopened caller uses the same durable
    // verified-service counter and canonical paid value, not current time.
    const auto reopened = *budget;
    BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(reopened, 2048 * 15ULL * 86400, 25), 50U);
    BOOST_CHECK(!cybou::ComputeAssignedStoragePayout(reopened, 2048 * 15ULL * 86400, 76));
}

BOOST_AUTO_TEST_CASE(assigned_arithmetic_rejects_invalid_counters_and_overflows)
{
    const auto max = std::numeric_limits<std::uint64_t>::max();
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(0, 2, 30, 86400, 5));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(1, 0, 30, 86400, 5));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(1, 2, 0, 86400, 5));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(1, 2, 30, 0, 5));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(1, 2, 30, 86400, 0));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(max, 2, 2, 1, 5));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(1, 2, max, 2, 5));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(max, 2, 1, 1, max));
    BOOST_CHECK(!cybou::ComputeAssignedStorageBudget(2048, max, 1, 86400, 5));
    const cybou::AssignedStorageBudget large{.per_replica = max, .total = max, .contracted_unit_seconds = max};
    BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(large, max, 0), max);
    BOOST_CHECK_EQUAL(*cybou::ComputeAssignedStoragePayout(large, max, max), 0U);
    BOOST_CHECK(!cybou::ComputeAssignedStoragePayout({}, 0, 0));
    const auto budget = cybou::ComputeAssignedStorageBudget(1, 2, 30, 86400, 5);
    BOOST_REQUIRE(budget);
    BOOST_CHECK(!cybou::ComputeAssignedStoragePayout(*budget, budget->contracted_unit_seconds + 1, 0));
    BOOST_CHECK(!cybou::ComputeAssignedStoragePayout(*budget, 0, 1));
    cybou::StorageRentAccumulator invalid{.cybou = 7, .remainder = max};
    BOOST_CHECK(!cybou::AccrueStorageRent(invalid, 1, 1, 1));
    BOOST_CHECK_EQUAL(invalid.cybou, 7U);
    BOOST_CHECK_EQUAL(invalid.remainder, max);
}

BOOST_AUTO_TEST_CASE(two_replica_funding_adds_at_most_one_cybou_per_term)
{
    const cybou::CybouProtocolParameters params;
    for (const auto periods : {1U, 2U, 30U, 365U}) {
        for (std::uint32_t units{1}; units <= 2048; ++units) {
            const auto current = cybou::ComputeStorageLeaseEscrow(params, units, 2, periods);
            const auto target = cybou::ComputeAssignedStorageBudget(units, 2, periods,
                params.storage_settlement_period_seconds, params.storage_rate_per_gib_day_replica);
            BOOST_REQUIRE(current && target);
            BOOST_CHECK_EQUAL(target->total, *current);
            const auto numerator = static_cast<std::uint64_t>(units) * 2 * periods *
                params.storage_settlement_period_seconds * params.storage_rate_per_gib_day_replica;
            const auto denominator = 2048ULL * 86400;
            const auto combined_ceil = numerator / denominator + (numerator % denominator != 0);
            BOOST_CHECK_GE(*current, combined_ceil);
            BOOST_CHECK_LE(*current - combined_ceil, 1U);
            BOOST_CHECK_EQUAL(target->total, 2 * target->per_replica);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
