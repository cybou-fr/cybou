// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ENCRYPTED_CHUNK_H
#define CYBOU_ENCRYPTED_CHUNK_H

#include <cybou/chunk_id.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

using ContentKey = std::array<unsigned char, 32>;

inline constexpr std::size_t ENCRYPTED_CHUNK_HEADER_SIZE{48};
inline constexpr std::size_t ENCRYPTED_CHUNK_MIN_STORED_BYTES{ENCRYPTED_CHUNK_HEADER_SIZE + 1024 + 16};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_PLAINTEXT_BYTES{512 * 1024 - 4};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_STORED_BYTES{
    ENCRYPTED_CHUNK_HEADER_SIZE + 512 * 1024 + 16};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_RANDOM_PADDING_BYTES{256 * 1024};

struct EncryptedChunk {
    ChunkId id{};
    std::vector<unsigned char> stored_bytes;
};

/** Create one random content key for a single immutable encrypted tree. */
std::optional<ContentKey> GenerateContentKey();

/** Pad and encrypt one bounded byte chunk. */
std::optional<EncryptedChunk> EncryptChunk(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    std::span<const unsigned char> plaintext);

/** Verify the address before authenticating and returning one bounded byte chunk. */
std::optional<std::vector<unsigned char>> DecryptChunk(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    const ChunkId& expected_id,
    std::span<const unsigned char> stored_bytes);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_H
