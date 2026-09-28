// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_store.h>

#include <cybou/crypto/cleanse.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace cybou {
namespace {

constexpr size_t CHUNK_HEADER_SIZE{4 + 12 + 32 + 4};
constexpr std::string_view STAGING_MAGIC{"STG1"};
constexpr uint32_t MAX_STAGING_OBJECTS{1024};
constexpr uint64_t MAX_STAGING_BYTES{64ULL << 20};
constexpr uint64_t MIN_STAGING_BYTES{2ULL * (STORAGE_OBJECT_CHUNK_SIZE + CHUNK_HEADER_SIZE + 16)};
constexpr auto DEFAULT_STAGING_TTL = std::chrono::hours{24};

void Put32(std::string& out, uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<char>(value >> shift));
}

uint32_t Read32(const unsigned char* bytes)
{
    uint32_t value{0};
    for (unsigned shift = 0; shift < 32; shift += 8) value |= uint32_t{bytes[shift / 8]} << shift;
    return value;
}

void Put64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8) out.push_back(static_cast<unsigned char>(value >> shift));
}

uint64_t Read64(const unsigned char* bytes)
{
    uint64_t value{0};
    for (unsigned shift = 0; shift < 64; shift += 8) value |= uint64_t{bytes[shift / 8]} << shift;
    return value;
}

void Put32(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<unsigned char>(value >> shift));
}

int64_t UnixTimeMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string EncodeChunk(const StorageEncryptedChunk& chunk)
{
    std::string result;
    result.reserve(CHUNK_HEADER_SIZE + chunk.ciphertext_and_tag.size());
    Put32(result, chunk.index);
    result.append(reinterpret_cast<const char*>(chunk.nonce.data()), chunk.nonce.size());
    result.append(reinterpret_cast<const char*>(chunk.chunk_id.data()), chunk.chunk_id.size());
    Put32(result, static_cast<uint32_t>(chunk.ciphertext_and_tag.size()));
    result.append(reinterpret_cast<const char*>(chunk.ciphertext_and_tag.data()), chunk.ciphertext_and_tag.size());
    return result;
}

std::optional<StorageEncryptedChunk> DecodeChunk(const std::string& bytes)
{
    if (bytes.size() < CHUNK_HEADER_SIZE) return std::nullopt;
    const auto* data = reinterpret_cast<const unsigned char*>(bytes.data());
    StorageEncryptedChunk chunk;
    chunk.index = Read32(data);
    std::copy_n(data + 4, chunk.nonce.size(), chunk.nonce.begin());
    std::copy_n(data + 4 + chunk.nonce.size(), chunk.chunk_id.size(), chunk.chunk_id.begin());
    const size_t length_offset = 4 + chunk.nonce.size() + chunk.chunk_id.size();
    const uint32_t size = Read32(data + length_offset);
    if (size < 16 || size > STORAGE_OBJECT_CHUNK_SIZE + 16 || bytes.size() != CHUNK_HEADER_SIZE + size) {
        return std::nullopt;
    }
    chunk.ciphertext_and_tag.assign(data + CHUNK_HEADER_SIZE, data + CHUNK_HEADER_SIZE + size);
    return chunk;
}

bool SameChunk(const StorageEncryptedChunk& left, const StorageEncryptedChunk& right)
{
    return left.index == right.index && left.nonce == right.nonce && left.chunk_id == right.chunk_id &&
        left.ciphertext_and_tag == right.ciphertext_and_tag;
}

} // namespace

StorageObjectStore::StorageObjectStore(
    const std::filesystem::path& path,
    const std::span<const unsigned char, 32> network_id,
    const uint64_t capacity_bytes,
    const bool memory_only,
    const bool wipe_data,
    StorageStagingPolicy staging_policy)
    : m_capacity_bytes{capacity_bytes}
{
    if (std::all_of(network_id.begin(), network_id.end(), [](unsigned char b) { return b == 0; }) ||
        capacity_bytes == 0 || (!memory_only && path.empty())) {
        throw std::invalid_argument("invalid storage provider configuration");
    }
    std::copy(network_id.begin(), network_id.end(), m_network_id.begin());
    m_db = std::make_unique<KVStore>(KVStoreOptions{
        .path = path,
        .cache_bytes = 8 << 20,
        .memory_only = memory_only,
        .wipe_data = wipe_data,
    });
    if (staging_policy.max_objects == 0 || staging_policy.ttl.count() <= 0) {
        throw std::invalid_argument("invalid storage staging policy");
    }
    m_max_staging_objects = std::min(staging_policy.max_objects, MAX_STAGING_OBJECTS);
    m_staging_ttl = staging_policy.ttl;
    const uint64_t default_staging_bytes = std::min(capacity_bytes,
        std::max(MIN_STAGING_BYTES, std::min(capacity_bytes / 4, MAX_STAGING_BYTES)));
    m_max_staging_bytes = staging_policy.max_bytes == 0
        ? default_staging_bytes : std::min(staging_policy.max_bytes, capacity_bytes);

    if (!m_db->Exists(StagingKey())) RecoverLegacyUncommittedChunks();
    else (void)ReadStagingRecords(); // fail closed on damaged persistent staging metadata
    (void)GarbageCollectExpiredStaging();
}

