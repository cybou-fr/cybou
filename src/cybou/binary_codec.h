// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see COPYING.
/// \file
/// \brief Русский заголовок для компактного бинарного кодека CYBOU.
#ifndef CYBOU_BINARY_CODEC_H
#define CYBOU_BINARY_CODEC_H
#include <cybou/crypto/cleanse.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace cybou {

/// \brief Проверяет, что строка содержит корректный UTF-8 без суррогатов и сверхдлинных форм.
inline bool IsValidUtf8(std::string_view text) {
    for (size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i++]);
        if (c < 0x80) continue;
        unsigned count; uint32_t value; uint32_t minimum;
        if (c >= 0xc2 && c <= 0xdf) { count = 1; value = c & 31; minimum = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { count = 2; value = c & 15; minimum = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { count = 3; value = c & 7; minimum = 0x10000; }
        else return false;
        if (count > text.size() - i) return false;
        while (count--) { const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) return false; value = (value << 6) | (next & 63); }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

template<typename UInt> inline void AppendLittleEndian(std::vector<unsigned char>& bytes, UInt value)
{
    std::array<unsigned char, sizeof(UInt)> encoded{};
    for (size_t i = 0; i < encoded.size(); ++i) {
        encoded[i] = static_cast<unsigned char>(value >> (8 * i));
    }
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

template<typename UInt> inline UInt ReadLittleEndian(std::span<const unsigned char> bytes)
{
    UInt value{0};
    for (size_t i = 0; i < sizeof(UInt); ++i) value |= UInt{bytes[i]} << (8 * i);
    return value;
}

/// \brief Пишет точные байтовые поля, little-endian числа и ограниченные строки.
class BinaryWriter {
    std::vector<unsigned char> m_bytes;
    size_t m_limit;
public:
    explicit BinaryWriter(size_t limit = 256 * 1024) : m_limit{limit} {}
    ~BinaryWriter() { crypto::CleanseMemory(m_bytes.data(), m_bytes.size()); }
    void Fixed(std::span<const unsigned char> bytes) {
        if (bytes.size() > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
    }
    void U8(uint8_t value) {
        if (m_bytes.size() >= m_limit) throw std::length_error{"binary object limit"};
        m_bytes.push_back(value);
    }
    void U16(uint16_t value) {
        if (sizeof(value) > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        AppendLittleEndian(m_bytes, value);
    }
    void U32(uint32_t value) {
        if (sizeof(value) > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        AppendLittleEndian(m_bytes, value);
    }
    void U64(uint64_t value) {
        if (sizeof(value) > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        AppendLittleEndian(m_bytes, value);
    }
    void Bytes(std::span<const unsigned char> bytes, size_t maximum) {
        if (bytes.size() > maximum || bytes.size() > std::numeric_limits<uint32_t>::max()) throw std::length_error{"binary byte length"};
        U32(static_cast<uint32_t>(bytes.size())); Fixed(bytes);
    }
    void Text(std::string_view text, size_t maximum) {
        if (!IsValidUtf8(text)) throw std::invalid_argument{"invalid UTF-8"};
        Bytes(std::span{reinterpret_cast<const unsigned char*>(text.data()), text.size()}, maximum);
    }
    std::vector<unsigned char> Take() { return std::move(m_bytes); }
};

/// \brief Читает те же поля из ограниченного бинарного буфера.
class BinaryReader {
    std::span<const unsigned char> m_bytes;
    size_t m_position{0};
public:
    explicit BinaryReader(std::span<const unsigned char> bytes, size_t limit = 256 * 1024) : m_bytes{bytes} {
        if (bytes.size() > limit) throw std::length_error{"binary object limit"};
    }
    std::span<const unsigned char> Fixed(size_t count) {
        if (count > m_bytes.size() - m_position) throw std::invalid_argument{"truncated binary object"};
        const auto bytes = m_bytes.subspan(m_position, count); m_position += count; return bytes;
    }
    template<typename T> T Fixed() { T value{}; const auto bytes = Fixed(value.size()); std::copy(bytes.begin(), bytes.end(), value.begin()); return value; }
    uint8_t U8() { return Fixed(1)[0]; }
    uint16_t U16() { return ReadLittleEndian<uint16_t>(Fixed(sizeof(uint16_t))); }
    uint32_t U32() { return ReadLittleEndian<uint32_t>(Fixed(sizeof(uint32_t))); }
    uint64_t U64() { return ReadLittleEndian<uint64_t>(Fixed(sizeof(uint64_t))); }
    bool Flag() { const auto value = U8(); if (value > 1) throw std::invalid_argument{"invalid binary flag"}; return value != 0; }
    std::span<const unsigned char> Bytes(size_t maximum) { const auto size = U32(); if (size > maximum) throw std::length_error{"binary byte length"}; return Fixed(size); }
    std::string Text(size_t maximum) { const auto bytes = Bytes(maximum); std::string text{bytes.begin(), bytes.end()}; if (!IsValidUtf8(text)) throw std::invalid_argument{"invalid UTF-8"}; return text; }
    void Finish() const { if (m_position != m_bytes.size()) throw std::invalid_argument{"trailing binary bytes"}; }
};
}
#endif
