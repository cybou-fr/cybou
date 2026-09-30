// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_SECRET32_H
#define CYBOU_SECRET32_H
#include <cybou/crypto/cleanse.h>
#include <array>
namespace cybou {
/** Move-only secret: moved-from buffers and exception paths are always cleansed. */
class Secret32 {
public:
    Secret32(const std::array<unsigned char,32>& bytes):m_bytes(bytes) {}
    ~Secret32() { Clear(); }
    Secret32(const Secret32&)=delete;
    Secret32& operator=(const Secret32&)=delete;
    Secret32(Secret32&& other) noexcept:m_bytes(other.m_bytes) { other.Clear(); }
    Secret32& operator=(Secret32&& other) noexcept { if(this!=&other) { Clear();m_bytes=other.m_bytes;other.Clear(); } return *this; }
    const std::array<unsigned char,32>& Get() const { return m_bytes; }
    unsigned char* data() { return m_bytes.data(); }
    constexpr size_t size() const { return 32; }
private:
    void Clear() { crypto::CleanseMemory(m_bytes.data(),m_bytes.size()); }
    std::array<unsigned char,32> m_bytes{};
};
}
#endif
