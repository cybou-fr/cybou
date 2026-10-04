// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Реализация фонового обновления локального Geo-датасета DB-IP Lite.

#include <cybou/p2p/geo_database_updater.h>

#include <cybou/crypto/sha256.h>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>

#include <openssl/evp.h>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#endif
#include <zlib.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cybou::p2p {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

constexpr std::string_view DB_IP_PAGE_HOST{"db-ip.com"};
constexpr std::string_view DB_IP_PAGE_PATH{"/db/download/ip-to-country-lite"};
constexpr std::string_view DB_IP_FILE_HOST{"download.db-ip.com"};
constexpr size_t MAX_PAGE_BYTES{2 * 1024 * 1024};
constexpr size_t MAX_GZIP_BYTES{64 * 1024 * 1024};
constexpr size_t MAX_CSV_BYTES{64 * 1024 * 1024};
constexpr auto UPDATE_INTERVAL{std::chrono::days{14}};
constexpr auto FAILED_UPDATE_RETRY_INTERVAL{std::chrono::hours{1}};
constexpr auto NO_DATABASE_RETRY_INTERVAL{std::chrono::minutes{5}};
constexpr std::array RETRY_DELAYS{
    std::chrono::seconds{0}, std::chrono::seconds{2}, std::chrono::seconds{5},
    std::chrono::seconds{15}, std::chrono::seconds{60},
};

/// \brief Форматирует `year_month` как `YYYY-MM` для имен кэша и журналов.
std::string MonthText(const std::chrono::year_month month)
{
    return std::to_string(static_cast<int>(month.year())) + "-" +
        (static_cast<unsigned>(month.month()) < 10 ? "0" : "") +
        std::to_string(static_cast<unsigned>(month.month()));
}

/// \brief Нормализует ASCII-строку к нижнему регистру без локали.
std::string ToLowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](const char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return text;
}

/// \brief Собирает строгий HTTPS GET без кэширования для официального DB-IP источника.
http::request<http::empty_body> GeoRequest(std::string_view host, std::string_view target)
{
    http::request<http::empty_body> request{http::verb::get, std::string{target}, 11};
    request.set(http::field::host, host);
    request.set(http::field::user_agent, "CYBOU Geo database updater/1.0");
    request.set(http::field::accept, "text/html,application/octet-stream");
    request.set(http::field::cache_control, "no-cache");
    request.set(http::field::pragma, "no-cache");
    request.set(http::field::accept_encoding, "identity");
    request.keep_alive(false);
    return request;
}

#ifdef _WIN32
void AddWindowsRootCertificates(asio::ssl::context& tls)
{
    HCERTSTORE store = CertOpenSystemStoreW(static_cast<HCRYPTPROV_LEGACY>(0), L"ROOT");
    if (!store) throw std::runtime_error("cannot open Windows trusted root certificate store");
    X509_STORE* roots = SSL_CTX_get_cert_store(tls.native_handle());
    PCCERT_CONTEXT context{nullptr};
    while ((context = CertEnumCertificatesInStore(store, context)) != nullptr) {
        const unsigned char* encoded = context->pbCertEncoded;
        X509* certificate = d2i_X509(nullptr, &encoded, static_cast<long>(context->cbCertEncoded));
        if (certificate) {
            if (X509_STORE_add_cert(roots, certificate) != 1) ERR_clear_error();
            X509_free(certificate);
        }
    }
    CertCloseStore(store, 0);
}
#endif

