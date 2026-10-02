// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_TEST_HELPERS_H
#define CYBOU_TEST_HELPERS_H

#include <cybou/identity_crypto.h>
#include <cybou/network_definition.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cybou {

namespace test {

inline std::string Hex(std::span<const unsigned char> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
        output.push_back(digits[byte >> 4]);
        output.push_back(digits[byte & 0x0f]);
    }
    return output;
}

inline std::vector<unsigned char> ParseHex(std::string_view input)
{
    auto nibble = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    std::vector<unsigned char> output;
    if ((input.size() & 1) != 0) return output;
    output.reserve(input.size() / 2);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        const int high = nibble(input[i]);
        const int low = nibble(input[i + 1]);
        if (high < 0 || low < 0) return {};
        output.push_back(static_cast<unsigned char>((high << 4) | low));
    }
    return output;
}

} // namespace test

/** A test Network Public Key; distinct seeds give distinct networks. */
inline IdentityHybridPublicKey TestNetworkPublicKey(unsigned char seed_byte = 0xA7)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    seed[1] = 0x4e;
    return DeriveIdentityPublicKey(seed, IdentityKeyPurpose::NETWORK_ROOT).value();
}

inline IdentityHybridPublicKey TestPoaFinalizerPublicKey(unsigned char seed_byte = 0xA7)
{
    std::array<unsigned char, 32> seed{};
    seed[0] = seed_byte;
    return DeriveIdentityPublicKey(seed, IdentityKeyPurpose::POA_FINALIZER).value();
}

} // namespace cybou

#endif
