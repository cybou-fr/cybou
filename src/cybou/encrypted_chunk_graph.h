// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ENCRYPTED_CHUNK_GRAPH_H
#define CYBOU_ENCRYPTED_CHUNK_GRAPH_H

#include <cybou/encrypted_chunk.h>

#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

inline constexpr std::size_t ENCRYPTED_GRAPH_LEAF_BYTES{180 * 1024};
inline constexpr std::size_t ENCRYPTED_GRAPH_MAX_CHILDREN{128};
inline constexpr std::size_t ENCRYPTED_GRAPH_MAX_CHUNKS{65536};
inline constexpr std::size_t ENCRYPTED_GRAPH_MAX_DEPTH{16};
inline constexpr std::size_t ENCRYPTED_GRAPH_MAX_PLAINTEXT_BYTES{256 * 1024 * 1024};

struct EncryptedChunkGraph {
    ChunkId root_id{};
    GraphContentKey content_key{};
    std::size_t plaintext_bytes{0};
    std::vector<EncryptedChunk> chunks;
};

using EncryptedChunkLookup = std::function<std::optional<std::vector<unsigned char>>(const ChunkId&)>;

/** Build an opaque byte graph. The input should already be app-private encoded data. */
std::optional<EncryptedChunkGraph> BuildEncryptedChunkGraph(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char> plaintext);

/** Fetch, validate, decrypt, and reconstruct a graph under fixed resource limits. */
std::optional<std::vector<unsigned char>> FetchEncryptedChunkGraph(
    std::span<const unsigned char, 32> network_id,
    std::span<const unsigned char, 32> content_key,
    const ChunkId& root_id,
    const EncryptedChunkLookup& lookup);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_GRAPH_H