std::string StorageObjectStore::ChunkKey(const StorageObjectId& object_id, const uint32_t index)
{
    std::string key{"chunk:"};
    key.append(reinterpret_cast<const char*>(object_id.data()), object_id.size());
    Put32(key, index);
    return key;
}

std::string StorageObjectStore::ManifestKey(const StorageObjectId& object_id)
{
    std::string key{"manifest:"};
    key.append(reinterpret_cast<const char*>(object_id.data()), object_id.size());
    return key;
}

std::string StorageObjectStore::UsageKey()
{
    return "storage:used-bytes";
}

std::string StorageObjectStore::StagingKey()
{
    return "storage:staging-v1";
}

std::vector<unsigned char> StorageObjectStore::EncodeStagingRecords(
    const std::map<StorageObjectId, StagingRecord>& records)
{
    if (records.size() > MAX_STAGING_OBJECTS) throw std::runtime_error("too many storage staging records");
    std::vector<unsigned char> encoded(STAGING_MAGIC.begin(), STAGING_MAGIC.end());
    Put32(encoded, static_cast<uint32_t>(records.size()));
    for (const auto& [object_id, record] : records) {
        encoded.insert(encoded.end(), object_id.begin(), object_id.end());
        Put64(encoded, record.received_bytes);
        Put64(encoded, static_cast<uint64_t>(record.last_activity_ms));
        Put32(encoded, record.chunk_count);
        Put32(encoded, record.highest_index);
    }
    return encoded;
}

std::optional<std::map<StorageObjectId, StorageObjectStore::StagingRecord>>
StorageObjectStore::DecodeStagingRecords(const std::vector<unsigned char>& encoded)
{
    constexpr size_t HEADER_SIZE{8};
    constexpr size_t RECORD_SIZE{32 + 8 + 8 + 4 + 4};
    if (encoded.size() < HEADER_SIZE ||
        !std::equal(STAGING_MAGIC.begin(), STAGING_MAGIC.end(), encoded.begin())) return std::nullopt;
    const uint32_t count = Read32(encoded.data() + 4);
    if (count > MAX_STAGING_OBJECTS || encoded.size() != HEADER_SIZE + size_t{count} * RECORD_SIZE) {
        return std::nullopt;
    }
    std::map<StorageObjectId, StagingRecord> records;
    size_t offset{HEADER_SIZE};
    for (uint32_t i = 0; i < count; ++i) {
        StorageObjectId object_id{};
        std::copy_n(encoded.data() + offset, object_id.size(), object_id.begin());
        offset += object_id.size();
        StagingRecord record{
            .received_bytes = Read64(encoded.data() + offset),
            .last_activity_ms = static_cast<int64_t>(Read64(encoded.data() + offset + 8)),
            .chunk_count = Read32(encoded.data() + offset + 16),
            .highest_index = Read32(encoded.data() + offset + 20),
        };
        offset += RECORD_SIZE - object_id.size();
        if (object_id == StorageObjectId{} || record.received_bytes == 0 || record.last_activity_ms <= 0 ||
            record.chunk_count == 0 || record.highest_index != record.chunk_count - 1 ||
            record.chunk_count > STORAGE_OBJECT_MAX_CHUNKS || !records.emplace(object_id, record).second) {
            return std::nullopt;
        }
    }
    return records;
}

std::map<StorageObjectId, StorageObjectStore::StagingRecord> StorageObjectStore::ReadStagingRecords() const
{
    std::vector<unsigned char> encoded;
    if (!m_db->Read(StagingKey(), encoded)) return {};
    const auto records = DecodeStagingRecords(encoded);
    if (!records) throw std::runtime_error("corrupt storage staging metadata");
    return *records;
}

