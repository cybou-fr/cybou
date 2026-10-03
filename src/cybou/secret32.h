// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API move-only контейнера для 32-байтового секрета.
#ifndef CYBOU_SECRET32_H
#define CYBOU_SECRET32_H
#include <cybou/crypto/cleanse.h>
#include <array>
namespace cybou {
/// \brief Move-only контейнер для 32-байтового секрета.
/// \details Буфер очищается при уничтожении, а также у объекта-источника после перемещения.
class Secret32 {
public:
    /// \param bytes Исходные 32 байта секрета.
    Secret32(const std::array<unsigned char,32>& bytes):m_bytes(bytes) {}
    /// \post Внутренний буфер очищен независимо от пути уничтожения.
    ~Secret32() { Clear(); }
    Secret32(const Secret32&)=delete;
    Secret32& operator=(const Secret32&)=delete;
    /// \post Новый объект получает байты, а источник очищается fail-closed.
    Secret32(Secret32&& other) noexcept:m_bytes(other.m_bytes) { other.Clear(); }
    /// \post Старое содержимое приемника очищается; источник очищается после переноса.
    Secret32& operator=(Secret32&& other) noexcept { if(this!=&other) { Clear();m_bytes=other.m_bytes;other.Clear(); } return *this; }
    /// \return Константная ссылка на внутренний массив; вызывающий код не должен хранить ее дольше жизни объекта.
    const std::array<unsigned char,32>& Get() const { return m_bytes; }
    /// \return Изменяемый указатель на внутренние 32 байта.
    unsigned char* data() { return m_bytes.data(); }
    /// \return Ровно `32`.
    constexpr size_t size() const { return 32; }
private:
    void Clear() { crypto::CleanseMemory(m_bytes.data(),m_bytes.size()); }
    std::array<unsigned char,32> m_bytes{};
};
}
#endif