std::string HttpsGet(const std::string_view host, const std::string_view target, const size_t body_limit)
{
    asio::io_context io;
    asio::ssl::context tls{asio::ssl::context::tls_client};
    tls.set_default_verify_paths();
#ifdef _WIN32
    AddWindowsRootCertificates(tls);
#endif
    tls.set_verify_mode(asio::ssl::verify_peer);
    tcp::resolver resolver{io};
    beast::ssl_stream<beast::tcp_stream> stream{io, tls};
    const std::string host_string{host};
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host_string.c_str())) {
        throw std::runtime_error("cannot set Geo update TLS server name");
    }
    stream.set_verify_callback(asio::ssl::host_name_verification(host_string));
    const auto endpoints = resolver.resolve(host_string, "443");
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds{20});
    beast::get_lowest_layer(stream).connect(endpoints);
    stream.handshake(asio::ssl::stream_base::client);

    const auto request = GeoRequest(host, target);
    http::write(stream, request);

    beast::flat_buffer buffer;
    http::response_parser<http::string_body> parser;
    parser.body_limit(body_limit);
    beast::get_lowest_layer(stream).expires_after(std::chrono::seconds{45});
    http::read(stream, buffer, parser);
    const auto response = parser.release();
    beast::error_code ignored;
    stream.shutdown(ignored);
    if (response.result() != http::status::ok) throw std::runtime_error("Geo update server returned a non-200 response");
    return response.body();
}

using Release = GeoDatabaseRelease;

std::optional<Release> ParseReleasePage(const std::string_view page)
{
    const std::string html{page};
    std::smatch card;
    const std::regex csv_card{
        R"(<dt>\s*Format\s*</dt>\s*<dd>\s*CSV\s*</dd>([\s\S]*?)Download IP to Country Lite CSV)",
        std::regex::icase};
    if (!std::regex_search(html, card, csv_card)) return std::nullopt;
    const std::string card_html = card[1].str();

    std::smatch match;
    const std::regex release_regex{R"(<dt>\s*Release\s*</dt>\s*<dd>\s*([A-Za-z]+)\s+([0-9]{4})\s*</dd>)",
        std::regex::icase};
    if (!std::regex_search(card_html, match, release_regex)) return std::nullopt;
    static const std::array<std::string_view, 12> months{
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december"};
    std::string month_name = ToLowerAscii(match[1].str());
    const auto month_pos = std::find(months.begin(), months.end(), month_name);
    if (month_pos == months.end()) return std::nullopt;
    const int year_value = std::stoi(match[2].str());
    const auto month_value = static_cast<unsigned>(std::distance(months.begin(), month_pos) + 1);
    const auto month = std::chrono::year{year_value} / month_value;
    if (!month.ok()) return std::nullopt;

    const std::regex sha1_regex{R"(<dt>\s*SHA1SUM\s*</dt>\s*<dd[^>]*>\s*([a-f0-9]{40})\s*</dd>)",
        std::regex::icase};
    if (!std::regex_search(card_html, match, sha1_regex)) return std::nullopt;
    std::string sha1 = ToLowerAscii(match[1].str());

    const std::regex link_regex{
        R"(href=['"]https://download\.db-ip\.com/free/(dbip-country-lite-([0-9]{4})-([0-9]{2})\.csv\.gz)['"])",
        std::regex::icase};
    if (!std::regex_search(html, match, link_regex)) return std::nullopt;
    unsigned linked_year{0}, linked_month{0};
    const auto year_text = match[2].str();
    const auto month_text = match[3].str();
    const auto year_result = std::from_chars(year_text.data(), year_text.data() + year_text.size(), linked_year);
    const auto month_result = std::from_chars(month_text.data(), month_text.data() + month_text.size(), linked_month);
    if (year_result.ec != std::errc{} || month_result.ec != std::errc{} ||
        linked_year != year_value || linked_month != month_value) return std::nullopt;
    return Release{month, "/free/" + match[1].str(), sha1};
}

std::array<unsigned char, 20> ComputeSha1(const std::string_view bytes)
{
    std::array<unsigned char, 20> digest{};
    unsigned int digest_size{0};
    if (EVP_Digest(bytes.data(), bytes.size(), digest.data(), &digest_size, EVP_sha1(), nullptr) != 1 ||
        digest_size != digest.size()) throw std::runtime_error("cannot compute Geo archive SHA-1");
    return digest;
}

std::string Hex(const std::span<const unsigned char> bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const auto byte : bytes) {
        text.push_back(digits[byte >> 4]);
        text.push_back(digits[byte & 0x0f]);
    }
    return text;
}