void StorageObjectStore::RecoverLegacyUncommittedChunks()
{
    std::map<StorageObjectId, uint64_t> legacy_orphans;
    std::map<StorageObjectId, std::vector<uint32_t>> legacy_indices;
    m_db->ForEachStringPrefix("chunk:", 42, [&](const std::string& key, const std::string& value) {
        if (key.size() != 42) return;
        StorageObjectId object_id{};
        std::copy_n(reinterpret_cast<const unsigned char*>(key.data() + 6), object_id.size(), object_id.begin());
        if (m_db->Exists(ManifestKey(object_id))) return;
        uint32_t index{0};
        const auto* index_bytes = reinterpret_cast<const unsigned char*>(key.data() + 38);
        index = Read32(index_bytes);
        legacy_orphans[object_id] += value.size();
        legacy_indices[object_id].push_back(index);
    });

    uint64_t released{0};
    uint64_t used{0};
    (void)m_db->Read(UsageKey(), used);
    if (legacy_orphans.empty()) {
        const auto empty = EncodeStagingRecords({});
        m_db->Write(StagingKey(), empty, true);
        return;
    }
    KVStore::Batch batch;
    for (const auto& [object_id, bytes] : legacy_orphans) {
        if (bytes > std::numeric_limits<uint64_t>::max() - released) {
            throw std::runtime_error("legacy storage usage overflow");
        }
        released += bytes;
        for (const auto index : legacy_indices[object_id]) batch.Erase(ChunkKey(object_id, index));
    }
    if (released > used) throw std::runtime_error("legacy storage usage metadata is inconsistent");
    batch.Write(UsageKey(), used - released);
    batch.Write(StagingKey(), EncodeStagingRecords({}));
    m_db->WriteBatch(batch, true);
}

std::optional<StorageEncryptedChunk> StorageObjectStore::ReadChunk(
    const StorageObjectId& object_id, const uint32_t index) const
{
    std::string encoded;
    if (!m_db->Read(ChunkKey(object_id, index), encoded)) return std::nullopt;
    const auto chunk = DecodeChunk(encoded);
    if (!chunk || chunk->index != index) return std::nullopt;
    StorageChunkId expected{};
    if (!ComputeStorageChunkId(m_network_id, object_id, index, chunk->nonce,
            chunk->ciphertext_and_tag, expected) || expected != chunk->chunk_id) return std::nullopt;
    return chunk;
}

std::optional<StoragePublicManifest> StorageObjectStore::ReadManifest(const StorageObjectId& object_id) const
{
    std::vector<unsigned char> encoded;
    if (!m_db->Read(ManifestKey(object_id), encoded)) return std::nullopt;
    const auto manifest = DecodeStoragePublicManifest(m_network_id, encoded);
    if (!manifest || manifest->object_id != object_id) return std::nullopt;
    return manifest;
}

