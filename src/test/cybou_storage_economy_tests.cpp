// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_economy.h>

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

BOOST_AUTO_TEST_SUITE_END()
