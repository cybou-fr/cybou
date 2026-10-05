// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

#include <cybou/p2p/geo_database_updater.h>

#include <boost/test/unit_test.hpp>
#if defined(CYBOU_ENABLE_TEST_HOOKS)
#include <openssl/evp.h>
#include <zlib.h>
#include <array>
#include <fstream>
#include <future>
#include <vector>
#endif

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

#if defined(CYBOU_ENABLE_TEST_HOOKS)
namespace {
using Updater = cybou::p2p::GeoDatabaseUpdater;
constexpr std::string_view CSV{"51.255.46.0,51.255.46.255,FR\n8.8.8.0,8.8.8.255,US\n"};

std::string Digest(std::string_view bytes, const EVP_MD* algorithm)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned size{0};
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &size, algorithm, nullptr) != 1)
        throw std::runtime_error("test digest failed");
    constexpr char hex[]{"0123456789abcdef"};
    std::string result;
    for (unsigned i = 0; i < size; ++i) { result += hex[digest[i] >> 4]; result += hex[digest[i] & 15]; }
    return result;
}

std::string Gzip(std::string_view csv)
{
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        throw std::runtime_error("test gzip initialization failed");
    std::array<char, 4096> buffer{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(csv.data()));
    stream.avail_in = static_cast<uInt>(csv.size());
    stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
    stream.avail_out = buffer.size();
    const int result = deflate(&stream, Z_FINISH);
    const size_t size = buffer.size() - stream.avail_out;
    deflateEnd(&stream);
    if (result != Z_STREAM_END) throw std::runtime_error("test gzip failed");
    return std::string{buffer.data(), size};
}

struct GeoFixture {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("cybou-geo-retry-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::chrono::year_month_day today{std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
    std::chrono::year_month month{today.year(), today.month()};
    std::vector<std::chrono::milliseconds> delays;
    GeoFixture() { std::filesystem::create_directories(directory); }
    ~GeoFixture() { std::error_code ec; std::filesystem::remove_all(directory, ec); }

    std::string MonthText(std::chrono::year_month issued) const
    {
        const unsigned number = static_cast<unsigned>(issued.month());
        return std::to_string(static_cast<int>(issued.year())) + "-" + (number < 10 ? "0" : "") + std::to_string(number);
    }
    std::string Page(std::string_view sha1, std::chrono::year_month issued) const
    {
        constexpr std::array months{"January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"};
        return "<dt>Format</dt><dd>CSV</dd><dt>Release</dt><dd>" +
            std::string{months[static_cast<unsigned>(issued.month()) - 1]} + " " +
            std::to_string(static_cast<int>(issued.year())) + "</dd><dt>SHA1SUM</dt><dd>" +
            std::string{sha1} + "</dd><a href='https://download.db-ip.com/free/dbip-country-lite-" +
            MonthText(issued) + ".csv.gz'>Download IP to Country Lite CSV</a>";
    }
    std::filesystem::path CachePath(std::string_view csv) const
    {
        return directory / ("dbip-country-lite-" + MonthText(month) + "-" + Digest(csv, EVP_sha256()) + ".csv");
    }
    void Write(const std::filesystem::path& path, std::string_view bytes)
    {
        std::ofstream file{path, std::ios::binary}; file << bytes;
        if (!file) throw std::runtime_error("test write failed");
    }
    auto Create(Updater::FetchForTest fetch)
    {
        return Updater::CreateForTest(directory, std::move(fetch), [this](auto stop, auto delay) {
            delays.push_back(delay); return !stop.stop_requested();
        });
    }
};
} // namespace