std::string Gunzip(const std::string_view compressed)
{
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
    stream.avail_in = static_cast<uInt>(compressed.size());
    if (inflateInit2(&stream, MAX_WBITS + 16) != Z_OK) throw std::runtime_error("cannot initialize Geo gzip decoder");
    std::string output;
    std::array<char, 64 * 1024> buffer{};
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        result = inflate(&stream, Z_NO_FLUSH);
        const auto produced = buffer.size() - stream.avail_out;
        // Лимит проверяется до append: переполнение CSV-памяти должно
        // завершать обновление fail-closed еще до публикации частичных данных.
        if (produced > MAX_CSV_BYTES - output.size()) {
            inflateEnd(&stream);
            throw std::runtime_error("Geo CSV exceeds the maximum allowed size");
        }
        output.append(buffer.data(), produced);
    }
    const bool complete = result == Z_STREAM_END && stream.avail_in == 0;
    inflateEnd(&stream);
    if (!complete || output.empty()) throw std::runtime_error("Geo gzip archive is invalid or truncated");
    return output;
}

std::shared_ptr<const FrenchIpDataset> LoadCachedDataset(const std::filesystem::path& path,
    const std::chrono::year_month month, const std::string_view sha256)
{
    if (!month.ok()) return nullptr;
    std::array<unsigned char, 32> digest{};
    if (sha256.size() != digest.size() * 2) return nullptr;
    for (size_t i = 0; i < digest.size(); ++i) {
        unsigned value{0};
        const auto parsed = std::from_chars(sha256.data() + i * 2, sha256.data() + i * 2 + 2, value, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != sha256.data() + i * 2 + 2 || value > 255) return nullptr;
        digest[i] = static_cast<unsigned char>(value);
    }
    return FrenchIpDataset::LoadDbIpCountryCsv(path, digest, month);
}

} // namespace

std::optional<GeoDatabaseRelease> GeoDatabaseUpdater::ParseOfficialReleasePage(const std::string_view page)
{
    return ParseReleasePage(page);
}

std::shared_ptr<GeoDatabaseUpdater> GeoDatabaseUpdater::Start(const std::filesystem::path& data_directory)
{
    auto updater = std::shared_ptr<GeoDatabaseUpdater>{new GeoDatabaseUpdater{data_directory}};
    updater->LoadCached();
    updater->m_worker = std::jthread{[raw = updater.get()](const std::stop_token stop) { raw->Run(stop); }};
    return updater;
}

GeoDatabaseUpdater::GeoDatabaseUpdater(std::filesystem::path data_directory)
    : m_data_directory{std::move(data_directory)}
{
}

bool GeoDatabaseUpdater::WaitUntilReady(const std::chrono::milliseconds timeout)
{
    std::unique_lock lock{m_ready_mutex};
    m_ready_changed.wait_for(lock, timeout, [this] { return m_first_cycle_done || Ready(); });
    return Ready();
}

GeoDatabaseUpdater::~GeoDatabaseUpdater()
{
    if (m_worker.joinable()) {
        m_worker.request_stop();
        m_wakeup.notify_all();
        m_worker.join();
    }
}

std::shared_ptr<const FrenchIpDataset> GeoDatabaseUpdater::CurrentDataset() const
{
    const auto snapshot = m_current.load();
    if (!snapshot) return nullptr;
    const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    const auto issued = std::chrono::sys_days{snapshot->issued_month / 1};
    if (today < issued || today - issued > std::chrono::days{45}) return nullptr;
    return snapshot->dataset;
}

