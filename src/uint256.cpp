// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <uint256.h>

template <unsigned int BITS>
std::string base_blob<BITS>::GetHex() const
{
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    std::string result(WIDTH * 2, '0');
    for (int i = 0; i < WIDTH; ++i) {
        const uint8_t byte = m_data[WIDTH - 1 - i];
        result[2 * i] = HEX_DIGITS[byte >> 4];
        result[2 * i + 1] = HEX_DIGITS[byte & 0x0f];
    }
    return result;
}

template <unsigned int BITS>
std::string base_blob<BITS>::ToString() const
{
    return GetHex();
}

// Explicit instantiations for base_blob<160>
template std::string base_blob<160>::GetHex() const;
template std::string base_blob<160>::ToString() const;

// Explicit instantiations for base_blob<256>
template std::string base_blob<256>::GetHex() const;
template std::string base_blob<256>::ToString() const;

const uint256 uint256::ZERO(0);
const uint256 uint256::ONE(1);