BOOST_FIXTURE_TEST_CASE(fresh_directory_refetches_metadata_after_checksum_mismatch, GeoFixture)
{
    const auto archive = Gzip(CSV);
    BOOST_REQUIRE(Digest(archive, EVP_sha1()) != Digest(CSV, EVP_sha1()));
    unsigned pages{0}, archives{0};
    std::vector<std::string> requests;
    auto updater = Create([&](auto host, auto path, size_t limit, const auto& headers) {
        BOOST_CHECK_EQUAL(headers.at("Cache-Control"), "no-cache");
        BOOST_CHECK_EQUAL(headers.at("Pragma"), "no-cache");
        BOOST_CHECK_EQUAL(headers.at("Accept-Encoding"), "identity");
        BOOST_CHECK_GT(limit, 0U);
        requests.emplace_back(host);
        if (host == "db-ip.com") {
            BOOST_CHECK_EQUAL(path, "/db/download/ip-to-country-lite");
            return Page(++pages == 1 ? std::string(40, '0') : Digest(CSV, EVP_sha1()), month);
        }
        ++archives;
        BOOST_CHECK_EQUAL(host, "download.db-ip.com");
        BOOST_CHECK_EQUAL(path, "/free/dbip-country-lite-" + MonthText(month) + ".csv.gz");
        return archive;
    });
    const auto policy = cybou::p2p::PeerAdmissionPolicy::PublicWithUpdater(updater);
    BOOST_CHECK(!policy.Ready()); BOOST_CHECK(!policy.Allows("51.255.46.58"));
    BOOST_REQUIRE(updater->RefreshForTest());
    BOOST_CHECK(policy.Ready()); BOOST_CHECK(policy.Allows("51.255.46.58"));
    BOOST_CHECK(!policy.Allows("8.8.8.8"));
    BOOST_CHECK_EQUAL(pages, 2U); BOOST_CHECK_EQUAL(archives, 2U);
    BOOST_REQUIRE_EQUAL(requests.size(), 4U);
    BOOST_CHECK_EQUAL(requests[0], requests[2]); BOOST_CHECK_EQUAL(requests[1], requests[3]);
    BOOST_REQUIRE_EQUAL(delays.size(), 2U); BOOST_CHECK(delays[1] == std::chrono::seconds{2});
    BOOST_CHECK(std::filesystem::exists(CachePath(CSV)));
    BOOST_CHECK(!std::filesystem::exists(CachePath(CSV).string() + ".part"));
    BOOST_CHECK(updater->NextDelayForTest(true) == std::chrono::days{14});
    auto restarted = Create([](auto, auto, auto, const auto&) -> std::string { throw std::runtime_error("no fetch"); });
    BOOST_CHECK(restarted->Ready());
}

BOOST_FIXTURE_TEST_CASE(exhausted_retries_remain_fail_closed_or_preserve_valid_cache, GeoFixture)
{
    const auto archive = Gzip(CSV);
    for (const bool cached : {false, true}) {
        if (cached) Write(CachePath(CSV), CSV);
        unsigned pages{0}, archives{0};
        delays.clear();
        auto updater = Create([&](auto host, auto, auto, const auto&) {
            if (host == "db-ip.com") { ++pages; return Page(std::string(40, '0'), month + std::chrono::months{1}); }
            ++archives; return archive;
        });
        const auto before = updater->CurrentDataset();
        BOOST_CHECK(!updater->RefreshForTest());
        BOOST_CHECK_EQUAL(pages, 5U); BOOST_CHECK_EQUAL(archives, 5U);
        BOOST_CHECK_EQUAL(updater->Ready(), cached); BOOST_CHECK(updater->CurrentDataset() == before);
        BOOST_CHECK(updater->NextDelayForTest(false) == (cached ? std::chrono::minutes{60} : std::chrono::minutes{5}));
        const std::vector<std::chrono::milliseconds> expected{std::chrono::seconds{0}, std::chrono::seconds{2},
            std::chrono::seconds{5}, std::chrono::seconds{15}, std::chrono::seconds{60}};
        BOOST_CHECK(delays == expected);
    }
}