void GeoDatabaseUpdater::LoadCached()
{
    std::error_code ec;
    if (!std::filesystem::is_directory(m_data_directory, ec)) return;
    const std::regex part_name{R"(dbip-country-lite-[0-9]{4}-[0-9]{2}-[a-f0-9]{64}\.csv\.part)"};
    const std::regex cache_name{R"(dbip-country-lite-([0-9]{4})-([0-9]{2})-([a-f0-9]{64})\.csv)"};
    std::vector<std::filesystem::path> candidates;
    for (std::filesystem::directory_iterator it{m_data_directory, ec}, end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (std::regex_match(it->path().filename().string(), part_name)) {
            // Оставшийся `.part` означает незавершенную прежнюю установку; в
            // кэш допускаются только полностью провалидированные atomically named CSV.
            std::error_code ignored;
            std::filesystem::remove(it->path(), ignored);
            continue;
        }
        if (std::regex_match(it->path().filename().string(), cache_name)) candidates.push_back(it->path());
    }
    std::sort(candidates.begin(), candidates.end(), std::greater<>());
    for (const auto& path : candidates) {
        std::smatch match;
        const auto filename = path.filename().string();
        if (!std::regex_match(filename, match, cache_name)) continue;
        const auto month = FrenchIpDataset::ParseIssuedMonth(match[1].str() + "-" + match[2].str());
        if (!month) continue;
        const auto dataset = LoadCachedDataset(path, *month, match[3].str());
        if (dataset) {
            m_current.store(std::make_shared<const Snapshot>(Snapshot{dataset, *month}));
            return;
        }
    }
}

GeoDatabaseUpdater::RefreshResult GeoDatabaseUpdater::RefreshOnce()
{
    const auto page = Fetch(DB_IP_PAGE_HOST, DB_IP_PAGE_PATH, MAX_PAGE_BYTES);
    const auto release = ParseOfficialReleasePage(page);
    if (!release) throw std::runtime_error("cannot parse the official DB-IP Lite release page");

    const auto current = m_current.load();
    // Уже валидный локальный месяц не понижаем и не перекачиваем заново:
    // downgrade недопустим, а повторная установка той же версии только расходует сеть.
    if (current && CurrentDataset() && release->month <= current->issued_month) return RefreshResult::CURRENT;
    const auto compressed = Fetch(DB_IP_FILE_HOST, release->download_path, MAX_GZIP_BYTES);
    const auto csv = Gunzip(compressed);
    // DB-IP publishes the checksum of the uncompressed CSV, not its gzip envelope.
    if (Hex(ComputeSha1(csv)) != release->sha1) {
        throw std::runtime_error("DB-IP Geo CSV does not match its published SHA-1");
    }
    std::array<unsigned char, 32> sha256{};
    if (!crypto::ComputeSha256({std::span<const unsigned char>{
            reinterpret_cast<const unsigned char*>(csv.data()), csv.size()}}, sha256.data())) {
        throw std::runtime_error("cannot compute Geo CSV SHA-256");
    }
    const auto digest = Hex(sha256);
    const auto month_text = MonthText(release->month);
    const auto temporary_directory = m_data_directory;
    std::filesystem::create_directories(temporary_directory);
    const auto target = temporary_directory / ("dbip-country-lite-" + month_text + "-" + digest + ".csv");
    const auto temp = target.string() + ".part";
    {
        std::ofstream output{temp, std::ios::binary | std::ios::trunc};
        if (!output) throw std::runtime_error("cannot create Geo database cache file");
        output.write(csv.data(), static_cast<std::streamsize>(csv.size()));
        output.flush();
        if (!output) throw std::runtime_error("cannot write Geo database cache file");
    }
    // Перед atomically visible rename повторно валидируем ровно тот файл,
    // который собираемся опубликовать в кэш, чтобы не разделять «скачано» и «разрешено к использованию».
    const auto dataset = LoadCachedDataset(temp, release->month, digest);
    if (!dataset) {
        std::filesystem::remove(temp);
        throw std::runtime_error("downloaded DB-IP Geo CSV failed validation");
    }
    std::error_code ec;
    std::filesystem::rename(temp, target, ec);
    if (ec && !std::filesystem::exists(target)) {
        std::filesystem::remove(temp);
        throw std::runtime_error("cannot atomically install Geo database cache file");
    }
    if (ec) std::filesystem::remove(temp);
    m_current.store(std::make_shared<const Snapshot>(Snapshot{dataset, release->month}));
    std::clog << "Updated DB-IP Lite country database to " << month_text << " (SHA-256 " << digest << ")\n";
    return RefreshResult::UPDATED;
}

