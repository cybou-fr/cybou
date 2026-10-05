// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0
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
    /// \param bytes Ровно 32 байта в каноническом порядке.
    /// \throw std::invalid_argument Если длина не равна `size()`.
    constexpr explicit Hash256(std::span<const unsigned char> bytes) {
        if(bytes.size()!=size()) throw std::invalid_argument{"Hash256 requires 32 bytes"};
        std::copy(bytes.begin(), bytes.end(), m_data.begin());
    }
    /// \brief Создает Hash256 из compile-time hex-литерала длиной 64 символа.
    /// \param hex 64 hex-символа без префикса.
    /// \throw const char* При неверной длине или неhex-символах во время compile-time проверки.
    consteval explicit Hash256(std::string_view hex) {
        if(hex.size()!=64) throw "Hash256 requires 64 hex digits";
        for(size_t i=0;i<size();++i) {
            const int hi=Nibble(hex[2*i]),lo=Nibble(hex[2*i+1]);
            if(hi<0 || lo<0) throw "invalid Hash256 hex";
            m_data[i]=static_cast<unsigned char>((hi<<4)|lo);
        }
    }
    /// \return `true`, если все 32 байта равны нулю.
    constexpr bool IsNull() const { return std::all_of(m_data.begin(),m_data.end(),[](auto b){return b==0;}); }
    /// \post Все 32 байта сброшены в ноль.
    constexpr void SetNull() { m_data.fill(0); }
    /// \return `-1`, `0` или `1` в лексикографическом порядке сырых байт.
    constexpr int Compare(const Hash256& other) const { return m_data<other.m_data ? -1 : m_data>other.m_data ? 1 : 0; }
    auto operator<=>(const Hash256&) const = default;
    /// \return Канонический размер идентификатора: `32` байта.
    static constexpr unsigned int size() { return 32; }
    constexpr unsigned char* data() { return m_data.data(); }
    constexpr const unsigned char* data() const { return m_data.data(); }
    constexpr unsigned char* begin() { return data(); }
    constexpr const unsigned char* begin() const { return data(); }
    constexpr unsigned char* end() { return data()+size(); }
    constexpr const unsigned char* end() const { return data()+size(); }
    /// \brief Возвращает строчный hex в прямом порядке байт.
    /// \return 64 hex-символа без префикса и без перестановки байт.
    std::string GetHex() const;
    std::string ToString() const { return GetHex(); }
    /// \brief Разбирает строчный hex длиной 64 символа.
    /// \param hex 64 hex-символа без префикса.
    /// \return Значение `Hash256`, либо `std::nullopt` при неверной длине или неhex-символах.
    static std::optional<Hash256> FromHex(std::string_view hex);
    /// \brief Все нули; удобен как «пустой» идентификатор, который обычно недопустим на wire.
    static const Hash256 ZERO;
    /// \brief Первый байт равен `0x01`, остальные нули; используется как стабильная тестовая/служебная константа.
    static const Hash256 ONE;
};
} // namespace cybou
#endif
