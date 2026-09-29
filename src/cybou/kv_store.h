// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_KV_STORE_H
#define CYBOU_KV_STORE_H

#include <serialize.h>

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace leveldb { class WriteBatch; }

namespace cybou {

namespace detail {

/** Byte writer for local LevelDB records using the existing Bitcoin serializer. */
class LocalRecordWriter final
{
public:
    void write(const std::span<const std::byte> bytes)
    {
        if (bytes.empty()) return;
        const auto* begin = reinterpret_cast<const unsigned char*>(bytes.data());
        m_bytes.insert(m_bytes.end(), begin, begin + bytes.size());
    }

    template <typename T>
    LocalRecordWriter& operator<<(const T& value)
    {
        ::Serialize(*this, value);
        return *this;
    }

    const std::vector<unsigned char>& Bytes() const { return m_bytes; }

private:
    std::vector<unsigned char> m_bytes;
};

/** Bounded reader for local LevelDB records using the existing deserializer. */
class LocalRecordReader final
{
public:
    explicit LocalRecordReader(const std::span<const std::byte> bytes) : m_bytes{bytes} {}

    template <typename T>
    LocalRecordReader& operator>>(T&& value)
    {
        ::Unserialize(*this, std::forward<T>(value));
        return *this;
    }

    size_t size() const { return m_bytes.size() - m_offset; }
    bool empty() const { return size() == 0; }

    void read(const std::span<std::byte> destination)
    {
        if (destination.size() > size()) throw std::ios_base::failure{"truncated local CYBOU record"};
        if (!destination.empty()) {
            std::memcpy(destination.data(), m_bytes.data() + m_offset, destination.size());
            m_offset += destination.size();
        }
    }

    void ignore(const size_t count)
    {
        if (count > size()) throw std::ios_base::failure{"truncated local CYBOU record"};
        m_offset += count;
    }

private:
    std::span<const std::byte> m_bytes;
    size_t m_offset{0};
};

template <typename T>
std::vector<unsigned char> SerializeLocalRecord(const T& value)
{
    LocalRecordWriter writer;
    writer << value;
    return writer.Bytes();
}

} // namespace detail

struct KVStoreOptions {
    std::filesystem::path path;
    size_t cache_bytes{8 << 20};
    bool memory_only{false};
    bool wipe_data{false};
};

/** Minimal serializing LevelDB adapter for canonical CYBOU state. */
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
        const auto* begin = reinterpret_cast<const std::byte*>(raw->data());
        detail::LocalRecordReader reader{std::span<const std::byte>{begin, raw->size()}};
        try {
            reader >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
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

    /** Visit entries whose serialized std::string key starts with prefix.
     *  key_size is the complete, un-serialized string key length. */
    void ForEachStringPrefix(const std::string& prefix, size_t key_size,
        const std::function<void(const std::string&, const std::string&)>& visitor) const;

    /** Visit entries whose serialized std::string key starts with prefix, preserving raw value bytes. */
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