StorageWriteResult StorageObjectStore::PutChunk(
    const StorageObjectId& object_id, const StorageEncryptedChunk& chunk)
{
    if (object_id == StorageObjectId{} || chunk.index >= STORAGE_OBJECT_MAX_CHUNKS || chunk.ciphertext_and_tag.size() < 16 ||
        chunk.ciphertext_and_tag.size() > STORAGE_OBJECT_CHUNK_SIZE + 16) return {};
    StorageChunkId computed_id{};
    if (!ComputeStorageChunkId(m_network_id, object_id, chunk.index, chunk.nonce,
            chunk.ciphertext_and_tag, computed_id) || computed_id != chunk.chunk_id) {
        return {StorageWriteStatus::INVALID};
    }
    const std::string encoded = EncodeChunk(chunk);
    std::lock_guard lock(m_mutex);
    const int64_t now_ms = UnixTimeMilliseconds();
    (void)GarbageCollectExpiredStagingLocked(now_ms);
    auto staging = ReadStagingRecords();
    const std::string chunk_key = ChunkKey(object_id, chunk.index);
    if (const auto manifest = ReadManifest(object_id)) {
        if (chunk.index >= manifest->chunk_count) return {StorageWriteStatus::CONFLICT};
        const auto& descriptor = manifest->chunks[chunk.index];
        if (descriptor.chunk_id != chunk.chunk_id || descriptor.ciphertext_size != chunk.ciphertext_and_tag.size()) {
            return {StorageWriteStatus::CONFLICT};
        }
        const auto existing = ReadChunk(object_id, chunk.index);
        return existing && SameChunk(*existing, chunk)
            ? StorageWriteResult{StorageWriteStatus::ALREADY_STORED, chunk.chunk_id}
            : StorageWriteResult{StorageWriteStatus::CONFLICT};
    }
    if (m_db->Exists(ManifestKey(object_id))) return {StorageWriteStatus::CONFLICT};
    if (m_db->Exists(chunk_key)) {
        const auto existing = ReadChunk(object_id, chunk.index);
        if (!existing) return {StorageWriteStatus::CONFLICT};
        return SameChunk(*existing, chunk)
            ? StorageWriteResult{StorageWriteStatus::ALREADY_STORED, chunk.chunk_id}
            : StorageWriteResult{StorageWriteStatus::CONFLICT};
    }
    auto staged = staging.find(object_id);
    if (staged == staging.end()) {
        if (chunk.index != 0) return {StorageWriteStatus::INCOMPLETE};
        if (staging.size() >= m_max_staging_objects) return {StorageWriteStatus::CAPACITY_EXCEEDED};
        staged = staging.emplace(object_id, StagingRecord{}).first;
    } else if (chunk.index != staged->second.chunk_count) {
        return {StorageWriteStatus::INCOMPLETE};
    }
    uint64_t staged_bytes{0};
    for (const auto& [staged_object, record] : staging) {
        if (record.received_bytes > std::numeric_limits<uint64_t>::max() - staged_bytes) {
            return {StorageWriteStatus::CAPACITY_EXCEEDED};
        }
        staged_bytes += record.received_bytes;
    }
    if (encoded.size() > m_max_staging_bytes || staged_bytes > m_max_staging_bytes - encoded.size()) {
        return {StorageWriteStatus::CAPACITY_EXCEEDED};
    }
    auto& record = staged->second;
    if (encoded.size() > std::numeric_limits<uint64_t>::max() - record.received_bytes ||
        record.chunk_count >= STORAGE_OBJECT_MAX_CHUNKS) {
        return {StorageWriteStatus::CAPACITY_EXCEEDED};
    }
    record.received_bytes += encoded.size();
    record.last_activity_ms = now_ms;
    record.highest_index = chunk.index;
    ++record.chunk_count;
    uint64_t used{0};
    (void)m_db->Read(UsageKey(), used);
    if (encoded.size() > m_capacity_bytes || used > m_capacity_bytes - encoded.size()) {
        return {StorageWriteStatus::CAPACITY_EXCEEDED};
    }
    KVStore::Batch batch;
    batch.Write(chunk_key, encoded);
    batch.Write(UsageKey(), used + encoded.size());
    batch.Write(StagingKey(), EncodeStagingRecords(staging));
    m_db->WriteBatch(batch, true);
    return {StorageWriteStatus::STORED, chunk.chunk_id};
}

StorageWriteResult StorageObjectStore::CommitManifest(const StoragePublicManifest& manifest)
{
    if (!VerifyStoragePublicManifest(m_network_id, manifest)) return {StorageWriteStatus::INVALID};
    const auto encoded = EncodeStoragePublicManifest(m_network_id, manifest);
    if (!encoded) return {StorageWriteStatus::INVALID};
    std::lock_guard lock(m_mutex);
    (void)GarbageCollectExpiredStagingLocked(UnixTimeMilliseconds());
    auto staging = ReadStagingRecords();
    const std::string manifest_key = ManifestKey(manifest.object_id);
    if (const auto existing = ReadManifest(manifest.object_id)) {
        const auto current = EncodeStoragePublicManifest(m_network_id, *existing);
        if (current && *current == *encoded) {
            return {StorageWriteStatus::ALREADY_STORED, manifest.commitment};
        }
        return {StorageWriteStatus::CONFLICT};
    }
    if (m_db->Exists(manifest_key)) return {StorageWriteStatus::CONFLICT};
    const auto staged = staging.find(manifest.object_id);
    if (staged == staging.end() || staged->second.chunk_count != manifest.chunk_count ||
        staged->second.highest_index + 1 != manifest.chunk_count) {
        return {StorageWriteStatus::INCOMPLETE};
    }
    for (uint32_t i = 0; i < manifest.chunk_count; ++i) {
        const auto chunk = ReadChunk(manifest.object_id, i);
        const auto& descriptor = manifest.chunks[i];
        if (!chunk) return {StorageWriteStatus::INCOMPLETE};
        if (chunk->chunk_id != descriptor.chunk_id || chunk->ciphertext_and_tag.size() != descriptor.ciphertext_size) {
            return {StorageWriteStatus::CONFLICT};
        }
    }
    uint64_t used{0};
    (void)m_db->Read(UsageKey(), used);
    if (encoded->size() > m_capacity_bytes || used > m_capacity_bytes - encoded->size()) {
        return {StorageWriteStatus::CAPACITY_EXCEEDED};
    }
    KVStore::Batch batch;
    batch.Write(manifest_key, *encoded);
    batch.Write(UsageKey(), used + encoded->size());
    staging.erase(manifest.object_id);
    batch.Write(StagingKey(), EncodeStagingRecords(staging));
    m_db->WriteBatch(batch, true);
    return {StorageWriteStatus::STORED, manifest.commitment};
}

