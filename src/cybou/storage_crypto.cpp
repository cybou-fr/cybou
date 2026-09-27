// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_crypto.h>

#include <cybou/crypto/chacha20_poly1305.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/hkdf_sha256.h>
#include <cybou/crypto/sha256.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
#include <utility>

namespace cybou {
namespace {

constexpr std::string_view OBJECT_KEY_INFO{"CYBOU/STORAGE/OBJECT/V1"};
constexpr std::string_view CHUNK_AAD_DOMAIN{"CYBOU/STORAGE/OBJECT-CHUNK/V1"};
constexpr std::string_view CHUNK_ID_DOMAIN{"CYBOU/STORAGE/CHUNK-ID/V1"};
constexpr std::string_view MANIFEST_DOMAIN{"CYBOU/STORAGE/MANIFEST/V1"};
constexpr size_t TAG_SIZE{crypto::CHACHA20_POLY1305_TAG_SIZE};
constexpr std::array<unsigned char, 4> MANIFEST_MAGIC{'C', 'S', 'M', '1'};

template <typename Range>
bool IsZero(const Range& bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](unsigned char byte) { return byte == 0; });
}

void Put32(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<unsigned char>(value >> shift));
    }
}

void Put64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<unsigned char>(value >> shift));
    }
}

uint32_t Read32(const unsigned char* bytes)
{
    uint32_t value{0};
    for (unsigned shift = 0; shift < 32; shift += 8) value |= uint32_t{bytes[shift / 8]} << shift;
    return value;
}

std::optional<uint32_t> GetChunkCount(uint64_t plaintext_size)
{
    if (plaintext_size > STORAGE_OBJECT_MAX_BYTES) return std::nullopt;
    if (plaintext_size == 0) return 1;
    const uint64_t count = (plaintext_size + STORAGE_OBJECT_CHUNK_SIZE - 1) / STORAGE_OBJECT_CHUNK_SIZE;
    if (count > STORAGE_OBJECT_MAX_CHUNKS) return std::nullopt;
    return static_cast<uint32_t>(count);
}

std::optional<std::array<unsigned char, 32>> DeriveObjectKey(
    std::span<const unsigned char, 32> storage_master_key,
    const StorageObjectPrivateMetadata& metadata)
{
    std::vector<unsigned char> info;
    const auto domain = crypto::Sha256Bytes(OBJECT_KEY_INFO);
    info.insert(info.end(), domain.begin(), domain.end());
    info.insert(info.end(), metadata.object_id.begin(), metadata.object_id.end());
    Put32(info, metadata.key_epoch);
    std::array<unsigned char, 32> object_key{};
    if (!crypto::HkdfSha256(storage_master_key, metadata.salt, info, object_key)) {
        crypto::CleanseMemory(object_key.data(), object_key.size());
        return std::nullopt;
    }
    return object_key;
}

std::vector<unsigned char> ChunkAad(
    std::span<const unsigned char, 32> network_id,
    const StorageObjectPrivateMetadata& metadata,
    uint32_t chunk_count,
    uint32_t index)
{
    std::vector<unsigned char> aad;
    const auto domain = crypto::Sha256Bytes(CHUNK_AAD_DOMAIN);
    aad.reserve(domain.size() + network_id.size() + metadata.object_id.size() + 12);
    aad.insert(aad.end(), domain.begin(), domain.end());
    aad.insert(aad.end(), network_id.begin(), network_id.end());
    aad.insert(aad.end(), metadata.object_id.begin(), metadata.object_id.end());
    Put32(aad, metadata.key_epoch);
    Put32(aad, chunk_count);
    Put32(aad, index);
    return aad;
}

bool ComputeChunkId(
    std::span<const unsigned char, 32> network_id,
    const StorageObjectId& object_id,
    uint32_t index,
    std::span<const unsigned char, 12> nonce,
    std::span<const unsigned char> ciphertext_and_tag,
    StorageChunkId& result)
{
    std::vector<unsigned char> index_bytes;
    Put32(index_bytes, index);
    const auto domain = crypto::Sha256Bytes(CHUNK_ID_DOMAIN);
    return crypto::ComputeSha256({domain, network_id, object_id, index_bytes, nonce, ciphertext_and_tag}, result.data());
}

