// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/hex.h>

#include <array>

namespace cybou {
namespace {

int HexNibble(const char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

} // namespace

std::optional<uint256> ParseUint256UserHex(std::string_view input)
{
    if (input.starts_with("0x")) input.remove_prefix(2);
    constexpr size_t HEX_SIZE{uint256::size() * 2};
    if (input.size() > HEX_SIZE) return std::nullopt;

    std::array<unsigned char, uint256::size()> bytes{};
    for (size_t i = 0; i < input.size(); ++i) {
        const int nibble = HexNibble(input[input.size() - i - 1]);
        if (nibble < 0) return std::nullopt;
        bytes[i / 2] |= static_cast<unsigned char>(nibble << ((i % 2) * 4));
    }
    return uint256{bytes};
}

} // namespace cybou
