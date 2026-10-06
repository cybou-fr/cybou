// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
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
/// \param text Входные байты без завершающего `NUL`.
/// \return `true`, если вся строка состоит из допустимых scalar value Unicode.
/// \post Не выделяет память и не изменяет входной буфер.
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
        while (count--) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    }
    return true;
}

/// \brief Дописывает беззнаковое целое в little-endian порядке.
/// \tparam UInt Один из поддерживаемых беззнаковых целочисленных типов фиксированной ширины.
/// \param bytes Буфер назначения.
/// \param value Значение для сериализации.
/// \post В конец `bytes` добавлено ровно `sizeof(UInt)` байт.
template<typename UInt> inline void AppendLittleEndian(std::vector<unsigned char>& bytes, UInt value)
{
    std::array<unsigned char, sizeof(UInt)> encoded{};
    for (size_t i = 0; i < encoded.size(); ++i) {
        encoded[i] = static_cast<unsigned char>(value >> (8 * i));
    }
    bytes.insert(bytes.end(), encoded.begin(), encoded.end());
}

/// \brief Читает беззнаковое little-endian целое из точного числа байт.
/// \tparam UInt Целевой беззнаковый тип фиксированной ширины.
/// \param bytes Буфер длиной не меньше `sizeof(UInt)`.
/// \return Декодированное значение.
/// \pre `bytes.size() >= sizeof(UInt)`.
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
    /// \param limit Максимальный итоговый размер кодируемого объекта в байтах.
    /// \post Новый writer пуст и готов к записи.
    explicit BinaryWriter(size_t limit = 256 * 1024) : m_limit{limit} {}
    /// \post Внутренний буфер очищается как потенциально чувствительный.
    ~BinaryWriter() { crypto::CleanseMemory(m_bytes.data(), m_bytes.size()); }
    /// \param bytes Поле фиксированной длины.
    /// \pre `bytes.size()` вместе с уже записанными данными не превышает `limit`.
    /// \throw std::length_error Если итоговый объект превысил лимит.
    void Fixed(std::span<const unsigned char> bytes) {
        if (bytes.size() > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
    }
    /// \param value Значение для записи.
    /// \throw std::length_error Если итоговый объект превысил лимит.
    void U8(uint8_t value) {
        if (m_bytes.size() >= m_limit) throw std::length_error{"binary object limit"};
        m_bytes.push_back(value);
    }
    /// \param value Значение для записи.
    /// \throw std::length_error Если итоговый объект превысил лимит.
    void U16(uint16_t value) {
        if (sizeof(value) > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        AppendLittleEndian(m_bytes, value);
    }
    /// \param value Значение для записи.
    /// \throw std::length_error Если итоговый объект превысил лимит.
    void U32(uint32_t value) {
        if (sizeof(value) > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        AppendLittleEndian(m_bytes, value);
    }
    /// \param value Значение для записи.
    /// \throw std::length_error Если итоговый объект превысил лимит.
    void U64(uint64_t value) {
        if (sizeof(value) > m_limit - m_bytes.size()) throw std::length_error{"binary object limit"};
        AppendLittleEndian(m_bytes, value);
    }
    /// \param bytes Поле переменной длины.
    /// \param maximum Верхняя граница длины полезной нагрузки.
    /// \throw std::length_error Если поле длиннее `maximum`, не помещается в `uint32_t` или превышает лимит объекта.
    void Bytes(std::span<const unsigned char> bytes, size_t maximum) {
        if (bytes.size() > maximum || bytes.size() > std::numeric_limits<uint32_t>::max()) throw std::length_error{"binary byte length"};
        U32(static_cast<uint32_t>(bytes.size())); Fixed(bytes);
    }
    /// \param text UTF-8 строка без перекодирования.
    /// \param maximum Верхняя граница длины в байтах.
    /// \throw std::invalid_argument Если строка невалидна как UTF-8.
    /// \throw std::length_error Если поле длиннее `maximum`, не помещается в `uint32_t` или превышает лимит объекта.
    void Text(std::string_view text, size_t maximum) {
        if (!IsValidUtf8(text)) throw std::invalid_argument{"invalid UTF-8"};
        Bytes(std::span{reinterpret_cast<const unsigned char*>(text.data()), text.size()}, maximum);
    }
    /// \return Накопленные байты; объект остается в допустимом, но перемещенном состоянии.
    std::vector<unsigned char> Take() { return std::move(m_bytes); }
};

/// \brief Читает те же поля из ограниченного бинарного буфера.
class BinaryReader {
    std::span<const unsigned char> m_bytes;
    size_t m_position{0};
public:
    /// \param bytes Источник байт.
    /// \param limit Максимальный допустимый размер всего объекта.
    /// \throw std::length_error Если вход длиннее `limit`.
    explicit BinaryReader(std::span<const unsigned char> bytes, size_t limit = 256 * 1024) : m_bytes{bytes} {
        if (bytes.size() > limit) throw std::length_error{"binary object limit"};
    }
    /// \param count Точное число байт для чтения.
    /// \return Подspan на исходный буфер без копирования.
    /// \throw std::invalid_argument Если вход усечен.
    std::span<const unsigned char> Fixed(size_t count) {
        if (count > m_bytes.size() - m_position) throw std::invalid_argument{"truncated binary object"};
        const auto bytes = m_bytes.subspan(m_position, count); m_position += count; return bytes;
    }
    /// \tparam T Контейнер фиксированного размера с `size()/begin()`.
    /// \return Значение `T`, заполненное точным числом байт из входа.
    template<typename T> T Fixed() { T value{}; const auto bytes = Fixed(value.size()); std::copy(bytes.begin(), bytes.end(), value.begin()); return value; }
    uint8_t U8() { return Fixed(1)[0]; }
    uint16_t U16() { return ReadLittleEndian<uint16_t>(Fixed(sizeof(uint16_t))); }
    uint32_t U32() { return ReadLittleEndian<uint32_t>(Fixed(sizeof(uint32_t))); }
    uint64_t U64() { return ReadLittleEndian<uint64_t>(Fixed(sizeof(uint64_t))); }
    /// \return Булев флаг, закодированный как `0` или `1`.
    /// \throw std::invalid_argument Если встретилось иное байтовое значение.
    bool Flag() { const auto value = U8(); if (value > 1) throw std::invalid_argument{"invalid binary flag"}; return value != 0; }
    /// \param maximum Верхняя граница длины полезной нагрузки.
    /// \return Подspan на байты полезной нагрузки.
    /// \throw std::length_error Если заявленная длина больше `maximum`.
    /// \throw std::invalid_argument Если вход усечен.
    std::span<const unsigned char> Bytes(size_t maximum) { const auto size = U32(); if (size > maximum) throw std::length_error{"binary byte length"}; return Fixed(size); }
    /// \param maximum Верхняя граница длины строки в байтах.
    /// \return Декодированная UTF-8 строка.
    /// \throw std::length_error Если заявленная длина больше `maximum`.
    /// \throw std::invalid_argument Если вход усечен или строка невалидна как UTF-8.
    std::string Text(size_t maximum) { const auto bytes = Bytes(maximum); std::string text{bytes.begin(), bytes.end()}; if (!IsValidUtf8(text)) throw std::invalid_argument{"invalid UTF-8"}; return text; }
    /// \throw std::invalid_argument Если после ожидаемых полей остались хвостовые байты.
    void Finish() const { if (m_position != m_bytes.size()) throw std::invalid_argument{"trailing binary bytes"}; }
};
}
#endif
