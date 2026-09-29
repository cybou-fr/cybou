// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/kv_store.h>

#include <leveldb/cache.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <leveldb/helpers/memenv/memenv.h>
#include <leveldb/iterator.h>
#include <leveldb/options.h>
#include <leveldb/status.h>
#include <leveldb/write_batch.h>

#include <algorithm>
#include <cstring>
#include <ios>
#include <stdexcept>
#include <string>

namespace cybou {
namespace {

std::string PathAsUtf8(const std::filesystem::path& path)
{
    const auto native = path.u8string();
    return {reinterpret_cast<const char*>(native.data()), native.size()};
}

void CheckLevelDB(const leveldb::Status& status)
{
    if (!status.ok()) throw std::runtime_error("CYBOU LevelDB error: " + status.ToString());
}

} // namespace

KVStore::Batch::Batch() : m_batch{std::make_unique<leveldb::WriteBatch>()} {}
KVStore::Batch::~Batch() = default;
KVStore::Batch::Batch(Batch&&) noexcept = default;
KVStore::Batch& KVStore::Batch::operator=(Batch&&) noexcept = default;

void KVStore::Batch::PutRaw(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value)
{
    m_batch->Put({reinterpret_cast<const char*>(key.data()), key.size()},
        {reinterpret_cast<const char*>(value.data()), value.size()});
}

void KVStore::Batch::EraseRaw(const std::vector<unsigned char>& key)
{
    m_batch->Delete({reinterpret_cast<const char*>(key.data()), key.size()});
}

struct KVStore::Impl {
    leveldb::Env* memory_environment{nullptr};
    leveldb::Options options;
    leveldb::ReadOptions read_options;
    leveldb::WriteOptions write_options;
    leveldb::WriteOptions sync_options;
    leveldb::DB* db{nullptr};

