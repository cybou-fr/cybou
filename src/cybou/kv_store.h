// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_KV_STORE_H
#define CYBOU_KV_STORE_H

#include <serialize.h>
#include <streams.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace leveldb { class WriteBatch; }

namespace cybou {

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
            DataStream serialized_key{};
            DataStream serialized_value{};
            serialized_key << key;
            serialized_value << value;
            PutRaw(serialized_key, serialized_value);
        }

        template <typename K>
        void Erase(const K& key)
        {
            DataStream serialized_key{};
            serialized_key << key;
            EraseRaw(serialized_key);
        }

    private:
        std::unique_ptr<leveldb::WriteBatch> m_batch;
        void PutRaw(const DataStream& key, const DataStream& value);
        void EraseRaw(const DataStream& key);
    };

    explicit KVStore(const KVStoreOptions& options);
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        DataStream serialized_key{};
        serialized_key << key;
        const auto raw = ReadRaw(serialized_key);
        if (!raw) return false;
        const auto* begin = reinterpret_cast<const std::byte*>(raw->data());
        SpanReader reader{std::span<const std::byte>{begin, raw->size()}};
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
        DataStream serialized_key{};
        serialized_key << key;
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

    std::optional<std::string> ReadRaw(const DataStream& key) const;
};

} // namespace cybou

#endif // CYBOU_KV_STORE_H
