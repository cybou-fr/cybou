// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ENCRYPTED_CHUNK_H
#define CYBOU_ENCRYPTED_CHUNK_H

#include <cybou/canonical_cbor.h>
#include <cybou/chunk_id.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

using GraphContentKey = std::array<unsigned char, 32>;

inline constexpr std::size_t ENCRYPTED_CHUNK_HEADER_SIZE{49};
inline constexpr std::size_t ENCRYPTED_CHUNK_MIN_STORED_BYTES{49 + 1024 + 16};
inline constexpr std::size_t ENCRYPTED_CHUNK_MAX_STORED_BYTES{
    ENCRYPTED_CHUNK_HEADER_SIZE + 512 * 1024 + 16};

struct EncryptedChunk {
    ChunkId id{};
    std::vector<unsigned char> stored_bytes;
};

/** Create one random content key for a single immutable encrypted graph. */
std::optional<GraphContentKey> GenerateGraphContentKey();

/** Encode, pad, and encrypt one private CBOR node. */
std::optional<EncryptedChunk> EncryptGraphChunk(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char, 32> graph_key,
    const CborValue& node);

/** Verify the address before authenticating and decoding one node. */
std::optional<CborValue> DecryptGraphChunk(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char, 32> graph_key,
    const ChunkId& expected_id,
    std::span<const unsigned char> stored_bytes);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_H