bool ComputeManifestCommitment(
    std::span<const unsigned char, 32> network_id,
    const StoragePublicManifest& manifest,
    StorageChunkId& result)
{
    std::vector<unsigned char> bytes;
    const auto domain = crypto::Sha256Bytes(MANIFEST_DOMAIN);
    bytes.reserve(domain.size() + network_id.size() + manifest.object_id.size() + 4 + manifest.chunks.size() * 36);
    bytes.insert(bytes.end(), domain.begin(), domain.end());
    bytes.insert(bytes.end(), network_id.begin(), network_id.end());
    bytes.insert(bytes.end(), manifest.object_id.begin(), manifest.object_id.end());
    Put32(bytes, manifest.chunk_count);
    for (const auto& chunk : manifest.chunks) {
        bytes.insert(bytes.end(), chunk.chunk_id.begin(), chunk.chunk_id.end());
        Put32(bytes, chunk.ciphertext_size);
    }
    return crypto::ComputeSha256({std::span<const unsigned char>{bytes}}, result.data());
}

} // namespace

bool ComputeStorageChunkId(
    const std::span<const unsigned char, 32> network_id,
    const StorageObjectId& object_id,
    const uint32_t index,
    const std::span<const unsigned char, 12> nonce,
    const std::span<const unsigned char> ciphertext_and_tag,
    StorageChunkId& chunk_id)
{
    if (IsZero(network_id) || object_id == StorageObjectId{} ||
        index >= STORAGE_OBJECT_MAX_CHUNKS || ciphertext_and_tag.size() < TAG_SIZE ||
        ciphertext_and_tag.size() > STORAGE_OBJECT_CHUNK_SIZE + TAG_SIZE) return false;
    return ComputeChunkId(network_id, object_id, index, nonce, ciphertext_and_tag, chunk_id);
}

std::optional<StorageObjectPrivateMetadata> CreateStorageObjectMetadata(
    const uint64_t plaintext_size, const uint32_t key_epoch)
{
    if (!GetChunkCount(plaintext_size)) return std::nullopt;
    StorageObjectPrivateMetadata metadata{.key_epoch = key_epoch, .plaintext_size = plaintext_size};
    if (RAND_bytes(metadata.object_id.data(), metadata.object_id.size()) != 1 ||
        RAND_bytes(metadata.salt.data(), metadata.salt.size()) != 1) {
        crypto::CleanseMemory(metadata.salt.data(), metadata.salt.size());
        return std::nullopt;
    }
    return metadata;
}

std::optional<StoragePublicManifest> BuildStoragePublicManifest(
    const std::span<const unsigned char, 32> network_id,
    const StorageObjectId& object_id,
    const std::span<const StorageEncryptedChunk> chunks)
{
    if (IsZero(network_id) || chunks.empty() || chunks.size() > STORAGE_OBJECT_MAX_CHUNKS) return std::nullopt;
    if (object_id == StorageObjectId{}) return std::nullopt;
    StoragePublicManifest manifest{.object_id = object_id, .chunk_count = static_cast<uint32_t>(chunks.size())};
    manifest.chunks.reserve(chunks.size());
    uint64_t total_ciphertext = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        const auto& chunk = chunks[i];
        StorageChunkId expected_id{};
        if (chunk.index != i || chunk.ciphertext_and_tag.size() < TAG_SIZE ||
            chunk.ciphertext_and_tag.size() > STORAGE_OBJECT_CHUNK_SIZE + TAG_SIZE ||
            (i + 1 < chunks.size() && chunk.ciphertext_and_tag.size() != STORAGE_OBJECT_CHUNK_SIZE + TAG_SIZE) ||
            !ComputeChunkId(network_id, object_id, chunk.index, chunk.nonce, chunk.ciphertext_and_tag, expected_id) ||
            expected_id != chunk.chunk_id) return std::nullopt;
        total_ciphertext += chunk.ciphertext_and_tag.size();
        if (total_ciphertext > STORAGE_OBJECT_MAX_BYTES + uint64_t{STORAGE_OBJECT_MAX_CHUNKS} * TAG_SIZE) {
            return std::nullopt;
        }
        manifest.chunks.push_back(StorageChunkDescriptor{
            .chunk_id = chunk.chunk_id,
            .ciphertext_size = static_cast<uint32_t>(chunk.ciphertext_and_tag.size()),
        });
    }
    if (!ComputeManifestCommitment(network_id, manifest, manifest.commitment)) return std::nullopt;
    return manifest;
}

std::optional<StoragePublicManifest> BuildStoragePublicManifestFromDescriptors(
    const std::span<const unsigned char, 32> network_id,
    const StorageObjectId& object_id,
    const std::span<const StorageChunkDescriptor> chunks)
{
    if (IsZero(network_id) || object_id == StorageObjectId{} || chunks.empty() ||
        chunks.size() > STORAGE_OBJECT_MAX_CHUNKS) return std::nullopt;
    StoragePublicManifest manifest{.object_id = object_id, .chunk_count = static_cast<uint32_t>(chunks.size())};
    manifest.chunks.assign(chunks.begin(), chunks.end());
    if (!ComputeManifestCommitment(network_id, manifest, manifest.commitment) ||
        !VerifyStoragePublicManifest(network_id, manifest)) return std::nullopt;
    return manifest;
}