std::string GeoDatabaseUpdater::Fetch(std::string_view host, std::string_view path, size_t limit)
{
#if defined(CYBOU_ENABLE_TEST_HOOKS)
    if (m_fetch_for_test) {
        std::map<std::string, std::string> headers;
        for (const auto& field : GeoRequest(host, path)) {
            headers.emplace(std::string{field.name_string()}, std::string{field.value()});
        }
        auto body = m_fetch_for_test(host, path, limit, headers);
        if (body.size() > limit) throw std::runtime_error("Geo response exceeds size limit");
        return body;
    }
#endif
    return HttpsGet(host, path, limit);
}

bool GeoDatabaseUpdater::Wait(const std::stop_token stop, const std::chrono::milliseconds delay)
{
    if (stop.stop_requested()) return false;
#if defined(CYBOU_ENABLE_TEST_HOOKS)
    if (m_wait_for_test) return m_wait_for_test(stop, delay) && !stop.stop_requested();
#endif
    std::unique_lock lock{m_wait_mutex};
    m_wakeup.wait_for(lock, stop, delay, [] { return false; });
    return !stop.stop_requested();
}

bool GeoDatabaseUpdater::RefreshWithRetries(const std::stop_token stop)
{
    for (size_t attempt = 0; attempt < RETRY_DELAYS.size(); ++attempt) {
        if (attempt > 0) std::clog << "DB-IP Geo update retry in " << RETRY_DELAYS[attempt].count() << "s\n";
        if (!Wait(stop, RETRY_DELAYS[attempt])) return false;
        try {
            const auto result = RefreshOnce();
            std::clog << "DB-IP Geo update attempt " << attempt + 1 << "/5 succeeded: "
                      << (result == RefreshResult::UPDATED ? "updated" : "current") << '\n';
            return true;
        } catch (const std::exception& error) {
            if (stop.stop_requested()) return false;
            std::clog << "DB-IP Geo update attempt " << attempt + 1 << "/5 failed: " << error.what() << '\n';
        } catch (...) {
            std::clog << "DB-IP Geo update attempt " << attempt + 1 << "/5 failed: unknown error\n";
        }
    }
    return false;
}

std::chrono::milliseconds GeoDatabaseUpdater::NextRefreshDelay(const bool success) const
{
    if (success) return UPDATE_INTERVAL;
    return CurrentDataset() ? std::chrono::milliseconds{FAILED_UPDATE_RETRY_INTERVAL}
                            : std::chrono::milliseconds{NO_DATABASE_RETRY_INTERVAL};
}

#if defined(CYBOU_ENABLE_TEST_HOOKS)
std::shared_ptr<GeoDatabaseUpdater> GeoDatabaseUpdater::CreateForTest(const std::filesystem::path& directory,
    FetchForTest fetch, WaitForTest wait)
{
    if (!fetch) throw std::invalid_argument("test Geo updater requires a fetch callback");
    auto updater = std::shared_ptr<GeoDatabaseUpdater>{new GeoDatabaseUpdater{directory}};
    updater->m_fetch_for_test = std::move(fetch);
    updater->m_wait_for_test = std::move(wait);
    updater->LoadCached();
    return updater;
}
#endif

void GeoDatabaseUpdater::Run(const std::stop_token stop)
{
    while (!stop.stop_requested()) {
        const bool success = RefreshWithRetries(stop);
        {
            std::lock_guard lock{m_ready_mutex};
            m_first_cycle_done = true;
        }
        m_ready_changed.notify_all();
        if (stop.stop_requested()) break;
        const auto next = NextRefreshDelay(success);
        if (!success) {
            std::clog << "DB-IP Geo update unavailable after 5 attempts; "
                      << (CurrentDataset() ? "valid cached dataset retained; retry in 1h"
                          : "no valid Geo dataset; public P2P remains fail-closed; retry in 5m") << '\n';
        }
        if (!Wait(stop, next)) break;
    }
}

} // namespace cybou::p2p
