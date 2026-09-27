// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_STORAGE_CRYPTO_H
#define CYBOU_STORAGE_CRYPTO_H

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

using StorageObjectId = std::array<unsigned char, 32>;
using StorageChunkId = std::array<unsigned char, 32>;

inline constexpr size_t STORAGE_OBJECT_CHUNK_SIZE{1U << 20};
inline constexpr uint32_t STORAGE_OBJECT_MAX_CHUNKS{1U << 16};
inline constexpr uint64_t STORAGE_OBJECT_MAX_BYTES{uint64_t{STORAGE_OBJECT_CHUNK_SIZE} * STORAGE_OBJECT_MAX_CHUNKS};
inline constexpr size_t STORAGE_PUBLIC_MANIFEST_MAX_BYTES{4 + 32 + 4 +
    static_cast<size_t>(STORAGE_OBJECT_MAX_CHUNKS) * 36 + 32};

/** Private per-object metadata. Persist inside the encrypted Files manifest. */
struct StorageObjectPrivateMetadata {
    StorageObjectId object_id{};
    std::array<unsigned char, 32> salt{};
    uint32_t key_epoch{0};
    uint64_t plaintext_size{0};
};

/** Public chunk data safe for an opaque storage provider to retain. */
struct StorageEncryptedChunk {
    uint32_t index{0};
    std::array<unsigned char, 12> nonce{};
    StorageChunkId chunk_id{};
    std::vector<unsigned char> ciphertext_and_tag;
};

struct StorageChunkDescriptor {
    StorageChunkId chunk_id{};
    uint32_t ciphertext_size{0};
    friend bool operator==(const StorageChunkDescriptor&, const StorageChunkDescriptor&) = default;
};

/** Provider-visible manifest. It contains no filename, path, key, or plaintext size. */
struct StoragePublicManifest {
    StorageObjectId object_id{};
    uint32_t chunk_count{0};
    std::vector<StorageChunkDescriptor> chunks;
    StorageChunkId commitment{};
};

/** Create fresh random object identity/salt. The key epoch is selected by the caller. */
std::optional<StorageObjectPrivateMetadata> CreateStorageObjectMetadata(
    uint64_t plaintext_size, uint32_t key_epoch);

/** Build and validate the provider-visible commitment from ordered ciphertext chunks. */
std::optional<StoragePublicManifest> BuildStoragePublicManifest(
    std::span<const unsigned char, 32> network_id,
    const StorageObjectId& object_id,
    std::span<const StorageEncryptedChunk> chunks);
bool VerifyStoragePublicManifest(
    std::span<const unsigned char, 32> network_id,
    const StoragePublicManifest& manifest);
bool ComputeStorageChunkId(
    std::span<const unsigned char, 32> network_id,
    const StorageObjectId& object_id,
    uint32_t index,
    std::span<const unsigned char, 12> nonce,
    std::span<const unsigned char> ciphertext_and_tag,
    StorageChunkId& chunk_id);
std::optional<std::vector<unsigned char>> EncodeStoragePublicManifest(
    std::span<const unsigned char, 32> network_id,
    const StoragePublicManifest& manifest);
std::optional<StoragePublicManifest> DecodeStoragePublicManifest(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char> bytes);

/**
 * Per-object streaming crypto context. The caller can process one bounded chunk
 * at a time and persist the exact ciphertext before sending it to storage peers.
 */
class StorageObjectCryptoContext final
{
public:
    static std::optional<StorageObjectCryptoContext> Create(
        std::span<const unsigned char, 32> network_id,
        std::span<const unsigned char, 32> storage_master_key,
        const StorageObjectPrivateMetadata& metadata);

    ~StorageObjectCryptoContext();
    StorageObjectCryptoContext(StorageObjectCryptoContext&& other) noexcept;
    StorageObjectCryptoContext& operator=(StorageObjectCryptoContext&& other) noexcept;
    StorageObjectCryptoContext(const StorageObjectCryptoContext&) = delete;
    StorageObjectCryptoContext& operator=(const StorageObjectCryptoContext&) = delete;

    const StorageObjectPrivateMetadata& Metadata() const { return m_metadata; }
    uint32_t ChunkCount() const;
    std::optional<size_t> ExpectedPlaintextChunkSize(uint32_t index) const;
    std::optional<StorageEncryptedChunk> EncryptChunk(
        uint32_t index, std::span<const unsigned char> plaintext) const;
    std::optional<std::vector<unsigned char>> DecryptChunk(
        const StorageEncryptedChunk& chunk) const;

private:
    StorageObjectCryptoContext(
        std::span<const unsigned char, 32> network_id,
        const StorageObjectPrivateMetadata& metadata,
        std::array<unsigned char, 32> object_key);

    std::array<unsigned char, 32> m_network_id{};
    StorageObjectPrivateMetadata m_metadata{};
    std::array<unsigned char, 32> m_object_key{};
};

} // namespace cybou

#endif // CYBOU_STORAGE_CRYPTO_H