bool VerifyStoragePublicManifest(
    const std::span<const unsigned char, 32> network_id,
    const StoragePublicManifest& manifest)
{
    if (IsZero(network_id) || manifest.object_id == StorageObjectId{} || manifest.chunk_count == 0 ||
        manifest.chunk_count > STORAGE_OBJECT_MAX_CHUNKS || manifest.chunks.size() != manifest.chunk_count) return false;
    uint64_t total = 0;
    for (size_t i = 0; i < manifest.chunks.size(); ++i) {
        const auto& chunk = manifest.chunks[i];
        if (chunk.chunk_id == StorageChunkId{} || chunk.ciphertext_size < TAG_SIZE ||
            chunk.ciphertext_size > STORAGE_OBJECT_CHUNK_SIZE + TAG_SIZE ||
            (i + 1 < manifest.chunks.size() && chunk.ciphertext_size != STORAGE_OBJECT_CHUNK_SIZE + TAG_SIZE)) return false;
        total += chunk.ciphertext_size;
        if (total > STORAGE_OBJECT_MAX_BYTES + uint64_t{STORAGE_OBJECT_MAX_CHUNKS} * TAG_SIZE) return false;
    }
    StorageChunkId commitment{};
    return ComputeManifestCommitment(network_id, manifest, commitment) && commitment == manifest.commitment;
}

std::optional<std::vector<unsigned char>> EncodeStoragePublicManifest(
    const std::span<const unsigned char, 32> network_id,
    const StoragePublicManifest& manifest)
{
    if (!VerifyStoragePublicManifest(network_id, manifest)) return std::nullopt;
    std::vector<unsigned char> bytes;
    bytes.reserve(4 + 32 + 4 + manifest.chunks.size() * 36 + 32);
    bytes.insert(bytes.end(), MANIFEST_MAGIC.begin(), MANIFEST_MAGIC.end());
    bytes.insert(bytes.end(), manifest.object_id.begin(), manifest.object_id.end());
    Put32(bytes, manifest.chunk_count);
    for (const auto& chunk : manifest.chunks) {
        bytes.insert(bytes.end(), chunk.chunk_id.begin(), chunk.chunk_id.end());
        Put32(bytes, chunk.ciphertext_size);
    }
    bytes.insert(bytes.end(), manifest.commitment.begin(), manifest.commitment.end());
    if (bytes.size() > STORAGE_PUBLIC_MANIFEST_MAX_BYTES) return std::nullopt;
    return bytes;
}

std::optional<StoragePublicManifest> DecodeStoragePublicManifest(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char> bytes)
{
    constexpr size_t FIXED_SIZE{4 + 32 + 4 + 32};
    if (bytes.size() < FIXED_SIZE || bytes.size() > STORAGE_PUBLIC_MANIFEST_MAX_BYTES ||
        !std::equal(MANIFEST_MAGIC.begin(), MANIFEST_MAGIC.end(), bytes.begin())) return std::nullopt;
    StoragePublicManifest manifest;
    size_t offset = MANIFEST_MAGIC.size();
    std::copy_n(bytes.begin() + offset, manifest.object_id.size(), manifest.object_id.begin());
    offset += manifest.object_id.size();
    manifest.chunk_count = Read32(bytes.data() + offset);
    offset += 4;
    if (manifest.chunk_count == 0 || manifest.chunk_count > STORAGE_OBJECT_MAX_CHUNKS ||
        bytes.size() != FIXED_SIZE + static_cast<size_t>(manifest.chunk_count) * 36) return std::nullopt;
    manifest.chunks.reserve(manifest.chunk_count);
    for (uint32_t i = 0; i < manifest.chunk_count; ++i) {
        StorageChunkDescriptor descriptor;
        std::copy_n(bytes.begin() + offset, descriptor.chunk_id.size(), descriptor.chunk_id.begin());
        offset += descriptor.chunk_id.size();
        descriptor.ciphertext_size = Read32(bytes.data() + offset);
        offset += 4;
        manifest.chunks.push_back(descriptor);
    }
    std::copy_n(bytes.begin() + offset, manifest.commitment.size(), manifest.commitment.begin());
    if (!VerifyStoragePublicManifest(network_id, manifest)) return std::nullopt;
    return manifest;
}

StorageObjectCryptoContext::StorageObjectCryptoContext(
    const std::span<const unsigned char, 32> network_id,
    const StorageObjectPrivateMetadata& metadata,
    std::array<unsigned char, 32> object_key)
    : m_metadata{metadata}, m_object_key{object_key}
{
    std::copy(network_id.begin(), network_id.end(), m_network_id.begin());
    crypto::CleanseMemory(object_key.data(), object_key.size());
}

