// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_admission.h>

#include <cybou/crypto/sha256.h>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
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

bool IsLabAddress(const boost::asio::ip::address& address)
{
    auto normalized = address;
    if (normalized.is_v6() && normalized.to_v6().is_v4_mapped()) {
        const auto mapped = normalized.to_v6().to_bytes();
        boost::asio::ip::address_v4::bytes_type v4{};
        std::copy_n(mapped.end() - static_cast<std::ptrdiff_t>(v4.size()), v4.size(), v4.begin());
        normalized = boost::asio::ip::address_v4{v4};
    }
    if (normalized.is_v4()) {
        const auto bytes = normalized.to_v4().to_bytes();
        return bytes[0] == 10 || bytes[0] == 127 ||
            (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) ||
            (bytes[0] == 192 && bytes[1] == 168) ||
            (bytes[0] == 169 && bytes[1] == 254);
    }
    const auto v6 = normalized.to_v6();
    if (v6.is_loopback() || v6.is_link_local()) return true;
    const auto bytes = v6.to_bytes();
    return (bytes[0] & 0xfeU) == 0xfcU; // IPv6 unique-local fc00::/7
}

} // namespace

std::shared_ptr<const FrenchIpDataset> FrenchIpDataset::LoadDbIpCountryCsv(const std::filesystem::path& path,
    const std::array<unsigned char, 32>& expected_sha256)
{
    try {
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

        std::istringstream lines{bytes};
        std::string line;
        std::vector<Range> ranges;
        size_t records{0};
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || ++records > MAX_RECORDS) return nullptr;
            const auto first_comma = line.find(',');
            const auto second_comma = first_comma == std::string::npos ? first_comma : line.find(',', first_comma + 1);
            if (first_comma == std::string::npos || second_comma == std::string::npos ||
                line.find(',', second_comma + 1) != std::string::npos || first_comma == 0 ||
                second_comma == first_comma + 1 || second_comma + 3 != line.size()) return nullptr;
            const auto first = AddressBytes(std::string_view{line}.substr(0, first_comma));
            const auto last = AddressBytes(std::string_view{line}.substr(first_comma + 1,
                second_comma - first_comma - 1));
            if (!first || !last || first->second != last->second || first->first > last->first) return nullptr;
            const std::string_view country{line.data() + second_comma + 1, 2};
            if (!std::all_of(country.begin(), country.end(), [](const char c) { return c >= 'A' && c <= 'Z'; })) {
                return nullptr;
            }
            if (country == "FR") ranges.push_back(Range{first->first, last->first, first->second});
        }
        if (!lines.eof() || ranges.empty()) return nullptr;
        std::sort(ranges.begin(), ranges.end(), [](const Range& left, const Range& right) {
            if (left.ipv6 != right.ipv6) return left.ipv6 < right.ipv6;
            return left.first < right.first;
        });
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
    return PeerAdmissionPolicy{std::move(dataset), false};
}

PeerAdmissionPolicy PeerAdmissionPolicy::Lab()
{
    return PeerAdmissionPolicy{nullptr, true};
}

bool PeerAdmissionPolicy::Allows(const std::string_view numeric_address) const
{
    boost::system::error_code ec;
    const auto address = boost::asio::ip::make_address(std::string{numeric_address}, ec);
    if (ec) return false;
    if (m_lab) return IsLabAddress(address);
    return m_dataset && m_dataset->IsFrench(numeric_address);
}

} // namespace cybou::p2p