BOOST_FIXTURE_TEST_CASE(invalid_gzip_and_malformed_csv_retry_before_activation, GeoFixture)
{
    const auto good = Gzip(CSV);
    const std::array bad{std::string{"not gzip"}, Gzip("invalid csv\n")};
    for (size_t variant = 0; variant < bad.size(); ++variant) {
        auto subdirectory = directory / std::to_string(variant);
        unsigned attempts{0};
        std::shared_ptr<Updater> updater;
        updater = Updater::CreateForTest(subdirectory, [&](auto host, auto, auto, const auto&) {
            if (host == "db-ip.com") {
                ++attempts;
                if (attempts == 2) { BOOST_CHECK(!updater->Ready()); }
                return Page(Digest(attempts == 1 && variant == 1 ? std::string_view{"invalid csv\n"} : CSV,
                    EVP_sha1()), month);
            }
            return attempts == 1 ? bad[variant] : good;
        }, [](auto, auto) { return true; });
        BOOST_REQUIRE(updater->RefreshForTest());
        BOOST_CHECK_EQUAL(attempts, 2U); BOOST_CHECK(updater->Ready());
        BOOST_CHECK_EQUAL(std::distance(std::filesystem::directory_iterator{subdirectory},
            std::filesystem::directory_iterator{}), 1);
    }
}

BOOST_FIXTURE_TEST_CASE(current_release_skips_archive_and_uses_normal_interval, GeoFixture)
{
    Write(CachePath(CSV), CSV);
    unsigned requests{0};
    auto updater = Create([&](auto host, auto, auto, const auto&) {
        ++requests; BOOST_CHECK_EQUAL(host, "db-ip.com"); return Page(std::string(40, '0'), month);
    });
    const auto before = updater->CurrentDataset();
    BOOST_REQUIRE(updater->RefreshForTest());
    BOOST_CHECK_EQUAL(requests, 1U); BOOST_CHECK(updater->CurrentDataset() == before);
    BOOST_CHECK(updater->NextDelayForTest(true) == std::chrono::days{14});
}

BOOST_FIXTURE_TEST_CASE(crash_part_cleanup_is_strict_and_never_activates_candidate, GeoFixture)
{
    const auto part = CachePath(CSV).string() + ".part";
    Write(part, CSV); Write(directory / "unrelated.part", CSV);
    Write(directory / "dbip-country-lite-2026-10-not-a-digest.csv.part", CSV);
    auto updater = Create([](auto, auto, auto, const auto&) -> std::string { throw std::runtime_error("no fetch"); });
    BOOST_CHECK(!updater->Ready()); BOOST_CHECK(!std::filesystem::exists(part));
    BOOST_CHECK(std::filesystem::exists(directory / "unrelated.part"));
    BOOST_CHECK(std::filesystem::exists(directory / "dbip-country-lite-2026-10-not-a-digest.csv.part"));
}

BOOST_FIXTURE_TEST_CASE(expired_cache_never_counts_as_current, GeoFixture)
{
    const auto old = month - std::chrono::months{2};
    Write(directory / ("dbip-country-lite-" + MonthText(old) + "-" + Digest(CSV, EVP_sha256()) + ".csv"), CSV);
    auto updater = Create([&](auto, auto, auto, const auto&) { return Page(std::string(40, '0'), old); });
    BOOST_CHECK(!updater->Ready()); BOOST_CHECK(!updater->RefreshForTest());
    BOOST_CHECK(updater->NextDelayForTest(false) == std::chrono::minutes{5});
}

BOOST_FIXTURE_TEST_CASE(stop_token_interrupts_real_retry_wait, GeoFixture)
{
    std::promise<void> attempted;
    std::promise<void> finished;
    unsigned requests{0};
    auto updater = Updater::CreateForTest(directory, [&](auto, auto, auto, const auto&) -> std::string {
        if (++requests == 1) attempted.set_value();
        throw std::runtime_error("simulated unavailable");
    });
    bool success{true};
    std::jthread worker{[&](auto stop) { success = updater->RefreshForTest(stop); finished.set_value(); }};
    BOOST_REQUIRE(attempted.get_future().wait_for(std::chrono::seconds{1}) == std::future_status::ready);
    BOOST_CHECK(finished.get_future().wait_for(std::chrono::milliseconds{50}) == std::future_status::timeout);
    const auto start = std::chrono::steady_clock::now();
    worker.request_stop(); worker.join();
    BOOST_CHECK(!success); BOOST_CHECK_EQUAL(requests, 1U);
    BOOST_CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{1});
}
#endif

BOOST_AUTO_TEST_SUITE_END()
