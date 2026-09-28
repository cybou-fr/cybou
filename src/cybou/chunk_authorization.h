// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CHUNK_AUTHORIZATION_H
#define CYBOU_CHUNK_AUTHORIZATION_H

#include <cybou/root_publication.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace cybou {

struct ChunkAuthorizationProof {
    ChunkId chunk_id{};
    std::uint64_t stored_bytes{0};
    std::uint32_t leaf_index{0};
    std::uint32_t chunk_count{0};
    std::uint64_t authorized_stored_bytes{0};
    std::vector<ChunkId> siblings;
};

struct ChunkAuthorizationCommitment {
    ChunkId root{};
    std::uint32_t chunk_count{0};
    std::uint64_t authorized_stored_bytes{0};
    std::vector<ChunkAuthorizationProof> proofs;
};

std::optional<ChunkAuthorizationCommitment> BuildChunkAuthorizationCommitment(
    std::span<const AuthorizedChunk> chunks);

bool VerifyChunkAuthorizationPath(
    const ChunkId& expected_root,
    const AuthorizedChunk& chunk,
    std::uint32_t leaf_index,
    std::uint32_t chunk_count,
    std::span<const ChunkId> siblings);

/** Check both the inclusion path and the aggregate fields committed by publication. */
bool VerifyChunkAuthorizationProof(
    const RootPublication& publication,
    const ChunkAuthorizationProof& proof);

} // namespace cybou

#endif // CYBOU_CHUNK_AUTHORIZATION_H
