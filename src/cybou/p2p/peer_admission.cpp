// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/p2p/peer_admission.h>

#include <cybou/crypto/sha256.h>

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <sstream>
#include <span>
#include <system_error>

namespace cybou::p2p {
namespace {

constexpr std::string_view DATASET_HEADER{"CYBOU-GEO-FR-v1"};
constexpr uintmax_t MAX_DATASET_BYTES{16 * 1024 * 1024};
constexpr size_t MAX_PREFIXES{200'000};

bool IsZeroHostBits(const std::array<unsigned char, 16>& address, const unsigned width,
    const unsigned prefix_bits)
{
    for (unsigned bit = prefix_bits; bit < width; ++bit) {
        const unsigned byte = bit / 8;
        const unsigned mask = 1U << (7 - (bit % 8));
        if ((address[byte] & mask) != 0) return false;
    }
    return true;
}

bool PrefixMatches(const std::array<unsigned char, 16>& address,
    const std::array<unsigned char, 16>& prefix, const unsigned bits)
{
    const unsigned whole_bytes = bits / 8;
    if (!std::equal(prefix.begin(), prefix.begin() + whole_bytes, address.begin())) return false;
    const unsigned remainder = bits % 8;
    if (remainder == 0) return true;
    const auto mask = static_cast<unsigned char>(0xffU << (8 - remainder));
    return (address[whole_bytes] & mask) == (prefix[whole_bytes] & mask);
}

bool IsLabAddress(const boost::asio::ip::address& address)
{
    if (address.is_v4()) {
        const auto bytes = address.to_v4().to_bytes();
        return bytes[0] == 10 || bytes[0] == 127 ||
            (bytes[0] == 172 && bytes[1] >= 16 && bytes[1] <= 31) ||
            (bytes[0] == 192 && bytes[1] == 168) ||
            (bytes[0] == 169 && bytes[1] == 254);
    }
    const auto v6 = address.to_v6();
    if (v6.is_loopback() || v6.is_link_local()) return true;
    const auto bytes = v6.to_bytes();
    return (bytes[0] & 0xfeU) == 0xfcU; // IPv6 unique-local fc00::/7
}

} // namespace

std::shared_ptr<const FrenchIpDataset> FrenchIpDataset::Load(const std::filesystem::path& path,
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
        if (!std::getline(lines, line)) return nullptr;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line != DATASET_HEADER) return nullptr;

        std::vector<Prefix> prefixes;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || prefixes.size() >= MAX_PREFIXES) return nullptr;
            const auto slash = line.find('/');
            if (slash == std::string::npos || slash == 0 || slash + 1 == line.size() ||
                line.find('/', slash + 1) != std::string::npos) return nullptr;
            boost::system::error_code address_error;
            auto address = boost::asio::ip::make_address(std::string{std::string_view{line}.substr(0, slash)}, address_error);
            if (address_error || (address.is_v6() && address.to_v6().is_v4_mapped())) return nullptr;
            unsigned bits{0};
            const auto prefix_text = std::string_view{line}.substr(slash + 1);
            const auto [end, parse_error] = std::from_chars(prefix_text.data(),
                prefix_text.data() + prefix_text.size(), bits);
            const unsigned width = address.is_v4() ? 32 : 128;
            if (parse_error != std::errc{} || end != prefix_text.data() + prefix_text.size() || bits > width)
                return nullptr;

            Prefix prefix;
            prefix.bits = static_cast<uint8_t>(bits);
            prefix.ipv6 = address.is_v6();
            if (address.is_v4()) {
                const auto v4_bytes = address.to_v4().to_bytes();
                std::copy(v4_bytes.begin(), v4_bytes.end(), prefix.address.begin());
            } else {
                prefix.address = address.to_v6().to_bytes();
            }
            if (!IsZeroHostBits(prefix.address, width, bits)) return nullptr;
            prefixes.push_back(prefix);
        }
        if (!lines.eof() || prefixes.empty()) return nullptr;
        return std::shared_ptr<const FrenchIpDataset>{new FrenchIpDataset{std::move(prefixes)}};
    } catch (...) {
        return nullptr;
    }
}

bool FrenchIpDataset::IsFrench(const std::string_view numeric_address) const
{
    boost::system::error_code ec;
    auto address = boost::asio::ip::make_address(std::string{numeric_address}, ec);
    if (ec) return false;
    if (address.is_v6() && address.to_v6().is_v4_mapped()) {
        const auto mapped = address.to_v6().to_bytes();
        boost::asio::ip::address_v4::bytes_type v4{};
        std::copy_n(mapped.end() - static_cast<std::ptrdiff_t>(v4.size()), v4.size(), v4.begin());
        address = boost::asio::ip::address_v4{v4};
    }
    std::array<unsigned char, 16> bytes{};
    const bool ipv6 = address.is_v6();
    if (ipv6) bytes = address.to_v6().to_bytes();
    else {
        const auto v4 = address.to_v4().to_bytes();
        std::copy(v4.begin(), v4.end(), bytes.begin());
    }
    return std::any_of(m_prefixes.begin(), m_prefixes.end(), [&](const Prefix& prefix) {
        return prefix.ipv6 == ipv6 && PrefixMatches(bytes, prefix.address, prefix.bits);
    });
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
