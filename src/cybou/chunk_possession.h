// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_CHUNK_POSSESSION_H
#define CYBOU_CHUNK_POSSESSION_H
#include <cybou/chunk_id.h>
#include <vector>
#include <optional>
#include <cstdint>
namespace cybou {
/** Native BLAKE3 tree inclusion, preserving the exact full ChunkID. Bottom-up sibling CVs. */
struct ChunkPossessionProof {
    uint64_t stored_bytes{0};
    uint32_t leaf_index{0};
    std::vector<unsigned char> leaf_bytes;
    std::vector<std::array<uint32_t, 8>> siblings;
    friend bool operator==(const ChunkPossessionProof&, const ChunkPossessionProof&) = default;
};
std::optional<ChunkPossessionProof> BuildChunkPossessionProof(std::span<const unsigned char> bytes, uint32_t leaf);
bool VerifyChunkPossessionProof(const ChunkId& id, const ChunkPossessionProof& proof);
std::optional<std::vector<unsigned char>> SerializeChunkPossessionProof(const ChunkPossessionProof& proof);
std::optional<ChunkPossessionProof> DeserializeChunkPossessionProof(std::span<const unsigned char> bytes);
} // namespace cybou
#endif
