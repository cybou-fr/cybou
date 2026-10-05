// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
#include <cybou/hex.h>
namespace cybou {
std::optional<Hash256> ParseHash256UserHex(std::string_view input) { return Hash256::FromHex(input); }
std::string HexEncode(std::span<const unsigned char> bytes) {
    constexpr char digits[]="0123456789abcdef";
    std::string result(bytes.size()*2,'0');
    for(size_t i=0;i<bytes.size();++i) { result[2*i]=digits[bytes[i]>>4]; result[2*i+1]=digits[bytes[i]&15]; }
    return result;
}
}
