// Copyright (c) 2026 CYBOU contributors
// Distributed under the MIT software license, see COPYING.
/// \file
/// \brief Русский публичный API канонического 32-байтового идентификатора.
#ifndef CYBOU_HASH256_H
#define CYBOU_HASH256_H
#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
namespace cybou {
/// \brief Непрозрачный канонический 32-байтовый идентификатор.
/// \details Сравнение и hex-представление следуют фактическому порядку байт в памяти.
class Hash256 {
    std::array<unsigned char,32> m_data{};
    static constexpr int Nibble(char c) {
        if(c >= '0' && c <= '9') return c-'0';
        if(c >= 'a' && c <= 'f') return c-'a'+10;
        if(c >= 'A' && c <= 'F') return c-'A'+10;
        return -1;
    }
public:
    constexpr Hash256() = default;
    constexpr explicit Hash256(uint8_t first_byte) : m_data{first_byte} {}
    /// \brief Создает Hash256 из точного 32-байтового span.
    constexpr explicit Hash256(std::span<const unsigned char> bytes) {
        if(bytes.size()!=size()) throw std::invalid_argument{"Hash256 requires 32 bytes"};
        std::copy(bytes.begin(), bytes.end(), m_data.begin());
    }
    /// \brief Создает Hash256 из compile-time hex-литерала длиной 64 символа.
    consteval explicit Hash256(std::string_view hex) {
        if(hex.size()!=64) throw "Hash256 requires 64 hex digits";
        for(size_t i=0;i<size();++i) {
            const int hi=Nibble(hex[2*i]),lo=Nibble(hex[2*i+1]);
            if(hi<0 || lo<0) throw "invalid Hash256 hex";
            m_data[i]=static_cast<unsigned char>((hi<<4)|lo);
        }
    }
    constexpr bool IsNull() const { return std::all_of(m_data.begin(),m_data.end(),[](auto b){return b==0;}); }
    constexpr void SetNull() { m_data.fill(0); }
    constexpr int Compare(const Hash256& other) const { return m_data<other.m_data ? -1 : m_data>other.m_data ? 1 : 0; }
    auto operator<=>(const Hash256&) const = default;
    static constexpr unsigned int size() { return 32; }
    constexpr unsigned char* data() { return m_data.data(); }
    constexpr const unsigned char* data() const { return m_data.data(); }
    constexpr unsigned char* begin() { return data(); }
    constexpr const unsigned char* begin() const { return data(); }
    constexpr unsigned char* end() { return data()+size(); }
    constexpr const unsigned char* end() const { return data()+size(); }
    /// \brief Возвращает строчный hex в прямом порядке байт.
    std::string GetHex() const;
    std::string ToString() const { return GetHex(); }
    /// \brief Разбирает строчный hex длиной 64 символа.
    static std::optional<Hash256> FromHex(std::string_view hex);
    static const Hash256 ZERO;
    static const Hash256 ONE;
};
} // namespace cybou
#endif
