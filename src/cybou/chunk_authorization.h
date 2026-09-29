// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CHUNK_AUTHORIZATION_H
#define CYBOU_CHUNK_AUTHORIZATION_H

#include <cybou/root_publication.h>

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct AuthorizedChunk {
    ChunkId id{};

    friend bool operator==(const AuthorizedChunk&, const AuthorizedChunk&) = default;
};

struct ChunkAuthorizationProof {
    ChunkId chunk_id{};
    std::uint32_t leaf_index{0};
    std::uint32_t chunk_count{0};
    std::vector<ChunkId> siblings;
};

struct ChunkAuthorizationCommitment {
    ChunkId root{};
    std::uint32_t chunk_count{0};
    std::vector<ChunkAuthorizationProof> proofs;
};

struct ChunkAuthorizationSummary {
    ChunkId root{};
    std::uint32_t chunk_count{0};
};

/** Streaming Merkle accumulator; caller rejects duplicate IDs; memory is O(log N). */
class ChunkAuthorizationAccumulator final {
public:
    bool Add(const AuthorizedChunk& chunk);
    std::optional<ChunkAuthorizationSummary> Finish() const;

private:
    std::array<std::optional<ChunkId>, 32> m_frontier{};
    std::uint32_t m_chunk_count{0};
    bool m_failed{false};
};

std::optional<ChunkAuthorizationCommitment> BuildChunkAuthorizationCommitment(
    std::span<const AuthorizedChunk> chunks);

bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const AuthorizedChunk& chunk,
    std::uint32_t leaf_index,
    std::uint32_t chunk_count,
    std::span<const ChunkId> siblings);

/** Check the chunk inclusion path against the finalized publication commitment. */
bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkAuthorizationProof& proof);

} // namespace cybou

#endif // CYBOU_CHUNK_AUTHORIZATION_H
