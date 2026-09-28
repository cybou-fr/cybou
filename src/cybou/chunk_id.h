// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_CHUNK_ID_H
#define CYBOU_CHUNK_ID_H

#include <array>
#include <span>

namespace cybou {

using ChunkId = std::array<unsigned char, 32>;

/** Address stored encrypted bytes by their full 256-bit BLAKE3 digest. */
ChunkId ComputeChunkId(std::span<const unsigned char> stored_encrypted_bytes);

} // namespace cybou

#endif // CYBOU_CHUNK_ID_H
