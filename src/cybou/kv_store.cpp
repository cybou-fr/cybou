// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/kv_store.h>

#include <leveldb/cache.h>
#include <leveldb/db.h>
#include <leveldb/env.h>
#include <leveldb/filter_policy.h>
#include <leveldb/helpers/memenv/memenv.h>
#include <leveldb/options.h>
#include <leveldb/status.h>
#include <leveldb/write_batch.h>

#include <algorithm>
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

void KVStore::Batch::PutRaw(const DataStream& key, const DataStream& value)
{
    m_batch->Put(
        {reinterpret_cast<const char*>(key.data()), key.size()},
        {reinterpret_cast<const char*>(value.data()), value.size()});
}

void KVStore::Batch::EraseRaw(const DataStream& key)
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
        std::error_code ec;
        std::filesystem::create_directories(config.path, ec);
        if (ec) throw std::runtime_error("cannot create CYBOU database directory: " + ec.message());
        if (config.wipe_data) CheckLevelDB(leveldb::DestroyDB(PathAsUtf8(config.path), impl.options));
    }

    CheckLevelDB(leveldb::DB::Open(impl.options, PathAsUtf8(config.path), &impl.db));
}

KVStore::~KVStore() = default;

std::optional<std::string> KVStore::ReadRaw(const DataStream& key) const
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

} // namespace cybou
