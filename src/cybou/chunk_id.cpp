// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Реализация BLAKE3-хэширования для ChunkId и родственных идентификаторов.

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

Blake3Digest ComputeBlake3Digest(const std::span<const std::span<const unsigned char>> parts)
{
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    for (const auto part : parts) {
        blake3_hasher_update(&hasher, part.data(), part.size());
    }

    Blake3Digest result{};
    blake3_hasher_finalize(&hasher, result.data(), result.size());
    return result;
}

ChunkId ComputeChunkId(const std::span<const unsigned char> stored_encrypted_bytes)
{
    return ComputeBlake3Digest(stored_encrypted_bytes);
}

} // namespace cybou
