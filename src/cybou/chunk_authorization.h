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
    std::uint32_t leaf_index{0};
    std::vector<ChunkId> siblings;
};

/** O(N) Merkle nodes; creates only the requested O(log N) inclusion path. */
class ChunkAuthorizationTree {
public:
    ChunkId root{};
    std::uint32_t chunk_count{0};
    const ChunkId& Root() const { return root; }
    std::uint32_t ChunkCount() const { return chunk_count; }
    ChunkAuthorizationProof Proof(std::uint32_t leaf_index) const;
private:
    std::vector<std::vector<ChunkId>> m_levels;
    friend std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(std::span<const AuthorizedChunk>);
};

struct ChunkAuthorizationSummary {
    ChunkId root{};
    std::uint32_t chunk_count{0};
};

ChunkId ChunkAuthorizationLeafHash(const ChunkId& chunk_id);
ChunkId ChunkAuthorizationNodeHash(const ChunkId& left, const ChunkId& right);

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

std::optional<ChunkAuthorizationTree> BuildChunkAuthorizationTree(
    std::span<const AuthorizedChunk> chunks);

bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const ChunkId& chunk_id,
    std::uint32_t leaf_index,
    std::uint32_t chunk_count,
    std::span<const ChunkId> siblings);

/** Check the chunk inclusion path against the finalized publication commitment. */
bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkId& chunk_id,
    const ChunkAuthorizationProof& proof);

} // namespace cybou

#endif // CYBOU_CHUNK_AUTHORIZATION_H