bool StorageObjectStore::AbortUncommittedObject(
    const StorageObjectId& object_id, const uint32_t chunk_count)
{
    if (object_id == StorageObjectId{} || chunk_count > STORAGE_OBJECT_MAX_CHUNKS) return false;
    std::lock_guard lock(m_mutex);
    if (m_db->Exists(ManifestKey(object_id))) return false;

    uint64_t used{0};
    (void)m_db->Read(UsageKey(), used);
    auto staging = ReadStagingRecords();
    const auto staged = staging.find(object_id);
    const uint32_t remove_count = staged == staging.end()
        ? chunk_count : staged->second.highest_index + 1;
    uint64_t released{0};
    KVStore::Batch batch;
    for (uint32_t index = 0; index < remove_count; ++index) {
        const auto key = ChunkKey(object_id, index);
        std::string encoded;
        if (!m_db->Read(key, encoded)) continue;
        if (encoded.size() > std::numeric_limits<uint64_t>::max() - released) return false;
        released += encoded.size();
        batch.Erase(key);
    }
    if (released > used) return false;
    batch.Write(UsageKey(), used - released);
    if (staged != staging.end()) staging.erase(staged);
    batch.Write(StagingKey(), EncodeStagingRecords(staging));
    m_db->WriteBatch(batch, true);
    return true;
}

std::optional<StoragePublicManifest> StorageObjectStore::GetManifest(const StorageObjectId& object_id) const
{
    std::lock_guard lock(m_mutex);
    return ReadManifest(object_id);
}

std::optional<StorageEncryptedChunk> StorageObjectStore::GetChunk(
    const StorageObjectId& object_id, const uint32_t index) const
{
    std::lock_guard lock(m_mutex);
    const auto manifest = ReadManifest(object_id);
    if (!manifest || index >= manifest->chunk_count) return std::nullopt;
    const auto chunk = ReadChunk(object_id, index);
    if (!chunk || chunk->chunk_id != manifest->chunks[index].chunk_id ||
        chunk->ciphertext_and_tag.size() != manifest->chunks[index].ciphertext_size) return std::nullopt;
    return chunk;
}

uint64_t StorageObjectStore::UsedBytes() const
{
    std::lock_guard lock(m_mutex);
    uint64_t used{0};
    if (!m_db->Read(UsageKey(), used)) return 0;
    return used;
}

uint64_t StorageObjectStore::StagedBytes() const
{
    std::lock_guard lock(m_mutex);
    uint64_t staged{0};
    for (const auto& [object_id, record] : ReadStagingRecords()) {
        if (record.received_bytes > std::numeric_limits<uint64_t>::max() - staged) {
            throw std::runtime_error("storage staging byte count overflow");
        }
        staged += record.received_bytes;
    }
    return staged;
}

size_t StorageObjectStore::StagedObjectCount() const
{
    std::lock_guard lock(m_mutex);
    return ReadStagingRecords().size();
}

uint64_t StorageObjectStore::GarbageCollectExpiredStagingLocked(const int64_t now_ms)
{
    auto staging = ReadStagingRecords();
    const auto ttl_ms = m_staging_ttl.count();
    uint64_t used{0};
    (void)m_db->Read(UsageKey(), used);
    uint64_t released{0};
    KVStore::Batch batch;
    for (auto it = staging.begin(); it != staging.end();) {
        if (now_ms < it->second.last_activity_ms ||
            now_ms - it->second.last_activity_ms < ttl_ms) {
            ++it;
            continue;
        }
        for (uint32_t index = 0; index <= it->second.highest_index; ++index) {
            const auto key = ChunkKey(it->first, index);
            std::string encoded;
            if (!m_db->Read(key, encoded)) continue;
            if (encoded.size() > std::numeric_limits<uint64_t>::max() - released) {
                throw std::runtime_error("storage staging byte count overflow");
            }
            released += encoded.size();
            batch.Erase(key);
        }
        it = staging.erase(it);
    }
    if (released == 0 && staging.size() == ReadStagingRecords().size()) return 0;
    if (released > used) throw std::runtime_error("storage staging usage metadata is inconsistent");
    batch.Write(UsageKey(), used - released);
    batch.Write(StagingKey(), EncodeStagingRecords(staging));
    m_db->WriteBatch(batch, true);
    return released;
}

uint64_t StorageObjectStore::GarbageCollectExpiredStaging()
{
    std::lock_guard lock(m_mutex);
    return GarbageCollectExpiredStagingLocked(UnixTimeMilliseconds());
}

} // namespace cybou
