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
    const bool wipe_data)
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
    uint64_t used{0};
    (void)m_db->Read(UsageKey(), used);
    if (encoded.size() > m_capacity_bytes || used > m_capacity_bytes - encoded.size()) {
        return {StorageWriteStatus::CAPACITY_EXCEEDED};
    }
    KVStore::Batch batch;
    batch.Write(chunk_key, encoded);
    batch.Write(UsageKey(), used + encoded.size());
    m_db->WriteBatch(batch, true);
    return {StorageWriteStatus::STORED, chunk.chunk_id};
}

StorageWriteResult StorageObjectStore::CommitManifest(const StoragePublicManifest& manifest)
{
    if (!VerifyStoragePublicManifest(m_network_id, manifest)) return {StorageWriteStatus::INVALID};
    const auto encoded = EncodeStoragePublicManifest(m_network_id, manifest);
    if (!encoded) return {StorageWriteStatus::INVALID};
    std::lock_guard lock(m_mutex);
    const std::string manifest_key = ManifestKey(manifest.object_id);
    if (const auto existing = ReadManifest(manifest.object_id)) {
        const auto current = EncodeStoragePublicManifest(m_network_id, *existing);
        if (current && *current == *encoded) {
            return {StorageWriteStatus::ALREADY_STORED, manifest.commitment};
        }
        return {StorageWriteStatus::CONFLICT};
    }
    if (m_db->Exists(manifest_key)) return {StorageWriteStatus::CONFLICT};
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
    uint64_t released{0};
    KVStore::Batch batch;
    for (uint32_t index = 0; index < chunk_count; ++index) {
        const auto key = ChunkKey(object_id, index);
        std::string encoded;
        if (!m_db->Read(key, encoded)) continue;
        if (encoded.size() > std::numeric_limits<uint64_t>::max() - released) return false;
        released += encoded.size();
        batch.Erase(key);
    }
    if (released > used) return false;
    batch.Write(UsageKey(), used - released);
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

} // namespace cybou
