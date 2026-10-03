// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_ENCRYPTED_CHUNK_TREE_H
#define CYBOU_ENCRYPTED_CHUNK_TREE_H

#include <cybou/protocol_limits.h>
#include <cybou/encrypted_chunk.h>
#include <cybou/chunk_authorization.h>

#include <functional>
#include <limits>
#include <optional>
#include <span>

namespace cybou {

inline constexpr std::size_t ENCRYPTED_TREE_MAX_CHILDREN{128};
inline constexpr std::size_t ENCRYPTED_TREE_MAX_DEPTH{32};
inline constexpr std::size_t ENCRYPTED_TREE_DATA_MIN_BYTES{160 * 1024};
inline constexpr std::size_t ENCRYPTED_TREE_DATA_MAX_BYTES{320 * 1024};
inline constexpr std::size_t ENCRYPTED_TREE_ROOT_PRIVATE_METADATA_MAX_BYTES{240 * 1024};

struct EncryptedTreeSummary {
    ChunkId root_chunk_id{};
    ChunkId chunk_authorization_root{};
    ContentKey content_key{};
    std::uint64_t plaintext_bytes{0};
    std::uint64_t chunk_count{0};
};

// A read returns nullopt on error, zero at EOF, or 1..output.size() bytes.
using EncryptedTreeSource = std::function<std::optional<std::size_t>(std::span<unsigned char> output)>;
// Atomically stage (leaf_index, chunk) before returning true; reject duplicate IDs.
using EncryptedTreeStage = std::function<bool(std::uint32_t leaf_index, const EncryptedChunk& chunk)>;
using EncryptedChunkLookup = std::function<std::optional<std::vector<unsigned char>>(const ChunkId&)>;
using EncryptedTreeSink = std::function<bool(std::span<const unsigned char> plaintext)>;
// Receives opaque bounded application metadata from ROOT after decryption.
using EncryptedTreeRootMetadataSink = std::function<bool(std::span<const unsigned char> metadata)>;
// Must atomically persist-and-accept each new ID (for example, a local SQLite unique key).
using EncryptedTreeVisit = std::function<bool(const ChunkId& chunk_id)>;

/** Stream source bytes into local encrypted-chunk staging; no network or whole-file buffer is used. */
std::optional<EncryptedTreeSummary> BuildEncryptedChunkTree(
    std::span<const unsigned char, 32> network_binding,
    const EncryptedTreeSource& source,
    const EncryptedTreeStage& stage,
    std::span<const unsigned char> private_root_metadata = {});

/**
 * Fetch, authenticate, decrypt, and stream an ordered ROOT/INDEX/DATA tree to sink.
 * Sink may receive partial output on later failure and should write to local staging.
 */
std::optional<std::uint64_t> FetchEncryptedChunkTree(
    std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key,
    const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup,
    const EncryptedTreeRootMetadataSink& root_metadata_sink,
    const EncryptedTreeVisit& visit,
    const EncryptedTreeSink& sink,
    std::uint64_t max_output_bytes);

/** Enumerates a tree's root/index/data ChunkIDs without downloading DATA chunks. */
bool EnumerateEncryptedTreeChunks(std::span<const unsigned char, 32> network_binding,
    std::span<const unsigned char, 32> content_key, const ChunkId& root_chunk_id,
    const EncryptedChunkLookup& lookup, const EncryptedTreeVisit& visit);

} // namespace cybou

#endif // CYBOU_ENCRYPTED_CHUNK_TREE_H
