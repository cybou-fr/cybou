// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.
/// \file
/// \brief Минимальный LevelDB-адаптер для локального хранения канонического состояния.

#ifndef CYBOU_KV_STORE_H
#define CYBOU_KV_STORE_H

#include <cybou/hash256.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace leveldb { class WriteBatch; }

namespace cybou {

namespace detail {

inline constexpr size_t CompactSizeWidth(const uint64_t value)
{
    if (value < 253) return 1;
    if (value <= std::numeric_limits<uint16_t>::max()) return 3;
    if (value <= std::numeric_limits<uint32_t>::max()) return 5;
    return 9;
}

inline void AppendCompactSize(std::vector<unsigned char>& out, const uint64_t value)
{
    if (value < 253) {
        out.push_back(static_cast<unsigned char>(value));
    } else if (value <= std::numeric_limits<uint16_t>::max()) {
        out.push_back(253);
        for (unsigned i{0}; i < 2; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    } else if (value <= std::numeric_limits<uint32_t>::max()) {
        out.push_back(254);
        for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    } else {
        out.push_back(255);
        for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
    }
}

inline bool ReadCompactSize(const std::span<const unsigned char> bytes, size_t& offset, uint64_t& value)
{
    if (offset >= bytes.size()) return false;
    const auto marker = bytes[offset++];
    if (marker < 253) {
        value = marker;
        return true;
    }
    const size_t width = marker == 253 ? 2 : marker == 254 ? 4 : 8;
    if (width > bytes.size() - offset) return false;
    value = 0;
    for (size_t i{0}; i < width; ++i) value |= uint64_t{bytes[offset++]} << (8 * i);
    return (width != 2 || value >= 253) && (width != 4 || value >= 0x10000) &&
        (width != 8 || value >= 0x100000000ULL) && value <= 0x02000000;
}

template <typename T, typename Enable = void>
struct LocalRecordCodec;

template <>
struct LocalRecordCodec<std::string> {
    static std::vector<unsigned char> Encode(const std::string& value)
    {
        std::vector<unsigned char> out;
        out.reserve(CompactSizeWidth(value.size()) + value.size());
        AppendCompactSize(out, value.size());
        out.insert(out.end(), value.begin(), value.end());
        return out;
    }
    static bool Decode(const std::span<const unsigned char> bytes, std::string& value)
    {
        size_t offset{0};
        uint64_t size{0};
        if (!ReadCompactSize(bytes, offset, size) || size > bytes.size() - offset) return false;
        value.assign(reinterpret_cast<const char*>(bytes.data() + offset), static_cast<size_t>(size));
        return true;
    }
};

template <>
struct LocalRecordCodec<std::vector<unsigned char>> {
    static std::vector<unsigned char> Encode(const std::vector<unsigned char>& value)
    {
        std::vector<unsigned char> out;
        out.reserve(CompactSizeWidth(value.size()) + value.size());
        AppendCompactSize(out, value.size());
        out.insert(out.end(), value.begin(), value.end());
        return out;
    }
    static bool Decode(const std::span<const unsigned char> bytes, std::vector<unsigned char>& value)
    {
        size_t offset{0};
        uint64_t size{0};
        if (!ReadCompactSize(bytes, offset, size) || size > bytes.size() - offset) return false;
        value.assign(bytes.begin() + offset, bytes.begin() + offset + static_cast<size_t>(size));
        return true;
    }
};

template <typename T>
struct LocalRecordCodec<T, std::enable_if_t<std::is_unsigned_v<T>>> {
    static std::vector<unsigned char> Encode(const T value)
    {
        std::vector<unsigned char> out(sizeof(T));
        for (size_t i{0}; i < sizeof(T); ++i) out[i] = static_cast<unsigned char>(value >> (8 * i));
        return out;
    }
    static bool Decode(const std::span<const unsigned char> bytes, T& value)
    {
        if (bytes.size() < sizeof(T)) return false;
        value = 0;
        for (size_t i{0}; i < sizeof(T); ++i) value |= static_cast<T>(bytes[i]) << (8 * i);
        return true;
    }
};

template <>
struct LocalRecordCodec<cybou::Hash256> {
    static std::vector<unsigned char> Encode(const cybou::Hash256& value)
    {
        return {value.begin(), value.end()};
    }
    static bool Decode(const std::span<const unsigned char> bytes, cybou::Hash256& value)
    {
        if (bytes.size() < cybou::Hash256::size()) return false;
        std::copy_n(bytes.begin(), cybou::Hash256::size(), value.begin());
        return true;
    }
};

template <typename T>
std::vector<unsigned char> SerializeLocalRecord(const T& value)
{
    return LocalRecordCodec<std::remove_cv_t<T>>::Encode(value);
}

template <typename T>
bool DeserializeLocalRecord(const std::span<const unsigned char> bytes, T& value)
{
    return LocalRecordCodec<T>::Decode(bytes, value);
}

} // namespace detail

/// \brief Параметры открытия локального key-value store CYBOU.
struct KVStoreOptions {
    std::filesystem::path path;
    size_t cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
};

/// \brief Минимальная типобезопасная оболочка над LevelDB для локальных записей CYBOU.
class KVStore final
{
public:
    class Batch final
    {
        friend class KVStore;

