// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#include <cybou/hash256.h>
namespace cybou {
std::string Hash256::GetHex() const {
    constexpr char digits[]="0123456789abcdef";
    std::string result(64,'0');
    for(size_t i=0;i<size();++i) { result[2*i]=digits[m_data[i]>>4]; result[2*i+1]=digits[m_data[i]&15]; }
    return result;
}
std::optional<Hash256> Hash256::FromHex(std::string_view hex) {
    if(hex.size()!=64) return std::nullopt;
    Hash256 result;
    for(size_t i=0;i<size();++i) {
        const int hi=Nibble(hex[2*i]),lo=Nibble(hex[2*i+1]);
        if(hi<0 || lo<0) return std::nullopt;
        result.m_data[i]=static_cast<unsigned char>((hi<<4)|lo);
    }
    return result;
}
const Hash256 Hash256::ZERO{};
const Hash256 Hash256::ONE{uint8_t{1}};
} // namespace cybou