StorageObjectCryptoContext::~StorageObjectCryptoContext()
{
    crypto::CleanseMemory(m_object_key.data(), m_object_key.size());
}

StorageObjectCryptoContext::StorageObjectCryptoContext(StorageObjectCryptoContext&& other) noexcept
    : m_network_id{other.m_network_id}, m_metadata{other.m_metadata}, m_object_key{other.m_object_key}
{
    crypto::CleanseMemory(other.m_object_key.data(), other.m_object_key.size());
}

StorageObjectCryptoContext& StorageObjectCryptoContext::operator=(StorageObjectCryptoContext&& other) noexcept
{
    if (this != &other) {
        crypto::CleanseMemory(m_object_key.data(), m_object_key.size());
        m_network_id = other.m_network_id;
        m_metadata = other.m_metadata;
        m_object_key = other.m_object_key;
        crypto::CleanseMemory(other.m_object_key.data(), other.m_object_key.size());
    }
    return *this;
}

std::optional<StorageObjectCryptoContext> StorageObjectCryptoContext::Create(
    const std::span<const unsigned char, 32> network_id,
    const std::span<const unsigned char, 32> storage_master_key,
    const StorageObjectPrivateMetadata& metadata)
{
    if (IsZero(network_id) || IsZero(storage_master_key) || metadata.object_id == StorageObjectId{} ||
        metadata.salt == std::array<unsigned char, 32>{} || !GetChunkCount(metadata.plaintext_size)) return std::nullopt;
    auto object_key = DeriveObjectKey(storage_master_key, metadata);
    if (!object_key) return std::nullopt;
    StorageObjectCryptoContext context{network_id, metadata, *object_key};
    crypto::CleanseMemory(object_key->data(), object_key->size());
    return context;
}

uint32_t StorageObjectCryptoContext::ChunkCount() const
{
    const auto count = GetChunkCount(m_metadata.plaintext_size);
    return count.value_or(0);
}

std::optional<size_t> StorageObjectCryptoContext::ExpectedPlaintextChunkSize(const uint32_t index) const
{
    const uint32_t count = ChunkCount();
    if (count == 0 || index >= count) return std::nullopt;
    if (m_metadata.plaintext_size == 0) return size_t{0};
    if (index + 1 < count) return STORAGE_OBJECT_CHUNK_SIZE;
    const uint64_t preceding = uint64_t{index} * STORAGE_OBJECT_CHUNK_SIZE;
    return static_cast<size_t>(m_metadata.plaintext_size - preceding);
}

std::optional<StorageEncryptedChunk> StorageObjectCryptoContext::EncryptChunk(
    const uint32_t index, const std::span<const unsigned char> plaintext) const
{
    const auto expected_size = ExpectedPlaintextChunkSize(index);
    if (!expected_size || plaintext.size() != *expected_size) return std::nullopt;
    StorageEncryptedChunk chunk{.index = index, .ciphertext_and_tag = std::vector<unsigned char>(plaintext.size() + TAG_SIZE)};
    if (RAND_bytes(chunk.nonce.data(), static_cast<int>(chunk.nonce.size())) != 1) return std::nullopt;
    const auto aad = ChunkAad(m_network_id, m_metadata, ChunkCount(), index);
    if (!crypto::ChaCha20Poly1305Encrypt(m_object_key, chunk.nonce, aad, plaintext, chunk.ciphertext_and_tag) ||
        !ComputeChunkId(m_network_id, m_metadata.object_id, index, chunk.nonce,
            chunk.ciphertext_and_tag, chunk.chunk_id)) return std::nullopt;
    return chunk;
}

std::optional<std::vector<unsigned char>> StorageObjectCryptoContext::DecryptChunk(
    const StorageEncryptedChunk& chunk) const
{
    const auto expected_size = ExpectedPlaintextChunkSize(chunk.index);
    if (!expected_size || chunk.ciphertext_and_tag.size() != *expected_size + TAG_SIZE) return std::nullopt;
    StorageChunkId expected_id{};
    if (!ComputeChunkId(m_network_id, m_metadata.object_id, chunk.index, chunk.nonce,
            chunk.ciphertext_and_tag, expected_id) || expected_id != chunk.chunk_id) return std::nullopt;
    std::vector<unsigned char> plaintext(*expected_size);
    const auto aad = ChunkAad(m_network_id, m_metadata, ChunkCount(), chunk.index);
    if (!crypto::ChaCha20Poly1305Decrypt(m_object_key, chunk.nonce, aad,
            chunk.ciphertext_and_tag, plaintext)) return std::nullopt;
    return plaintext;
}

} // namespace cybou
