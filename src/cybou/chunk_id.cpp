// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/chunk_id.h>

#include <blake3.h>

namespace cybou {

Blake3Digest ComputeBlake3Digest(const std::span<const unsigned char> bytes)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, bytes.data(), bytes.size());

    Blake3Digest result{};
    blake3_hasher_finalize(&hasher, result.data(), result.size());
    return result;
}

ChunkId ComputeChunkId(const std::span<const unsigned char> stored_encrypted_bytes)
{
    return ComputeBlake3Digest(stored_encrypted_bytes);
}

} // namespace cybou
