// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/geo_database_updater.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(cybou_geo_database_updater_tests)

BOOST_AUTO_TEST_CASE(parses_only_official_csv_release_metadata)
{
    constexpr std::string_view page = R"html(
        <dl class="card-body">
          <dt>Format</dt><dd>CSV</dd>
          <dt>Release</dt><dd>October 2026</dd>
          <dt>File size</dt><dd>29.8 MB</dd>
          <dt>SHA1SUM</dt><dd class="small">c8ad1dfe98cb29bcacd82a5c3ec361c880e5bb59</dd>
        </dl>
        <a href='https://download.db-ip.com/free/dbip-country-lite-2026-10.csv.gz'>
          Download IP to Country Lite CSV
        </a>
        <a href='https://download.db-ip.com/free/dbip-country-lite-2026-10.mmdb.gz'>
          Download IP to Country Lite MMDB
        </a>
    )html";
    const auto release = cybou::p2p::GeoDatabaseUpdater::ParseOfficialReleasePage(page);
    BOOST_REQUIRE(release);
    BOOST_CHECK_EQUAL(static_cast<int>(release->month.year()), 2026);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(release->month.month()), 10);
    BOOST_CHECK_EQUAL(release->download_path, "/free/dbip-country-lite-2026-10.csv.gz");
    BOOST_CHECK_EQUAL(release->sha1, "c8ad1dfe98cb29bcacd82a5c3ec361c880e5bb59");
}

BOOST_AUTO_TEST_CASE(rejects_untrusted_or_inconsistent_release_metadata)
{
    constexpr std::string_view wrong_host = R"html(
        <dt>Format</dt><dd>CSV</dd><dt>Release</dt><dd>October 2026</dd>
        <dt>SHA1SUM</dt><dd class="small">c8ad1dfe98cb29bcacd82a5c3ec361c880e5bb59</dd>
        <a href='https://attacker.invalid/dbip-country-lite-2026-10.csv.gz'>
          Download IP to Country Lite CSV</a>)html";
    constexpr std::string_view mismatched_month = R"html(
        <dt>Format</dt><dd>CSV</dd><dt>Release</dt><dd>October 2026</dd>
        <dt>SHA1SUM</dt><dd class="small">c8ad1dfe98cb29bcacd82a5c3ec361c880e5bb59</dd>
        <a href='https://download.db-ip.com/free/dbip-country-lite-2026-09.csv.gz'>
          Download IP to Country Lite CSV</a>)html";
    BOOST_CHECK(!cybou::p2p::GeoDatabaseUpdater::ParseOfficialReleasePage(wrong_host));
    BOOST_CHECK(!cybou::p2p::GeoDatabaseUpdater::ParseOfficialReleasePage(mismatched_month));
    BOOST_CHECK(!cybou::p2p::GeoDatabaseUpdater::ParseOfficialReleasePage("<html>unavailable</html>"));
}

BOOST_AUTO_TEST_SUITE_END()