    public:
        Batch();
        ~Batch();

        Batch(Batch&&) noexcept;
        Batch& operator=(Batch&&) noexcept;
        Batch(const Batch&) = delete;
        Batch& operator=(const Batch&) = delete;

        template <typename K, typename V>
        void Write(const K& key, const V& value)
        {
            auto serialized_key = detail::SerializeLocalRecord(key);
            auto serialized_value = detail::SerializeLocalRecord(value);
            PutRaw(serialized_key, serialized_value);
        }

        template <typename K>
        void Erase(const K& key)
        {
            const auto serialized_key = detail::SerializeLocalRecord(key);
            EraseRaw(serialized_key);
        }

    private:
        std::unique_ptr<leveldb::WriteBatch> m_batch;
        void PutRaw(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value);
        void EraseRaw(const std::vector<unsigned char>& key);
    };

    explicit KVStore(const KVStoreOptions& options);
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        const auto serialized_key = detail::SerializeLocalRecord(key);
        const auto raw = ReadRaw(serialized_key);
        if (!raw) return false;
        const auto bytes = std::span{reinterpret_cast<const unsigned char*>(raw->data()), raw->size()};
        try {
            return detail::DeserializeLocalRecord(bytes, value);
        } catch (const std::exception&) {
            return false;
        }
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        const auto serialized_key = detail::SerializeLocalRecord(key);
        return ReadRaw(serialized_key).has_value();
    }

    template <typename K, typename V>
    void Write(const K& key, const V& value, bool sync = false)
    {
        Batch batch;
        batch.Write(key, value);
        WriteBatch(batch, sync);
    }

    void WriteBatch(Batch& batch, bool sync = false);

    /// \brief Обходит строки-ключи с заданным префиксом после локальной сериализации ключа.
    void ForEachStringPrefix(const std::string& prefix, size_t key_size,
        const std::function<void(const std::string&, const std::string&)>& visitor) const;

    /// \brief Обходит строки-ключи с заданным префиксом, сохраняя сырые value bytes.
    void ForEachStringPrefixRaw(const std::string& prefix, size_t key_size,
        const std::function<void(const std::string&, const std::string&)>& visitor) const;

    template <typename K>
    void Erase(const K& key, bool sync = false)
    {
        Batch batch;
        batch.Erase(key);
        WriteBatch(batch, sync);
    }

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    std::optional<std::string> ReadRaw(const std::vector<unsigned char>& key) const;
};

} // namespace cybou

#endif // CYBOU_KV_STORE_H
