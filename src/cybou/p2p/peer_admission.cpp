// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Реализация локального географического допуска пиров по датасету DB-IP Lite.

#include <cybou/p2p/peer_admission.h>
#include <cybou/p2p/geo_database_updater.h>

#include <cybou/crypto/sha256.h>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <span>
#include <system_error>

namespace cybou::p2p {
namespace {

constexpr uintmax_t MAX_DATASET_BYTES{64 * 1024 * 1024};
constexpr size_t MAX_RECORDS{1'000'000};
constexpr std::chrono::days MAX_DATASET_AGE{45};

/// \brief Возвращает следующую CSV-строку без копирования и нормализует `CRLF`.
std::optional<std::string_view> NextLine(const std::string_view text, size_t& offset)
{
    if (offset >= text.size()) return std::nullopt;
    const size_t start = offset;
    size_t end = text.find('\n', start);
    offset = end == std::string_view::npos ? text.size() : end + 1;
    if (end == std::string_view::npos) end = text.size();
    if (end > start && text[end - 1] == '\r') --end;
    return text.substr(start, end - start);
}

/// \brief Нормализует IPv4/IPv6 адрес в 16-байтовую форму для диапазонного сравнения.
/// \details IPv4-mapped IPv6 схлопываются обратно в IPv4, чтобы один и тот же
///          числовой адрес не мог обойти локальную политику разными текстовыми формами.
std::optional<std::pair<std::array<unsigned char, 16>, bool>> AddressBytes(std::string_view text)
{
    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(std::string{text}, ec);
    if (ec) return std::nullopt;
    if (address.is_v6() && address.to_v6().is_v4_mapped()) {
        const auto mapped = address.to_v6().to_bytes();
        boost::asio::ip::address_v4::bytes_type v4{};
        std::copy_n(mapped.end() - static_cast<std::ptrdiff_t>(v4.size()), v4.size(), v4.begin());
        address = boost::asio::ip::address_v4{v4};
    }
    std::array<unsigned char, 16> bytes{};
    if (address.is_v4()) {
        const auto v4 = address.to_v4().to_bytes();
        std::copy(v4.begin(), v4.end(), bytes.begin());
    } else {
        bytes = address.to_v6().to_bytes();
    }
    return std::pair{bytes, address.is_v6()};
}

} // namespace

std::optional<std::chrono::year_month> FrenchIpDataset::ParseIssuedMonth(const std::string_view text)
{
    if (text.size() != 7 || text[4] != '-') return std::nullopt;
    unsigned year_value{0};
    unsigned month_value{0};
    const auto year_parse = std::from_chars(text.data(), text.data() + 4, year_value);
    const auto month_parse = std::from_chars(text.data() + 5, text.data() + 7, month_value);
    if (year_parse.ec != std::errc{} || year_parse.ptr != text.data() + 4 ||
        month_parse.ec != std::errc{} || month_parse.ptr != text.data() + 7 || year_value < 1970 ||
        year_value > 9999 || month_value < 1 || month_value > 12) return std::nullopt;
    const auto result = std::chrono::year{static_cast<int>(year_value)} / month_value;
    return result.ok() ? std::optional{result} : std::nullopt;
}

std::shared_ptr<const FrenchIpDataset> FrenchIpDataset::LoadDbIpCountryCsv(const std::filesystem::path& path,
    const std::array<unsigned char, 32>& expected_sha256, const std::chrono::year_month issued_month,
    const std::chrono::sys_days today)
{
    try {
        if (!issued_month.ok()) return nullptr;
        const auto issued = std::chrono::sys_days{issued_month / 1};
        // Возраст проверяется до чтения файла: будущий или просроченный датасет
        // не должен даже частично использоваться для публичного допуска.
        if (today < issued || today - issued > MAX_DATASET_AGE) return nullptr;
        std::error_code ec;
        const auto size = std::filesystem::file_size(path, ec);
        if (ec || size == 0 || size > MAX_DATASET_BYTES) return nullptr;
        std::ifstream input{path, std::ios::binary};
        if (!input) return nullptr;
        std::string bytes(static_cast<size_t>(size), '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof()) return nullptr;
        std::array<unsigned char, 32> actual_sha256{};
        if (!crypto::ComputeSha256({std::span<const unsigned char>{
                reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()}},
                actual_sha256.data()) || actual_sha256 != expected_sha256) return nullptr;

        const std::string_view csv{bytes};
        std::vector<Range> ranges;
        ranges.reserve(std::min<size_t>(MAX_RECORDS, bytes.size() / 24));
        size_t records{0};
        size_t offset{0};
        while (true) {
            const auto line = NextLine(csv, offset);
            if (!line) break;
            if (line->empty() || ++records > MAX_RECORDS) return nullptr;
            // CSV строго фиксирован: лишние столбцы, пустые поля и перекрывающиеся
            // диапазоны считаем повреждением, а не пытаемся «починить» локально.
            const auto first_comma = line->find(',');
            const auto second_comma = first_comma == std::string_view::npos ? first_comma : line->find(',', first_comma + 1);
            if (first_comma == std::string_view::npos || second_comma == std::string_view::npos ||
                line->find(',', second_comma + 1) != std::string_view::npos || first_comma == 0 ||
                second_comma == first_comma + 1 || second_comma + 3 != line->size()) return nullptr;
            const auto first = AddressBytes(line->substr(0, first_comma));
            const auto last = AddressBytes(line->substr(first_comma + 1, second_comma - first_comma - 1));
            if (!first || !last || first->second != last->second || first->first > last->first) return nullptr;
            const std::string_view country{line->data() + second_comma + 1, 2};
            if (!std::all_of(country.begin(), country.end(), [](const char c) { return c >= 'A' && c <= 'Z'; })) {
                return nullptr;
            }
            if (country == "FR") ranges.push_back(Range{first->first, last->first, first->second});
        }
        if (ranges.empty()) return nullptr;
        std::sort(ranges.begin(), ranges.end(), [](const Range& left, const Range& right) {
            if (left.ipv6 != right.ipv6) return left.ipv6 < right.ipv6;
            return left.first < right.first;
        });
        // Любое перекрытие диапазонов трактуем как недоверенный вход: бинарный
        // поиск дальше предполагает строгую упорядоченность без неоднозначностей.
        for (size_t i = 1; i < ranges.size(); ++i) {
            if (ranges[i - 1].ipv6 == ranges[i].ipv6 && ranges[i].first <= ranges[i - 1].last) return nullptr;
        }
        return std::shared_ptr<const FrenchIpDataset>{new FrenchIpDataset{std::move(ranges)}};
    } catch (...) {
        return nullptr;
    }
}

bool FrenchIpDataset::IsFrench(const std::string_view numeric_address) const
{
    const auto parsed = AddressBytes(numeric_address);
    if (!parsed) return false;
    const auto& [bytes, ipv6] = *parsed;
    const auto found = std::lower_bound(m_ranges.begin(), m_ranges.end(), std::pair{ipv6, bytes},
        [](const Range& range, const auto& key) {
            return range.ipv6 < key.first || (range.ipv6 == key.first && range.last < key.second);
        });
    return found != m_ranges.end() && found->ipv6 == ipv6 && found->first <= bytes && bytes <= found->last;
}

PeerAdmissionPolicy PeerAdmissionPolicy::Public(std::shared_ptr<const FrenchIpDataset> dataset)
{
    return PeerAdmissionPolicy{std::move(dataset)};
}

PeerAdmissionPolicy PeerAdmissionPolicy::PublicWithUpdater(std::shared_ptr<GeoDatabaseUpdater> updater)
{
    PeerAdmissionPolicy policy{nullptr};
    policy.m_updater = std::move(updater);
    return policy;
}

bool IsLocalNetworkAddress(const std::string_view numeric_address)
{
    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(std::string{numeric_address}, ec);
    if (ec) return false;
    if (address.is_v6() && address.to_v6().is_v4_mapped()) {
        address = boost::asio::ip::make_address_v4(boost::asio::ip::v4_mapped, address.to_v6());
    }
    if (address.is_v4()) {
        const auto value = address.to_v4().to_uint();
        return (value & 0xFF000000U) == 0x7F000000U || // 127.0.0.0/8
            (value & 0xFF000000U) == 0x0A000000U ||    // 10.0.0.0/8
            (value & 0xFFF00000U) == 0xAC100000U ||    // 172.16.0.0/12
            (value & 0xFFFF0000U) == 0xC0A80000U ||    // 192.168.0.0/16
            (value & 0xFFFF0000U) == 0xA9FE0000U;      // 169.254.0.0/16
    }
    const auto bytes = address.to_v6().to_bytes();
    return address.is_loopback() || (bytes[0] & 0xFEU) == 0xFCU ||   // fc00::/7
        (bytes[0] == 0xFEU && (bytes[1] & 0xC0U) == 0x80U);          // fe80::/10
}

bool PeerAdmissionPolicy::Allows(const std::string_view numeric_address) const
{
    if (IsLocalNetworkAddress(numeric_address)) return true;
    const auto dataset = m_dataset ? m_dataset : (m_updater ? m_updater->CurrentDataset() : nullptr);
    return dataset && dataset->IsFrench(numeric_address);
}

bool PeerAdmissionPolicy::Ready() const
{
    return static_cast<bool>(m_dataset) || (m_updater && m_updater->Ready());
}

} // namespace cybou::p2p