    ~Impl()
    {
        delete db;
        delete options.filter_policy;
        delete options.block_cache;
        delete memory_environment;
    }
};

KVStore::KVStore(const KVStoreOptions& config)
    : m_impl{std::make_unique<Impl>()}
{
    auto& impl = *m_impl;
    impl.read_options.verify_checksums = true;
    impl.sync_options.sync = true;
    impl.options.create_if_missing = true;
    impl.options.paranoid_checks = true;
    impl.options.compression = leveldb::kNoCompression;
    const size_t cache_bytes = std::max<size_t>(config.cache_bytes, 1 << 20);
    impl.options.block_cache = leveldb::NewLRUCache(cache_bytes / 2);
    impl.options.write_buffer_size = cache_bytes / 4;
    impl.options.filter_policy = leveldb::NewBloomFilterPolicy(10);

    if (config.memory_only) {
        impl.memory_environment = leveldb::NewMemEnv(leveldb::Env::Default());
        impl.options.env = impl.memory_environment;
    } else {
        if (config.wipe_data) {
            if (config.path.empty()) {
                throw std::runtime_error("refusing to wipe CYBOU database at an empty path");
            }
            std::error_code ec;
            const auto absolute_path = std::filesystem::absolute(config.path, ec).lexically_normal();
            if (ec) throw std::runtime_error("cannot resolve CYBOU database path: " + ec.message());
            if (absolute_path == absolute_path.root_path()) {
                throw std::runtime_error("refusing to wipe CYBOU database at a filesystem root");
            }
            if (std::filesystem::exists(absolute_path, ec) && !ec) {
                const auto canonical_path = std::filesystem::weakly_canonical(absolute_path, ec);
                if (ec) throw std::runtime_error("cannot canonicalize CYBOU database path: " + ec.message());
                if (canonical_path == canonical_path.root_path()) {
                    throw std::runtime_error("refusing to wipe CYBOU database at a filesystem root");
                }
            }
        }
        std::error_code ec;
        std::filesystem::create_directories(config.path, ec);
        if (ec) throw std::runtime_error("cannot create CYBOU database directory: " + ec.message());
        if (config.wipe_data) CheckLevelDB(leveldb::DestroyDB(PathAsUtf8(config.path), impl.options));
    }

    CheckLevelDB(leveldb::DB::Open(impl.options, PathAsUtf8(config.path), &impl.db));
}

KVStore::~KVStore() = default;

std::optional<std::string> KVStore::ReadRaw(const std::vector<unsigned char>& key) const
{
    std::string value;
    const leveldb::Slice key_slice{reinterpret_cast<const char*>(key.data()), key.size()};
    const auto status = m_impl->db->Get(m_impl->read_options, key_slice, &value);
    if (status.IsNotFound()) return std::nullopt;
    CheckLevelDB(status);
    return value;
}

void KVStore::WriteBatch(Batch& batch, bool sync)
{
    CheckLevelDB(m_impl->db->Write(sync ? m_impl->sync_options : m_impl->write_options,
        batch.m_batch.get()));
}

void KVStore::ForEachStringPrefix(const std::string& prefix, const size_t key_size,
    const std::function<void(const std::string&, const std::string&)>& visitor) const
{
    if (prefix.size() > key_size || !visitor) throw std::invalid_argument("invalid CYBOU KV prefix scan");

    const auto encoded_key = detail::SerializeLocalRecord(std::string(key_size, '\0'));
    const size_t size_header = encoded_key.size() - key_size;
    std::string serialized_prefix{reinterpret_cast<const char*>(encoded_key.data()), size_header};
    serialized_prefix.append(prefix);

    std::unique_ptr<leveldb::Iterator> iterator{m_impl->db->NewIterator(m_impl->read_options)};
    for (iterator->Seek(serialized_prefix); iterator->Valid(); iterator->Next()) {
        const auto key_slice = iterator->key();
        if (key_slice.size() < serialized_prefix.size() ||
            std::memcmp(key_slice.data(), serialized_prefix.data(), serialized_prefix.size()) != 0) break;
        const auto value_slice = iterator->value();
        const auto key_bytes = std::span{reinterpret_cast<const unsigned char*>(key_slice.data()), key_slice.size()};
        const auto value_bytes = std::span{reinterpret_cast<const unsigned char*>(value_slice.data()), value_slice.size()};
        std::string decoded_key;
        std::string decoded_value;
        if (!detail::DeserializeLocalRecord(key_bytes, decoded_key) ||
            !detail::DeserializeLocalRecord(value_bytes, decoded_value)) {
            throw std::ios_base::failure{"corrupt local CYBOU string record"};
        }
        visitor(decoded_key, decoded_value);
    }
    CheckLevelDB(iterator->status());
}

void KVStore::ForEachStringPrefixRaw(const std::string& prefix, const size_t key_size,
    const std::function<void(const std::string&, const std::string&)>& visitor) const
{
    if (prefix.size() > key_size || !visitor) throw std::invalid_argument("invalid CYBOU KV prefix scan");

    const auto encoded_key = detail::SerializeLocalRecord(std::string(key_size, '\0'));
    const size_t size_header = encoded_key.size() - key_size;
    std::string serialized_prefix{reinterpret_cast<const char*>(encoded_key.data()), size_header};
    serialized_prefix.append(prefix);

    std::unique_ptr<leveldb::Iterator> iterator{m_impl->db->NewIterator(m_impl->read_options)};
    for (iterator->Seek(serialized_prefix); iterator->Valid(); iterator->Next()) {
        const auto key_slice = iterator->key();
        if (key_slice.size() < serialized_prefix.size() ||
            std::memcmp(key_slice.data(), serialized_prefix.data(), serialized_prefix.size()) != 0) break;
        const auto value_slice = iterator->value();
        const auto key_bytes = std::span{reinterpret_cast<const unsigned char*>(key_slice.data()), key_slice.size()};
        std::string decoded_key;
        if (!detail::DeserializeLocalRecord(key_bytes, decoded_key)) {
            throw std::ios_base::failure{"corrupt local CYBOU string key"};
        }
        visitor(decoded_key, std::string{value_slice.data(), value_slice.size()});
    }
    CheckLevelDB(iterator->status());
}

} // namespace cybou
